-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
local frame = 0
local courseRestart, firstRetry, secondRetry = 320, 945, 1035
local startImage, checkpointImage
local irqStep, cameraX, cameraY
local nearHit, firstHit, secondHit, secondCue = false, false, false, false
local shotOnHit = false

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

watch(0xf010, function() irqStep = 1 end)
watch(0x2006, function(_, value)
    if irqStep == 1 then
        cameraX = (value & 4) ~= 0 and 256 or 0
        irqStep = 2
    elseif irqStep == 4 then
        irqStep = nil
    end
end)
watch(0x2005, function(_, value)
    if irqStep == 2 then
        cameraY = value - 31
        irqStep = 3
    elseif irqStep == 3 then
        cameraX = cameraX + value
        irqStep = 4
    end
end)

emu.addEventCallback(function()
    local t = frame - courseRestart
    emu.setInput({
        start = frame == 144 or frame == courseRestart or frame == firstRetry or frame == secondRetry,
        right = (frame >= 160 and frame <= 300) or (t >= 16 and t <= 584)
            or (frame >= firstRetry + 16 and frame <= firstRetry + 38),
        left = frame == firstRetry + 39,
        a = frame == 122 or (t >= 170 and t <= 584 and (t + 144) % 19 == 0) or frame == firstRetry + 16,
        b = t == 94 or frame == firstRetry + 40,
    }, 0)
end, emu.eventType.inputPolled)

local function samePicture(a, b)
    for i, pixel in ipairs(a) do
        if pixel ~= b[i] then return false end
    end
    return true
end

local function checkRetry()
    -- X=512 uses the same physical horizontal nametable as X=0.
    if cameraX ~= 0 or cameraY ~= 16 or sprite(3) ~= 184 or sprite(0) ~= 143
        or sprite(2) ~= 0 or sprite(8) ~= 0xf8 or sprite(16) ~= 0xf8
        or sprite(20) ~= 127 or sprite(22) ~= 3 or sprite(23) ~= 222 or sprite(28) ~= 0xf8 then
        fail(123, "checkpoint did not restore player, camera, kiln and projectile slots")
        return
    end
    local picture = emu.getScreenBuffer()
    for i = 1, 32 * 256 do
        if startImage[i] ~= picture[i] then
            fail(124, "checkpoint changed the fixed HUD")
            return
        end
    end
    local sky = picture[100 * 256 + 65]
    if picture[168 * 256 + 188] == sky or picture[226 * 256 + 65] == sky
        or picture[234 * 256 + 65] == sky then
        fail(125, "checkpoint marker or lower nametable rows were missing")
        return
    end
    if checkpointImage and not samePicture(checkpointImage, picture) then
        fail(126, "the second checkpoint retry drew a different screen")
        return
    end
    checkpointImage = picture
    return true
end

emu.addEventCallback(function()
    frame = frame + 1
    local hit = sprite(2) == 2
    if frame == 160 then
        startImage = emu.getScreenBuffer()
    elseif frame > 160 and frame <= courseRestart then
        nearHit = nearHit or hit
    elseif frame == courseRestart + 16 then
        if not nearHit then
            fail(120, "first enemy did not hit before the checkpoint")
            return
        end
        -- The OAM shadow can already contain the next frame's scroll adjustment.
        if sprite(2) ~= 0 or sprite(3) ~= 168 or cameraX ~= 0 or math.abs(sprite(0) + cameraY - 159) > 1 then
            fail(121, "pre-checkpoint hit did not restart the course at its original position")
        end
    elseif frame == courseRestart + 94 and sprite(8) == 0xf8 then
        fail(128, "first enemy did not return after restarting the course")
    elseif frame > courseRestart + 584 and frame <= firstRetry then
        firstHit = firstHit or hit
    elseif frame == firstRetry + 16 then
        if not firstHit then fail(122, "first encounter never hit the player"); return end
        checkRetry()
    elseif frame > firstRetry + 16 and frame <= secondRetry then
        secondCue = secondCue or sprite(22) == 2
        secondHit = secondHit or hit
        shotOnHit = shotOnHit or (hit and sprite(16) ~= 0xf8)
    elseif frame == secondRetry + 16 then
        if not secondCue or not secondHit then
            fail(127, "reset kiln did not wind up and hit the player again")
            return
        end
        if not shotOnHit then fail(129, "no player shot was present to reset"); return end
        if checkRetry() then emu.stop(0) end
    end
end, emu.eventType.endFrame)
