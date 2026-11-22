-- MAME 0.289, makaimurg, default DIP switches.
-- Logs every part of the dragon (enemy type $0C) every frame, so the way the body follows the head
-- can be read from the log. The player walks right until X reaches REVG_DRAGON_STOPX and then
-- stands still, kept invulnerable with the hit timer $AD.
-- REVG_DRAGON_SECTION picks the section (default 3), REVG_DRAGON_PAGE (hex) the start page
-- (default 58), REVG_DRAGON_STOPX the X where the player stops (default 6330; 3300 suits section 2
-- from page 4c), REVG_DRAGON_FRAMES the play length (default 2000), REVG_DRAGON_FIRE=1 fires every
-- 8 frames, REVG_DRAGON_RANK=<n> sets the difficulty counter $61 when the section is chosen, and
-- REVG_DRAGON_EVERY=<n> prints the parts only every n frames.
-- Output lines (t counts frames of play; Y is up):
--   f,<t>,p=<player x,y>,cam=<camera x,y>,b5=,b6=,a7=,<part> <part> ...
--     each part is <number +$1B>:<x>,<y>,<direction -$02>,<state.step>,f<facing +$14>,d<kind +$0D>,hp<+$10>
--   shot,<t>,type=,x=,y=,dir=   (an enemy projectile appeared)
--   shot16,<t>,dx=,dy=          (how far it went in 16 frames)
--   shotfree,<t>,life=          (it was freed)
--   sound,<t>,<request>
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = tonumber(os.getenv("REVG_DRAGON_SECTION") or "3")
local page = tonumber(os.getenv("REVG_DRAGON_PAGE") or "58", 16)
local stopx = tonumber(os.getenv("REVG_DRAGON_STOPX") or "6330")
local frames = tonumber(os.getenv("REVG_DRAGON_FRAMES") or "2000")
local firing = os.getenv("REVG_DRAGON_FIRE") == "1"
local rank = tonumber(os.getenv("REVG_DRAGON_RANK") or "")
local every = tonumber(os.getenv("REVG_DRAGON_EVERY") or "1")

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local right, fire = field(":P1", 0x01), field(":P1", 0x10)
local frame, selected, playing_at = 0, false, nil
local entry = 0x6cee + 4 * section

local function word(address)
    return mem:read_u8(address) << 8 | mem:read_u8(address + 1)
end

local function signed(value)
    return value >= 0x8000 and value - 0x10000 or value
end

revg_dragon_page = mem:install_read_tap(entry, entry + 1, "start page", function(address)
    if not selected or cpu.state.PC.value ~= 0x6d39 then return end
    if address == entry then return 0x40 + (page >> 8) end
    return page & 0xff
end)

revg_dragon_sound = mem:install_write_tap(0x0140, 0x017f, "sound queue", function(_, value)
    local pc = cpu.state.PC.value
    if pc < 0x7778 or pc > 0x777a or not playing_at or value == 0xff then return end
    print(string.format("sound,%d,%02x", frame - playing_at, value))
end)

local shots = {}

revg_dragon_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
        if rank then mem:write_u8(0x61, rank) end
    end
    if selected and not playing_at and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 1 and mem:read_u8(0x0e) == 0 then playing_at = frame end
    assert(frame < 3000 + frames, "Play did not start")
    if not playing_at then return end
    local t = frame - playing_at
    mem:write_u8(0xad, 0x20)
    local px, py = word(0x0506), word(0x0509)
    right:set_value(px < stopx and 1 or 0)
    fire:set_value(firing and t % 8 < 2 and 1 or 0)
    local parts = {}
    for i = 0, 39 do
        local x = 0x0890 + i * 0x30
        if mem:read_u8(x - 0x10) ~= 0 and mem:read_u8(x + 5) == 0x0c then
            parts[#parts + 1] = string.format("%d:%d,%d,%02x,%04x.%02x,f%d,d%d,hp%d", mem:read_u8(x + 0x1b),
                signed(word(x - 0x0a)), signed(word(x - 0x07)), mem:read_u8(x - 2), word(x - 0x0d),
                mem:read_u8(x - 0x0b), mem:read_u8(x + 0x14), mem:read_u8(x + 0x0d), mem:read_u8(x + 0x10))
        end
    end
    if #parts > 0 and t % every == 0 then
        print(string.format("f,%d,p=%d,%d,cam=%d,%d,b5=%d,b6=%d,a7=%d,%s", t, px, py, word(0x9c), word(0x9e),
            mem:read_u8(0xb5), mem:read_u8(0xb6), mem:read_u8(0xa7), table.concat(parts, " ")))
    end
    for i = 0, 5 do
        local x = 0x13d0 + i * 0x20
        if mem:read_u8(x - 0x10) ~= 0 then
            local s = shots[x]
            local sx, sy = signed(word(x - 0x0a)), signed(word(x - 0x07))
            if not s then
                shots[x] = { t = t, x = sx, y = sy }
                print(string.format("shot,%d,type=%02x,x=%d,y=%d,dir=%02x", t, mem:read_u8(x + 5), sx, sy,
                    mem:read_u8(x - 2)))
            elseif t - s.t == 16 then
                print(string.format("shot16,%d,dx=%d,dy=%d", s.t, sx - s.x, sy - s.y))
            end
        else
            if shots[x] then print(string.format("shotfree,%d,life=%d", t, t - shots[x].t)) end
            shots[x] = nil
        end
    end
    if t >= frames then
        print(string.format("end,%d", t))
        machine:exit()
    end
end)
