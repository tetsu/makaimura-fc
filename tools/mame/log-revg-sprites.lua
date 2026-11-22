-- MAME 0.289, makaimurg, default DIP switches.
-- Records which animation frames each object draws, for tools/mame/extract-revg-sprites.py.
-- The sprite list is built by $F6F2 every frame: it walks the object pools below, and each object
-- with its visible flag -$0F set is drawn from its shape -$01 and its frame pointer +$03.
-- REVG_SPRITES_SECTION picks the section (default 0), REVG_SPRITES_PAGE (hex, optional) the start
-- page and REVG_SPRITES_FRAMES the play length (default 3600). The player holds right, jumps every
-- 64 frames and fires every 8 frames, kept invulnerable by the hit timer $AD.
-- REVG_SPRITES_SEED=<n> instead picks a random direction and jump every 16 frames from a fixed
-- pseudo-random sequence. REVG_SPRITES_HITS=1 lets enemies hit. REVG_SPRITES_STILL=1 stands still.
-- REVG_SPRITES_WEAPON=<n> sets the weapon $73 when play starts. REVG_SPRITES_NOFIRE=1 never fires, so
-- enemies stay on screen.
-- REVG_SPRITES_PASSIVE=1 only logs while another script plays: an -autoboot_script that runs
-- dofile("tools/mame/measure-revg-magician.lua") and then dofile("tools/mame/log-revg-sprites.lua").
-- The other options are then ignored.
-- REVG_SPRITES_CHECK=<n> also prints the built list every n frames, to check the decoder, and
-- REVG_SPRITES_SNAP=1 saves the screen of that frame and the next two in snap/revg-sprites/.
-- Output lines:
--   frame,<t>,pool=<name>,state=<ssss>,section=<$72>,type=<tt>,variant=<vv>,shape=<s>,ptr=<pppp>
--     printed once for each new (pool, section, type, variant, shape, frame pointer)
-- REVG_SPRITES_LOAD=1 prints the drawn objects of every frame, for the load subcommand:
--   load,<t>,cam=<camera x>,<camera y low>,builder=<bbbb>,budget=<n>,free=<free enemy slots $93>,
--     difficulty=<$9A>,objs=<pool:type:shape:ptr:x:ylow ...>
--   check,<t>,cam=<camera x>,<camera y low>,builder=<return address of $F6E4>,budget=<n>,
--     objs=<pool:shape:ptr:x:ylow ...>,
--     ram=<hex of sprite RAM $1E00-$1FFF>,pal=<$39xx $38xx byte pairs of colours $40-$7F>
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = tonumber(os.getenv("REVG_SPRITES_SECTION") or "0")
local page = os.getenv("REVG_SPRITES_PAGE") and tonumber(os.getenv("REVG_SPRITES_PAGE"), 16)
local frames = tonumber(os.getenv("REVG_SPRITES_FRAMES") or "3600")
local still = os.getenv("REVG_SPRITES_STILL") == "1"
local seed = tonumber(os.getenv("REVG_SPRITES_SEED") or "")
local hits = os.getenv("REVG_SPRITES_HITS") == "1"
local weapon = tonumber(os.getenv("REVG_SPRITES_WEAPON") or "")
local check = tonumber(os.getenv("REVG_SPRITES_CHECK") or "")
local snap = os.getenv("REVG_SPRITES_SNAP") == "1"
local passive = os.getenv("REVG_SPRITES_PASSIVE") == "1"
local load = os.getenv("REVG_SPRITES_LOAD") == "1"
local hold_fire = os.getenv("REVG_SPRITES_NOFIRE") == "1"
local snap_from

-- The pools in the order $F6F2 draws them.
local pools = {
    { name = "extra", base = 0x15e2, count = 2, stride = 0x30 },
    { name = "weapon", base = 0x0540, count = 4, stride = 0x40 },
    { name = "enemyshot", base = 0x13d0, count = 6, stride = 0x20 },
    { name = "enemy", base = 0x0890, count = 40, stride = 0x30 },
    { name = "support", base = 0x1010, count = 30, stride = 0x20 },
    -- $F71F and $F725 draw these two without looking at the visible flag.
    { name = "player", base = 0x0510, count = 1, stride = 0, always = true },
    { name = "final", base = 0x15c2, count = 1, stride = 0, section = 6, always = true },
}

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
    revg_sprites_page = mem:install_read_tap(entry, entry + 1, "start page", function(address)
        if not selected or cpu.state.PC.value ~= 0x6d39 then return end
        if address == entry then return 0x40 + (page >> 8) end
        return page & 0xff
    end)
end

local function drawn(pool)
    local list = {}
    if pool.section and mem:read_u8(0x72) ~= pool.section then return list end
    for i = 0, pool.count - 1 do
        local x = pool.base + i * pool.stride
        if pool.always or mem:read_u8(x - 0x0f) ~= 0 then list[#list + 1] = x end
    end
    return list
end

-- $F6E4 sets the sprite budget <$29. Its return address tells which list is built: $F6F4 for $F6F2, which
-- draws every pool; $714E, $7257, $792D and $7D4D draw a few objects in their own order for special scenes.
local check_due, builder, built_at = false, 0, nil
revg_sprites_start = mem:install_write_tap(0x0029, 0x0029, "sprite budget set", function()
    local pc = cpu.state.PC.value
    if pc >= 0xf6ef and pc <= 0xf6f1 then builder = mem:read_u16(cpu.state.S.value) end
end)

-- $F70F reads the sprite budget <$29 once every object has been drawn.
revg_sprites_built = mem:install_read_tap(0x0029, 0x0029, "sprite budget", function()
    local pc = cpu.state.PC.value
    -- The loop at $F70F reads the budget once for each entry it hides; act on the first read only.
    if pc < 0xf70f or pc > 0xf711 or not playing_at or built_at == frame then return end
    built_at = frame
    local t = frame - playing_at
    local cam_x, cam_y = word(0x9c), mem:read_u8(0x9f)
    for _, pool in ipairs(pools) do
        for _, x in ipairs(drawn(pool)) do
            local key = string.format("%s,section=%d,type=%02x,variant=%02x,shape=%d,ptr=%04x", pool.name,
                mem:read_u8(0x72), mem:read_u8(x + 5), mem:read_u8(x - 0x0e), mem:read_u8(x - 1), word(x + 3))
            if not seen[key] then
                seen[key] = true
                local pool_name, rest = key:match("^([^,]+),(.*)$")
                print(string.format("frame,%d,pool=%s,state=%04x,%s", t, pool_name, word(x - 0x0d), rest))
            end
        end
    end
    if load then
        local objs = {}
        for _, pool in ipairs(pools) do
            for _, x in ipairs(drawn(pool)) do
                objs[#objs + 1] = string.format("%s:%02x:%d:%04x:%d:%d", pool.name, mem:read_u8(x + 5),
                    mem:read_u8(x - 1), word(x + 3), word(x - 0x0a), mem:read_u8(x - 0x06))
            end
        end
        print(string.format("load,%d,cam=%d,%d,builder=%04x,budget=%d,free=%d,difficulty=%02x,objs=%s", t, cam_x,
            cam_y, builder, mem:read_u8(0x29), mem:read_u8(0x93), mem:read_u8(0x9a), table.concat(objs, " ")))
    end
    if check_due then
        check_due = false
        local objs = {}
        for _, pool in ipairs(pools) do
            for _, x in ipairs(drawn(pool)) do
                objs[#objs + 1] = string.format("%s:%d:%04x:%d:%d", pool.name, mem:read_u8(x - 1), word(x + 3),
                    word(x - 0x0a), mem:read_u8(x - 0x06))
            end
        end
        local ram, pal = {}, {}
        for a = 0x1e00, 0x1fff do ram[#ram + 1] = string.format("%02x", mem:read_u8(a)) end
        -- Sprite colours $40-$7F: BBBBxxxx in "palette" ($3900) and RRRRGGGG in "palette_ext" ($3800).
        local base, ext = machine.memory.shares[":palette"], machine.memory.shares[":palette_ext"]
        for i = 0x40, 0x7f do pal[#pal + 1] = string.format("%02x%02x", base:read_u8(i), ext:read_u8(i)) end
        print(string.format("check,%d,cam=%d,%d,builder=%04x,budget=%d,objs=%s,ram=%s,pal=%s", t, cam_x, cam_y,
            builder, mem:read_u8(0x29), table.concat(objs, " "), table.concat(ram), table.concat(pal)))
        if snap then snap_from = frame end
    end
end)

revg_sprites_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if passive then
        -- Another script plays; only notice when play starts.
        if not playing_at and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
            and mem:read_u8(0x0b) == 1 and mem:read_u8(0x0e) == 0 then playing_at = frame end
        return
    end
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
    end
    if selected and not playing_at and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 1 and mem:read_u8(0x0e) == 0 then
        playing_at = frame
        if weapon then mem:write_u8(0x73, weapon) end
    end
    assert(frame < 3000 + frames, "Play did not start")
    if snap_from and frame - snap_from <= 2 then
        machine.screens[":screen"]:snapshot(string.format("revg-sprites/s%d-%d-%d.png", section,
            snap_from - playing_at, frame - snap_from))
    end
    if not playing_at then return end
    local t = frame - playing_at
    if not hits then mem:write_u8(0xad, 0x20) end
    if check and t % check == 0 then check_due = true end
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
    fire:set_value(not hold_fire and t % 8 < 2 and 1 or 0)
    if t >= frames then
        print(string.format("end,%d,player_x=%d", t, word(0x0506)))
        machine:exit()
    end
end)
