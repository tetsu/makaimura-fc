-- MAME 0.289, makaimurg, first 1P credit at the start of section 0.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local difficulty = os.getenv("REVG_ZOMBIE_DIFFICULTY") or "Normal"
local trial = os.getenv("REVG_ZOMBIE_CADENCE_TRIAL") or "idle"
local profiles = {
    Easy = { limit = 3, spawns = {1062, 1074, 1106, 1490}, release = 1449 },
    Normal = { limit = 3, spawns = {1062, 1074, 1106, 1490}, release = 1449 },
    Difficult = { limit = 2, spawns = {1062, 1074, 1420}, release = 1389 },
    ["Very Difficult"] = { limit = 2, spawns = {1062, 1074, 1400}, release = 1369 },
}
local profile = assert(profiles[difficulty], "Unknown Difficulty DIP")
assert(trial == "idle" or trial == "pool" and difficulty == "Normal",
    "Expected idle, or pool on Normal")

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
local dip = field(":DSW2", 0x60)
for value, label in pairs(dip.settings) do
    if label == difficulty then dip.user_value = value end
end
assert(dip.settings[dip.user_value] == difficulty, "Difficulty DIP did not change")

local frame, spawns, resets, stalled = 0, 0, 0, 0
local blocked, first_release, timer_before, quota_before, failure
local scheduler, quota, timer = 0x07f0, 0x3b, 0x07ef
local expected_entries = {0x56de, 0x56fd, 0x5703, 0x5709}
local expected_delays = {31, 31, 41, 31}

local function log(event, format, ...)
    print(string.format("zombie_cadence,%s,%s,%s,%d,", difficulty, trial, event, frame)
        .. string.format(format, ...))
end

revg_zombie_cadence_taps = {
    reset = mem:install_write_tap(scheduler - 16, scheduler - 16, "opening zombie scheduler group", function(_, value)
        if frame < 1050 or cpu.state.PC.value ~= 0xe920 then return end
        local group, pattern = mem:read_u8(0x0670), mem:read_u8(scheduler + 3)
        log("group", "%d,%d,%d,%d", group, pattern, word(0x9c), word(0x91))
        if value ~= 1 or group ~= resets or pattern ~= resets or resets >= 2
            or word(0x9c) ~= 0 or word(0x91) ~= 0 then
            failure = "Unexpected scheduler group at the stationary opening"
        end
        resets = resets + 1
    end),
    pool = mem:install_read_tap(0x93, 0x93, "opening zombie shared slot check", function(_, value)
        if trial ~= "pool" or blocked or frame < 1050 or cpu.state.PC.value ~= 0xe9fd
            or cpu.state.X.value ~= scheduler then return end
        if value == 0 or mem:read_u8(quota) ~= profile.limit then failure = "Expected both slots and quota" end
        blocked = frame
        log("pool", "%d,%d", value, mem:read_u8(quota))
        return 0
    end),
    wait = mem:install_read_tap(quota, quota, "opening zombie countdown gate", function(_, value)
        if frame < 1050 or cpu.state.PC.value ~= 0xead3 or cpu.state.X.value ~= scheduler then return end
        timer_before, quota_before = mem:read_u8(timer), value
        if value == 0 then
            if stalled == 0 then log("stall", "%d,%d", timer_before, mem:read_u8(0x93)) end
            if mem:read_u8(0x93) == 0 then failure = "Expected free shared slots during the quota stall" end
            stalled = stalled + 1
        end
    end),
    quota = mem:install_write_tap(quota, quota, "opening zombie quota changes", function(_, value)
        if frame < 1050 then return end
        local pc = cpu.state.PC.value
        local before, free = mem:read_u8(quota), mem:read_u8(0x93)
        if pc == 0xeb02 and cpu.state.X.value == scheduler then
            spawns = spawns + 1
            local slot = cpu.state.Y.value
            local entry, delay = word(scheduler + 0x0e) - 6, mem:read_u8(timer)
            log("spawn", "%04x,%04x,%d,%d,%d,%d", slot, entry, delay, before, value, free)
            local expected_frame = trial == "pool" and 1063 or profile.spawns[spawns]
            if frame ~= expected_frame or entry ~= expected_entries[spawns]
                or delay ~= expected_delays[spawns] or value ~= before - 1
                or mem:read_u8(slot + 5) ~= 0x0b or (spawns == 1 and before ~= profile.limit) then
                failure = "Unexpected zombie spawn or quota decrement"
            end
        elseif pc == 0xd451 then
            log("release", "%04x,%d,%d,%d,%d", cpu.state.X.value, mem:read_u8(timer), before, value, free)
            if not first_release then
                first_release = frame
                if frame ~= profile.release or before ~= 0 or stalled == 0 then
                    failure = "Expected first zombie removal during the quota stall"
                end
            end
            if value ~= before + 1 or value > profile.limit then failure = "Unexpected quota restoration" end
        end
    end),
}

revg_zombie_cadence_frame = emu.add_machine_frame_notifier(function()
    if timer_before then
        local expected_timer = timer_before - (quota_before > 0 and 1 or 0)
        if mem:read_u8(timer) ~= expected_timer then failure = "Unexpected countdown change" end
        timer_before, quota_before = nil, nil
    end
    local done = trial == "pool" and spawns == 1 or trial == "idle" and spawns == #profile.spawns
    if failure or done or frame >= 1600 then
        machine:exit()
        assert(not failure, failure)
        assert(done, "Zombie cadence trial did not complete")
        if trial == "pool" then
            assert(blocked == 1062 and resets == 1 and stalled == 0, "Missing isolated pool retry")
        else
            assert(first_release and stalled == first_release - profile.spawns[profile.limit] - 1
                and resets == 2, "Missing expected quota stall and release")
        end
        log("done", "%d,%d,%d", spawns, stalled, resets)
    end
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
end)
