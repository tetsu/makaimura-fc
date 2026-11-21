-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
local frame = 0
local start, earlyRetry, checkpointRetry = 144, 320, 945
local returnToMenu, reenter, resetAt, afterReset = 1055, 1080, 1112, 1240
local menu
local earlyHit, checkpointHit = false, false

local function sprite(offset)
    return emu.read(0x200 + offset, emu.memType.nesMemory)
end

local function pixel(picture, x, y)
    return picture[y * 256 + x + 1]
end

local function samePicture(a, b)
    for i, color in ipairs(a) do
        if color ~= b[i] then return false end
    end
    return #a == #b
end

local function atCourseStart()
    -- The camera is still settling vertically after the blanked redraw.
    local y = sprite(0)
    return sprite(3) == 168 and y >= 143 and y <= 159 and sprite(2) == 0 and sprite(28) == 0xf8
end

local function fail(code, message)
    emu.log(message)
    emu.stop(code)
end

emu.addEventCallback(function()
    local t, r = frame - earlyRetry, frame - checkpointRetry
    emu.setInput({
        b = frame == start or frame == reenter or frame == afterReset or t == 94 or r == 25,
        start = t == 0 or r == 0,
        select = frame == returnToMenu,
        right = (frame >= start + 16 and frame <= 300) or (t >= 16 and t <= 584)
            or (r >= 16 and frame < returnToMenu) or (frame >= reenter + 16 and frame < resetAt),
        a = (t >= 170 and t <= 584 and (t + 144) % 19 == 0) or r == 16,
    }, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    local hit = sprite(2) == 2
    if frame == 115 then
        menu = emu.getScreenBuffer()
        -- First row of the B in B COURSE.
        if pixel(menu, 67, 73) == menu[1] then fail(160, "course control was missing from the menu") end
    elseif frame == start + 16 then
        if not atCourseStart() then
            fail(161, "B did not start the two-axis course")
        end
    elseif frame > start + 16 and frame <= earlyRetry then
        earlyHit = earlyHit or hit
    elseif frame == earlyRetry + 16 then
        if not earlyHit or not atCourseStart() then
            fail(162, "retry before the checkpoint did not restore the course start")
        end
    elseif frame > earlyRetry + 584 and frame <= checkpointRetry then
        checkpointHit = checkpointHit or hit
    elseif frame == checkpointRetry + 16 then
        if not checkpointHit or hit or sprite(3) ~= 184 or sprite(0) ~= 143
            or sprite(20) ~= 127 or sprite(28) ~= 0xf8 then
            fail(163, "retry after the checkpoint did not restore the player and encounter")
        end
    elseif frame == checkpointRetry + 90 then
        local clear = emu.getScreenBuffer()
        -- Strokes of C in COURSE CLEAR and S in SELECT MENU.
        if pixel(clear, 83, 98) == clear[1] or pixel(clear, 92, 121) == clear[1] then
            fail(164, "clear screen or menu control was missing")
        end
    elseif frame == returnToMenu + 16 or frame == afterReset - 12 then
        if not samePicture(menu, emu.getScreenBuffer()) then
            fail(165, "SELECT after clear or reset did not restore the boot menu")
        end
    elseif frame == reenter + 16 or frame == afterReset + 16 then
        if not atCourseStart() then
            fail(166, "a new course did not start from its initial state")
            return
        end
        if frame > afterReset then emu.stop(0) end
    elseif frame == resetAt then
        emu.reset()
    end
end, emu.eventType.endFrame)
