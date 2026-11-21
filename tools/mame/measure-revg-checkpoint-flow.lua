-- MAME 0.289, makaimurg, default DIP switches.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = tonumber(os.getenv("REVG_CHECKPOINT_SECTION"))
assert(section and math.tointeger(section) and section >= 0 and section <= 6,
    "Expected REVG_CHECKPOINT_SECTION=0 through 6")
local trial = os.getenv("REVG_CHECKPOINT_BOUNDARY")
assert(trial == "equal" or trial == "above", "Expected REVG_CHECKPOINT_BOUNDARY=equal or above")
local expected_flag = trial == "above" and 1 or 0

local frame = 0
local selected, started, compared, timed_out, reloaded
local flag_writes = 0

local function log(event, format, ...)
    print(string.format("checkpoint_flow,%d,%s,%s,%d,", section, trial, event, frame)
        .. string.format(format, ...))
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
local function timer(flag)
    return word((flag == 0 and 0x6eb5 or 0x6ec3) + 2 * section)
end

revg_checkpoint_flow_taps = {
    spawn = mem:install_write_tap(0x050a, 0x050a, "checkpoint spawn", function(_, value)
        if not selected or cpu.state.PC.value ~= 0x6d75 then return end
        local flag = timed_out and expected_flag or 0
        local entry = (flag == 0 and 0x6cee or 0x6d0a) + 4 * section
        local page = word(0x6c) - 0x4000
        local x, y = word(0x0506), 256 * mem:read_u8(0x0509) + value
        assert(mem:read_u8(0x72) == section and mem:read_u8(0x81) == flag
            and page == word(entry) - 0x4000 and word(0xaa) == timer(flag),
            "Unexpected checkpoint or timer record")
        assert(x == (page & 31) * 256 + mem:read_u8(entry + 2)
            and y == (page >> 5) * 256 + mem:read_u8(entry + 3), "Unexpected spawn coordinates")
        log(timed_out and "reload" or "start", "%d,%04x,%03x,%d,%d,%d", flag, entry, page, x, y, word(0xaa))
        started, reloaded = true, timed_out
    end),
    camera = mem:install_read_tap(0x9c, 0x9d, "checkpoint comparison", function(address, value)
        if not started or timed_out or cpu.state.PC.value ~= 0xecba then return end
        assert(cpu.state.X.value == 0xece4 + 2 * section, "Unexpected checkpoint threshold entry")
        local threshold = word(cpu.state.X.value)
        local camera = threshold + expected_flag
        if address == 0x9d and not compared then
            log("compare", "%d,%d,%d,%d", camera, threshold, word(0x58), word(0x5a))
            compared = true
        end
        local compared_byte = address == 0x9c and camera >> 8 or camera & 0xff
        if section == 6 and trial == "equal" then
            assert(value == compared_byte, "Expected the fixed camera to equal the threshold")
            return
        end
        return compared_byte
    end),
    checkpoint = mem:install_write_tap(0x81, 0x81, "checkpoint flag", function(_, value)
        if not compared or cpu.state.PC.value ~= 0xecdd then return end
        assert(trial == "above" and value == 1 and word(0xaa) == timer(1), "Unexpected checkpoint update")
        flag_writes = flag_writes + 1
        log("checkpoint", "%d,%d", value, word(0xaa))
    end),
    timer = mem:install_read_tap(0xaa, 0xab, "shortened countdown", function(address)
        if compared and not timed_out and cpu.state.PC.value == 0xec7a then
            return address == 0xaa and 0 or 1
        end
    end),
    timeout = mem:install_write_tap(0xac, 0xac, "ordinary timeout", function(_, value)
        if not compared or cpu.state.PC.value ~= 0xec88 then return end
        assert(value == 1 and word(0xaa) == 0 and mem:read_u8(0x81) == expected_flag
            and flag_writes == expected_flag, "Unexpected checkpoint at timeout")
        timed_out = true
        log("timeout", "%d", expected_flag)
    end),
}

revg_checkpoint_flow_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
    end
    if reloaded then
        log("done", "%d", expected_flag)
        machine:exit()
    elseif frame >= 2200 then
        machine:exit()
        error("Checkpoint trial did not complete")
    end
end)
