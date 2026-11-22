-- MAME 0.289, makaimurg, default DIP switches.
-- Logs every program that reads or writes $74-$77 (the bits the 200/500-point discs clear),
-- from power-on through play of one section. REVG_BITS_SECTION picks the section, and
-- REVG_BITS_WALK=1 holds right (with a jump every 64 frames) to cover more of it.
-- Each PC is printed once per address and direction, with the frame relative to play start.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = tonumber(os.getenv("REVG_BITS_SECTION") or "0")
local walk = os.getenv("REVG_BITS_WALK") == "1"
local frames = tonumber(os.getenv("REVG_BITS_FRAMES") or "3000")

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local right, jump = field(":P1", 0x01), field(":P1", 0x20)
local frame, selected, playing_at = 0, false, nil
local seen = {}

local function note(kind, address, data)
    local pc = cpu.state.PC.value
    local key = string.format("%s%04x%04x", kind, address, pc)
    if seen[key] then return end
    seen[key] = true
    print(string.format("%s,%d,%04x,%02x,pc=%04x,bank=%02x", kind, playing_at and frame - playing_at or -1,
        address, data, pc, mem:read_u8(0xd9)))
end

revg_bits_read = mem:install_read_tap(0x74, 0x77, "bits read", function(address, data)
    note("read", address, data)
end)
revg_bits_write = mem:install_write_tap(0x74, 0x77, "bits write", function(address, data)
    note("write", address, data)
end)

revg_bits_frame = emu.add_machine_frame_notifier(function()
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
    if playing_at then
        mem:write_u8(0xad, 0x20)
        right:set_value(walk and 1 or 0)
        jump:set_value(walk and (frame - playing_at) % 64 < 4 and 1 or 0)
        if frame - playing_at >= frames then
            print(string.format("end,%02x%02x%02x%02x", mem:read_u8(0x74), mem:read_u8(0x75),
                mem:read_u8(0x76), mem:read_u8(0x77)))
            machine:exit()
        end
    end
    assert(frame < 3000 + frames, "Play did not start")
end)
