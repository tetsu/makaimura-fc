-- MAME 0.289, makaimurg, default DIP switches.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = tonumber(os.getenv("REVG_KEY_SECTION"))
assert(section == 4 or section == 5, "Expected REVG_KEY_SECTION=4 or 5")

local pickup_cases = {
    { name = "jump", x = 0, y = 0, state = 2 },
    { name = "left", x = -9, y = 0, state = 1 },
    { name = "right", x = 9, y = 0, state = 1 },
    { name = "above", x = 0, y = -9, state = 1 },
    { name = "below", x = 0, y = 9, state = 1 },
    { name = "edge", x = section == 4 and -8 or 8, y = section == 4 and -8 or 8, state = 1 },
}
local frame, approach_checks, pickup_checks = 0, 0, 0
local selected, injected, visible, landed, picked_up, advanced

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local function log(event, format, ...)
    print(string.format("vertical_key,%d,%s,%d,", section, event, frame) .. string.format(format, ...))
end

local function field(mask)
    for _, item in pairs(machine.ioport.ports[":SYSTEM"].fields) do
        if item.mask == mask then return item end
    end
    error("Missing SYSTEM input")
end

local coin, start = field(0x40), field(0x01)
local bounds = 0x6cac + 4 * section
local page = 0x4000 + 32 * mem:read_u8(bounds + 2) + mem:read_u8(bounds + 1)
local entry = 0x6cee + 4 * section

revg_vertical_key_taps = {
    spawn_page = mem:install_read_tap(entry, entry + 1, "key area page", function(address)
        if selected and not injected and cpu.state.PC.value == 0x6d39 then
            return address == entry and page >> 8 or page & 0xff
        end
    end),
    player = mem:install_read_tap(0x0503, 0x050a, "key boundary inputs", function(address)
        if not injected or picked_up then return end
        local pc = cpu.state.PC.value
        local position
        if pc == 0x72fc then
            local threshold = word(0x5c) + 32
            position = threshold + (approach_checks == 0 and -1 or 0)
            if address == 0x050a then
                approach_checks = approach_checks + 1
                log("approach", "%d,%d,%d", position, threshold, mem:read_u8(0x0553))
            end
        elseif pc == 0x730f then
            if not landed then return 2 end
            pickup_checks = pickup_checks + 1
            local trial = assert(pickup_cases[pickup_checks], "Key pickup did not finish on the edge")
            log("overlap", "%s,%d,%d,%d", trial.name, trial.x, trial.y, trial.state)
            return trial.state
        elseif pc == 0x7317 then
            position = word(0x0521) + pickup_cases[pickup_checks].x
        elseif pc == 0x7327 then
            position = word(0x0550) + pickup_cases[pickup_checks].y
        end
        if position then
            local low_byte = address == 0x0507 or address == 0x050a
            return low_byte and position & 0xff or position >> 8
        end
    end),
    key = mem:install_write_tap(0x053a, 0x0555, "key activation and pickup", function(address, value)
        if not injected then return end
        local pc = cpu.state.PC.value
        if address == 0x0553 and pc == 0x72ef then
            assert(approach_checks == 2 and value == 1 and word(0x0521) == word(0x5a) + 37,
                "Unexpected key activation")
            visible = true
            log("activate", "%d", word(0x0521))
        elseif address == 0x053a and pc == 0x7409 then
            local spawn_y = 256 * mem:read_u8(0x0539) + value
            assert(visible and word(0x0536) == word(0x0521)
                and spawn_y == word(0x7416 + 2 * section), "Unexpected key spawn coordinates")
            log("spawn", "%d,%d", word(0x0536), spawn_y)
        elseif address == 0x0551 and pc == 0x7467 then
            assert(visible and not landed, "Unexpected key landing")
            landed = true
            local y = word(0x0539)
            local pickup_y = 256 * mem:read_u8(0x0550) + value
            assert(word(0x0536) == word(0x0521), "Key X differs from the pickup target")
            assert(pickup_y == y + 8, "Unexpected key pickup Y")
            log("land", "%d,%d,%d", word(0x0536), y, pickup_y)
        elseif address == 0x0555 and pc == 0x733e then
            assert(landed and pickup_checks == #pickup_cases and value == 9, "Unexpected key pickup")
            picked_up = frame
            log("pickup", "%d", value)
        end
    end),
    section = mem:install_write_tap(0x72, 0x72, "key section advance", function(_, value)
        if cpu.state.PC.value ~= 0x73a3 or not injected then return end
        assert(picked_up and value == section + 1, "Unexpected section advance")
        advanced = true
        log("advance", "%d,%d", value, frame - picked_up)
    end),
}

revg_vertical_key_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    local playing = mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
    local phase = mem:read_u8(0x0b)
    if not selected and playing and phase == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
    elseif selected and not injected and playing and phase == 1 and mem:read_u8(0x0503) == 1 then
        -- Wait until the player is active before entering the post-boss handler.
        injected = true
        mem:write_u8(0x08, 2)
        mem:write_u8(0x0b, 0)
        mem:write_u8(0x0e, 0)
        if section == 5 then mem:write_u8(0x73, 3) end
        log("inject", "%03x,%d,%d,%d", page - 0x4000, word(0x5a), word(0x5c), mem:read_u8(0x73))
    end
    if advanced then
        log("done", "%d,%d", approach_checks, pickup_checks)
        machine:exit()
    elseif frame >= 2200 then
        machine:exit()
        error("Vertical key trial did not complete")
    end
end)
