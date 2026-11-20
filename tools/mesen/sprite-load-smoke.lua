-- Mesen 2.1.1 with the standard NES controller and PPU sprite limit enabled.
local frame = 0
local startFrame, trialFrames = 120, 130
local baseline = {}
local idleRow
local shotFrames = 0
local previousMask
local rotated = false
local seenBeforeShot = 0

local function spriteByte(offset)
    return emu.read(0x200 + offset, emu.memType.nesMemory)
end

local function fail(code, message)
    emu.log(message)
    emu.stop(code)
end

emu.addEventCallback(function()
    local elapsed = frame - startFrame
    if elapsed < 0 or elapsed >= trialFrames * 2 then return end
    local t = elapsed % trialFrames
    emu.setInput({
        start = t == 2,
        right = t >= 18 and t < 78,
        up = elapsed >= trialFrames and (t == 82 or t == 109),
        b = t == 96,
    }, 0)
end, emu.eventType.inputPolled)

local function readRow(buffer, y)
    local row = {}
    for x = 0, 255 do row[x + 1] = buffer[y * 256 + x + 1] end
    return row
end

local function compareRow(row, previous)
    local mask, count = 0, 0
    for i = 0, 7 do
        local changed = false
        for x = 9 + i * 16, 16 + i * 16 do
            if row[x] ~= previous[x] then changed = true end
        end
        if changed then
            count = count + 1
            mask = mask | (1 << i)
        end
    end
    return mask, count
end

emu.addEventCallback(function()
    frame = frame + 1
    local elapsed = frame - startFrame - 1
    if elapsed < 0 then return end
    local stressed = elapsed >= trialFrames
    local t = elapsed % trialFrames
    if t == 20 then shotFrames = 0 end
    if not stressed and t == 90 then idleRow = readRow(emu.getScreenBuffer(), 167) end
    if stressed and t >= 85 and t <= 94 then
        local row = readRow(emu.getScreenBuffer(), 167)
        local mask, count = compareRow(row, idleRow)
        if count ~= 4 then fail(51, "expected four test pieces before firing"); return end
        seenBeforeShot = seenBeforeShot | mask
    end
    if t >= 95 and t <= 108 and spriteByte(16) < 240 then
        if spriteByte(0) ~= 159 or spriteByte(4) ~= 159
            or spriteByte(8) ~= 159 or spriteByte(12) ~= 159 then
            fail(41, "gameplay sprites were not aligned")
            return
        end
        shotFrames = shotFrames + 1
        -- Mesen's completed framebuffer can lag the sprite shadow by one frame.
        if shotFrames == 1 then return end
        local y = spriteByte(16) + 2
        local screen = emu.getScreenBuffer()
        local row = readRow(screen, y)
        if not stressed then
            local shotDrawn = false
            for x = 129, 256 do
                if row[x] ~= idleRow[x] then shotDrawn = true; break end
            end
            if not shotDrawn then fail(50, "baseline shot was not rendered"); return end
            baseline[shotFrames] = row
        else
            local previous = baseline[shotFrames]
            if not previous then fail(42, "unexpected shot duration"); return end
            for i = 20, 48, 4 do
                if spriteByte(i) ~= 159 then fail(43, "missing test sprite on scanline"); return end
            end
            -- The test pieces occupy only X=8..127; compare every gameplay pixel.
            for x = 129, 256 do
                if row[x] ~= previous[x] then fail(44, "gameplay pixels differed under load"); return end
            end
            local mask, count = compareRow(row, previous)
            if count ~= 3 then fail(45, "expected only three of eight test pieces to draw"); return end
            if previousMask and mask ~= previousMask then rotated = true end
            previousMask = mask
        end
    end
    if t == 118 then
        if shotFrames < 4 then fail(46, "did not observe enough aligned frames"); return end
        if spriteByte(8) ~= 0xf8 then fail(47, "shot did not defeat the enemy"); return end
        if stressed then
            if not rotated then fail(48, "test sprites did not rotate"); return end
            if seenBeforeShot ~= 0xff then fail(52, "a test sprite never appeared"); return end
            for i = 20, 48, 4 do
                if spriteByte(i) ~= 0xf8 then fail(49, "toggle left a test sprite visible"); return end
            end
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
