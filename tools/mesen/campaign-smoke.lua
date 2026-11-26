-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
local frame = 0
local openDiagnosticMenu = 64
local firstStart, secondStart = 144, 880
local checkpointRetry = secondStart + 669
local finish = checkpointRetry + 120
local newGame, quit, diagnostic = finish + 32, finish + 64, finish + 96
local leaveDiagnostic = diagnostic + 48
local lossRun = leaveDiagnostic + 32
local lossStage = lossRun + secondStart - firstStart
local firstRetry, secondRetry, returnAfterLoss, restartAfterLoss
local expiredAt, hitAt, hitClock
local menu, supportsCampaign, tookHit
local displayMode, courseEntries = nil, 0
local writes, combinedHud = 0, false
local switchedDisplay
local digitZero = 0x1b

local function read(address)
    return emu.read(address, emu.memType.nesMemory)
end

local function hud(address)
    return emu.read(address, emu.memType.nesPpuMemory)
end

local function clock()
    return (hud(0x2035) - digitZero) * 100
        + (hud(0x2036) - digitZero) * 10
        + hud(0x2037) - digitZero
end

local function lives()
    return hud(0x202e) - digitZero
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
    print(string.format("FAIL frame=%d: %s", frame, message))
    emu.stop(1)
    return false
end

local function cleanStart(bankTag, spriteX)
    return displayMode == 0x18 and read(0x8000) == bankTag and read(0x203) == spriteX
        and read(0x202) == 0 and read(0x210) == 0xf8 and read(0x21c) == 0xf8
end

emu.addMemoryCallback(function(_, value)
    displayMode = value
    switchedDisplay = true
    if value == 0x18 then courseEntries = courseEntries + 1 end
end, emu.callbackType.write, 0x2001, 0x2001, emu.memType.nesMemory)

emu.addMemoryCallback(function()
    if displayMode ~= 0x18 or switchedDisplay or frame >= diagnostic then return end
    local state = emu.getState()
    local line = state["ppu.scanline"]
    if not check(line >= 241 and line <= 260, "campaign upload outside VBlank") then return end
    writes = writes + 1
end, emu.callbackType.write, 0x2007, 0x2007, emu.memType.nesMemory)

emu.addMemoryCallback(function(_, value)
    if displayMode ~= 0x18 or switchedDisplay or frame >= diagnostic then return end
    local state = emu.getState()
    local dots = (state["ppu.scanline"] - 241) * 341 + state["ppu.cycle"]
    check(value == 2 and dots >= 0 and math.ceil(dots / 3) + 514 < 2273, "campaign DMA exceeds VBlank")
    combinedHud = combinedHud or writes == 73 -- 6 HUD + 40 row + 27 column.
end, emu.callbackType.write, 0x4014, 0x4014, emu.memType.nesMemory)

emu.addEventCallback(function()
    local t, r = frame - secondStart, frame - checkpointRetry
    local first = frame - (frame >= lossRun and lossRun or firstStart)
    -- Start moving one frame earlier so a clock tick coincides with both terrain transfers.
    local route = first + 1
    local function approach(retry)
        return retry and frame >= retry + 16 and frame < retry + 150
    end
    emu.setInput({
        start = first == 0 or first == 60 or t == 0 or r == 0 or frame == finish or frame == newGame
            or frame == lossStage or frame == firstRetry or frame == secondRetry or frame == returnAfterLoss
            or frame == restartAfterLoss,
        select = (supportsCampaign and frame == openDiagnosticMenu) or frame == quit or frame == leaveDiagnostic,
        up = first == 0 or frame == newGame or frame == restartAfterLoss,
        down = t == 0 or frame == diagnostic,
        right = (route >= 16 and route <= 584) or (route >= 594 and route <= 700)
            or (t >= 16 and t <= 603) or (r >= 16 and r < 96) or approach(firstRetry) or approach(secondRetry),
        a = (route >= 170 and route <= 584 and (route + 144) % 19 == 0) or route == 593
            or t == 40 or (t >= 180 and t <= 603 and (t + 144) % 19 == 0) or r == 16
            or (firstRetry and frame == firstRetry + 40) or (secondRetry and frame == secondRetry + 40),
        b = route == 94 or (route >= 200 and route <= 500 and route % 13 == 0)
            or t == 115 or t == 141 or (t >= 200 and t <= 500 and t % 13 == 0)
            or r == 25 or frame == diagnostic,
    }, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    writes, switchedDisplay = 0, false
    if frame == openDiagnosticMenu - 1 then
        supportsCampaign = read(0xe000) == 0xbf
    elseif frame == firstStart - 16 then
        menu = emu.getScreenBuffer()
        -- First row of U in UP+START CAMPAIGN.
        check((pixel(menu, 67, 153) ~= menu[1]) == supportsCampaign, "incorrect campaign menu for ROM size")
    elseif frame == firstStart + 12 then
        if not supportsCampaign then
            -- The chord still takes the ordinary Start path on 32 KiB.
            if check(displayMode == 0x1e and read(0x8000) == 0x80, "32 KiB horizontal diagnostic failed") then
                emu.stop(0)
            end
        else
            check(cleanStart(0x80, 168) and hud(0x2027) == 0x1c and lives() == 3
                and clock() == 120 and hud(0x203c) == 0x0e, "campaign did not start with a clean HUD")
        end
    elseif frame == firstStart + 72 then
        check(courseEntries == 1 and read(0x203) > 168 and read(0x8000) == 0x80 and clock() == 119,
            "Start restarted stage 1 during live play")
    elseif frame == secondStart - 8 then
        local picture = emu.getScreenBuffer()
        check(pixel(picture, 83, 98) ~= picture[1] and pixel(picture, 139, 114) ~= picture[1],
            "stage 1 did not reach COURSE CLEAR / START STG2")
    elseif frame == secondStart + 16 then
        check(cleanStart(0x81, 152) and hud(0x2027) == 0x1d and lives() == 3 and clock() == 120
            and hud(0x203c) == 0x0e, "stage 2 did not reset the clock and checkpoint")
    elseif frame > secondStart + 603 and frame <= checkpointRetry then
        tookHit = tookHit or read(0x202) == 2
    elseif frame == checkpointRetry + 16 then
        check(tookHit and hud(0x203c) == 0x17 and clock() == 120 and lives() == 2
            and read(0x8000) == 0x81 and read(0x203) == 176 and read(0x200) == 143
            and read(0x202) == 0 and read(0x21c) == 0xf8, "stage 2 checkpoint retry failed")
    elseif frame == finish - 8 then
        local picture = emu.getScreenBuffer()
        check(pixel(picture, 83, 97) ~= picture[1] and pixel(picture, 139, 113) ~= picture[1],
            "stage 2 did not reach FINAL CLEAR / START MENU")
    elseif frame == finish + 16 or frame == quit + 16 then
        check(read(0x8000) == 0x80 and samePicture(menu, emu.getScreenBuffer()), "title did not reset")
    elseif frame == newGame + 16 then
        check(cleanStart(0x80, 168) and lives() == 3 and clock() == 120 and hud(0x203c) == 0x0e,
            "new game inherited campaign progress")
    elseif frame == diagnostic + 16 then
        check(cleanStart(0x81, 152), "alternate diagnostic could not start after leaving the campaign")
        check(combinedHud, "campaign did not test a combined HUD, row and column upload")
    elseif frame == lossStage + 16 then
        check(cleanStart(0x81, 152) and lives() == 3 and clock() == 120, "timeout run did not reach stage 2")
    elseif frame > lossStage + 16 and not expiredAt then
        if read(0x202) == 2 then
            expiredAt = frame
            check(frame >= lossStage + 7200 and frame <= lossStage + 7216, "stage 2 time limit expired early")
            firstRetry = frame + 80
        elseif frame > lossStage + 7216 then
            check(false, "stage 2 never timed out")
        end
    elseif expiredAt and frame == expiredAt + 8 then
        check(clock() == 0 and lives() == 2, "stage 2 time limit did not take exactly one life")
    elseif firstRetry and frame == firstRetry - 8 then
        check(clock() == 0 and lives() == 2, "time or life changed while waiting for retry")
    elseif firstRetry and frame == firstRetry + 16 then
        check(cleanStart(0x81, 152) and lives() == 2 and clock() == 120 and hud(0x203c) == 0x0e,
            "timeout retry did not restart stage 2 at the entrance")
    elseif firstRetry and frame > firstRetry + 16 and not hitAt then
        if read(0x202) == 2 then
            hitAt = frame
            secondRetry = frame + 80
        elseif frame > firstRetry + 150 then
            check(false, "stage 2 player never hit the enemy")
        end
    elseif hitAt and frame == hitAt + 8 then
        hitClock = clock()
        check(lives() == 1 and hitClock > 0, "enemy contact did not take exactly one life")
    elseif secondRetry and frame == secondRetry - 8 then
        check(lives() == 1 and clock() == hitClock, "time or life changed after a hit before START")
    elseif secondRetry and frame == secondRetry + 16 then
        check(cleanStart(0x81, 152) and lives() == 1 and clock() == 120, "last life did not restart stage 2")
    elseif secondRetry and frame == secondRetry + 150 then
        local picture = emu.getScreenBuffer()
        -- First row of G in GAME OVER and M in START MENU.
        check(displayMode == 0x0a and pixel(picture, 84, 97) ~= picture[1]
            and pixel(picture, 139, 113) ~= picture[1], "final loss did not show GAME OVER / START MENU")
        returnAfterLoss = frame + 32
        restartAfterLoss = returnAfterLoss + 32
    elseif returnAfterLoss and frame == returnAfterLoss + 16 then
        check(read(0x8000) == 0x80 and samePicture(menu, emu.getScreenBuffer()), "game over did not return to title")
    elseif restartAfterLoss and frame == restartAfterLoss + 16 then
        if check(cleanStart(0x80, 168) and lives() == 3 and clock() == 120 and hud(0x203c) == 0x0e,
                "fresh game after stage 2 game over inherited progress") then
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
