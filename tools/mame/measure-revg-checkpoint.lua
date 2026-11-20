-- MAME 0.289, makaimurg, default DIP switches, first credit from cold boot.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local mode = os.getenv("REVG_CHECKPOINT") or "cross"
local test_frame = 2800
local requested_camera = os.getenv("REVG_CHECKPOINT_CAMERA_X")
local test_camera = requested_camera and math.tointeger(tonumber(requested_camera))
assert(mode == "cross" or mode == "start", "Expected REVG_CHECKPOINT=cross or start")
assert(not requested_camera or mode == "start" and (test_camera == 1792 or test_camera == 1793),
    "Expected REVG_CHECKPOINT_CAMERA_X=1792 or 1793 with the start trial")
local last = mode == "cross" and 4915 or 3320

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local right, left = field(":P1", 0x01), field(":P1", 0x02)
local jump, fire = field(":P1", 0x20), field(":P1", 0x10)

local function word(addr)
    return mem:read_u8(addr) * 256 + mem:read_u8(addr + 1)
end

local camera_high
revg_checkpoint_compare = mem:install_read_tap(0x009c, 0x009d, "checkpoint camera comparison", function(addr, value)
    if cpu.state.PC.value ~= 0xecba then return end
    local override = frame == test_frame and test_camera
    if addr == 0x009c then
        camera_high = value
        if override then return override >> 8 end
    else
        local native = camera_high * 256 + value
        if frame == test_frame or native >= 1788 then
            print(string.format("compare,%d,%d,%d,%d", frame, native, override or native, word(cpu.state.X.value)))
        end
        if override then return override & 0xff end
    end
end)

revg_checkpoint_flag = mem:install_write_tap(0x0081, 0x0081, "checkpoint flag", function(_, value)
    if cpu.state.PC.value == 0xecdd then
        print(string.format("checkpoint,%d,%d,%d,%d,%d", frame, value, word(0x0506), word(0x0509), word(0x009c)))
    end
end)
revg_checkpoint_restart = mem:install_write_tap(0x0500, 0x0500, "checkpoint restart", function()
    local pc = cpu.state.PC.value
    if frame > 2700 and (pc == 0x6d50 or pc == 0x8f3a) then
        print(string.format("restart,%d,%04x,%d,%d", frame, pc, mem:read_u8(0x0072), mem:read_u8(0x0081)))
    end
end)
revg_checkpoint_sound = mem:install_write_tap(0x3a00, 0x3a00, "checkpoint death and restart", function(_, value)
    if frame >= 2800 and (value == 0x31 or value == 0x2b or value == 0x3f) then
        print(string.format("sound,%d,%02x", frame, value))
    end
end)

local cross_samples = {
    [3034] = true, [3390] = true, [3420] = true, [3430] = true, [3800] = true, [3920] = true,
    [3945] = true, [3950] = true, [4283] = true, [4284] = true, [4300] = true, [4310] = true,
    [4438] = true, [4439] = true, [4440] = true, [4863] = true, [4902] = true,
}
local start_samples = {[2801] = true, [3261] = true, [3300] = true}
local samples = mode == "cross" and cross_samples or start_samples
print(string.format("trial,%s,%s", mode, test_camera or "native"))
print("type,frame,player_x,player_y,camera_x,section,checkpoint,airborne,platform_x,platform_y")
revg_checkpoint_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if samples[frame] then
        print(string.format("sample,%d,%d,%d,%d,%d,%d,%d,%d,%d", frame,
            word(0x0506), word(0x0509), word(0x009c), mem:read_u8(0x0072), mem:read_u8(0x0081),
            mem:read_u8(0x0504), word(0x1166), word(0x1169)))
    end
    if frame == last then
        print("end," .. frame)
        machine:exit()
        return
    end
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    local moving, jumping, returning
    if mode == "cross" then
        -- Wait ashore, board the moving platform, then stay on until the far bank.
        moving = frame > 1140 and (frame < 3033 or (frame >= 3389 and frame < 3420)
            or (frame >= 3950 and frame < 4300))
        jumping = (moving and frame < 2650 and frame % 49 == 0) or frame == 3390
        returning = frame >= 4300 and frame < 4750
    else
        moving = frame > 1140 and frame < 2890
        jumping = moving and frame % 49 == 0
    end
    right:set_value(moving and 1 or 0)
    left:set_value(returning and 1 or 0)
    jump:set_value(jumping and 1 or 0)
    fire:set_value(moving and frame % 22 == 0 and 1 or 0)
end)
