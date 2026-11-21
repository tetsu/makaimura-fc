-- MAME 0.289, makaimurg. Cross the first scroll boundary and turn back using 1P inputs.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local scheduler = 0x07f0
local group_address = 0x0670
local quota = 0x3b

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input %s/$%02x", port, mask))
end

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local right, left, fire = field(":P1", 0x01), field(":P1", 0x02), field(":P1", 0x10)
local frame, phase, groups, schedules = 0, "outbound", 0, 0
local resume_at, crossing, retired_slots, failure
local zombies = {}

-- Frame, page, published camera X at the write, new group, direction, PC after write.
local expected_groups = {
    {1057, 0,   0, 0, 0x00, 0x8e3e},
    {1070, 0,   0, 1, 0x01, 0xe96c},
    {1842, 1, 239, 1, 0x01, 0x8e3e},
    {1846, 0, 241, 0, 0xff, 0x8e16},
    {1857, 0, 238, 1, 0x01, 0xe96c},
    {1950, 1, 239, 1, 0x01, 0x8e3e},
    {1981, 0, 241, 0, 0xff, 0x8e16},
    {1992, 0, 228, 1, 0x01, 0xe96c},
}

-- Frame, active flag being written, owner group, pattern, direction, current group.
local expected_schedules = {
    {1060, 1, 0, 0, 0x00, 0},
    {1070, 0, 0, 0, 0x00, 1},
    {1072, 1, 1, 1, 0x01, 1},
    {1843, 1, 1, 1, 0x01, 1},
    {1847, 1, 0, 0, 0xff, 0},
    {1857, 0, 0, 0, 0xff, 1},
    {1859, 1, 1, 1, 0x01, 1},
    {1951, 1, 1, 1, 0x01, 1},
    {1981, 0, 1, 1, 0x01, 0},
    {1982, 1, 0, 0, 0xff, 0},
    {1994, 1, 1, 1, 0x01, 1},
}

local function check(expected, ...)
    if not expected then
        failure = "Unexpected page trace event"
        return
    end
    for index, value in ipairs({...}) do
        if value ~= expected[index] then
            failure = string.format("Unexpected page trace value at frame %d, field %d", frame, index)
            return
        end
    end
end

local function log(event, format, ...)
    print(string.format("enemy_pages,%s,%d,", event, frame) .. string.format(format, ...))
end

revg_enemy_pages_taps = {
    quota = mem:install_write_tap(quota, quota, "zombie slot lifetime", function()
        if frame < 1057 then return end
        local pc = cpu.state.PC.value
        if pc == 0xeb02 and cpu.state.X.value == scheduler then
            zombies[cpu.state.Y.value] = true
        elseif pc == 0xd451 then
            zombies[cpu.state.X.value] = nil
        end
    end),
    group = mem:install_write_tap(group_address, group_address, "scroll encounter group", function(_, value)
        local pc = cpu.state.PC.value
        if frame < 1057 or (pc ~= 0x8e16 and pc ~= 0x8e3e and pc ~= 0xe96c) then return end
        local page, camera, direction = word(0x91), word(0x9c), cpu.state.B.value
        groups = groups + 1
        check(expected_groups[groups], frame, page, camera, value, direction, pc)
        log("group", "%d,%d,%d,%02x,%04x", page, camera, value, direction, pc)
        if direction ~= 0 and pc ~= 0xe96c then crossing = direction end
    end),
    scheduler = mem:install_write_tap(scheduler - 16, scheduler - 16, "page scheduler state", function(_, value)
        local pc = cpu.state.PC.value
        if frame < 1057 or (pc ~= 0xe920 and pc ~= 0xeaeb) then return end
        local owner, pattern, direction = mem:read_u8(scheduler + 4), mem:read_u8(scheduler + 3),
            mem:read_u8(scheduler + 1)
        local group = mem:read_u8(group_address)
        schedules = schedules + 1
        check(expected_schedules[schedules], frame, value, owner, pattern, direction, group)
        if (pc == 0xe920 and value ~= 1) or (pc == 0xeaeb and value ~= 0) then
            failure = "Unexpected scheduler state write"
        end
        log("schedule", "%d,%d,%d,%d,%02x,%d,%d", word(0x91), value, owner, pattern, direction,
            group, mem:read_u8(scheduler - 1))
        if pc == 0xeaeb and frame >= 1842 then
            retired_slots = {}
            for slot in pairs(zombies) do
                if mem:read_u8(slot - 16) == 0 then failure = "Tracked zombie slot is inactive" end
                table.insert(retired_slots, slot)
            end
            table.sort(retired_slots)
        end
    end),
}

revg_enemy_pages_frame = emu.add_machine_frame_notifier(function()
    if crossing then
        local camera = word(0x9c)
        if camera ~= (crossing == 1 and 241 or 239) then failure = "Unexpected published camera X" end
        log("camera", "%d,%d", word(0x91), camera)
        crossing = nil
    end
    if retired_slots then
        if #retired_slots ~= 3 or mem:read_u8(quota) ~= 0 or mem:read_u8(scheduler - 16) ~= 0 then
            failure = "Expected full zombie quota with an inactive scheduler"
        end
        for _, slot in ipairs(retired_slots) do
            if not zombies[slot] or mem:read_u8(slot - 16) == 0 then
                failure = "Retiring the scheduler removed an active zombie slot"
            end
            log("retained", "%04x,%d", slot, mem:read_u8(slot - 16))
        end
        retired_slots = nil
    end

    if failure or frame >= 1994 then
        machine:exit()
        assert(not failure, failure)
        assert(groups == #expected_groups and schedules == #expected_schedules, "Incomplete enemy page trace")
        log("done", "%d,%d", groups, schedules)
        return
    end

    frame = frame + 1
    local page = word(0x91)
    if phase == "outbound" and frame > 1100 and page == 1 then
        phase = "return"
    elseif phase == "return" and page == 0 then
        phase = "wait"
        resume_at = frame + 100
    elseif phase == "wait" and frame >= resume_at then
        phase = "outbound-again"
    end
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    right:set_value(frame >= 1064 and (phase == "outbound" or phase == "outbound-again") and 1 or 0)
    left:set_value(phase == "return" and 1 or 0)
    fire:set_value(frame >= 1064 and frame % 24 < 3 and 1 or 0)
end)
