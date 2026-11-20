-- MAME 0.289, makaimurg, default DIP switches, first credit from cold boot.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local mode = os.getenv("REVG_PIT") or "jump"
local trials = {
    jump = {jump_until = 2890, stop = 2890, death = 2837, last = 3350},
    walk = {jump_until = 2650, stop = 3035, death = 3043, last = 3556},
    shore = {jump_until = 2650, stop = 3034, last = 3052},
    edge = {jump_until = 2650, stop = 3034, last = 3036},
}
local trial = assert(trials[mode], "Unknown REVG_PIT: " .. mode)
local requested_x = os.getenv("REVG_PIT_X")
local test_x = requested_x and math.tointeger(tonumber(requested_x))
assert((mode == "edge" and (test_x == 1655 or test_x == 1656)) or (mode ~= "edge" and not requested_x),
    "Use REVG_PIT_X=1655 or 1656 with edge")

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local right, jump, fire = field(":P1", 0x01), field(":P1", 0x20), field(":P1", 0x10)
local function byte(addr)
    return mem:read_u8(addr)
end
local function word(addr)
    return byte(addr) * 256 + byte(addr + 1)
end

local edge_frame = 3034
if test_x then
    revg_pit_edge = mem:install_read_tap(0x0507, 0x0507, "shore support X", function(_, value)
        if frame == edge_frame and cpu.state.X.value == 0x0510 and cpu.state.PC.value == 0x8b17 then
            print(string.format("test_x,%d,%d,%d", frame, byte(0x0506) * 256 + value, test_x))
            return test_x & 0xff
        end
    end)
end

revg_pit_tile = mem:install_read_tap(0x2c00, 0x2fff, "shore tile", function(addr, value)
    if mode == "jump" or frame < 3032 or frame > 3036 or cpu.state.X.value ~= 0x0510 then return end
    local pc = cpu.state.PC.value
    if pc == 0x8b2f or pc == 0x8b64 then
        print(string.format("tile,%d,%s,%04x,%02x,%02x", frame, pc == 0x8b2f and "left" or "right",
            addr - 0x400, byte(addr - 0x400), value))
    end
end)

revg_pit_sound = mem:install_write_tap(0x3a00, 0x3a00, "pit sound", function(_, value)
    if frame >= 2780 and (value == 0x31 or value == 0x2b or value == 0x3f) then
        print(string.format("sound,%d,%02x", frame, value))
    end
end)
revg_pit_restart = mem:install_write_tap(0x0500, 0x0500, "restart", function(_, value)
    if frame >= 2780 and (cpu.state.PC.value == 0x6d50 or cpu.state.PC.value == 0x8f3a) then
        print(string.format("restart,%d,%04x,%d", frame, cpu.state.PC.value, value))
    end
end)

print(string.format("trial,%s,%s", mode, test_x or "native"))
print("type,frame,player_x,player_y,camera_x,checkpoint,armor,airborne,arremer_x,arremer_y,arremer_active,zombie_active")
revg_pit_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    local death = trial.death
    if (mode ~= "jump" and frame >= 3032 and frame <= 3052)
        or (death and (math.abs(frame - death) <= 10 or math.abs(frame - death - 423) <= 1
            or math.abs(frame - death - 463) <= 1 or frame == trial.last)) then
        print(string.format("sample,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", frame,
            word(0x0506), word(0x0509), word(0x009c), byte(0x0072), byte(0x0500), byte(0x0504),
            word(0x0eb6), word(0x0eb9), byte(0x0eb1), byte(0x0df0)))
    end
    if frame == trial.last then
        print("end," .. frame)
        machine:exit()
        return
    end
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    local moving = frame > 1140 and frame < trial.stop
    right:set_value(moving and 1 or 0)
    jump:set_value(moving and frame < trial.jump_until and frame % 49 == 0 and 1 or 0)
    fire:set_value(moving and frame % 22 == 0 and 1 or 0)
end)
