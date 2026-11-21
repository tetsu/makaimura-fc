-- MAME 0.289, makaimurg, default DIP switches.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = tonumber(os.getenv("REVG_OFFSCREEN_SECTION"))
assert(section == 0 or section == 4 or section == 5, "Expected REVG_OFFSCREEN_SECTION=0, 4 or 5")
local trials = {
    underflow = { offset = -1, dies = false },
    zero = { offset = 0, dies = true, clears_blink = true },
    inside = { offset = 31, dies = true, clears_blink = true },
    edge = { offset = 32, dies = true, clears_blink = true },
    outside = { offset = 33, dies = false },
    dying = { offset = 32, dies = true, player_state = 3 },
    blinking = { offset = 32, dies = true, player_state = 3, blink = 5, clears_blink = true },
}
local profile = os.getenv("REVG_OFFSCREEN_PROFILE")
local trial = assert(trials[profile], "Expected underflow, zero, inside, edge, outside, dying or blinking")

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local function field(mask)
    for _, item in pairs(machine.ioport.ports[":SYSTEM"].fields) do
        if item.mask == mask then return item end
    end
    error("Missing SYSTEM input")
end

local coin, start = field(0x40), field(0x01)
local frame, flag_writes, blink_clears, state_reads = 0, 0, 0, 0
local selected, ready, comparison_frame, compared_y, lives, death_frame, reloaded, failure

local function log(event, format, ...)
    print(string.format("offscreen,%d,%s,%s,%d,", section, profile, event, frame)
        .. string.format(format, ...))
end

revg_offscreen_taps = {
    y = mem:install_read_tap(0xa2, 0xa3, "player Y for offscreen comparison", function(address)
        if not ready or cpu.state.PC.value ~= 0xf6c0 then return end
        if address == 0xa2 and not comparison_frame then
            comparison_frame = frame
            local camera = word(0x9e)
            compared_y = (camera + trial.offset) & 0xffff
            log("compare", "%d,%d,%d,%d", camera, compared_y, (compared_y - camera) & 0xffff,
                mem:read_u8(0x21) & 1)
        end
        if comparison_frame ~= frame then return end
        return address == 0xa2 and compared_y >> 8 or compared_y & 0xff
    end),
    blink = mem:install_read_tap(0xad, 0xad, "blink value for offscreen handling", function(_, value)
        if comparison_frame ~= frame or cpu.state.PC.value ~= 0xf6ce then return end
        if value ~= 0 then failure = "Expected blink timer clear before trial" end
        return trial.blink
    end),
    state = mem:install_read_tap(0x0503, 0x0503, "player state for offscreen handling", function(_, value)
        if comparison_frame ~= frame or cpu.state.PC.value ~= 0xf6d3 then return end
        state_reads = state_reads + 1
        if value ~= 1 then failure = "Expected ordinary player state before trial" end
        return trial.player_state
    end),
    flag = mem:install_write_tap(0x90, 0x90, "offscreen death flag", function(_, value)
        if comparison_frame ~= frame or cpu.state.PC.value ~= 0xf6cc then return end
        flag_writes = flag_writes + 1
        if value ~= 1 then failure = "Unexpected offscreen death flag" end
    end),
    clear = mem:install_write_tap(0xad, 0xad, "offscreen blink clear", function(_, value)
        if comparison_frame ~= frame or cpu.state.PC.value ~= 0xf6d9 then return end
        blink_clears = blink_clears + 1
        if value ~= 0 then failure = "Unexpected offscreen blink clear" end
    end),
    spawn = mem:install_write_tap(0x050a, 0x050a, "spawn after offscreen death", function(_, value)
        if not death_frame or cpu.state.PC.value ~= 0x6d75 then return end
        local entry = 0x6cee + 4 * section
        local page = word(0x6c) - 0x4000
        local x, y = word(0x0506), 256 * mem:read_u8(0x0509) + value
        if mem:read_u8(0x72) ~= section or mem:read_u8(0x81) ~= 0
            or mem:read_u8(0x60) ~= lives - 1 or page ~= word(entry) - 0x4000
            or x ~= (page & 31) * 256 + mem:read_u8(entry + 2)
            or y ~= (page >> 5) * 256 + mem:read_u8(entry + 3) then
            failure = "Unexpected respawn record after offscreen death"
        end
        log("reload", "%03x,%d,%d,%d,%d", page, x, y, mem:read_u8(0x60), frame - death_frame)
        reloaded = true
    end),
}

revg_offscreen_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    local state, phase = mem:read_u8(0x08), mem:read_u8(0x0b)
    if not selected and mem:read_u8(0x05) == 1 and state == 1 and phase == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
    elseif selected and not ready and state == 1 and phase == 1 and mem:read_u8(0x0503) == 1 then
        ready = true
        lives = mem:read_u8(0x60)
    end
    if failure then
        machine:exit()
        error(failure)
    end
    if comparison_frame == frame - 1 then
        local expected_state = trial.dies and 3 or 1
        local expected_reads = trial.dies and not trial.blink and 1 or 0
        if state ~= expected_state or flag_writes ~= (trial.dies and 1 or 0)
            or blink_clears ~= (trial.clears_blink and 1 or 0) or state_reads ~= expected_reads then
            machine:exit()
            error(string.format("Unexpected result: state=%d flag=%d blink=%d state_reads=%d",
                state, flag_writes, blink_clears, state_reads))
        end
        log("result", "%d,%d,%d,%d", state, flag_writes, blink_clears, state_reads)
        if trial.dies then death_frame = frame else reloaded = true end
    end
    if reloaded then
        log("done", "%d", state)
        machine:exit()
    elseif frame >= 2200 then
        machine:exit()
        error("Offscreen trial did not complete")
    end
end)
