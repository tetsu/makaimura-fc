-- MAME 0.289, makaimurg, default DIP switches, first credit from cold boot.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local trial = os.getenv("REVG_STAGE1_END") or "key"
local section1_normal_hp = trial == "section1-end-normal-hp"
local section1_end = trial == "section1-end-one" or trial == "section1-end-both" or section1_normal_hp
local upper_projectile = trial == "section1-upper-projectile"
local section1_upper = trial == "section1-upper" or upper_projectile
local section1_climb = trial == "section1-climb" or section1_upper
local section1_respawn_climb = trial == "section1-respawn-climb"
local section1_cross = trial == "section1-cross" or section1_climb or section1_respawn_climb or section1_end
local section1_trial = trial == "section1-fall" or trial == "section1-jump" or section1_cross
assert(trial == "key" or trial == "straight" or trial == "controller" or section1_trial,
    "Expected REVG_STAGE1_END=key, straight, controller, section1-fall, section1-jump, section1-cross, "
        .. "section1-climb, section1-upper, section1-upper-projectile, section1-respawn-climb, "
        .. "section1-end-one, section1-end-both, "
        .. "or section1-end-normal-hp")
local requested_upper_input = os.getenv("REVG_UPPER_INPUT")
local upper_input = requested_upper_input or "stand"
local upper_wall = upper_input == "wall-walk" or upper_input == "wall-jump"
local upper_ladder = upper_input == "ladder-down" or upper_input == "ladder-pause"
local resume_jump_delay = ({
    ["resume-early-jump"] = 12,
    ["resume-jump"] = 22,
    ["wall-walk"] = 22,
    ["wall-jump"] = 22,
})[upper_input]
local upper_resumes = upper_input == "resume-walk" or resume_jump_delay ~= nil
assert(not requested_upper_input or upper_projectile and
    (upper_input == "stand" or upper_input == "crouch" or upper_input == "jump" or upper_input == "late-jump"
        or upper_resumes or upper_ladder),
    "Expected REVG_UPPER_INPUT=stand, crouch, jump, late-jump, resume-walk, resume-early-jump, resume-jump, "
        .. "wall-walk, wall-jump, ladder-down or ladder-pause with section1-upper-projectile")
local requested_camera = os.getenv("REVG_SECTION1_CAMERA_X")
local test_camera = tonumber(requested_camera)
assert(not requested_camera or trial == "section1-fall" and (test_camera == 4608 or test_camera == 4609),
    "Expected REVG_SECTION1_CAMERA_X=4608 or 4609 with section1-fall")
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
local up = (section1_climb or section1_respawn_climb) and field(":P1", 0x08)
local down = upper_projectile and field(":P1", 0x04)

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

local third_floor_reached, upper_ladder_finished
if upper_projectile then
    -- Observe the operands when the CPU compares the projectile's Y with the player's Y.
    revg_upper_compare = mem:install_read_tap(0x0509, 0x0509, "upper floor projectile comparison", function(_, high)
        if not third_floor_reached or section1_dead or cpu.state.PC.value ~= 0xf4bf then return end
        local slot = cpu.state.U.value - 0x10
        local player_x, player_y = word(0x0506), high * 256 + mem:read_u8(0x050a)
        local shot_x, shot_y = word(slot + 6), word(slot + 9)
        if math.abs(shot_x - player_x) <= 32 and math.abs(shot_y - player_y) <= 32 then
            print(string.format("upper_compare,%d,%04x,%02x,%04x,%d,%d,%d,%d", frame,
                slot, mem:read_u8(slot + 21), cpu.state.X.value, player_x, player_y, shot_x, shot_y))
        end
    end)
    revg_upper_armor = mem:install_write_tap(0x0500, 0x0500, "upper floor armor loss", function(_, value)
        if not third_floor_reached or section1_dead or value ~= 2 or mem:read_u8(0x0500) ~= 1 then return end
        local pc = cpu.state.PC.value
        local slot = pc == 0xf4e4 and string.format("%04x", cpu.state.U.value - 0x10) or "-"
        record_section1("upper_armor_loss", slot)
        if upper_ladder then
            upper_ladder_finished = true
            if pc == 0xf0a3 then
                local enemy = cpu.state.U.value - 0x10
                print(string.format("upper_ladder_contact,%d,%04x,%d,%d", frame, enemy,
                    word(enemy + 6), word(enemy + 9)))
            end
        end
    end)
end

if section1_upper or section1_respawn_climb then
    revg_water_armor = mem:install_write_tap(0x0500, 0x0500, "armor loss at the water", function(_, value)
        local player_x = word(0x0506)
        if mem:read_u8(0x0072) == 1 and player_x >= 5090 and player_x < 5196
            and mem:read_u8(0x0500) == 1 and value == 2 then
            record_section1("water_armor_loss", "1->2")
        end
    end)
end

local end_first_boss, end_second_boss, end_camera_reached
local end_bosses, end_defeated = {}, 0
if section1_end then
    -- Keep the checkpoint death and select a later page using one restart-table read.
    revg_section1_end_restart = mem:install_read_tap(0x6d0f, 0x6d0f, "section 1 end test start", function(_, value)
        if frame ~= 19801 or cpu.state.PC.value ~= 0x6d39 then return end
        record_section1("restart_page_override", string.format("%02x->1a", value))
        return 0x1a
    end)
    revg_section1_end_slots = mem:install_write_tap(0x0880, 0x1150, "section 1 end bosses", function(addr, value)
        if frame < 19840 or mem:read_u8(0x0072) ~= 1 then return end
        local pc = cpu.state.PC.value
        if pc == 0xeaf9 and addr == cpu.state.Y.value - 0x10 and mem:read_u8(cpu.state.X.value + 5) == 8 then
            if not end_first_boss then
                end_first_boss = addr
            elseif addr ~= end_first_boss then
                end_second_boss = addr
            end
            end_bosses[addr] = true
            record_section1("boss_slot", string.format("%04x/%d/%d", addr, word(addr + 6), word(addr + 9)))
        elseif (pc == 0xc40b or section1_normal_hp and pc == 0xef2a) and end_bosses[addr - 0x26] then
            record_section1("boss_hp", string.format("%04x/%d", addr - 0x26, value))
        end
    end)
    if not section1_normal_hp then
        -- Let the actual projectile hit run the death handling after a single hit.
        revg_section1_end_hp = mem:install_read_tap(0x0880, 0x1150, "section 1 end test durability", function(addr, value)
            if cpu.state.PC.value ~= 0xef2a then return end
            local slot = addr - 0x26
            if end_bosses[slot] ~= true or (trial == "section1-end-one" and slot ~= end_first_boss) then return end
            end_bosses[slot] = false
            record_section1("boss_hp_override", string.format("%04x/%d->1", slot, value))
            return 1
        end)
    end
    revg_section1_end_count = mem:install_write_tap(0x00b6, 0x00b6, "section 1 remaining bosses", function(_, value)
        if frame >= 19840 and cpu.state.PC.value == 0xe78e then
            end_defeated = end_defeated + 1
            record_section1("bosses_remaining", value)
        end
    end)
end

if test_camera then
    local camera_high
    revg_section1_compare = mem:install_read_tap(0x009c, 0x009d, "section 1 camera comparison", function(addr, value)
        if frame ~= 8530 or cpu.state.PC.value ~= 0xecba then return end
        if addr == 0x009c then
            camera_high = value
            return test_camera >> 8
        end
        local native = camera_high * 256 + value
        print(string.format("section1_compare,%d,%d,%d,%d", frame, native, test_camera, word(cpu.state.X.value)))
        return test_camera & 0xff
    end)
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
        if mem:read_u8(0x0072) ~= 1 then return end
        local pc = cpu.state.PC.value
        if pc == 0x6c58 then record_section1("checkpoint_reset", value) end
        if pc == 0xecdd then record_section1("checkpoint_set", value) end
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

-- Each row gives the start frame, walking delay, jump period and phase,
-- then the fire period and phase for one stretch of the checkpoint route.
local checkpoint_route = {
    {9150, 0, 37, 0, 5, 0},
    {9238, 18, 44, 12, 14, 13},
    {9342, 0, 37, 6, 11, 7},
    {9446, 3, 44, 15, 5, 0},
    {9550, 0, 37, 6, 11, 7},
    {9654, 0, 37, 0, 5, 0},
}

local first_floor_at, second_floor_reached, upper_input_started
local upper_airborne, upper_resumed_at
if upper_wall then
    local function observing_wall()
        if not upper_resumed_at or section1_dead or cpu.state.X.value ~= 0x0510 then return false end
        local elapsed = frame - upper_resumed_at
        return elapsed >= 62 and elapsed <= 93
    end

    local right_wall_limits = 0x458e
    revg_upper_wall_compare = mem:install_read_tap(0x00e8, 0x00e8,
        "upper floor right wall", function(_, local_x)
            if cpu.state.PC.value ~= 0x8ba6 or not observing_wall() then return end
            local limit_address = cpu.state.Y.value
            -- The 6809 sign-extends the tile byte when adding it to the table base.
            local tile = (limit_address - right_wall_limits) & 0xff
            print(string.format("upper_wall_compare,%d,%02x,%d,%d,%d,%d,%d,%d", frame,
                tile, local_x, mem:read_u8(limit_address),
                word(0x0506), word(0x0509), word(0x00ec), word(0x00ee)))
        end)
    revg_upper_wall_correction = mem:install_write_tap(0x0507, 0x0507, "upper floor X correction",
        function(_, value)
            local pc = cpu.state.PC.value
            if (pc ~= 0x9283 and pc ~= 0x93f0) or not observing_wall() then return end
            local x_before = word(0x0506)
            print(string.format("upper_wall_correct,%d,%04x,%d,%d", frame, pc, x_before, (x_before & 0xff00) | value))
        end)
end

local function upper_resume_inputs()
    local air_state = mem:read_u8(0x0504)
    if not upper_resumed_at then
        if air_state ~= 0 then upper_airborne = true end
        if not upper_airborne or air_state ~= 0 then return false, false end
        upper_resumed_at = frame
        record_section1("upper_resume", "-")
    end

    local elapsed = frame - upper_resumed_at
    local jumping = elapsed == resume_jump_delay
    if jumping then record_section1("upper_second_jump", "-") end
    if upper_input == "wall-jump" and elapsed == 72 then
        jumping = true
        record_section1("upper_third_jump", air_state)
    end
    -- Compare the inputs at the same times around the second landing and the stalled advance.
    if elapsed == 60 or elapsed == 66 or elapsed == 84 then
        record_section1("upper_resume_sample", air_state)
    end
    return true, jumping
end

local descent_started, descent_paused_at
local function upper_ladder_inputs(player_x)
    local ladder_x = 5616
    local pressing_down = math.abs(player_x - ladder_x) <= 5
    if pressing_down and not descent_started then
        descent_started = true
        record_section1("upper_ladder_down", "-")
    end
    if upper_input == "ladder-pause" and descent_started then
        if not descent_paused_at and word(0x0509) <= 365 then
            descent_paused_at = frame
            record_section1("upper_ladder_pause", "-")
        end
        if descent_paused_at then
            local elapsed = frame - descent_paused_at
            pressing_down = pressing_down and elapsed >= 64
            if elapsed == 32 or elapsed == 64 then
                record_section1("upper_ladder_sample", elapsed)
            end
        end
    end
    down:set_value(pressing_down and 1 or 0)
    return player_x < ladder_x, false, player_x > ladder_x
end

local function section1_climb_inputs()
    if frame < 10300 then return true, frame % 42 == 0, false, frame % 7 == 0 end
    if frame < 10415 then
        local tick = frame - 10300
        return tick >= 16, (tick + 20) % 37 == 0, false, (tick + 4) % 17 == 0
    end
    local player_x = word(0x0506)
    if upper_ladder and third_floor_reached then return upper_ladder_inputs(player_x) end
    if upper_projectile and third_floor_reached then
        local stop_x = upper_input == "late-jump" and 5678 or 5668
        if player_x < stop_x and not upper_input_started then return true, false, false, false, false end
        if upper_input_started and upper_resumes then return upper_resume_inputs() end
        local jumping = not upper_input_started and (upper_input == "jump" or upper_input == "late-jump" or upper_resumes)
        if not upper_input_started then
            upper_input_started = true
            record_section1("upper_input", upper_input)
        end
        down:set_value(upper_input == "crouch" and 1 or 0)
        return false, jumping, false, false, false
    end
    if section1_upper and first_floor_at and frame >= first_floor_at + 10 then
        -- Climb the ladder to the left, then return to the next ladder on the right.
        local target_x = second_floor_reached and 5520 or 5424
        local firing = not second_floor_reached and frame % 16 == 0
        return player_x < target_x, false, player_x > target_x, firing, math.abs(player_x - target_x) <= 5
    end
    if player_x >= 5514 then
        return player_x < 5519, false, player_x > 5521, false, true
    end
    local tick = frame - 10415
    return true, tick % 37 == 0, false, tick % 5 == 0
end

local function section1_inputs()
    if section1_dead then return false, false, false, false end
    if trial == "section1-fall" then return true, false, false, false end
    if not section1_cross or frame < checkpoint_route[1][1] then
        local first_jump = section1_cross and 8533 or 8529
        return true, frame >= first_jump and (frame - first_jump) % 49 == 0, false, frame % 14 == 0
    end
    if mem:read_u8(0x0081) == 1 then
        if section1_climb then return section1_climb_inputs() end
        -- Land, walk back below the threshold, and let the timer expire.
        return false, false, frame >= 9750, false
    end

    local segment = #checkpoint_route
    while frame < checkpoint_route[segment][1] do segment = segment - 1 end
    local start_frame, delay, jump_period, jump_phase, fire_period, fire_phase =
        table.unpack(checkpoint_route[segment])
    local tick = frame - start_frame
    local player_x = word(0x0506)
    if segment == 1 and player_x >= 4205 and player_x <= 4227 and word(0x0509) >= 236
        and mem:read_u8(0x0504) == 0 then
        return true, player_x >= 4206 and tick % 2 == 0, false, tick % 6 == 0
    end
    return tick >= delay, (tick + jump_phase) % jump_period == 0, false, (tick + fire_phase) % fire_period == 0
end

local water_approach, water_jumped, respawn_landed_at
local function section1_respawn_inputs()
    local player_x = word(0x0506)
    if respawn_landed_at then
        local tick = frame - respawn_landed_at
        return player_x < 5223, false, player_x > 5225, (tick + 4) % 17 == 0, player_x >= 5221
    end
    if not water_approach and player_x >= 5090 then
        water_approach = true
        record_section1("respawn_water_stop_fire", "-")
    end
    if water_approach then
        local jumping = not water_jumped and player_x >= 5143 and mem:read_u8(0x0504) == 0
        if jumping then
            water_jumped = true
            record_section1("respawn_water_jump", "-")
        end
        return true, jumping, false, false
    end
    local tick = frame - 19841
    return true, tick % 42 == 0, false, tick % 7 == 0
end

local samples = controller_only and {[7417] = true, [8700] = true}
    or {[4863] = true, [4902] = true, [5900] = true, [6200] = true, [6500] = true}
local last = section1_end and 21200 or (section1_cross and 21000
    or (section1_trial and 10000 or (controller_only and 8700 or 6500)))
if section1_climb then last = 10570 end
if section1_upper then last = 11400 end
if upper_resumes or upper_ladder then last = 11500 end
if section1_respawn_climb then last = 20500 end
if section1_normal_hp then last = 21800 end
local previous_camera
local water_crossed, ladder_reached
print("trial," .. trial)
print("event,notice,value,pc,player_x,player_y,camera_x,camera_limit,section,boss_x,boss_y,key_x,key_y")
if section1_trial then
    print("section1,event,notice,value,pc,player_x,player_y,camera_x,section,checkpoint,lives_before,timer_hex")
end
if upper_projectile then
    print("upper_trial," .. upper_input)
    print("upper_compare,notice,slot,type,table,player_x,player_y,shot_x,shot_y")
end
if upper_wall then
    print("upper_wall_compare,notice,tile,local_x,limit,player_x,player_y,probe_x,probe_y")
    print("upper_wall_correct,notice,pc,before_x,corrected_x")
end
if upper_ladder then print("upper_ladder_contact,notice,enemy_slot,enemy_x,enemy_y") end
revg_stage1_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame == section1_sample_at then record_section1("ready", "-") end
    local respawn_observed = false
    if section1_sample_at and frame == section1_sample_at + 5 then
        record_section1("camera", "-")
        respawn_observed = section1_dead and not section1_end and not section1_respawn_climb
    end
    if section1_cross and (frame == 9721 or frame == 9862) then record_section1("route_sample", "-") end
    local trial_finished = false
    if section1_climb and not section1_dead and frame >= 10122 then
        local player_x, player_y = word(0x0506), word(0x0509)
        if not water_crossed and player_x >= 5180 and player_y >= 159 then
            water_crossed = true
            record_section1("water_crossed", "-")
        end
        if not ladder_reached and player_x == 5520 and player_y >= 170 then
            ladder_reached = true
            record_section1("ladder_reached", "-")
        end
        if ladder_reached and not first_floor_at and player_y >= 238 then
            first_floor_at = frame
            record_section1("upper_floor_reached", "-")
            trial_finished = not section1_upper
        end
        if section1_upper and first_floor_at and not second_floor_reached and player_y >= 318 then
            second_floor_reached = true
            record_section1("second_floor_reached", "-")
        end
        if second_floor_reached and not third_floor_reached and player_y >= 398 then
            third_floor_reached = true
            record_section1("third_floor_reached", "-")
            trial_finished = not upper_projectile
        end
        if upper_ladder and descent_started and player_y <= 319 and mem:read_u8(0x0504) == 0 then
            record_section1("upper_ladder_bottom", "-")
            upper_ladder_finished = true
        end
    end
    if section1_respawn_climb and water_jumped then
        local player_x, player_y = word(0x0506), word(0x0509)
        local grounded = mem:read_u8(0x0504) == 0
        if not respawn_landed_at and player_x >= 5196 and player_y >= 79 and grounded then
            respawn_landed_at = frame
            record_section1("respawn_water_landed", "-")
        end
        if respawn_landed_at and player_y >= 158 and grounded then
            record_section1("respawn_first_floor", "-")
            trial_finished = true
        end
    end
    if section1_end and frame == 20600 then record_section1("end_sample", mem:read_u8(0x00b6)) end
    if samples[frame] then record("sample", "-") end
    local camera = word(0x009c)
    if frame >= observation_start and boss and not reached_limit and camera >= word(0x005a) then
        reached_limit = true
        record("camera_limit", previous_camera)
    end
    if section1_end and frame >= 19841 and not end_camera_reached and mem:read_u8(0x0072) == 1
        and camera >= word(0x005a) then
        end_camera_reached = true
        record_section1("camera_limit", previous_camera)
    end
    previous_camera = camera
    if frame == last or respawn_observed or trial_finished or upper_ladder_finished then
        print("end," .. frame)
        machine:exit()
        return
    end

    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)

    local moving, jumping, returning, firing, climbing
    if controller_only then
        moving, jumping, returning, firing = controller_inputs()
        if section1_trial and frame >= 8512 then
            moving, jumping, returning, firing, climbing = section1_inputs()
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

    if section1_respawn_climb and frame >= 19841 then
        moving, jumping, returning, firing, climbing = section1_respawn_inputs()
    end

    if section1_end and frame >= 19841 then
        local player_x = word(0x0506)
        moving, jumping, returning, firing = true, false, false, frame % 8 == 0
        if section1_normal_hp and frame >= 20265 and end_defeated < 2 and end_second_boss then
            -- Follow the first boss when it jumps past the player, then target the second.
            local target = mem:read_u8(end_first_boss + 0x26) > 0 and end_first_boss or end_second_boss
            local target_x = word(target + 6)
            moving, returning = player_x <= target_x, player_x > target_x
        end
        if trial == "section1-end-one" and end_defeated > 0 then firing = false end
        if end_defeated == 2 and mem:read_u8(0x0072) == 1 then
            moving, returning, firing = player_x < 6949, player_x > 6949, false
        end
    end

    right:set_value(moving and 1 or 0)
    left:set_value(returning and 1 or 0)
    jump:set_value(jumping and 1 or 0)
    fire:set_value(firing and 1 or 0)
    if up then up:set_value(climbing and 1 or 0) end
end)
