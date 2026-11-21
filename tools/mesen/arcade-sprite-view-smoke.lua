-- Drive the sprite viewer through the test menu and inspect its rendered poses.
local frame = 0
local memory = emu.memType.nesMemory
local inputs = {
    [130] = {select = true},
    [160] = {right = true, a = true},
    [175] = {right = true},
    [180] = {start = true},
    [200] = {a = true},
    [220] = {b = true},
    [240] = {start = true},
    [267] = {b = true},
    [275] = {right = true},
    [285] = {right = true},
    [295] = {right = true},
    [305] = {right = true},
    [315] = {right = true},
    [325] = {right = true},
    [335] = {right = true},
    [345] = {right = true},
    [355] = {left = true},
    [365] = {select = true},
    [380] = {right = true, a = true},
    [395] = {select = true},
    [410] = {right = true, start = true},
}

local function ppu(address)
    return emu.read(address, emu.memType.nesPpuDebug)
end

local publishedPlayer = {}
local function oam(slot, field)
    if slot < 4 then return publishedPlayer[4 * slot + field] end
    return emu.read(0x200 + 4 * slot + field, memory)
end

local function check(condition, message)
    if not condition then
        print(string.format("frame %d: %s", frame, message))
        emu.stop(1)
    end
end

local mask, dmaCount = 0, 0
local function checkVblank()
    if mask == 0 or frame < 170 or frame >= 365 then return end
    local line = emu.getState()["ppu.scanline"]
    check(line >= 241 and line <= 260, "viewer committed rendering data outside VBlank")
end

emu.addMemoryCallback(function(_, value) mask = value end,
    emu.callbackType.write, 0x2001, 0x2001, memory)
emu.addMemoryCallback(checkVblank, emu.callbackType.write, 0x2007, 0x2007, memory)
emu.addMemoryCallback(function()
    checkVblank()
    dmaCount = dmaCount + 1
    -- Capture the buffer published with the pose label, before the next frame
    -- stages different tiles in CPU RAM.
    if frame >= 160 and frame < 395 then
        for i = 0, 15 do publishedPlayer[i] = emu.read(0x200 + i, memory) end
    end
end, emu.callbackType.write, 0x4014, 0x4014, memory)
emu.addEventCallback(function() emu.setInput(inputs[frame] or {}, 0) end, emu.eventType.inputPolled)

local full, none = 0x07, 0x0e
local function checkPose(armor, poseLetter, bottom, top, flipped)
    check(ppu(0x2108) == armor and ppu(0x210e) == poseLetter, "incorrect pose label")
    local offset = flipped and 2 or 0
    check(oam(0, 1) == bottom + offset and oam(1, 1) == bottom + 2 - offset,
        "incorrect bottom sprite tiles")
    check(oam(0, 3) == 120 and oam(1, 3) == 128, "pose moved away from the preview center")
    local attributes = (flipped and 0x40 or 0) | (armor == full and 0 or 1)
    check(oam(0, 2) == attributes and oam(1, 2) == attributes, "incorrect sprite flip or palette")
    if top then
        check(oam(2, 1) == top + offset and oam(3, 1) == top + 2 - offset
            and oam(2, 0) < 240 and oam(3, 0) < 240, "incorrect top sprite tiles")
    else
        check(oam(2, 0) >= 240 and oam(3, 0) >= 240, "crouch retained the previous pose's top")
    end
end

local pausedFrame, capturedDMA
local playingFrames = {}
local function checkWalk(armor, flipped)
    local shown = ppu(0x2116) - 0x1b
    check(shown == 1 or shown == 2, "invalid displayed walk frame")
    if armor == full then
        checkPose(full, 0x16, shown == 1 and 0x19 or 0x21, shown == 1 and 0x15 or 0x1d, flipped)
    else
        checkPose(none, 0x16, shown == 1 and 0x3d or 0x45, shown == 1 and 0x39 or 0x41, flipped)
    end
    return shown
end

emu.addEventCallback(function()
    frame = frame + 1
    if frame == 150 then
        check(ppu(0x22c8) == 0x11, "sprite viewer shortcut missing from the menu")
    elseif frame == 170 or frame == 390 then
        check(mask == 0x1e and ppu(0x2088) == 0x12, "sprite viewer did not open")
        checkPose(full, 0x12, 0x01, 0x11, false)
        for slot = 4, 63 do check(oam(slot, 0) >= 240, "unrelated sprite visible") end
        local image = emu.getScreenBuffer()
        local visible = false
        for y = 88, 119 do
            for x = 120, 135 do
                visible = visible or image[y * 256 + x + 1] ~= image[y * 256 + 110 + 1]
            end
        end
        check(visible, "player sprite did not render")
        capturedDMA = dmaCount
    elseif frame == 185 then
        pausedFrame = checkWalk(full, false)
    elseif frame == 195 then
        check(checkWalk(full, false) == pausedFrame, "START did not pause the walk cycle")
        check(dmaCount > capturedDMA, "viewer did not publish OAM")
    elseif frame == 205 or frame == 215 then
        check(checkWalk(full, false) == 3 - pausedFrame, "A did not advance exactly one frame")
    elseif frame == 225 then
        check(checkWalk(full, true) == 3 - pausedFrame, "flipping changed the paused frame")
    elseif frame >= 245 and frame < 265 then
        playingFrames[checkWalk(full, true)] = true
    elseif frame == 270 then
        check(playingFrames[1] and playingFrames[2], "START did not resume both walk frames")
    elseif frame == 280 then
        checkPose(full, 0x19, 0x29, 0x25, false)
    elseif frame == 290 then
        checkPose(full, 0x04, 0x2d, nil, false)
    elseif frame == 300 then
        checkPose(full, 0x0c, 0x35, 0x31, false)
    elseif frame == 310 then
        checkPose(none, 0x12, 0x3d, 0x39, false)
    elseif frame == 320 then
        checkWalk(none, false)
    elseif frame == 330 then
        checkPose(none, 0x19, 0x4d, 0x49, false)
    elseif frame == 340 or frame == 360 then
        checkPose(none, 0x04, 0x51, nil, false)
    elseif frame == 350 then
        checkPose(full, 0x12, 0x01, 0x11, false)
    elseif frame == 373 then
        check(mask == 0x0a and ppu(0x2108) == 0x15, "SELECT did not restore the test menu")
    elseif frame == 440 then
        check(emu.read(0x8000, memory) == 0x8c and mask == 0x18,
            "Right+START did not enter Bank 3 gameplay from the menu")
        emu.stop(0)
    end
end, emu.eventType.endFrame)
