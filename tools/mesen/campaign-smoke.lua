-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
local frame = 0
local firstStart, secondStart = 144, 880
local checkpointRetry = secondStart + 669
local finish = checkpointRetry + 120
local newGame, quit, diagnostic = finish + 32, finish + 64, finish + 96
local menu, supportsCampaign, tookHit
local displayMode, courseEntries = nil, 0

local function read(address)
    return emu.read(address, emu.memType.nesMemory)
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

local function check(condition, message)
    if condition then return true end
    emu.log(message)
    emu.stop(1)
    return false
end

local function cleanStart(bankTag, spriteX)
    return displayMode == 0x18 and read(0x8000) == bankTag and read(0x203) == spriteX
        and read(0x202) == 0 and read(0x210) == 0xf8 and read(0x21c) == 0xf8
end

emu.addMemoryCallback(function(_, value)
    displayMode = value
    if value == 0x18 then courseEntries = courseEntries + 1 end
end, emu.callbackType.write, 0x2001, 0x2001, emu.memType.nesMemory)

emu.addEventCallback(function()
    local t, r = frame - secondStart, frame - checkpointRetry
    local first = frame - firstStart
    emu.setInput({
        start = first == 0 or first == 60 or t == 0 or r == 0 or frame == finish or frame == newGame,
        select = frame == quit,
        up = first == 0 or frame == newGame,
        down = t == 0 or frame == diagnostic,
        right = (first >= 16 and first <= 584) or (first >= 594 and first <= 700)
            or (t >= 16 and t <= 603) or (r >= 16 and r < 96),
        a = (first >= 170 and first <= 584 and (first + 144) % 19 == 0) or first == 593
            or t == 40 or (t >= 180 and t <= 603 and (t + 144) % 19 == 0) or r == 16,
        b = first == 94 or (first >= 200 and first <= 500 and first % 13 == 0)
            or t == 115 or t == 141 or (t >= 200 and t <= 500 and t % 13 == 0)
            or r == 25 or frame == diagnostic,
    }, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    if frame == firstStart - 16 then
        menu = emu.getScreenBuffer()
        supportsCampaign = read(0xe000) == 0xbf
        -- First row of U in UP+START CAMPAIGN.
        check((pixel(menu, 67, 153) ~= menu[1]) == supportsCampaign, "incorrect campaign menu for ROM size")
    elseif frame == firstStart + 16 then
        if not supportsCampaign then
            -- The chord still takes the ordinary Start path on 32 KiB.
            if check(displayMode == 0x1e and read(0x8000) == 0x80, "32 KiB horizontal diagnostic failed") then
                emu.stop(0)
            end
        else
            check(cleanStart(0x80, 168), "campaign did not start on stage 1")
        end
    elseif frame == firstStart + 70 then
        check(courseEntries == 1 and read(0x203) > 168 and read(0x8000) == 0x80,
            "Start restarted stage 1 during live play")
    elseif frame == secondStart - 8 then
        local picture = emu.getScreenBuffer()
        check(pixel(picture, 83, 98) ~= picture[1] and pixel(picture, 139, 114) ~= picture[1],
            "stage 1 did not reach COURSE CLEAR / START STG2")
    elseif frame == secondStart + 16 then
        check(cleanStart(0x81, 152), "stage 2 did not start from its own entrance")
    elseif frame > secondStart + 603 and frame <= checkpointRetry then
        tookHit = tookHit or read(0x202) == 2
    elseif frame == checkpointRetry + 16 then
        check(tookHit and read(0x8000) == 0x81 and read(0x203) == 176 and read(0x200) == 143
            and read(0x202) == 0 and read(0x21c) == 0xf8, "stage 2 checkpoint retry failed")
    elseif frame == finish - 8 then
        local picture = emu.getScreenBuffer()
        check(pixel(picture, 83, 97) ~= picture[1] and pixel(picture, 139, 113) ~= picture[1],
            "stage 2 did not reach FINAL CLEAR / START MENU")
    elseif frame == finish + 16 or frame == quit + 16 then
        check(read(0x8000) == 0x80 and samePicture(menu, emu.getScreenBuffer()), "title did not reset")
    elseif frame == newGame + 16 then
        check(cleanStart(0x80, 168), "new game inherited campaign progress")
    elseif frame == diagnostic + 16 then
        if check(cleanStart(0x81, 152), "alternate diagnostic could not start after leaving the campaign") then
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
