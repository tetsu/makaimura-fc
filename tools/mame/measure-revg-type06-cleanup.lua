-- MAME 0.289, makaimurg, default DIP switches. Controlled section 4 camera comparisons.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local axis = os.getenv("REVG_TYPE06_AXIS")
local offset = tonumber(os.getenv("REVG_TYPE06_OFFSET"))
local limits = { x = { -128, 384 }, y = { -64, 320 } }
assert(limits[axis] and offset and math.tointeger(offset) and offset >= -32768 and offset <= 32767,
    "Expected axis x or y and a signed 16-bit offset")
local outside = offset < limits[axis][1] or offset > limits[axis][2]
local entry = 0x6cee + 4 * 4
local frame = 0
local selected, slot, activated, checked, marked, removed, failure

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
    print(string.format("type06_cleanup,%s,%d,%s,%d,", axis, offset, event, frame) .. string.format(format, ...))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
revg_type06_cleanup_taps = {
    page = mem:install_read_tap(entry, entry + 1, "section 4 start page", function(address)
        if selected and cpu.state.PC.value == 0x6d39 then
            return address == entry and 0x40 or 0xdc
        end
    end),
    camera = mem:install_read_tap(0x9c, 0x9f, "boss cleanup camera comparison", function(address)
        if not activated or cpu.state.X.value ~= slot then return end
        local pc = cpu.state.PC.value
        local compare_axis, position
        if pc == 0x8dc9 then
            compare_axis, position = "x", slot - 0x0a
        elseif pc == 0x8dd6 then
            compare_axis, position = "y", slot - 7
        else
            return
        end
        local displacement = compare_axis == axis and offset or 0
        if compare_axis == axis and address % 2 == 0 and not checked then
            checked = frame
            log("check", "%04x,%d", slot, mem:read_u8(slot + 0x14))
        end
        local camera = (word(position) - displacement) & 0xffff
        return address % 2 == 0 and camera >> 8 or camera & 0xff
    end),
    enemy = mem:install_write_tap(0x0880, 0x0fff, "boss cleanup states", function(address, value)
        if not selected then return end
        local pc, current_slot = cpu.state.PC.value, cpu.state.X.value
        if pc == 0xbd70 and address == current_slot + 0x14 then
            if value ~= 4 or mem:read_u8(current_slot + 5) ~= 0x06 or slot then
                failure = "Unexpected boss initialization"
            end
            slot = current_slot
        end
        if current_slot ~= slot then return end
        if pc == 0xbdd6 and address == slot + 0x14 then
            if activated or value ~= 4 then failure = "Unexpected activation" end
            activated = frame
            log("activate", "%04x,%d", slot, value)
        elseif pc == 0x8de4 and address == slot - 0x0d then
            if not outside or marked or frame ~= checked or value ~= 3 then
                failure = "Unexpected offscreen state"
            end
            marked = frame
            log("offscreen", "%04x,%d", slot, mem:read_u8(slot + 0x14))
        elseif pc == 0xc248 and address == slot - 0x10 then
            local hp = mem:read_u8(slot + 0x14)
            if not marked or frame ~= marked + 1 or value ~= 0 or hp ~= 4 then
                failure = "Unexpected boss removal"
            end
            removed = frame
            log("remove", "%04x,%d,%d", slot, hp, mem:read_u8(0xb6))
        end
    end),
    remaining = mem:install_write_tap(0xb6, 0xb6, "boss count after cleanup", function(_, value)
        if selected and cpu.state.PC.value == 0xe78e then
            if removed ~= frame or value ~= 0 then failure = "Unexpected boss count update" end
            log("remaining", "%d", value)
        end
    end),
    progress = mem:install_write_tap(0x08, 0x08, "section progress after cleanup", function(_, value)
        if selected and cpu.state.PC.value == 0xe79d then
            if removed ~= frame or value ~= 2 then failure = "Unexpected progress update" end
            log("progress", "%d", value)
        end
    end),
}

revg_type06_cleanup_frame = emu.add_machine_frame_notifier(function()
    if failure or (checked and frame >= checked + 4) or frame >= 1150 then
        machine:exit()
        assert(not failure, failure)
        assert(checked and checked == activated + 1, "Missing cleanup comparison after activation")
        assert((removed ~= nil) == outside, "Unexpected cleanup result")
        local remaining, progress = mem:read_u8(0xb6), mem:read_u8(0x08)
        assert(remaining == (outside and 0 or 1) and progress == (outside and 2 or 1),
            "Unexpected section state")
        log("done", "%d,%d", remaining, progress)
        return
    end
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, 4)
    end
end)
