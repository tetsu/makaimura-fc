-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
local frame = 0
local firstStart, alternateStart = 144, 240
local approachEnd = 603
local retry = alternateStart + 669
local replay, returnToMenu, finalStart = retry + 118, retry + 150, retry + 176
local originalPicture, alternatePicture
local sawBolt, tookHit = false, false
local cameraX, cameraY, splitStep
local playing, displayChanged
local uploads, dmaCount, splitCount, budget = 0, 0, 0, 0
local worstCombined, combinedShotBudget, retryBudget = 0, 0, 0
local fmRegister, fmKeys, apu = nil, 0, 0
local lastNote, musicDuringShot, retryShot, soundAtDma
local previousX, previousY
local preBoltPicture, preBoltCameraY, boltPicture, boltX, boltY, boltFrame

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

watch(0x2001, function(_, value)
    playing = value == 0x18
    displayChanged = true
end)
watch(0x2007, function()
    if not playing or displayChanged then return end
    local line = emu.getState()["ppu.scanline"]
    if line < 241 or line > 260 then fail(176, "terrain upload outside VBlank") end
    uploads = uploads + 1
end)
watch(0x4014, function(_, value)
    if not playing or displayChanged then return end
    local state = emu.getState()
    local dots = (state["ppu.scanline"] - 241) * 341 + state["ppu.cycle"]
    budget = math.ceil(dots / 3) + 514
    if value ~= 2 or dots < 0 or budget >= 2273 then
        fail(177, "OAM DMA would cross VBlank or used the wrong buffer")
    end
    soundAtDma = fmKeys == 0x3f and (apu & 3) == 3
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
            fail(178, "HUD split missed scanline 31")
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
    if playing and frame >= alternateStart and frame < retry and (apu & 2) ~= 0 then
        musicDuringShot = true
    end
end)
watch(0x4007, function()
    if playing and frame > retry and frame < replay then retryShot = true end
end)

local function samePicture(a, b, count)
    for i = 1, count or #a do
        if a[i] ~= b[i] then return false end
    end
    return #a == #b
end

local function cleanStart(bankTag, x, feet)
    -- The OAM shadow may already include the next frame's camera adjustment.
    return read(0x8000) == bankTag and cameraX == 0
        and read(0x203) == x - 8 and math.abs(read(0x200) + cameraY - (feet - 17)) <= 1
        and read(0x202) == 0 and read(0x210) == 0xf8 and read(0x21c) == 0xf8
end

local function checkPlayfield(picture)
    if splitCount ~= 1 or splitStep or dmaCount ~= 1 then
        fail(179, "expected one completed raster split and DMA")
        return
    end
    if fmKeys ~= 0x3f or (apu & 1) == 0 or not lastNote or frame - lastNote > 30 then
        fail(180, "FM or APU music stopped during the alternate course")
        return
    end
    if not samePicture(alternatePicture, picture, 32 * 256) then
        fail(181, "fixed HUD pixels changed in the alternate course")
        return
    end
    if uploads == 67 then
        if not previousX or cameraX == previousX or cameraY == previousY then
            fail(184, "row and column uploads occurred without diagonal movement")
            return
        end
        worstCombined = math.max(worstCombined, budget)
        if soundAtDma then combinedShotBudget = math.max(combinedShotBudget, budget) end
    end
    previousX, previousY = cameraX, cameraY
    if frame > retry then retryBudget = math.max(retryBudget, budget) end
end

local function watchStandingBolt(picture)
    if not boltFrame then
        local top = read(0x21c)
        if top == 0xf8 then
            preBoltPicture, preBoltCameraY = picture, cameraY
            return
        end
        boltX, boltY, boltFrame = read(0x21f) + 3, top + 3, frame
        if not preBoltPicture or cameraX ~= 0 or cameraY ~= preBoltCameraY then
            fail(182, "first bolt was not at the expected stationary camera position")
            return
        end
        for _, offset in ipairs({0, 4, 20, 24, 32, 36, 40, 44, 48, 52, 56, 60}) do
            local y = read(0x200 + offset)
            if boltY <= y or boltY > y + 16 then
                fail(182, "the bolt did not share a scanline with the player, kiln and eight markers")
                return
            end
        end
    end
    -- The framebuffer can lag the OAM shadow by one frame.
    if not boltPicture and frame <= boltFrame + 1
        and pixel(picture, boltX, boltY) ~= pixel(preBoltPicture, boltX, boltY) then
        boltPicture = picture
    end
end

emu.addEventCallback(function()
    local t, r = frame - alternateStart, frame - retry
    -- The shot at t=141 is still active when the first diagonal upload reaches DMA.
    emu.setInput({
        b = frame == firstStart or frame == firstStart + 30 or t == 0 or t == 115 or t == 141
            or (t >= 200 and t <= 500 and t % 13 == 0) or r == 25 or frame == finalStart,
        down = t == 0,
        right = (t >= 16 and t <= approachEnd) or (r >= 16 and r < 96),
        a = t == 40 or (t >= 180 and t <= approachEnd and (t + 144) % 19 == 0) or r == 16,
        up = t == 26 or r == 26,
        start = r == 0 or frame == replay,
        select = frame == firstStart + 40 or frame == returnToMenu,
    }, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    if alternatePicture and frame <= retry + 100 and playing and not displayChanged then
        local picture = emu.getScreenBuffer()
        checkPlayfield(picture)
        if frame > alternateStart + approachEnd and frame <= retry then watchStandingBolt(picture) end
    elseif displayChanged then
        lastNote = frame
        previousX, previousY = nil, nil
    end
    uploads, dmaCount, splitCount, budget = 0, 0, 0, 0
    displayChanged = false

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
    elseif frame > alternateStart + approachEnd and frame <= retry then
        sawBolt = sawBolt or read(0x21c) ~= 0xf8
        tookHit = tookHit or read(0x202) == 2
    elseif frame == retry + 16 then
        if not sawBolt or not tookHit or read(0x8000) ~= 0x81 or cameraX ~= 0 or cameraY ~= 32
            or read(0x203) ~= 176 or read(0x200) ~= 143 or read(0x202) ~= 0
            or read(0x210) ~= 0xf8 or read(0x21c) ~= 0xf8 or read(0x214) ~= 111
            or read(0x216) ~= 3 or read(0x217) ~= 222 then
            fail(171, "alternate checkpoint did not restore camera, player and ranged encounter")
            return
        end
        if not boltPicture then fail(183, "standing bolt lost its visible pixels"); return end
        -- The checkpoint redraw preserves world X but shifts Y and switches the markers off.
        local picture = emu.getScreenBuffer()
        local y = boltY + preBoltCameraY - cameraY
        local markers = 0
        for x = 8, 120, 16 do
            for markX = x, x + 7 do
                if pixel(boltPicture, markX, boltY) ~= pixel(picture, markX, y) then
                    markers = markers + 1
                    break
                end
            end
        end
        if markers ~= 3 then fail(183, "expected three visible markers alongside the bolt") end
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
        if combinedShotBudget == 0 or retryBudget == 0 or not musicDuringShot or not retryShot then
            fail(185, "missing concurrent rendering and sound, or post-checkpoint DMA and shot")
            return
        end
        emu.log(string.format("Alternate VBlank CPU cycles, diagonal %d, with shot %d, after retry %d",
            worstCombined, combinedShotBudget, retryBudget))
        emu.stop(0)
    end
end, emu.eventType.endFrame)
