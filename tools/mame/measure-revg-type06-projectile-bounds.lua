-- MAME 0.289, makaimurg, default DIP switches. First section 4 boss projectile.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local check, axis, offset = (os.getenv("REVG_TYPE06_PROJECTILE_BOUNDS") or ""):match("^(%a+),([xy]),(-?%d+)$")
offset = tonumber(offset)
assert((check == "display" or check == "cleanup") and offset and offset >= -32768 and offset <= 32767,
    "Expected REVG_TYPE06_PROJECTILE_BOUNDS=display|cleanup,x|y,offset")

local entry = 0x6cee + 4 * 4
local frame, reads, moves = 0, 0, 0
local selected, slot, born, displayed, marked, cleared, returned, failure

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
    print(string.format("type06_projectile_bounds,%s,%s,%d,%s,%d,", check, axis, offset, event, frame)
        .. string.format(format, ...))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
revg_type06_projectile_bounds_taps = {
    page = mem:install_read_tap(entry, entry + 1, "section 4 start page", function(address)
        if selected and cpu.state.PC.value == 0x6d39 then return address == entry and 0x40 or 0xdc end
    end),
    camera = mem:install_read_tap(0x9c, 0x9f, "projectile camera comparison", function(address)
        if not born or frame ~= born + 1 or cpu.state.X.value ~= slot then return end
        local pc = cpu.state.PC.value
        local x_read, y_read
        if check == "display" then
            if word(cpu.state.S.value) ~= 0xfe1d then return end
            x_read, y_read = pc == 0x8cb7, pc == 0x8cc2
        else
            x_read, y_read = pc == 0x8dc9, pc == 0x8dd6
        end
        if not x_read and not y_read then return end
        local comparison_axis = x_read and "x" or "y"
        local position = word(slot - (x_read and 0x0a or 7))
        local difference = comparison_axis == axis and offset or 0
        local camera = (position - difference) & 0xffff
        if comparison_axis == axis then reads = reads + 1 end
        if address % 2 == 0 then log("check", "%s,%d", comparison_axis, difference) end
        return address % 2 == 0 and (camera >> 8) or (camera & 0xff)
    end),
    projectile = mem:install_write_tap(0x13c0, 0x1475, "projectile lifecycle", function(address, value)
        local pc = cpu.state.PC.value
        if selected and not slot and pc == 0xc120 and value == 0x0b and address == cpu.state.Y.value + 5 then
            slot, born = cpu.state.Y.value, frame
            log("shot", "%04x,%d", slot, mem:read_u8(0xa7))
            return
        end
        if not slot or cpu.state.X.value ~= slot then return end
        if pc == 0xa2c3 and address == slot - 7 then
            moves = moves + 1
            if moves == 1 then
                displayed = mem:read_u8(slot - 0x0f)
                log("display", "%d", displayed)
            end
        elseif pc == 0x8de4 and address == slot - 0x0d then
            if marked or value ~= 3 then failure = "Unexpected offscreen marking" end
            marked = frame
            log("mark", "%d,%d", value, mem:read_u8(0xa7))
        elseif pc == 0xfe2d and address == slot - 0x10 then
            if not marked or frame ~= marked + 1 or value ~= 0 then failure = "Unexpected slot clearing" end
            cleared = frame
            log("clear", "%d", value)
        end
    end),
    pool = mem:install_write_tap(0xa7, 0xa7, "projectile slot return", function(_, value)
        if not slot or cpu.state.X.value ~= slot or cpu.state.PC.value ~= 0x8ca3 then return end
        if cleared ~= frame or word(word(0xa8)) ~= slot then failure = "Unexpected slot return" end
        returned = frame
        log("return", "%d", value)
    end),
}

revg_type06_projectile_bounds_frame = emu.add_machine_frame_notifier(function()
    if failure or (born and frame >= born + 3) or frame >= 1400 then
        machine:exit()
        assert(not failure, failure)
        assert(born and reads == 2 and displayed ~= nil, "Missing camera comparison or movement")
        assert(marked == nil or (marked == born + 1 and returned == marked + 1 and moves == 1),
            "Unexpected lifecycle after marking")
        assert(marked ~= nil or moves == 3, "Missing movement on a retained slot")
        log("done", "%d,%d,%d", moves, mem:read_u8(slot - 0x10), mem:read_u8(0xa7))
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
