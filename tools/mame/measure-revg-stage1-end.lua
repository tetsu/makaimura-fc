-- MAME 0.289, makaimurg, default DIP switches, first credit from cold boot.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local trial = os.getenv("REVG_STAGE1_END") or "key"
assert(trial == "key" or trial == "straight", "Expected REVG_STAGE1_END=key or straight")

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

local function record(event, value)
    print(string.format("%s,%d,%s,%04x,%d,%d,%d,%d,%d,%d,%d,%d,%d", event, frame, value,
        cpu.state.PC.value, word(0x0506), word(0x0509), word(0x009c), word(0x005a),
        mem:read_u8(0x0072), word(0x0df6), word(0x0df9), word(0x0536), word(0x0539)))
end

revg_stage1_restart = mem:install_read_tap(0x6d0b, 0x6d0c, "stage 1 test restart", function(addr, native)
    if frame ~= 4862 then return end
    local pc = cpu.state.PC.value
    if addr == 0x6d0b and pc == 0x6d39 then
        record("restart_page", string.format("%02x->0c", native))
        return 0x0c
    end
    if addr == 0x6d0c and pc == 0x6d42 then
        record("restart_local_x", string.format("%02x->a0", native))
        return 0xa0
    end
end)

local events = {
    [0x0008] = {[0xe79d] = "boss_clear", [0x7030] = "section_play"},
    [0x000b] = {[0x70f0] = "play_ready"},
    [0x0072] = {[0x73a3] = "section_advance"},
    [0x0533] = {[0x744b] = "key_state", [0x7469] = "key_landed"},
    [0x0553] = {[0x72ef] = "key_request"},
    [0x0555] = {[0x733e] = "key_contact"},
    [0x0df0] = {[0xeaf9] = "boss_slot", [0xcb18] = "boss_removed"},
    [0x0df4] = {[0xc4a6] = "boss_active"},
    [0x0e16] = {[0xc40b] = "boss_hp", [0xef2a] = "boss_hp"},
}

revg_stage1_events = {}
for addr, names in pairs(events) do
    revg_stage1_events[addr] = mem:install_write_tap(addr, addr, "stage 1 end event", function(_, value)
        local event = frame >= 4900 and names[cpu.state.PC.value]
        if event then record(event, value) end
    end)
end

local samples = {[4863] = true, [4902] = true, [5900] = true, [6200] = true, [6500] = true}
local stopped, reached_limit = false, false
local previous_camera
print("trial," .. trial)
print("event,notice,value,pc,player_x,player_y,camera_x,camera_limit,section,boss_x,boss_y,key_x,key_y")
revg_stage1_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if samples[frame] then record("sample", "-") end
    local camera = word(0x009c)
    if frame >= 4902 and not reached_limit and camera >= word(0x005a) then
        reached_limit = true
        record("camera_limit", previous_camera)
    end
    previous_camera = camera
    if frame == 6500 then
        print("end," .. frame)
        machine:exit()
        return
    end

    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)

    -- Repeat the crossing and return into the water from the checkpoint trial.
    local moving = frame > 1140 and (frame < 3033 or (frame >= 3389 and frame < 3420)
        or (frame >= 3950 and frame < 4300))
    local jumping = (moving and frame < 2650 and frame % 49 == 0) or frame == 3390
    local returning = frame >= 4300 and frame < 4750
    local firing = moving and frame % 22 == 0
    if frame >= 4910 then
        moving, jumping, returning = true, false, false
        firing = frame % 8 == 0
    end
    -- After the kill, turn back to the key's X and jump while waiting for it.
    if trial == "key" and frame >= 5775 then
        if word(0x0506) <= 3365 then stopped = true end
        moving, jumping, returning = false, stopped and frame % 61 == 0, not stopped
        firing = false
    end

    right:set_value(moving and 1 or 0)
    left:set_value(returning and 1 or 0)
    jump:set_value(jumping and 1 or 0)
    fire:set_value(firing and 1 or 0)
end)
