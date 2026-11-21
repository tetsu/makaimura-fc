-- MAME 0.289, makaimurg, default DIP switches.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local trial = os.getenv("REVG_BOSS_FLOW")
assert(trial == "restart" or trial == "loop-ending", "Expected REVG_BOSS_FLOW=restart or loop-ending")

local frame, visit, returned = 0, 0, 0
local boss, death_frame, pose_frame, progress_frame, ending_frame
local sounds, timers = {}, {}

local function log(event, format, ...)
    print(string.format("boss_flow,%s,%d,%s,%d,", trial, visit, event, frame) .. string.format(format, ...))
end

local function field(mask)
    for _, item in pairs(machine.ioport.ports[":SYSTEM"].fields) do
        if item.mask == mask then return item end
    end
    error("Missing SYSTEM input")
end

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local coin, start = field(0x40), field(0x01)
local timer_pcs = { [0x7286] = true, [0x7200] = true, [0x721b] = true, [0x723f] = true }

revg_boss_flow_taps = {
    boss = mem:install_write_tap(0x0880, 0x0fff, "section 6 boss", function(address, value)
        local pc = cpu.state.PC.value
        if visit == returned then return end
        if pc == 0x6e5e and value == 0x0d then
            boss = address - 5
        elseif boss and pc == 0xdce4 and address == boss + 4 and not pose_frame then
            local pointer = 256 * mem:read_u8(address - 1) + value
            pose_frame = frame
            assert(frame - death_frame == 22 and pointer == (visit == 1 and 0xe890 or 0xe894),
                "Unexpected death pose")
            assert(mem:read_u8(boss + 0x0d) == 63, "Unexpected death pose counter")
            log("pose", "%04x,%d", pointer, 63)
        end
    end),
    sound = mem:install_write_tap(0x0140, 0x017f, "section 6 sound commands", function(_, value)
        if frame ~= death_frame or cpu.state.PC.value ~= 0x777a then return end
        sounds[#sounds + 1] = value
        log("sound", "%02x", value)
    end),
    remaining = mem:install_write_tap(0xb6, 0xb6, "section 6 remaining bosses", function(_, value)
        if not death_frame or visit == returned or cpu.state.PC.value ~= 0xe78e then return end
        assert(pose_frame and frame - pose_frame == 63 and mem:read_u8(0xb6) == 1 and value == 0,
            "Unexpected remaining boss count")
        log("remaining", "%d", value)
    end),
    progress = mem:install_write_tap(0x08, 0x08, "section 6 completion", function(_, value)
        if not death_frame or visit == returned or cpu.state.PC.value ~= 0xe79d then return end
        assert(value == 2 and mem:read_u8(0xb6) == 0 and mem:read_u8(boss - 0x10) == 0,
            "Expected boss removal before the section completion")
        progress_frame = frame
        log("progress", "%d", value)
    end),
    ending = mem:install_write_tap(0x7f, 0x7f, "section 6 ending phase", function(_, value)
        if progress_frame and visit > returned and cpu.state.PC.value == 0x7e1c then
            assert(visit == 2 and value == 2, "Unexpected ending phase")
            ending_frame = frame
            log("ending_phase", "%d", value)
        end
    end),
    timer = mem:install_write_tap(0x0a, 0x0a, "section 6 transition timers", function(_, value)
        if not progress_frame or visit == returned or not timer_pcs[cpu.state.PC.value] then return end
        local delay = 256 * mem:read_u8(0x09) + value
        timers[#timers + 1] = delay
        log("timer", "%d", delay)
    end),
    section = mem:install_write_tap(0x72, 0x72, "section 6 return", function(_, value)
        local pc = cpu.state.PC.value
        if not progress_frame or visit == returned or (pc ~= 0x729e and pc ~= 0x71d0) then return end
        local scene, round = mem:read_u8(0x71), mem:read_u8(0x7f)
        assert(mem:read_u8(0x72) == 6 and value == 0, "Expected return to section 0")
        assert(#sounds == 4 and sounds[1] == 0x3f and sounds[2] == 0x07 and sounds[3] == 0x08
            and sounds[4] == (visit == 1 and 0x1c or 0x32),
            "Unexpected death sound commands")
        local delays = table.concat(timers, ",")
        if trial == "restart" then
            assert(pc == 0x71d0 and scene == 1 and round == 0 and delays == "" and frame == progress_frame + 1,
                "Unexpected direct return")
        elseif visit == 1 then
            assert(pc == 0x729e and scene == 1 and round == 1 and delays == "300" and frame == progress_frame + 300,
                "Unexpected first loop return")
        else
            assert(pc == 0x729e and scene == 0 and round == 0 and ending_frame and delays == "540,420,180",
                "Unexpected ending return")
        end
        log("return", "%04x,%d,%d,%d", pc, value, scene, round)
        returned = visit
    end),
}

revg_boss_flow_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if returned == (trial == "restart" and 1 or 2) then
        log("done", "%d", returned)
        machine:exit()
        return
    end
    local playing = mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
    if visit == returned and playing and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        visit = visit + 1
        boss, death_frame, pose_frame, progress_frame, ending_frame = nil, nil, nil, nil, nil
        sounds, timers = {}, {}
        assert(mem:read_u8(0x72) == 0 and mem:read_u8(0x71) == 1 and mem:read_u8(0x7f) == visit - 1,
            "Unexpected state before selecting section 6")
        mem:write_u8(0x72, 6)
        if visit == 1 and trial == "loop-ending" then mem:write_u8(0x71, 0) end
        log("select", "%d,%d,%d", 6, mem:read_u8(0x71), mem:read_u8(0x7f))
    end
    if boss and not death_frame and playing and mem:read_u8(0x0b) == 1 and mem:read_u8(boss + 0x16) == 10 then
        assert(mem:read_u8(0xb6) == 1 and mem:read_u8(boss - 0x10) == 1
            and mem:read_u8(boss - 0x0d) == 0, "Expected a newly initialized boss")
        log("death_start", "%04x,%d,%d,%d", boss, word(boss - 0x0a), word(boss - 0x07), 10)
        -- Set the slot fields written by the final player projectile hit at $EF3C?$EF43.
        death_frame = frame
        mem:write_u8(boss - 0x10, 2)
        mem:write_u8(boss - 0x0d, 2)
        mem:write_u8(boss - 0x0c, 0)
        mem:write_u8(boss - 0x0b, 0)
    end
    if frame >= 3500 then
        machine:exit()
        error("Section 6 boss flow did not complete")
    end
end)
