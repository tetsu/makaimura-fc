-- MAME 0.289, makaimurg, default DIP switches.
-- Records which object requests each sound command during play of one section.
-- REVG_SOURCES_SECTION picks the section, REVG_SOURCES_PAGE (hex, optional) the start page,
-- REVG_SOURCES_FRAMES the play length (default 3600). The player holds right, jumps every
-- 64 frames, fires every 8 frames, and is kept invulnerable to contact by the hit timer $AD.
-- REVG_SOURCES_SEED=<n> instead picks a random direction (right, left, up, down or none) and
-- jump every 16 frames from a fixed pseudo-random sequence. REVG_SOURCES_HITS=1 lets enemies hit.
-- Each (command, caller, object type, variant) is printed once:
--   source,<t>,cmd=<cc>,ret=<return address after the call>,obj=<enemy|support|player|other>,type=<tt>,variant=<vv>,state=<ssss>,x=,y=
-- X points at the object being updated: enemy slots are $0890 + n*$30 (40 slots),
-- support slots $1010 + n*$20 (30 slots) and the player is $0510.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = tonumber(os.getenv("REVG_SOURCES_SECTION") or "0")
local page = os.getenv("REVG_SOURCES_PAGE") and tonumber(os.getenv("REVG_SOURCES_PAGE"), 16)
local frames = tonumber(os.getenv("REVG_SOURCES_FRAMES") or "3600")
local still = os.getenv("REVG_SOURCES_STILL") == "1"
local seed = tonumber(os.getenv("REVG_SOURCES_SEED") or "")
local hits = os.getenv("REVG_SOURCES_HITS") == "1"
-- REVG_SOURCES_SNAP=<hex command> saves sources-<section>-<command>.png when that command is first
-- requested, and a second picture 20 frames later.
local snap = os.getenv("REVG_SOURCES_SNAP") and tonumber(os.getenv("REVG_SOURCES_SNAP"), 16)
local snap_at

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local right, jump, fire = field(":P1", 0x01), field(":P1", 0x20), field(":P1", 0x10)
local left, down, up = field(":P1", 0x02), field(":P1", 0x04), field(":P1", 0x08)
local directions = { right, left, up, down }
local choice, jumping = 1, false

local function next_random()
    seed = (seed * 1103515245 + 12345) % 0x80000000
    return seed >> 16
end
local frame, selected, playing_at = 0, false, nil
local seen = {}
local entry = 0x6cee + 4 * section

local function word(address)
    return mem:read_u8(address) << 8 | mem:read_u8(address + 1)
end

if page then
    revg_sources_page = mem:install_read_tap(entry, entry + 1, "start page", function(address)
        if not selected or cpu.state.PC.value ~= 0x6d39 then return end
        if address == entry then return 0x40 + (page >> 8) end
        return page & 0xff
    end)
end

local function object(x)
    if x >= 0x0890 and x < 0x0890 + 40 * 0x30 and (x - 0x0890) % 0x30 == 0 then return "enemy" end
    if x >= 0x1010 and x < 0x1010 + 30 * 0x20 and (x - 0x1010) % 0x20 == 0 then return "support" end
    if x == 0x0510 then return "player" end
    return "other"
end

-- $776D/$7775 store the command at $7778 (the same tap as log-revg-sound.lua).
revg_sources_queue = mem:install_write_tap(0x0140, 0x017f, "sound queue", function(_, value)
    local pc = cpu.state.PC.value
    if pc < 0x7778 or pc > 0x777a or not playing_at or value == 0xff then return end
    local s = cpu.state.S.value
    -- The stub ($779E-$78CB) was called with JSR, so the word above $776D's own return is the caller.
    local caller = mem:read_u16(s)
    if caller >= 0x7700 and caller < 0x78d0 then caller = mem:read_u16(s + 2) end
    local x = cpu.state.X.value
    local kind = object(x)
    local key = string.format("%02x%04x%s%02x%02x", value, caller, kind, mem:read_u8(x + 5), mem:read_u8(x - 0x0e))
    if seen[key] then return end
    seen[key] = true
    if value == snap and not snap_at then snap_at = frame end
    print(string.format("source,%d,cmd=%02x,ret=%04x,obj=%s,type=%02x,variant=%02x,state=%04x,x=%d,y=%d,section=%d,slot=%04x",
        frame - playing_at, value, caller, kind, mem:read_u8(x + 5), mem:read_u8(x - 0x0e),
        word(x - 0x0d), word(x - 0x0a), word(x - 0x07), mem:read_u8(0x72), x))
end)

revg_sources_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
    end
    if selected and not playing_at and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 1 and mem:read_u8(0x0e) == 0 then playing_at = frame end
    if snap_at and (frame == snap_at + 1 or frame == snap_at + 20) then
        machine.screens[":screen"]:snapshot(string.format("sources-%d-%02x%s.png", section, snap,
            frame == snap_at + 1 and "" or "-later"))
    end
    if playing_at then
        local t = frame - playing_at
        if not hits then mem:write_u8(0xad, 0x20) end
        if seed then
            if t % 16 == 0 then
                local r = next_random()
                choice, jumping = r % 6, (r >> 4) % 3 == 0
            end
            for i, item in ipairs(directions) do item:set_value(choice == i and 1 or 0) end
            jump:set_value(jumping and t % 16 < 4 and 1 or 0)
        else
            right:set_value(still and 0 or 1)
            jump:set_value(not still and t % 64 < 4 and 1 or 0)
        end
        fire:set_value(t % 8 < 2 and 1 or 0)
        if t >= frames then
            print(string.format("end,%d,player_x=%d", t, word(0x0506)))
            machine:exit()
        end
    end
    assert(frame < 3000 + frames, "Play did not start")
end)
