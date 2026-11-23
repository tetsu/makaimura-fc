-- MAME 0.289, makaimurg, default DIP switches.
-- Captures what tools/convert-revg-frontend.py turns into the FC title and opening:
--   * the title before a coin (snapshot revg-frontend/title-attract.png, at frame 540) and after
--     one (revg-frontend/title-credit.png, at frame 701 with the background layer black; the
--     coin goes in at 560);
--   * the opening demo after 1P START (frame 760): every frame until the play list builder
--     $F6F4 takes over from the opening's own builder $792D, the four objects the opening
--     draws ($0510 Arthur, $15C2 the princess, $0890 the demon, $0540 the armour; see $7907)
--     and the sum of the background colours, which shows the fade.
-- Run with -snapshot_directory <dir>; the opening goes to standard output:
--   op,<t>,<bg colour sum>,<addr>:<visible -$0F>:<shape -$01>:<frame +$03>:<x -$0A>:<y -$06> x4
--   end,<t>
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local objects = { 0x0510, 0x15c2, 0x0890, 0x0540 }
local frame, started, builder, done, saved = 0, nil, 0, false, nil

local function word(address)
    return mem:read_u8(address) << 8 | mem:read_u8(address + 1)
end

-- $F6E4 sets the sprite budget <$29; its return address says which list builder runs.
revg_frontend_budget = mem:install_write_tap(0x0029, 0x0029, "sprite budget set", function()
    local pc = cpu.state.PC.value
    if pc >= 0xf6ef and pc <= 0xf6f1 then builder = mem:read_u16(cpu.state.S.value) end
end)

revg_frontend_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 560 and frame < 564 and 1 or 0)
    start:set_value(frame >= 760 and frame < 764 and 1 or 0)
    local screen = machine.screens[":screen"]
    if frame == 540 then screen:snapshot("revg-frontend/title-attract.png") end
    -- The background layer keeps whatever the attract demo left (part of the graveyard);
    -- the title is the text layer, so the background colours $00-$3F go black for the
    -- snapshot and come back after it.
    local base, ext = machine.memory.shares[":palette"], machine.memory.shares[":palette_ext"]
    if frame == 699 then
        saved = {}
        for i = 0, 0x3f do
            saved[i] = { base:read_u8(i), ext:read_u8(i) }
            base:write_u8(i, 0)
            ext:write_u8(i, 0)
        end
    end
    if frame == 701 then screen:snapshot("revg-frontend/title-credit.png") end
    if frame == 702 then
        for i = 0, 0x3f do
            base:write_u8(i, saved[i][1])
            ext:write_u8(i, saved[i][2])
        end
    end
    if frame >= 760 and builder == 0x792d and not started then started = frame end
    if started and not done then
        local t = frame - started
        if builder ~= 0x792d then
            print(string.format("end,%d", t))
            done = true
            machine:exit()
            return
        end
        local base, ext = machine.memory.shares[":palette"], machine.memory.shares[":palette_ext"]
        local sum = 0
        for i = 0, 0x3f do sum = sum + base:read_u8(i) + ext:read_u8(i) end
        local list = {}
        for _, x in ipairs(objects) do
            list[#list + 1] = string.format("%04x:%d:%d:%04x:%d:%d", x, mem:read_u8(x - 0x0f), mem:read_u8(x - 1),
                word(x + 3), word(x - 0x0a), mem:read_u8(x - 0x06))
        end
        print(string.format("op,%d,%d,%s", t, sum, table.concat(list, ",")))
    end
    assert(frame < 3000, "The opening did not end")
end)
