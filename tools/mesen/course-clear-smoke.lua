-- Mesen 2.1.1, NES controller on port 1, NTSC, PPU sprite limit enabled.
local frame = 0
local firstStart, secondStart, finalStart = 144, 870, 1800
local retry, earlyRestart = secondStart + 800, finalStart + 180
local cameraX, cameraY, splitStep
local startImage, clearImage
local sawFlag, crossedBolt, tookHit, earlyHit = false, false, false, false
local playing, displayChanged
local uploads, dmaCount, splitCount, budget = 0, 0, 0, 0
local worstBudget, worstCombined, retryBudget = 0, 0, 0
local fmRegister, fmKeys, apu = nil, 0, 0
local lastNote, musicDuringShot, retryShot
local previousTerrain, previousX, previousY, preHitTerrain
local boltX, boltY, boltBackground, boltFrame, boltRow

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

watch(0x2001, function(_, value)
    playing = value == 0x18
    displayChanged = true
end)
watch(0x2007, function()
    if not playing or displayChanged then return end
    local line = emu.getState()["ppu.scanline"]
    if line < 241 or line > 260 then fail(138, "terrain upload outside VBlank") end
    uploads = uploads + 1
end)
watch(0x4014, function(_, value)
    if not playing or displayChanged then return end
    local state = emu.getState()
    local dots = (state["ppu.scanline"] - 241) * 341 + state["ppu.cycle"]
    budget = math.ceil(dots / 3) + 514
    if value ~= 2 or dots < 0 or budget >= 2273 then
        fail(139, "OAM DMA would cross VBlank or used the wrong buffer")
    end
    dmaCount = dmaCount + 1
end)
watch(0xf010, function()
    splitCount = splitCount + 1
    splitStep = 1
end)
watch(0x2006, function(_, value)
    if splitStep == 1 then
        cameraX = (value & 4) ~= 0 and 256 or 0
        splitStep = 2
    elseif splitStep == 4 then
        local state = emu.getState()
        if state["ppu.scanline"] ~= 31 or state["ppu.cycle"] >= 255 then
            fail(140, "HUD split missed scanline 31")
        end
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
watch(0x9010, function(_, value) fmRegister = value end)
watch(0x9030, function(_, value)
    if fmRegister and fmRegister >= 0x20 and fmRegister <= 0x25 then
        local bit = 1 << (fmRegister - 0x20)
        if (value & 16) ~= 0 then fmKeys = fmKeys | bit else fmKeys = fmKeys & ~bit end
    end
end)
watch(0x4015, function(_, value) apu = value end)
watch(0x4003, function()
    lastNote = frame
    if playing and (apu & 2) ~= 0 then musicDuringShot = true end
end)
watch(0x4007, function()
    if playing and frame > retry and frame < finalStart then retryShot = true end
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
        b = t == 94 or (t >= 200 and t <= 500 and t % 13 == 0) or r == 25 or onClear,
        left = onClear,
        up = t == 26 or r == 26,
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

local function sameTerrain(a, b, dx, dy)
    -- The strip between the stress markers and the actors crosses both nametables.
    for y = 40, math.min(236, 239 - dy) do
        for x = 132, 140 do
            if pixel(a, x, y) ~= pixel(b, x + dx, y + dy) then return false end
        end
    end
    return true
end

local function checkPlayfield(picture)
    if splitCount ~= 1 or splitStep or dmaCount ~= 1 then
        fail(141, "expected one completed raster split and DMA")
        return
    end
    if fmKeys ~= 0x3f or (apu & 1) == 0 or not lastNote or frame - lastNote > 30 then
        fail(142, "FM or APU music stopped during the course")
        return
    end
    if not samePixels(startImage, picture, 32 * 256) then
        fail(143, "fixed HUD pixels changed")
        return
    end
    local elapsed = frame - (frame < secondStart and firstStart
        or frame < retry and secondStart or retry)
    if elapsed >= 28 then
        for offset = 32, 60, 4 do
            if sprite(offset) ~= sprite(0) then
                fail(152, "stress markers left the player's scanline")
                return
            end
        end
    end
    worstBudget = math.max(worstBudget, budget)
    if frame > retry and frame < finalStart then retryBudget = math.max(retryBudget, budget) end
    if previousTerrain then
        local dx = (cameraX - previousX + 256) % 512 - 256
        local dy = cameraY - previousY
        if math.abs(dx) > 1 or math.abs(dy) > 1
            or not sameTerrain(picture, previousTerrain, dx, dy) then
            fail(144, "terrain pixels lost alignment with the camera at frame " .. frame)
            return
        end
        if uploads == 67 then
            if dx == 0 or dy == 0 then fail(145, "both uploads require diagonal movement") end
            worstCombined = math.max(worstCombined, budget)
        end
    end
    previousTerrain, previousX, previousY = picture, cameraX, cameraY
end

local function onScanline(offset, line)
    local top = sprite(offset)
    return line > top and line <= top + 16
end

local function watchStandingBolt(picture)
    if not boltX then
        local y = sprite(28)
        if y == 0xf8 or not previousTerrain then return end
        boltX, boltY = sprite(31), y + 3
        boltFrame = frame
        boltBackground = pixel(previousTerrain, boltX + 3, boltY)
        if cameraX ~= 0 or cameraY ~= 0 then
            fail(151, "standing bolt was not at the expected camera position")
            return
        end
        preHitTerrain = previousTerrain
        for _, offset in ipairs({0, 4, 20, 24}) do
            if not onScanline(offset, boltY) then
                fail(146, "the standing bolt did not share a scanline with the player and kiln")
                return
            end
        end
        for offset = 32, 60, 4 do
            if not onScanline(offset, boltY) then
                fail(146, "the standing bolt did not share a scanline with all eight markers")
                return
            end
        end
    end
    -- The framebuffer can lag the OAM shadow by one frame.
    if not boltRow and frame <= boltFrame + 1 and pixel(picture, boltX + 3, boltY) ~= boltBackground then
        boltRow = {}
        for x = 8, 127 do boltRow[x] = pixel(picture, x, boltY) end
    end
end

emu.addEventCallback(function()
    frame = frame + 1
    local picture
    if startImage and frame < finalStart and playing and not displayChanged then
        picture = emu.getScreenBuffer()
        if frame > secondStart + 584 and frame < retry then
            watchStandingBolt(picture)
        end
        checkPlayfield(picture)
    else
        previousTerrain = nil
        if displayChanged then lastNote = frame end
    end
    uploads, dmaCount, splitCount, budget = 0, 0, 0, 0
    displayChanged = false

    local hit = sprite(2) == 2
    local t = frame - firstStart
    if t == 16 then
        startImage = emu.getScreenBuffer()
    elseif t >= 584 and t <= 625 then
        picture = picture or emu.getScreenBuffer()
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
        picture = picture or emu.getScreenBuffer()
        if not tookHit or cameraX ~= 0 or cameraY ~= 16 or sprite(3) ~= 184 or sprite(0) ~= 143
            or hit or sprite(28) ~= 0xf8 then
            fail(135, "the retry route did not restore the checkpoint after the hit")
            return
        end
        if not preHitTerrain or not sameTerrain(picture, preHitTerrain, 0, 16) then
            fail(147, "the checkpoint redraw changed the terrain")
            return
        end
        if not boltRow then fail(148, "standing bolt lost its visible pixels"); return end
        -- The checkpoint redraw has the same world X and an extra 16 pixels of Y scroll.
        -- Its markers are still off, so the overlapping row gives us the bare terrain.
        local markers = 0
        for x = 8, 120, 16 do
            for markX = x, x + 7 do
                if boltRow[markX] ~= pixel(picture, markX, boltY - 16) then
                    markers = markers + 1
                    break
                end
            end
        end
        if markers ~= 3 then
            fail(149, "expected three visible markers alongside the player, kiln and spark")
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
        if worstCombined == 0 or retryBudget == 0 or not musicDuringShot or not retryShot then
            fail(150, "missing diagonal transfer, checkpoint DMA or concurrent music and shots")
            return
        end
        emu.log(string.format("VBlank CPU cycles, maximum %d, diagonal %d, after retry %d",
            worstBudget, worstCombined, retryBudget))
        emu.stop(0)
    end
end, emu.eventType.endFrame)
