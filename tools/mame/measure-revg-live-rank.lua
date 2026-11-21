-- MAME 0.289, makaimurg, default DIP switches, first 1P credit.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local trial = os.getenv("REVG_LIVE_RANK_TRIAL") or "natural"
local rank_text, variant = trial:match("^speed%-(%d+)%-(%a+)$")
local rank = tonumber(rank_text)
local speeds = {
    [0] = { normal = 208, alternate = 416 },
    [16] = { normal = 272, alternate = 432 },
    [32] = { normal = 304, alternate = 448 },
    [48] = { normal = 384, alternate = 512 },
}
local speed = speeds[rank] and speeds[rank][variant]
assert(trial == "natural" or trial == "cap" or speed, "Unknown REVG_LIVE_RANK_TRIAL: " .. trial)

local function field(mask)
    for _, item in pairs(machine.ioport.ports[":SYSTEM"].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing SYSTEM input $%02x", mask))
end

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local function x256(address)
    return 256 * word(address) + mem:read_u8(address + 2)
end

local function signed_word(address)
    local value = word(address)
    return value < 0x8000 and value or value - 0x10000
end

local frame = 0
local coin, start = field(0x40), field(0x01)
local zombie = 0x0e00
local clock, sampled_frame, previous_clock, previous_tick, failure
local ticks, skipped, moves = 0, 0, 0
local seeded, assigned
local pointer, previous_x

local function log(event, format, ...)
    print(string.format("live_rank,%s,%s,%d,", trial, event, frame) .. string.format(format, ...))
end

revg_live_rank_taps = {
    clock = mem:install_read_tap(0x21, 0x21, "rank update clock", function(_, value)
        if speed or mem:read_u8(0x05) ~= 1 or mem:read_u8(0x08) ~= 1
            or mem:read_u8(0x0b) ~= 1 or cpu.state.PC.value ~= 0xee17 then return end
        if sampled_frame == frame then failure = "Multiple rank updates checked in one frame" end
        clock, sampled_frame = 256 * mem:read_u8(0x20) + value, frame
    end),
    rank = mem:install_read_tap(0x9a, 0x9a, "first zombie rank selection", function(_, value)
        if not speed or pointer or frame < 1050 or cpu.state.PC.value ~= 0xd35c
            or cpu.state.X.value ~= zombie then return end
        if value ~= 6 then failure = "Expected Normal rank for the first zombie" end
        return rank
    end),
    variant = mem:install_read_tap(zombie + 0x0b, zombie + 0x0b, "first zombie alternate speed", function()
        if speed and variant == "alternate" and not pointer and frame >= 1050
            and cpu.state.PC.value == 0xd364 and cpu.state.X.value == zombie then return 3 end
    end),
    velocity = mem:install_write_tap(zombie - 4, zombie - 3, "first zombie velocity pointer",
        function(address, value)
            if not speed or frame < 1050 or cpu.state.PC.value ~= 0xd36f
                or cpu.state.X.value ~= zombie then return end
            if address == zombie - 4 then
                local source = cpu.state.U.value
                pointer = word(source)
                log("table", "%d,%04x,%04x", rank, source, pointer)
                local expected_source = 0x52a8 + rank // 4 + (variant == "alternate" and 2 or 0)
                if source ~= expected_source or value ~= pointer >> 8
                    or signed_word(pointer + 4) ~= -speed or word(pointer + 6) ~= 0 then
                    failure = "Unexpected zombie velocity table"
                end
            else
                assigned = pointer ~= nil and value == (pointer & 0xff)
                if not assigned then failure = "Unexpected velocity pointer low byte" end
            end
        end),
}

revg_live_rank_frame = emu.add_machine_frame_notifier(function()
    coin:set_value(frame >= 399 and frame < 403 and 1 or 0)
    start:set_value(frame >= 499 and frame < 503 and 1 or 0)
    local done
    if assigned then
        local x = x256(zombie - 10)
        if previous_x then
            log("motion", "%d,%d,%d", previous_x, x, x - previous_x)
            if word(zombie - 4) ~= pointer or x - previous_x ~= -speed then
                failure = "Unexpected first zombie X displacement"
            end
            moves = moves + 1
        end
        previous_x = x
        done = moves == 8
    elseif sampled_frame == frame then
        local before = previous_tick or (seeded and 0x3fc0 or 0x0600)
        local after = word(0x9a)
        if mem:read_u8(0x61) ~= 6 then failure = "Unexpected carried rank change" end
        if (clock & 0xff) == 0 then
            local sum = before + 0x006b
            local expected = (math.min(sum >> 8, 63) << 8) | (sum & 0xff)
            log("tick", "%04x,%04x,%04x", clock, before, after)
            if after ~= expected then failure = "Missing or incorrect in-play rank update" end
            if previous_clock and ((clock - previous_clock) & 0xffff) ~= 256 then
                failure = "Unexpected ticks between rank updates"
            end
            previous_clock = clock
            previous_tick, ticks = after, ticks + 1
        else
            if after ~= before then failure = "Rank changed outside the update boundary" end
            skipped = skipped + 1
        end
        if trial == "cap" and not seeded then
            -- Seed only the live rank after the first ordinary gameplay update.
            mem:write_u8(0x9a, 0x3f)
            mem:write_u8(0x9b, 0xc0)
            seeded = true
            log("seed", "%04x", word(0x9a))
        end
        done = ticks == (trial == "cap" and 2 or 3) and skipped > 0
    end
    if failure or done or frame >= 1700 then
        machine:exit()
        assert(not failure, failure)
        assert(done, "Live rank trial did not complete")
        log("done", "%d,%d,%d", ticks, skipped, moves)
    end
    frame = frame + 1
end)
