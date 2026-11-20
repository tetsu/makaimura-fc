-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
local trials = {
    {hit = true},
    {button = "a", input = 82, hit = false},
    {button = "a", input = 83, hit = true},
    {button = "b", input = 81, hit = false, defeated = true, overlap = true},
    {button = "b", input = 85, hit = false, defeated = true},
    {button = "b", input = 86, hit = true},
}
local frame = 0
local warmup, caseLength, walkStart = 120, 150, 18
local observedCase, visibleFrame, sawOverlap
local function fail(code, message)
    emu.log(message)
    emu.stop(code)
end

-- Sprite shadow in CPU RAM. Its first five entries hold player, enemy and shot.
local function spriteByte(offset)
    return emu.read(0x200 + offset, emu.memType.nesMemory)
end

local function enemyPixelsVisible()
    if spriteByte(8) ~= 159 then return false end
    local buffer = emu.getScreenBuffer()
    -- At entry, this strip contains sky until the enemy is drawn.
    for y = 162, 166 do
        for x = 240, 255 do
            if buffer[y * 256 + x + 1] ~= buffer[1] then return true end
        end
    end
    return false
end

emu.addEventCallback(function()
    local elapsed = frame - warmup
    if elapsed < 0 then return end
    local spec = trials[elapsed // caseLength + 1]
    if not spec then return end
    local t = elapsed % caseLength
    local input = {start = t == 2, right = t >= walkStart and t < 138, a = false, b = false}
    if spec.button and t == walkStart + spec.input then input[spec.button] = true end
    emu.setInput(input, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    local elapsed = frame - warmup - 1
    if elapsed < 0 then return end
    local trial = elapsed // caseLength + 1
    local spec = trials[trial]
    if not spec then emu.stop(0); return end
    if trial ~= observedCase then
        observedCase, visibleFrame, sawOverlap = trial, nil, false
    end
    local t = elapsed % caseLength
    if t >= walkStart and t <= 138 then
        local playerY, enemyY, shotY = spriteByte(0), spriteByte(8), spriteByte(16)
        if not visibleFrame and enemyPixelsVisible() then visibleFrame = t - walkStart end
        if playerY == enemyY and enemyY == 159 and math.abs(shotY - enemyY) < 16 then
            sawOverlap = true
        end
    end
    if t == 140 then
        if visibleFrame ~= 25 then fail(25, "enemy appearance changed"); return end
        local hit = spriteByte(2) == 2
        local defeated = spriteByte(8) == 0xf8
        if hit ~= spec.hit or defeated ~= (spec.defeated == true) then
            fail(26 + trial, "unexpected encounter result")
            return
        end
        if not hit and spriteByte(0) ~= 175 then
            fail(33, "player did not land on the lower step")
            return
        end
        if spec.overlap and not sawOverlap then fail(34, "missing player/enemy/shot overlap"); return end
    end
end, emu.eventType.endFrame)
