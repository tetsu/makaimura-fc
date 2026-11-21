-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
local frame = 0
local firstStart, alternateStart = 144, 240
local retry = alternateStart + 669
local replay, returnToMenu, finalStart = retry + 118, retry + 150, retry + 176
local originalPicture, alternatePicture
local sawBolt, tookHit = false, false
local cameraX, cameraY, splitStep

local function read(address)
    return emu.read(address, emu.memType.nesMemory)
end

local function pixel(picture, x, y)
    return picture[y * 256 + x + 1]
end

local function fail(code, message)
    emu.log(message)
    emu.stop(code)
end

local function watch(address, callback)
    emu.addMemoryCallback(callback, emu.callbackType.write, address, address, emu.memType.nesMemory)
end

watch(0xf010, function() splitStep = 1 end)
watch(0x2006, function(_, value)
    if splitStep == 1 then
        cameraX = (value & 4) ~= 0 and 256 or 0
        splitStep = 2
    elseif splitStep == 4 then
        splitStep = nil
    end
end)
watch(0x2005, function(_, value)
    if splitStep == 2 then
        cameraY = value - 31
        splitStep = 3
    elseif splitStep == 3 then
        cameraX = cameraX + value
        splitStep = 4
    end
end)

local function samePicture(a, b)
    for i, color in ipairs(a) do
        if color ~= b[i] then return false end
    end
    return #a == #b
end

local function cleanStart(bankTag, x, feet)
    -- The OAM shadow may already include the next frame's camera adjustment.
    return read(0x8000) == bankTag and cameraX == 0
        and read(0x203) == x - 8 and math.abs(read(0x200) + cameraY - (feet - 17)) <= 1
        and read(0x202) == 0 and read(0x210) == 0xf8 and read(0x21c) == 0xf8
end

emu.addEventCallback(function()
    local t, r = frame - alternateStart, frame - retry
    emu.setInput({
        b = frame == firstStart or frame == firstStart + 30 or t == 0 or t == 115
            or r == 25 or frame == finalStart,
        down = t == 0,
        right = (t >= 16 and t <= 609) or (r >= 16 and r < 96),
        a = t == 40 or (t >= 180 and t <= 609 and (t + 144) % 19 == 0) or r == 16,
        start = r == 0 or frame == replay,
        select = frame == firstStart + 40 or frame == returnToMenu,
    }, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    if frame == firstStart + 16 then
        if not cleanStart(0x80, 176, 176) then fail(167, "original course did not start"); return end
        originalPicture = emu.getScreenBuffer()
    elseif frame == firstStart + 40 then
        if read(0x210) == 0xf8 then fail(168, "no player shot was present before switching courses") end
    elseif frame == alternateStart + 16 then
        if not cleanStart(0x81, 160, 192) then fail(169, "alternate course did not start"); return end
        alternatePicture = emu.getScreenBuffer()
        if pixel(originalPicture, 64, 170) == originalPicture[1]
            or pixel(alternatePicture, 64, 170) ~= alternatePicture[1] then
            fail(170, "alternate terrain did not replace the original background")
        end
    elseif frame > alternateStart + 609 and frame <= retry then
        sawBolt = sawBolt or read(0x21c) ~= 0xf8
        tookHit = tookHit or read(0x202) == 2
    elseif frame == retry + 16 then
        if not sawBolt or not tookHit or read(0x8000) ~= 0x81 or cameraX ~= 0 or cameraY ~= 32
            or read(0x203) ~= 176 or read(0x200) ~= 143 or read(0x202) ~= 0
            or read(0x210) ~= 0xf8 or read(0x21c) ~= 0xf8 or read(0x214) ~= 111
            or read(0x216) ~= 3 or read(0x217) ~= 222 then
            fail(171, "alternate checkpoint did not restore camera, player and ranged encounter")
        end
    elseif frame == retry + 100 then
        local clear = emu.getScreenBuffer()
        if pixel(clear, 83, 98) == clear[1] then fail(172, "alternate course did not reach the clear screen") end
    elseif frame == replay + 16 then
        if not cleanStart(0x81, 160, 192) or not samePicture(alternatePicture, emu.getScreenBuffer()) then
            fail(173, "replaying from the clear screen did not reset the alternate course")
        end
    elseif frame == returnToMenu + 16 then
        if read(0x8000) ~= 0x80 then fail(174, "returning to the menu did not restore bank zero") end
    elseif frame == finalStart + 16 then
        if not cleanStart(0x80, 176, 176) or not samePicture(originalPicture, emu.getScreenBuffer()) then
            fail(175, "switching back did not restore the original course")
            return
        end
        emu.stop(0)
    end
end, emu.eventType.endFrame)
