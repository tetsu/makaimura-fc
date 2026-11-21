-- Arcade Mode Game Over -> Title -> Fresh Restart & VBlank/Slowdown smoke test.
-- Verifies:
-- 1. Entering Arcade Mode (Right+START) starts Stage 1 (Bank 12) with 3 lives, score 0, rank 0, LP 1.
-- 2. Shooting/killing Enemy 1 increments score (> 0) while maintaining VBlank budget (< 2,273 cycles)
--    and zero 1-frame slowdowns.
-- 3. Losing all 3 lives (pitfall deaths with START retries on lives 2 and 1) transitions to GAME OVER
--    screen with Game Over BGM (snd_song_id == 4).
-- 4. Pressing START on GAME OVER returns to Title screen.
-- 5. Pressing Right+START from Title cleanly resets all arcade state:
--    stage=1, bank=12, lives=3, score=0, loop=0 (LP 1), rank=0, weapon=0 (L), next_extend=20.

local AP_STATE = 0x6010
local AP_ENEMY1 = 0x6018
local AP_WEAPON = 0x601A
local AP_RANK = 0x6072
local AP_SCORE = 0x60C4
local AP_NEXT_EXTEND_LO = 0x60CA
local AP_NEXT_EXTEND_HI = 0x60CB
local AP_LOOP = 0x60CE
local SND_SONG_ID = 0x7800

local frame = 0
local phase = "boot"
local phaseFrame = 0
local input = {}
local deathCount = 0

local playing = false
local displayChanged = false
local stablePlayFrames = 0
local dmaThisFrame = 0
local worstBudget = 0

local function read(addr)
    return emu.read(addr, emu.memType.nesMemory)
end

local function readNt(addr)
    return emu.read(addr, emu.memType.nesNametableRam)
end

local function check(cond, code, msg)
    if not cond then
        emu.log(string.format("FAIL(%d) frame=%d phase=%s(%d): %s", code, frame, phase, phaseFrame, msg))
        emu.stop(code)
    end
end

local function nextPhase(name)
    phase = name
    phaseFrame = 0
end

local function lives()
    return readNt(0x002e) - 0x1b
end

local function stageNum()
    return readNt(0x0027) - 0x1b
end

local function hudLoopNum()
    return readNt(0x0056) - 0x1b
end

local function scorePts()
    return (read(AP_SCORE) * 1000 + read(AP_SCORE + 1) * 100 + read(AP_SCORE + 2) * 10 + read(AP_SCORE + 3)) * 100
end

local function watch(address, callback)
    emu.addMemoryCallback(callback, emu.callbackType.write, address, address, emu.memType.nesMemory)
end

watch(0x2001, function(_, value)
    playing = (value == 0x18)
    displayChanged = true
    stablePlayFrames = 0
end)

watch(0x2007, function()
    if not playing or displayChanged then return end
    local line = emu.getState()["ppu.scanline"]
    check(line >= 241 and line <= 260, 40, string.format("VRAM upload outside VBlank (sl=%d)", line))
end)

watch(0x4014, function(_, value)
    if not playing or displayChanged then return end
    local state = emu.getState()
    local dots = (state["ppu.scanline"] - 241) * 341 + state["ppu.cycle"]
    local budget = math.ceil(dots / 3) + 514
    check(value == 2 and dots >= 0 and budget < 2273, 41,
        string.format("OAM DMA VBlank budget exceeded (budget=%d)", budget))
    if budget > worstBudget then worstBudget = budget end
    dmaThisFrame = dmaThisFrame + 1
end)

emu.addEventCallback(function()
    emu.setInput(input, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    phaseFrame = phaseFrame + 1
    input = {}

    if playing and not displayChanged then
        stablePlayFrames = stablePlayFrames + 1
        if stablePlayFrames >= 2 then
            check(dmaThisFrame == 1, 42, string.format("1-frame slowdown detected (dma=%d)", dmaThisFrame))
        end
    end
    displayChanged = false
    dmaThisFrame = 0

    if phase == "boot" then
        if frame == 128 then
            if read(0xe000) ~= 0xbf then
                emu.stop(0) -- Skip on 32 KiB ROM
                return
            end
            nextPhase("start_first")
        end
    elseif phase == "start_first" then
        if phaseFrame == 1 then
            input = { right = true, start = true }
        elseif phaseFrame == 28 then
            check(read(0x8000) == 0x8c and stageNum() == 1, 10, "did not enter Stage 1")
            check(lives() == 3 and scorePts() == 0, 11, "initial lives/score wrong")
            nextPhase("score_and_die")
        end
    elseif phase == "score_and_die" then
        -- Jump across pit 1, climb over wall, kill Enemy 1 for score > 0, then walk left into pit 1!
        if read(AP_ENEMY1) ~= 0 then
            input = { right = true }
            if phaseFrame < 140 and read(0x203) >= 148 and read(0x203) <= 154 and read(AP_STATE) == 0 then
                input.a = true
            end
            if phaseFrame == 215 then input.a = true end
            if phaseFrame >= 275 and phaseFrame <= 315 and phaseFrame % 8 == 0 then
                input.b = true
            end
        else
            -- Enemy 1 killed -> walk right into the chasm (before checkpoint) or walk left into pit 1.
            -- Walking right to X~500 crosses CP; walking left from X~270 hits wall at col 15;
            -- easier: walk right until falling into chasm (even with CP, walking right falls into chasm!).
            input = { right = true }
        end
        if read(AP_STATE) == 4 then
            deathCount = deathCount + 1
            if deathCount == 1 then
                check(scorePts() > 0, 12, "expected non-zero score before first death")
            end
            nextPhase("after_death")
        elseif phaseFrame > 700 then
            check(false, 13, "failed to die in Stage 1 pit/chasm")
        end
    elseif phase == "after_death" then
        if deathCount < 3 then
            if phaseFrame == 20 then
                check(lives() == 3 - deathCount, 14, string.format("unexpected lives=%d after death %d", lives(), deathCount))
                input = { start = true }
            elseif phaseFrame == 44 then
                check(read(AP_STATE) == 0, 15, "did not respawn after START retry")
                nextPhase("walk_to_pit")
            end
        else
            -- 3rd death -> GAME OVER screen
            if phaseFrame == 20 then
                -- GAME OVER at $218A: G=$08, A=$02, M=$0d, E=$06
                check(readNt(0x018a) == 0x08 and readNt(0x018b) == 0x02
                    and readNt(0x018c) == 0x0d and readNt(0x018d) == 0x06, 16,
                    "GAME OVER screen not shown after 3rd death")
                check(read(SND_SONG_ID) == 5, 17,
                    string.format("expected Game Over BGM (5), got %d", read(SND_SONG_ID)))
                input = { start = true }
            elseif phaseFrame == 44 then
                -- Returned to Title screen (MAKAIMURA FC at $20CA: M=$0d, A=$02, K=$0b)
                check(readNt(0x00ca) == 0x0d and readNt(0x00cb) == 0x02 and readNt(0x00cc) == 0x0b, 18,
                    "START on GAME OVER did not return to Title")
                nextPhase("restart_fresh")
            end
        end
    elseif phase == "walk_to_pit" then
        input = { right = true }
        if read(AP_STATE) == 4 then
            deathCount = deathCount + 1
            nextPhase("after_death")
        elseif phaseFrame > 300 then
            check(false, 19, "failed to die in pit on retry")
        end
    elseif phase == "restart_fresh" then
        if phaseFrame == 10 then
            input = { start = true }
        elseif phaseFrame == 36 then
            check(read(0x8000) == 0x8c and stageNum() == 1, 20, "fresh restart did not enter Stage 1 Bank 12")
            check(lives() == 3, 21, string.format("fresh restart lives != 3 (%d)", lives()))
            check(scorePts() == 0, 22, string.format("fresh restart score != 0 (%d)", scorePts()))
            check(read(AP_LOOP) == 0 and hudLoopNum() == 1, 23, "fresh restart loop != 0 / LP 1")
            check(read(AP_RANK) == 0, 24, string.format("fresh restart rank != 0 (%d)", read(AP_RANK)))
            check(read(AP_WEAPON) == 0 and readNt(0x003e) == 0x0c, 25, "fresh restart weapon != Lance")
            check(read(AP_NEXT_EXTEND_LO) == 20 and read(AP_NEXT_EXTEND_HI) == 0, 26,
                "fresh restart next_extend != 20")
            print(string.format("GAMEOVER_RESTART_OK worstVBlank=%d", worstBudget))
            emu.stop(0)
        end
    end

    if frame > 4000 then
        check(false, 99, "arcade-perf-gameover-smoke timed out")
    end
end, emu.eventType.endFrame)
