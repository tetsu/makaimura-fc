-- Mesen 2.1.1, NTSC, NES controller on port 1, matching ca65 .dbg file.
local trials = {"walk", "jump", "forward", "shot"}
local warmup, trial_length, action = 120, 110, 20
local frame = 0
local addresses = {}
local samples = {}

local function address(label)
    if not addresses[label] then
        local ok, found = pcall(emu.getLabelAddress, label)
        assert(ok and found, "Load the matching ca65 .dbg file for " .. label)
        addresses[label] = found.address
    end
    return addresses[label]
end

local function read(addr)
    return emu.read(addr, emu.memType.nesMemory)
end

local function current_trial(elapsed)
    if elapsed < 0 then return end
    return trials[elapsed // trial_length + 1], elapsed % trial_length
end

emu.addEventCallback(function()
    local trial, t = current_trial(frame - warmup)
    if not trial then return end
    local dt = t - action
    emu.setInput({
        start = t == 2,
        right = (trial == "walk" and ((dt >= 0 and dt < 8) or (dt >= 16 and dt < 20)))
            or (trial == "forward" and dt >= 0 and dt < 34),
        left = trial == "walk" and dt >= 12 and dt < 16,
        a = (trial == "jump" or trial == "forward") and dt == 0,
        b = trial == "shot" and dt == 0,
    }, 0)
end, emu.eventType.inputPolled)

local function report()
    local walk = samples.walk
    for _, dt in ipairs({-1, 0, 1, 7, 8, 12, 15, 16}) do
        emu.log(string.format("walk,%d,%d,%d", dt, walk[dt].pad, walk[dt].x))
    end
    for _, trial in ipairs({"jump", "forward"}) do
        local rows = samples[trial]
        local lift, landing, peak
        for dt = 0, 40 do
            local row = rows[dt]
            if row.jump ~= 0xff then
                lift = lift or dt
                peak = math.min(peak or row.y, row.y)
            elseif lift then
                landing = dt
                break
            end
        end
        assert(lift and landing, "Failed to record lift-off and landing")
        emu.log(string.format("%s,%d,%d,%d,%d", trial, lift, landing,
            rows[-1].y - peak, rows[landing].x - rows[lift].x))
    end
    for dt = 0, 40 do
        if samples.shot[dt].shot > 0 then
            emu.log(string.format("shot,%d", dt))
            return
        end
    end
    error("Failed to observe a shot")
end

emu.addEventCallback(function()
    frame = frame + 1
    local elapsed = frame - warmup - 1
    local trial, t = current_trial(elapsed)
    if elapsed < 0 then return end
    if not trial then
        report()
        emu.stop(0)
        return
    end
    local dt = t - action
    if dt < -1 or dt > 40 then return end
    local x = address("player_x")
    if dt == -1 then
        assert(read(x) == 176 and read(x + 1) == 0 and read(address("player_feet")) == 176,
            "Trial did not start from the expected playable scene")
        samples[trial] = {}
    end
    samples[trial][dt] = {
        pad = read(address("pad")),
        x = read(x) + 256 * read(x + 1),
        y = read(address("player_feet")),
        jump = read(address("jump_step")),
        shot = read(address("shot_life")),
    }
end, emu.eventType.endFrame)
