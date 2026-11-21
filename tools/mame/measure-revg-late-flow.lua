-- MAME 0.289, makaimurg, default DIP switches.
-- Start a selected section and inject the post-boss state once normal updates begin.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local trial = os.getenv("REVG_LATE_FLOW")
local cases = {
    retry = { section = 5, round = 0, scene = 0, advance_pc = 0x71bc, next_section = 4, timers = "240" },
    loop = { section = 6, round = 0, scene = 0, advance_pc = 0x729e, next_section = 0, timers = "300" },
    restart = { section = 6, round = 0, scene = 1, advance_pc = 0x71d0, next_section = 0, timers = "" },
    ending = { section = 6, round = 1, scene = 1, advance_pc = 0x729e, next_section = 0, timers = "540,420,180" },
}
local case = assert(cases[trial], "Expected REVG_LATE_FLOW=retry, loop, restart or ending")
local frame, started, injected, completed = 0, false, false, false
local entered_ending = false
local timers = {}

local function log(event, format, ...)
    print(string.format("late,%s,%s,%d,", trial, event, frame) .. string.format(format, ...))
end

local function field(mask)
    for _, item in pairs(machine.ioport.ports[":SYSTEM"].fields) do
        if item.mask == mask then return item end
    end
    error("Missing SYSTEM input")
end

local coin, start = field(0x40), field(0x01)
local timer_writes = {
    [0x71a9] = 240,
    [0x7286] = 300,
    [0x7200] = 540,
    [0x721b] = 420,
    [0x723f] = 180,
}
revg_late_flow_taps = {
    section = mem:install_write_tap(0x72, 0x72, "late section", function(_, value)
        local pc = cpu.state.PC.value
        if not started or completed or pc ~= case.advance_pc then return end
        assert(mem:read_u8(0x72) == case.section and value == case.next_section,
            "Unexpected return section")
        log("advance", "%04x,%d,%d,%d,%d", pc, case.section, value, mem:read_u8(0x71), mem:read_u8(0x7f))
        completed = true
    end),
    round = mem:install_write_tap(0x7f, 0x7f, "late ending phase", function(_, value)
        if started and not completed and cpu.state.PC.value == 0x7e1c then
            assert(value == 2, "Unexpected ending phase")
            entered_ending = true
            log("ending_phase", "%d", value)
        end
    end),
    timer = mem:install_write_tap(0x0a, 0x0a, "late transition timer", function(_, value)
        if not started or completed then return end
        local pc = cpu.state.PC.value
        if not timer_writes[pc] then return end
        local delay = 256 * mem:read_u8(0x09) + value
        assert(delay == timer_writes[pc], "Unexpected delay")
        timers[#timers + 1] = delay
        log("timer", "%04x,%d", pc, delay)
    end),
}

revg_late_flow_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    local playing = frame >= 500 and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
    local phase = mem:read_u8(0x0b)
    -- $6FCE enters state 1 before the next update calls $6C67 to load the section.
    if not started and playing and phase == 0 and mem:read_u8(0x0e) == 0 then
        mem:write_u8(0x72, case.section)
        started = true
        log("select", "%d", case.section)
    end
    if started and not injected and playing and phase == 1 then
        injected = true
        assert(mem:read_u8(0x72) == case.section, "Unexpected section when injecting state")
        mem:write_u8(0x08, 2)
        mem:write_u8(0x0b, 0)
        mem:write_u8(0x0e, 0)
        mem:write_u8(0x71, case.scene)
        mem:write_u8(0x7f, case.round)
        log("inject", "%d,%d,%d", mem:read_u8(0x73), mem:read_u8(0x71), mem:read_u8(0x7f))
    end
    if completed then
        machine:exit()
        assert(table.concat(timers, ",") == case.timers, "Unexpected timer sequence")
        assert(entered_ending == (trial == "ending"), "Unexpected ending phase transition")
        log("done", "%d", #timers)
    end
    if frame == 3000 then
        machine:exit()
        error(string.format("Late section did not complete (section=%d, state=%d, phase=%d)",
            mem:read_u8(0x72), mem:read_u8(0x08), mem:read_u8(0x0b)))
    end
end)
