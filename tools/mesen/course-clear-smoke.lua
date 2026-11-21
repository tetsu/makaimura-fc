-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
local frame = 0
local firstStart, secondStart, finalStart = 144, 870, 1800
local retry, earlyRestart = secondStart + 800, finalStart + 180
local cameraY, readingSplit
local startImage, clearImage
local sawFlag, crossedBolt, tookHit, earlyHit = false, false, false, false

local function sprite(offset)
    return emu.read(0x200 + offset, emu.memType.nesMemory)
end

local function fail(code, message)
    emu.log(message)
    emu.stop(code)
end

local function watch(address, callback)
    emu.addMemoryCallback(callback, emu.callbackType.write, address, address, emu.memType.nesMemory)
end

watch(0xf010, function() readingSplit = true end)
watch(0x2005, function(_, value)
    if readingSplit then
        cameraY = value - 31
        readingSplit = false
    end
end)

emu.addEventCallback(function()
    local courseStart = frame < secondStart and firstStart or secondStart
    local t, r = frame - courseStart, frame - retry
    local evading = courseStart == firstStart
    local onClear = frame == firstStart + 700 or r == 100
    emu.setInput({
        start = frame == firstStart or frame == secondStart or r == 0
            or frame == finalStart or frame == earlyRestart,
        right = not onClear and ((t >= 16 and t <= 584) or (evading and t >= 594)
            or (r >= 16 and frame < finalStart)
            or (frame >= finalStart + 16 and frame < earlyRestart)),
        a = frame == 122 or (t >= 170 and t <= 584 and (t + 144) % 19 == 0)
            or (evading and t == 593) or r == 16 or onClear,
        b = t == 94 or onClear,
        left = onClear,
    }, 0)
end, emu.eventType.inputPolled)

local function pixel(picture, x, y)
    return picture[y * 256 + x + 1]
end

local function showingClear(picture)
    local background = picture[1]
    -- The left strokes of C and S in COURSE CLEAR and START PLAY.
    return pixel(picture, 83, 98) ~= background and pixel(picture, 91, 114) ~= background
        and pixel(picture, 0, 220) == background
end

local function samePixels(a, b, count)
    for i = 1, count do
        if a[i] ~= b[i] then return false end
    end
    return true
end

local function checkStart(picture)
    return sprite(2) == 0 and sprite(3) == 168 and sprite(28) == 0xf8
        and math.abs(sprite(0) + cameraY - 159) <= 1
        and samePixels(startImage, picture, 32 * 256)
end

emu.addEventCallback(function()
    frame = frame + 1
    local hit = sprite(2) == 2
    local t = frame - firstStart
    if t == 16 then
        startImage = emu.getScreenBuffer()
    elseif t >= 584 and t <= 625 then
        local picture = emu.getScreenBuffer()
        if hit or showingClear(picture) then
            fail(131, "the evasion route was hit or cleared before the final ledge")
            return
        end
        local playerX, boltX = sprite(3) + 8, sprite(31) + 2
        if sprite(28) ~= 0xf8 and math.abs(playerX - boltX) <= 4 and sprite(0) + 16 < sprite(28) + 5 then
            crossedBolt = true
        end
        if t == 584 then
            -- The camera has reached world X=512. Sample the pennant on the last ledge.
            sawFlag = pixel(picture, 249, 160 - cameraY) ~= pixel(picture, 244, 160 - cameraY)
        end
    elseif t == 690 or t == 708 then
        local picture = emu.getScreenBuffer()
        if not sawFlag or not crossedBolt or not showingClear(picture) then
            fail(132, "the flag, airborne dodge or clear text was missing")
            return
        end
        if clearImage and not samePixels(clearImage, picture, 256 * 240) then
            fail(133, "movement and fire changed the clear screen")
            return
        end
        clearImage = picture
    elseif frame == secondStart + 16 or frame == finalStart + 16 then
        if not checkStart(emu.getScreenBuffer()) then
            fail(134, "START from clear did not restore the original course")
        end
    elseif frame > secondStart + 584 and frame <= retry then
        tookHit = tookHit or hit
    elseif frame == retry + 16 then
        if not tookHit or cameraY ~= 16 or sprite(3) ~= 184 or sprite(0) ~= 143
            or hit or sprite(28) ~= 0xf8 then
            fail(135, "the retry route did not restore the checkpoint after the hit")
        end
    elseif frame == retry + 90 or frame == retry + 108 then
        if not samePixels(clearImage, emu.getScreenBuffer(), 256 * 240) then
            fail(136, "the checkpoint route did not reach the same clear screen")
        end
    elseif frame > finalStart + 16 and frame <= earlyRestart then
        earlyHit = earlyHit or hit
    elseif frame == earlyRestart + 16 then
        if not earlyHit or not checkStart(emu.getScreenBuffer()) then
            fail(137, "a new course retained its previous checkpoint")
            return
        end
        emu.stop(0)
    end
end, emu.eventType.endFrame)
