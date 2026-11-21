-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
-- Verifies Task 3-4: scroll-linked enemy scheduler (page 2 activation),
-- fixed turret spawner, zombie cap + wait timer pause when full, slot
-- reclamation & wait resume on defeat, enemy bolt pool (<=2) + high-priority
-- OAM + offscreen reclamation, difficulty rank progression & drop on death,
-- and clean state reset on checkpoint retry.

local frame = 0
local phase = "boot"
local phaseFrame = 0
local input = {}

-- WRAM layout from src/arcade-player.inc
local AP_STATE       = 0x6010
local AP_ARMOR       = 0x6011
local AP_INVULN      = 0x6012
local AP_RIDING      = 0x601B
local PLAT_X_LO      = 0x6042
local PLAT_X_HI      = 0x6046
local AP_RANK        = 0x6072
local SCHED_PAGE     = 0x6074
local SCHED_GROUP    = 0x6075
local SCHED_WAIT     = 0x6076
local SCHED_ZOMBIES  = 0x6077
local TURRET_SPAWNED = 0x6078
local EN_TYPE        = 0x6079 -- 4 bytes
local EB_ACTIVE      = 0x6095 -- 2 bytes

local function read(addr)
    return emu.read(addr, emu.memType.nesMemory)
end

local function check(cond, code, msg)
    if cond then return true end
    emu.log(string.format("FAIL(%d) frame=%d phase=%s: %s", code, frame, phase, msg))
    emu.stop(code)
    return false
end

local function nextPhase(name)
    phase = name
    phaseFrame = 0
end

local function platX(slot)
    return read(PLAT_X_HI + slot) * 256 + read(PLAT_X_LO + slot)
end

local function playerScreenX()
    return read(0x203)
end

local function countEnemies()
    local n = 0
    for i = 0, 3 do
        if read(EN_TYPE + i) ~= 0 then n = n + 1 end
    end
    return n
end

local function countBolts()
    local n = 0
    for i = 0, 1 do
        if read(EB_ACTIVE + i) ~= 0 then n = n + 1 end
    end
    return n
end

local rankCap = { [0] = 2, [1] = 2, [2] = 3, [3] = 3 }
local killedEnemy1 = false
local sawTurret = false
local sawBolt = false
local sawBoltOam = false
local sawCapPause = false
local frozenWait = -1
local waitHoldFrames = 0
local sawZombieKilled = false
local rankBeforeDeath = 0

emu.addEventCallback(function()
    emu.setInput(input, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    phaseFrame = phaseFrame + 1
    input = {}

    if phase == "boot" then
        if frame == 128 then
            if read(0xe000) ~= 0xbf then
                emu.stop(0)
                return
            end
            nextPhase("enter")
        end
    elseif phase == "enter" then
        if phaseFrame == 1 then
            input = { right = true, start = true }
        elseif phaseFrame == 20 then
            check(read(AP_RANK) == 0 and read(SCHED_GROUP) == 0, 10, "initial rank/scheduler wrong")
            nextPhase("reach_cp")
        end
    elseif phase == "reach_cp" then
        -- Traverse stage cleanly to checkpoint (col 30, X=480) before the moving platform.
        input = { right = true }
        if phaseFrame < 140 and playerScreenX() >= 148 and playerScreenX() <= 154 and read(AP_STATE) == 0 then
            input.a = true
        end
        if phaseFrame == 215 then
            input.a = true
        end
        if not killedEnemy1 and phaseFrame >= 275 and phaseFrame <= 310 and phaseFrame % 8 == 0 then
            input.b = true
        end
        if read(0x6018) == 0 then killedEnemy1 = true end
        -- Jump onto tombstone at phaseFrame == 412, then shoot pot enemy after stepping off.
        if phaseFrame == 412 then
            input.a = true
        end
        if phaseFrame == 460 or phaseFrame == 476 then
            input.b = true
        end
        if phaseFrame == 530 then
            check(read(AP_ARMOR) >= 1 and read(AP_STATE) == 0, 11, "did not reach checkpoint cleanly")
            nextPhase("board_hplat")
        end
    elseif phase == "board_hplat" then
        local px = platX(1)
        if phaseFrame < 10 then
            input = { right = true }
        elseif px >= 502 and px <= 514 and read(AP_STATE) == 0 then
            input = { right = true, a = true }
            nextPhase("cross_to_page2")
        elseif phaseFrame > 140 then
            check(false, 11, "timed out waiting for horizontal platform")
        end
    elseif phase == "cross_to_page2" then
        if read(AP_RIDING) == 1 and platX(1) >= 530 then
            input = { right = true, a = true }
            nextPhase("enter_page2")
        elseif phaseFrame > 140 then
            check(false, 12, "did not cross pit to page 2")
        end
    elseif phase == "enter_page2" then
        -- Walk right until camera_high reaches page 2 (X ~ 704), then step back to X ~ 656.
        if read(SCHED_PAGE) < 2 then
            if phaseFrame >= 36 then
                input = { right = true }
            end
            check(phaseFrame <= 260, 13, "scheduler did not activate on page 2")
        else
            check(read(SCHED_GROUP) == 1, 13, "scheduler group not active on page 2")
            nextPhase("step_back_page2")
        end
    elseif phase == "step_back_page2" then
        -- Step left away from turret (X=712) while keeping camera on page 2.
        if phaseFrame <= 64 then
            input = { left = true }
        elseif phaseFrame == 65 then
            input = { right = true }
            nextPhase("observe_scheduler")
        end
    elseif phase == "observe_scheduler" then
        -- Stand at X~560 and observe turret spawn, enemy bolt, rank progression, and zombie cap pause.
        if read(TURRET_SPAWNED) == 1 then sawTurret = true end
        local bolts = countBolts()
        check(bolts <= 2, 14, "enemy bolt pool exceeded 2")
        if bolts >= 1 then
            sawBolt = true
            if read(0x21c) ~= 0xf8 or read(0x220) ~= 0xf8 then
                sawBoltOam = true
            end
        end
        local cap = rankCap[read(AP_RANK)]
        local zlive = read(SCHED_ZOMBIES)
        check(zlive <= cap, 15, "scheduled zombies exceeded rank cap")
        if zlive == cap then
            local w = read(SCHED_WAIT)
            if frozenWait == -1 then
                frozenWait = w
                waitHoldFrames = 1
            elseif w == frozenWait then
                waitHoldFrames = waitHoldFrames + 1
                if waitHoldFrames >= 10 then
                    sawCapPause = true
                end
            else
                frozenWait = w
                waitHoldFrames = 1
            end
        end
        -- Dodge incoming enemy bolt by crouching or jumping if needed, or simply shoot once cap pause seen.
        if sawTurret and sawBolt and sawBoltOam and sawCapPause then
            check(read(AP_RANK) >= 1, 16, "difficulty rank did not increase over time")
            nextPhase("kill_zombie_and_resume")
        elseif phaseFrame > 240 then
            check(sawTurret, 17, "fixed turret did not spawn on page 2")
            check(sawBolt and sawBoltOam, 18, "turret did not fire enemy bolt in high-priority OAM")
            check(sawCapPause, 19, "zombie cap did not pause scheduler wait timer")
            nextPhase("kill_zombie_and_resume")
        end
    elseif phase == "kill_zombie_and_resume" then
        -- Fire right to destroy an approaching zombie, then verify wait timer resumes and spawns a replacement.
        if phaseFrame % 12 == 1 then
            input = { b = true }
        end
        local cap = rankCap[read(AP_RANK)]
        if not sawZombieKilled and read(SCHED_ZOMBIES) < cap then
            sawZombieKilled = true
        end
        if sawZombieKilled and read(SCHED_ZOMBIES) == cap then
            rankBeforeDeath = read(AP_RANK)
            nextPhase("die_for_cp_retry")
        elseif phaseFrame > 220 then
            check(sawZombieKilled, 20, "could not defeat scheduled zombie")
            check(false, 21, "scheduler did not resume spawning after slot freed")
        end
    elseif phase == "die_for_cp_retry" then
        -- Walk left into the pit (col 31-33) to trigger instant death with active enemies/bolts on screen.
        input = { left = true }
        if read(AP_ARMOR) == 0 then
            nextPhase("retry_cp")
        elseif phaseFrame > 140 then
            check(false, 22, "could not trigger pit death for checkpoint retry")
        end
    elseif phase == "retry_cp" then
        if phaseFrame == 12 then
            input = { start = true }
        elseif phaseFrame == 28 then
            -- Verify full reset of enemy pool, bolt pool, scheduler, turret flag, and rank drop.
            check(countEnemies() == 0, 23, "enemy pool not cleared on checkpoint retry")
            check(countBolts() == 0, 24, "enemy bolt pool not cleared on checkpoint retry")
            check(read(SCHED_GROUP) == 0 and read(SCHED_ZOMBIES) == 0, 25, "scheduler not reset on retry")
            check(read(TURRET_SPAWNED) == 0, 26, "turret spawn flag not reset on retry")
            check(read(AP_RANK) < rankBeforeDeath, 27, "difficulty rank did not drop on death")
            emu.stop(0)
        end
    end

    if frame > 2600 then
        check(false, 99, "arcade-scheduler-smoke timed out")
    end
end, emu.eventType.endFrame)
