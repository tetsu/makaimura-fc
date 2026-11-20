-- MAME 0.289, makaimurg, default DIP switches, first credit from cold boot.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local first, last = 1700, 2018
local plant = 0x0fa0
local plant_x_register = plant + 16
local mode = os.getenv("REVG_PLANT") or "baseline"
local pauses = {
    baseline = {0, 0},
    wait_outside = {1730, 500},
    wait_inside = {1760, 500},
    delay = {1730, 64},
    no_slot = {0, 0},
}
local pause = assert(pauses[mode], "Unknown REVG_PLANT: " .. mode)
local requested_camera = os.getenv("REVG_PLANT_CAMERA_X")
local camera_x = requested_camera and math.tointeger(tonumber(requested_camera))
assert(not requested_camera or (mode == "baseline" and camera_x and camera_x >= 0 and camera_x <= 1023),
    "Expected REVG_PLANT_CAMERA_X in 0..1023 with the baseline trial")

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local right, jump, fire = field(":P1", 0x01), field(":P1", 0x20), field(":P1", 0x10)
local function word(addr)
    return mem:read_u8(addr) * 256 + mem:read_u8(addr + 1)
end

local camera_high
revg_plant_window = mem:install_read_tap(0x009c, 0x009d, "plant window", function(addr, value)
    if frame < first or cpu.state.X.value ~= plant_x_register or cpu.state.PC.value ~= 0x8cb7 then return end
    local override = frame == 1755 and camera_x
    if addr == 0x009c then
        camera_high = value
        if override then return override >> 8 end
    else
        local original = camera_high * 256 + value
        print(string.format("window,%d,%d,%d,%d", frame, word(plant + 6), original, override or original))
        if override then return override & 0xff end
    end
end)

revg_plant_slots = mem:install_read_tap(0x00a7, 0x00a7, "plant projectile capacity", function(_, value)
    if frame < first or cpu.state.X.value ~= plant_x_register or cpu.state.PC.value ~= 0xdec1 then return end
    local effective = mode == "no_slot" and frame == 1845 and 0 or value
    print(string.format("capacity,%d,%d,%d", frame, value, effective))
    if effective ~= value then return effective end
end)

revg_plant_launch = mem:install_write_tap(0x13c0, 0x1460, "plant launch", function(addr, value)
    if frame < first or (addr - 0x13c0) % 32 ~= 0 or value ~= 1 then return end
    if cpu.state.X.value == plant_x_register and cpu.state.PC.value == 0xdedd then
        print(string.format("launch,%d,%04x,%d,%d,%d", frame, addr, word(0x0506), word(0x0509), word(0x009c)))
    end
end)

print(string.format("trial,%s,%s", mode, camera_x or "native"))
print("type,frame,player_x,player_y,camera_x,plant_x,active,phase,subphase,cycle_timer,windup")
local previous
revg_plant_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame >= first then
        local active, phase, subphase = mem:read_u8(plant + 1), mem:read_u8(plant + 4), mem:read_u8(plant + 5)
        local state = active * 65536 + phase * 256 + subphase
        if state ~= previous or frame % 25 == 0 or frame == 1846 or frame == 1847 then
            print(string.format("sample,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                frame, word(0x0506), word(0x0509), word(0x009c), word(plant + 6),
                active, phase, subphase, mem:read_u8(plant + 22), mem:read_u8(plant + 29)))
            previous = state
        end
    end
    if frame == last then
        print("end," .. frame)
        machine:exit()
        return
    end
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    local resume = pause[1] + pause[2]
    local moving = frame > 1140 and (frame < pause[1] or frame >= resume)
    local progress = frame >= resume and frame - pause[2] or frame
    right:set_value(moving and 1 or 0)
    jump:set_value(moving and progress % 49 == 0 and 1 or 0)
    fire:set_value(moving and progress % 22 == 0 and 1 or 0)
end)
