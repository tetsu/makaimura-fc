-- MAME 0.289, makaimurg, default DIP switches.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local trial = os.getenv("REVG_WEAPON")
local offsets = { inside = { 8, 8 }, ["outside-x"] = { 9, 8 }, ["outside-y"] = { 8, 9 } }
local offset = assert(offsets[trial], "Expected REVG_WEAPON=inside, outside-x or outside-y")
local frame, selected, seeded = 0, false, false
local holder, compared, branch
local dropped, checked_y, picked_up, checking_branch = false, false, false, false

local function log(event, format, ...)
    print(string.format("weapon,%s,%s,%d,", trial, event, frame) .. string.format(format, ...))
end

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error("Missing input")
end

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)

revg_weapon_taps = {
    spawn = mem:install_write_tap(0x15d4, 0x15d4, "item subtype", function(_, value)
        if not seeded or holder or cpu.state.PC.value ~= 0xeb33 then return end
        assert(value == 4 and mem:read_u8(0x7d) == 2, "Expected subtype 4 from the weapon selector")
        holder = word(0x15f0)
        log("select", "%d,%d,%d,%04x", mem:read_u8(0x72), mem:read_u8(0x7d), value, holder)
    end),
    -- Only the item update sees the holder in its death phase.
    release = mem:install_read_tap(0x0880, 0x0fff, "weapon holder state", function(address, value)
        if not holder or dropped or cpu.state.PC.value ~= 0xe5de or cpu.state.X.value ~= 0x15e2
            or address ~= holder - 0x0d or value ~= 1 or mem:read_u8(0x15d3) == 0 then return end
        if math.abs(word(0x15d8) - word(0xa0)) <= 48 then
            dropped = true
            log("release", "%04x,%d,%d", address, word(0x15d8), word(0x15db))
            return 2
        end
    end),
    overlap = mem:install_read_tap(0xa0, 0xa3, "item overlap", function(address)
        if not dropped or checking_branch or cpu.state.X.value ~= 0x15e2 then return end
        local pc = cpu.state.PC.value
        local axis = pc == 0xe6bc and 1 or pc == 0xe6c9 and 2
        if not axis then return end
        local item = word(axis == 1 and 0x15d8 or 0x15db)
        local test_position = (item - offset[axis]) & 0xffff
        if address % 2 == 0 then
            if axis == 1 and not compared then
                compared = frame
                log("overlap", "%d,%d,%d,%d", offset[1], offset[2], word(0x15d8), word(0x15db))
            elseif axis == 2 then
                checked_y = true
            end
            return test_position >> 8
        end
        return test_position & 0xff
    end),
    pickup = mem:install_write_tap(0x73, 0x73, "selected weapon", function(_, value)
        if not dropped or checking_branch or cpu.state.X.value ~= 0x15e2 or cpu.state.PC.value ~= 0xe704 then return end
        assert(trial == "inside" and value == 3, "Unexpected weapon change")
        picked_up = true
        log("pickup", "%d", value)
    end),
    key_branch = mem:install_write_tap(0x3e00, 0x3e00, "normal key path", function()
        if checking_branch and cpu.state.PC.value == 0x7134 then branch = "key" end
    end),
    retry_branch = mem:install_write_tap(0x0a, 0x0a, "retry path", function()
        if checking_branch and cpu.state.PC.value == 0x71a9 then branch = "retry" end
    end),
}

revg_weapon_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and frame >= 500 and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        mem:write_u8(0x72, 5)
        selected = true
    end
    if selected and not seeded and mem:read_u8(0x0b) == 1 then
        assert(mem:read_u8(0x73) == 1 and mem:read_u8(0xb0) == 6, "Unexpected initial item state")
        mem:write_u8(0x7d, 1)
        mem:write_u8(0x7b, 0x80)
        seeded = true
        log("seed", "%d,%d", mem:read_u8(0x7d), mem:read_u8(0x7b))
    end
    if compared and frame == compared + 2 then
        assert(checked_y == (trial ~= "outside-x") and picked_up == (trial == "inside")
            and mem:read_u8(0x73) == (picked_up and 3 or 1),
            "Weapon state did not match the overlap")
        -- Enter the post-boss handler without changing the tested weapon value.
        mem:write_u8(0x08, 2)
        mem:write_u8(0x0b, 0)
        mem:write_u8(0x0e, 0)
        checking_branch = true
    end
    if branch then
        machine:exit()
        assert(branch == (picked_up and "key" or "retry"), "Unexpected section 5 branch")
        log("done", "%d,%s", mem:read_u8(0x73), branch)
    end
    if frame == 5500 then
        machine:exit()
        error("No section 5 branch observed")
    end
end)
