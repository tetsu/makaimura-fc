-- MAME 0.289, makaimurg, default DIP switches, first credit from cold boot.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local trial = os.getenv("REVG_STAGE1_END") or "key"
local section1_trial = trial == "section1-fall" or trial == "section1-jump"
assert(trial == "key" or trial == "straight" or trial == "controller" or section1_trial,
    "Expected REVG_STAGE1_END=key, straight, controller, section1-fall, or section1-jump")
local controller_only = trial == "controller" or section1_trial
local observation_start = controller_only and 6900 or 4900
local boss, cleared
local stopped_at_key, reached_limit = false, false

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
    local section = mem:read_u8(0x0072)
    local boss_slot = section == 0 and boss
    print(string.format("%s,%d,%s,%04x,%d,%d,%d,%d,%d,%d,%d,%d,%d", event, frame, value,
        cpu.state.PC.value, word(0x0506), word(0x0509), word(0x009c), word(0x005a),
        section, boss_slot and word(boss_slot + 6) or 0, boss_slot and word(boss_slot + 9) or 0,
        word(0x0536), word(0x0539)))
end

local section1_dead, section1_sample_at
local function record_section1(event, value)
    print(string.format("section1,%s,%d,%s,%04x,%d,%d,%d,%d,%d,%d,%04x", event, frame, value,
        cpu.state.PC.value, word(0x0506), word(0x0509), word(0x009c),
        mem:read_u8(0x0072), mem:read_u8(0x0081), mem:read_u8(0x0060), word(0x00aa)))
end

if section1_trial then
    revg_section1_restart = mem:install_read_tap(0x6cf2, 0x6d11, "section 1 restart table", function(addr, value)
        local pc = cpu.state.PC.value
        if mem:read_u8(0x0072) == 1 and (pc == 0x6d39 or pc == 0x6d42 or pc == 0x6d47) then
            record_section1("restart_byte", string.format("%04x=%02x", addr, value))
        end
    end)
    revg_section1_death = mem:install_write_tap(0x0008, 0x0008, "section 1 death", function(_, value)
        if value == 3 and cpu.state.PC.value == 0x7110 and mem:read_u8(0x0072) == 1 then
            section1_dead = true
            record_section1("death", value)
        end
    end)
    revg_section1_lives = mem:install_write_tap(0x0060, 0x0060, "section 1 lives", function(_, value)
        local pc = cpu.state.PC.value
        if mem:read_u8(0x0072) == 1 and (pc == 0xed4f or pc == 0x75ff) then
            record_section1("lives", value)
        end
    end)
    revg_section1_checkpoint = mem:install_write_tap(0x0081, 0x0081, "section 1 checkpoint", function(_, value)
        if cpu.state.PC.value == 0x6c58 and mem:read_u8(0x0072) == 1 then
            record_section1("checkpoint_reset", value)
        end
    end)
end

if not controller_only then
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
end

local events = {
    [0x0008] = {[0xe79d] = "boss_clear", [0x7030] = "section_play"},
    [0x000b] = {[0x70f0] = "play_ready"},
    [0x0072] = {[0x73a3] = "section_advance"},
    [0x0533] = {[0x744b] = "key_state", [0x7469] = "key_landed"},
    [0x0553] = {[0x72ef] = "key_request"},
    [0x0555] = {[0x733e] = "key_contact"},
    [0x0670] = {[0x8e3e] = "scroll_page"},
}

revg_stage1_events = {}
for addr, names in pairs(events) do
    revg_stage1_events[addr] = mem:install_write_tap(addr, addr, "stage 1 end event", function(_, value)
        local event = frame >= observation_start and names[cpu.state.PC.value]
        if event == "boss_clear" then cleared = true end
        if section1_trial and event == "play_ready" and mem:read_u8(0x0072) == 1 then
            section1_sample_at = frame + 1
        end
        if event then record(event, value) end
    end)
end

revg_stage1_boss = mem:install_write_tap(0x0880, 0x1150, "stage 1 boss", function(addr, value)
    if frame < observation_start or mem:read_u8(0x0072) ~= 0 then return end
    local pc = cpu.state.PC.value
    if pc == 0xeaf9 and addr == cpu.state.Y.value - 0x10 and mem:read_u8(cpu.state.X.value + 5) == 8 then
        boss = addr
        record("boss_slot", string.format("%04x", boss))
    elseif boss then
        if addr == boss + 0x26 and (pc == 0xc40b or pc == 0xef2a) then
            record("boss_hp", value)
        elseif addr == boss + 0x21 and (pc == 0xc42f or pc == 0xc49d) then
            record("boss_wait", value)
        elseif addr == boss + 4 and pc == 0xc4a6 then
            record("boss_active", value)
        elseif addr == boss and pc == 0xcb18 then
            record("boss_removed", value)
        end
    end
end)

local function controller_inputs()
    local player_x = word(0x0506)
    -- Jump at the forest gaps, then wait for the flying enemies to pass.
    local moving = frame > 1140 and (frame < 3033 or (frame >= 3389 and frame < 3420) or frame >= 3950)
    local jumping = (moving and frame < 2650 and frame % 49 == 0) or frame == 3390
    if frame >= 4300 then
        jumping = (player_x - 1946) % 64 <= 5 and frame % 2 == 0
    end
    if frame >= 6450 and player_x >= 2270 then
        jumping = (frame + 18) % 42 == 0
    end
    local firing = moving and (frame < 4300 and frame % 22 == 0
        or frame >= 4300 and mem:read_u8(0x0504) ~= 0 and frame % 14 == 0)
    if frame >= 6875 then
        moving = frame >= 6923
        jumping = frame >= 6932 and (frame - 6932) % 56 == 0
        firing = frame % 14 == 0
    end
    local returning = false
    -- Stop short of the boss, back away from its jump, then face right again.
    if frame >= 7610 then
        moving = player_x < 3413
        jumping = (frame + 2) % 41 == 0
        firing = frame % 10 == 0
    end
    if frame >= 7699 then
        returning = frame < 7724
        moving = moving and not returning
        jumping = frame >= 7717 and (frame - 7717) % 42 == 0
    end
    if cleared then
        if player_x <= 3365 then stopped_at_key = true end
        moving, jumping, returning, firing = false, false, not stopped_at_key, false
    end
    return moving, jumping, returning, firing
end

local samples = controller_only and {[7417] = true, [8700] = true}
    or {[4863] = true, [4902] = true, [5900] = true, [6200] = true, [6500] = true}
local last = section1_trial and 10000 or (controller_only and 8700 or 6500)
local previous_camera
print("trial," .. trial)
print("event,notice,value,pc,player_x,player_y,camera_x,camera_limit,section,boss_x,boss_y,key_x,key_y")
if section1_trial then
    print("section1,event,notice,value,pc,player_x,player_y,camera_x,section,checkpoint,lives_before,timer_hex")
end
revg_stage1_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame == section1_sample_at then record_section1("ready", "-") end
    local respawn_observed = false
    if section1_sample_at and frame == section1_sample_at + 5 then
        record_section1("camera", "-")
        respawn_observed = section1_dead
    end
    if samples[frame] then record("sample", "-") end
    local camera = word(0x009c)
    if frame >= observation_start and boss and not reached_limit and camera >= word(0x005a) then
        reached_limit = true
        record("camera_limit", previous_camera)
    end
    previous_camera = camera
    if frame == last or respawn_observed then
        print("end," .. frame)
        machine:exit()
        return
    end

    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)

    local moving, jumping, returning, firing
    if controller_only then
        moving, jumping, returning, firing = controller_inputs()
        if section1_trial and frame >= 8512 then
            moving, returning = not section1_dead, false
            jumping = moving and trial == "section1-jump" and frame >= 8529 and (frame - 8529) % 49 == 0
            firing = moving and trial == "section1-jump" and frame % 14 == 0
        end
    else
        -- Repeat the crossing and return into the water from the checkpoint trial.
        moving = frame > 1140 and (frame < 3033 or (frame >= 3389 and frame < 3420)
            or (frame >= 3950 and frame < 4300))
        jumping = (moving and frame < 2650 and frame % 49 == 0) or frame == 3390
        returning = frame >= 4300 and frame < 4750
        firing = moving and frame % 22 == 0
        if frame >= 4910 then
            moving, jumping, returning = true, false, false
            firing = frame % 8 == 0
        end
        -- After the kill, turn back to the key's X and jump while waiting for it.
        if trial == "key" and frame >= 5775 then
            if word(0x0506) <= 3365 then stopped_at_key = true end
            moving, jumping, returning = false, stopped_at_key and frame % 61 == 0, not stopped_at_key
            firing = false
        end
    end

    right:set_value(moving and 1 or 0)
    left:set_value(returning and 1 or 0)
    jump:set_value(jumping and 1 or 0)
    fire:set_value(firing and 1 or 0)
end)
