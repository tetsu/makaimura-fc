-- Run from power-on with the Mesen 2.1.1 WAV recorder already started.
-- record-course-audio.py sets alternateCourse and muteShot.
local frame = 0
local start = alternateCourse and 240 or 144
local earlyRetry = 320
local retry = alternateCourse and start + 669 or 945
local returnToMenu = retry + (alternateCourse and 150 or 110)
local scene, hit

local function read(address)
    return emu.read(address, emu.memType.nesMemory)
end

local function mark(event)
    emu.log(string.format("%4d %s", frame, event))
end

emu.addMemoryCallback(function()
    mark("shot")
end, emu.callbackType.write, 0x4007, 0x4007, emu.memType.nesMemory)

if muteShot then
    emu.addMemoryCallback(function(_, value)
        -- Pulse 2 uses constant volume. Leave duty and timing intact.
        return value & 0xf0
    end, emu.callbackType.write, 0x4004, 0x4004, emu.memType.nesMemory)
end

emu.addEventCallback(function()
    local r = frame - retry
    local t = frame - (alternateCourse and start or earlyRetry)
    local input = {
        start = r == 0 or (not alternateCourse and t == 0),
        select = frame == returnToMenu,
        b = frame == start or r == 25,
        a = r == 16,
        right = r >= 16 and r < 96,
    }
    if alternateCourse then
        -- Same approach and checkpoint recovery as course-bank-smoke.lua.
        input.down = t == 0
        input.up = t == 26 or r == 26
        input.right = input.right or (t >= 16 and t <= 603)
        input.a = input.a or t == 40 or (t >= 180 and t <= 603 and (t + 144) % 19 == 0)
        input.b = input.b or t == 115 or t == 141 or (t >= 200 and t <= 500 and t % 13 == 0)
    else
        -- Same early retry and checkpoint approach as course-menu-smoke.lua.
        input.right = input.right or (frame >= start + 16 and frame <= 300) or (t >= 16 and t <= 584)
        input.a = input.a or (t >= 170 and t <= 584 and (t + 144) % 19 == 0)
        input.b = input.b or t == 94
    end
    emu.setInput(input, 0)
end, emu.eventType.inputPolled)

local reachedCheckpoint, resumed, cleared, returned
emu.addEventCallback(function()
    local nextScene, nextHit = read(0x07), read(0x3c)
    if frame >= start and nextScene ~= scene then
        if nextScene == 4 then
            mark("course start")
        elseif nextScene == 5 then
            mark("clear")
            cleared = true
        elseif nextScene == 0 then
            mark("menu")
            returned = cleared
        end
    end
    if nextScene == 4 and nextHit ~= hit then
        if nextHit == 1 then
            reachedCheckpoint = read(0x54) == 1
            mark(reachedCheckpoint and "hit after checkpoint" or "hit before checkpoint")
        elseif hit == 1 then
            resumed = reachedCheckpoint and read(0x54) == 1
            mark(resumed and "checkpoint retry" or "early retry")
        end
    end
    scene, hit = nextScene, nextHit
    frame = frame + 1
    if frame == returnToMenu + 80 then
        emu.stop(resumed and returned and 0 or 1)
    end
end, emu.eventType.endFrame)
