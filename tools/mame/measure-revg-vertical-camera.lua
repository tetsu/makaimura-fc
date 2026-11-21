-- MAME 0.289, makaimurg, default DIP switches.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = tonumber(os.getenv("REVG_CAMERA_SECTION"))
assert(section == 4 or section == 5, "Expected REVG_CAMERA_SECTION=4 or 5")

-- Each step gives the player's relative Y, expected movement, and optional $99 value.
local profiles = {
    speed = {
        { 142, 0 }, { 143, 1 }, { 146, 1 }, { 147, 3 }, { 151, 3 }, { 143, 3 }, { 152, 5 }, { 152, 5 },
        { 143, 5 }, { 141, -1 }, { 138, -1 }, { 137, -3 }, { 133, -3 }, { 132, -5 },
        { 152, 0, 1 }, { 132, -5, 1 }, { 142, 0 }, { 134, -3 },
    },
    minimum = { { 132, 0 }, { 142, 0 }, { 143, 1 }, { 132, -5 }, { 132, 0 }, { 152, 5 } },
    maximum = { { 147, 3 }, { 152, 5 }, { 152, 0 }, { 132, -5 } },
}
local profile = os.getenv("REVG_CAMERA_PROFILE")
local steps = assert(profiles[profile], "Expected REVG_CAMERA_PROFILE=speed, minimum or maximum")

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local function field(mask)
    for _, item in pairs(machine.ioport.ports[":SYSTEM"].fields) do
        if item.mask == mask then return item end
    end
    error("Missing SYSTEM input")
end

local bounds = 0x6cac + 4 * section
local minimum, maximum = mem:read_u8(bounds + 3), mem:read_u8(bounds + 2)
local initial_page_y = profile == "minimum" and minimum or profile == "maximum" and maximum
    or (minimum + maximum) // 2
local entry = 0x6cee + 4 * section
local page = 0x4000 + 32 * initial_page_y + (word(entry) & 31)
local coin, start = field(0x40), field(0x01)

local frame, index, row_updates = 0, 1, 0
local selected, active, before, row_direction, failure

local function log(event, format, ...)
    print(string.format("vertical_camera,%d,%s,%s,%d,", section, profile, event, frame)
        .. string.format(format, ...))
end

revg_vertical_camera_taps = {
    page = mem:install_read_tap(entry, entry + 1, "camera starting page", function(address)
        if selected and not active and cpu.state.PC.value == 0x6d39 then
            return address == entry and page >> 8 or page & 0xff
        end
    end),
    flag = mem:install_read_tap(0x99, 0x99, "vertical follow flag", function(_, value)
        if not active or not steps[index] or cpu.state.PC.value ~= 0x8529 then return end
        if value ~= 0 then failure = "Expected normal play with $99 clear" end
        return steps[index][3]
    end),
    player = mem:install_read_tap(0x0509, 0x050a, "camera-relative player Y", function(address)
        if not active or not steps[index] then return end
        local pc = cpu.state.PC.value
        if pc ~= 0x852e and pc ~= 0x8554 then return end
        before = word(0x0639)
        local y = before + steps[index][1]
        return address == 0x0509 and y >> 8 or y & 0xff
    end),
    offscreen = mem:install_read_tap(0xa2, 0xa3, "offscreen check during camera trial", function(address)
        if not active or not steps[index] or cpu.state.PC.value ~= 0xf6c0 then return end
        -- Keep the offscreen check independent of the artificial camera motion.
        local y = word(0x9e) + 64
        return address == 0xa2 and y >> 8 or y & 0xff
    end),
    floor = mem:install_write_tap(0x5e, 0x5e, "moving camera minimum", function(_, value)
        if not selected or cpu.state.PC.value ~= 0xee62 then return end
        if value * 256 ~= word(0x5e) then
            if value ~= mem:read_u8(0x9e) - 2 then failure = "Unexpected moving camera minimum" end
            log("floor", "%d,%d", word(0x5e), value * 256)
        end
    end),
    row = mem:install_write_tap(0x2b, 0x2b, "vertical tile row update", function(_, value)
        if not before then return end
        local pc = cpu.state.PC.value
        if pc == 0x859f or pc == 0x85dd then
            if row_direction then failure = "Multiple tile row updates on one step" end
            row_direction = value == 0 and 1 or -1
            row_updates = row_updates + 1
        end
    end),
    scroll = mem:install_write_tap(0x9f, 0x9f, "published camera Y", function(_, value)
        if not before or cpu.state.PC.value ~= 0x7fd1 then return end
        local step = steps[index]
        local after = 256 * mem:read_u8(0x9e) + value
        local delta = after - before
        log("step", "%d,%d,%d,%d,%d,%d", step[1], step[3] or 0, before, after, delta, row_direction or 0)
        local crossed_row = before // 16 ~= after // 16
        if after ~= word(0x0639) or delta ~= step[2] then
            failure = "Unexpected camera displacement at step " .. index
        elseif (row_direction ~= nil) ~= crossed_row or (row_direction and row_direction * delta <= 0) then
            failure = "Unexpected tile row update at step " .. index
        end
        before, row_direction = nil, nil
        index = index + 1
    end),
}

revg_vertical_camera_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    local playing = mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
    local phase = mem:read_u8(0x0b)
    if not selected and playing and phase == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
    elseif selected and not active and playing and phase == 1 and mem:read_u8(0x0503) == 1 then
        assert(word(0x5c) == maximum * 256, "Unexpected camera maximum")
        active = true
        log("start", "%03x,%d,%d", page - 0x4000, word(0x5e), word(0x5c))
    end
    if failure then
        machine:exit()
        error(failure)
    elseif index > #steps then
        machine:exit()
        local final_minimum = profile == "maximum" and maximum - 2 or minimum
        assert(word(0x5e) == final_minimum * 256, "Unexpected final camera minimum")
        log("done", "%d,%d", #steps, row_updates)
    elseif frame >= 1500 then
        machine:exit()
        error("Vertical camera trial did not complete")
    end
end)
