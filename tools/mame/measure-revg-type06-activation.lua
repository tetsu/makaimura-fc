-- MAME 0.289, makaimurg, default DIP switches. Controlled boss activation comparisons.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local axis = os.getenv("REVG_TYPE06_AXIS")
local offset = tonumber(os.getenv("REVG_TYPE06_OFFSET"))
local delay_trial = axis == "delay"
local limits = { x = 256, y = 240 }
assert(delay_trial or (limits[axis] and offset and math.tointeger(offset) and offset >= -32768 and offset <= 32767),
    "Expected axis x or y and signed 16-bit offset, or axis delay")
local outside = not delay_trial and (offset < 0 or offset > limits[axis])
local section, variant = delay_trial and 5 or 4, delay_trial and 1 or 0
local entry = 0x6cee + 4 * section
local page = delay_trial and 0x415e or 0x40dc
local frame, ticks = 0, 0
local selected, slot, initialized, checked, activated, first_tick, blocked_tick, resumed, failure

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input %s/$%02x", port, mask))
end

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local function log(event, format, ...)
    local label = delay_trial and "delay," or string.format("%s,%d", axis, offset)
    print(string.format("type06_activation,%s,%s,%d,", label, event, frame) .. string.format(format, ...))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
revg_type06_activation_taps = {
    page = mem:install_read_tap(entry, entry + 2, "boss section start position", function(address)
        if not selected then return end
        local pc = cpu.state.PC.value
        if pc == 0x6d39 then return address == entry and page >> 8 or page & 0xff end
        if delay_trial and pc == 0x6d42 and address == entry + 2 then return 0xc0 end
    end),
    camera = mem:install_read_tap(0x9c, 0x9f, "boss activation camera comparison", function(address)
        if not slot or activated or cpu.state.X.value ~= slot then return end
        local pc = cpu.state.PC.value
        local compare_axis, position
        if pc == 0xbda5 then
            compare_axis, position = "x", slot - 0x0a
        elseif pc == 0xbdaf then
            compare_axis, position = "y", slot - 7
        else
            return
        end
        local displacement = compare_axis == axis and offset or 0
        if delay_trial and compare_axis == "x" and first_tick then
            if frame == first_tick + 64 then
                displacement = 257
                if address == 0x9c then
                    blocked_tick = frame
                    log("blocked", "%d,%d", displacement, mem:read_u8(slot + 0x11))
                end
            elseif blocked_tick and frame == blocked_tick + 1 and address == 0x9c then
                local countdown = mem:read_u8(slot + 0x11)
                if countdown ~= 5 then failure = "Countdown changed while out of range" end
                resumed = frame
                log("in_range", "%d,%d", displacement, countdown)
            end
        end
        if compare_axis == axis and address % 2 == 0 and not checked then
            checked = frame
            log("check", "%04x,%d", slot, displacement)
        end
        local camera = (word(position) - displacement) & 0xffff
        return address % 2 == 0 and camera >> 8 or camera & 0xff
    end),
    enemy = mem:install_write_tap(0x0880, 0x0fff, "boss activation states", function(address, value)
        if not selected then return end
        local pc, current_slot = cpu.state.PC.value, cpu.state.X.value
        if pc == 0xbd70 and address == current_slot + 0x14 and mem:read_u8(current_slot - 0x0e) == variant then
            if slot or value ~= 4 or mem:read_u8(current_slot + 5) ~= 0x06 then
                failure = "Unexpected boss initialization"
            end
            slot, initialized = current_slot, frame
            log("init", "%04x,%d,%d", slot, variant, value)
        end
        if current_slot ~= slot then return end
        if pc == 0xbdcf and address == slot + 0x11 then
            ticks = ticks + 1
            first_tick = first_tick or frame
            local elapsed = ticks == 1 and 0 or ticks * 64
            if not delay_trial or value ~= 6 - ticks or frame ~= first_tick + elapsed then
                failure = "Unexpected activation countdown"
            end
            log("tick", "%d", value)
        elseif pc == 0xbddb and address == slot - 0x0d then
            if value ~= 1 or activated then failure = "Unexpected boss activation" end
            activated = frame
            log("activate", "%04x", slot)
        end
    end),
}

revg_type06_activation_frame = emu.add_machine_frame_notifier(function()
    if failure or activated or (checked and frame >= checked + 4) or frame >= 1500 then
        machine:exit()
        assert(not failure, failure)
        assert(slot and initialized, "Missing boss initialization")
        local state = mem:read_u8(slot - 0x0d)
        if delay_trial then
            assert(resumed and ticks == 6 and activated == first_tick + 384 and state == 1,
                "Expected activation after skipping one countdown tick")
        else
            assert(checked == initialized + 1, "Missing initial camera comparison")
            if outside then
                assert(not activated and state == 0, "Boss activated outside the range")
            else
                assert(activated == checked and state == 1, "Boss did not activate inside the range")
            end
        end
        log("done", "%d,%d", state, ticks)
        return
    end
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
    end
end)
