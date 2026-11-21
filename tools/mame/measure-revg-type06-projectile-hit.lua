-- MAME 0.289, makaimurg, default DIP switches. First section 4 boss projectile.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local trial = os.getenv("REVG_TYPE06_HIT") or "stand,0,0"
local pose, dx, dy = trial:match("^(%a+),(-?%d+),(-?%d+)$")
dx, dy = tonumber(dx), tonumber(dy)
assert((pose == "stand" or pose == "crouch" or pose == "hidden") and dx and dy
    and math.abs(dx) <= 64 and math.abs(dy) <= 64, "Expected REVG_TYPE06_HIT=stand|crouch|hidden,dx,dy")

local entry = 0x6cee + 4 * 4
local frame, comparisons = 0, 0
local selected, slot, born, checked, table_address, hit, marked, returned

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
    print(string.format("type06_projectile_hit,%s,%s,%d,", trial, event, frame) .. string.format(format, ...))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
revg_type06_projectile_hit_taps = {
    page = mem:install_read_tap(entry, entry + 1, "section 4 start page", function(address)
        if selected and cpu.state.PC.value == 0x6d39 then return address == entry and 0x40 or 0xdc end
    end),
    enemy = mem:install_write_tap(0x13c0, 0x1475, "projectile hit and cleanup", function(address, value)
        local pc = cpu.state.PC.value
        if selected and not slot and pc == 0xc120 and value == 0x0b and address == cpu.state.Y.value + 5 then
            slot, born = cpu.state.Y.value, frame
            log("shot", "%04x", slot)
        elseif slot and pc == 0xf4f2 and frame == checked and address == slot - 0x0d then
            log("hit_state", "%d,%d", mem:read_u8(slot - 0x10), value)
        elseif slot and pc == 0xfe28 and cpu.state.X.value == slot and address == slot - 0x0d then
            marked = frame
            log("mark", "%d", value)
        end
    end),
    comparison = mem:install_read_tap(0x13c1, 0x146a, "boss projectile comparison", function(address, value)
        if not slot or cpu.state.U.value ~= slot then return end
        local pc = cpu.state.PC.value
        if pc == 0xf4b1 and address == slot - 0x0f and not checked and value ~= 0 then
            checked = frame
            log("gate", "%d,%d", value, mem:read_u8(0x0500))
            if pose == "hidden" then return 0 end
        end
        if frame ~= checked then return end
        local position, anchor, delta
        if pc == 0xf4bd then
            position, anchor, delta = slot - 7, 0x0509, dy
        elseif pc == 0xf4c8 then
            position, anchor, delta = slot - 0x0a, 0x0506, dx
        else
            return
        end
        local operand = (word(anchor) + delta) & 0xffff
        if address == position then
            table_address = cpu.state.X.value
            comparisons = comparisons + 1
            return operand >> 8
        elseif address == position + 1 then
            return operand & 0xff
        end
    end),
    posture = mem:install_read_tap(0x051f, 0x051f, "projectile crouch table selection", function()
        if pose == "crouch" and born and frame == born + 1 and cpu.state.PC.value == 0xf49e then return 1 end
    end),
    armor = mem:install_write_tap(0x0500, 0x0500, "projectile damage", function(_, value)
        if slot and frame == checked and cpu.state.U.value == slot and cpu.state.PC.value == 0xf4e4 then
            hit = frame
            log("armor", "%d,%d", mem:read_u8(0x0500), value)
        end
    end),
    pool = mem:install_write_tap(0xa7, 0xa7, "projectile slot return", function(_, value)
        if slot and cpu.state.X.value == slot and cpu.state.PC.value == 0x8ca3 then
            returned = frame
            log("return", "%d", value)
        end
    end),
}

revg_type06_projectile_hit_frame = emu.add_machine_frame_notifier(function()
    if (checked and frame >= checked + 3) or frame >= 1350 then
        machine:exit()
        assert(checked and checked == born + 1 and ((pose == "hidden") == (comparisons == 0)),
            "Missing projectile eligibility or comparison")
        assert(not table_address or table_address == (pose == "crouch" and 0xf5d6 or 0xf556),
            "Unexpected collision table")
        if hit then
            assert(marked == hit + 1 and returned == hit + 2, "Unexpected projectile cleanup")
        else
            assert(not marked and not returned, "Projectile was removed without a hit")
        end
        log("done", "%s,%s,%d", hit and "hit" or "miss", table_address and string.format("%04x", table_address) or "-",
            comparisons)
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
