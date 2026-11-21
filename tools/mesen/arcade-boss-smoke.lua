-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
-- Verifies Task 3-5:
-- 1. Boss encounter trigger at X >= 720, arena camera & left-edge lock,
--    regular enemy/bolt cleanup, <= 8 sprites per scanline across 24x32 boss.
-- 2. Boss patrol & enemy bolt attacks, death in boss arena -> checkpoint retry
--    full state reset.
-- 3. Multi-part HP (lower shield armor -> core/head HP), boss defeat -> Key
--    spawn, exit locked without Key, Key pickup -> Stage Clear -> START
--    transition to Stage 2 (Bank 1) with lives carried and state reset.
-- 4. Simultaneous timeout + final boss-killing blow on the exact same frame:
--    deducts exactly one life, does not falsely clear, and resets cleanly on retry.

local frame = 0
local phase = "boot"
local phaseFrame = 0
local input = {}

-- WRAM addresses from src/arcade-player.inc
local AP_STATE          = 0x6010
local AP_ARMOR          = 0x6011
local AP_WEAPON         = 0x601A
local AP_RIDING         = 0x601B
local PS_ACTIVE         = 0x6026 -- 3 bytes
local PLAT_X_LO         = 0x6042
local PLAT_X_HI         = 0x6046
local ITEM_TYPE         = 0x6062 -- 4 bytes (slot 3 = key)
local EN_TYPE           = 0x6079 -- 4 bytes
local EB_ACTIVE         = 0x6095 -- 2 bytes
local EB_X_LO           = 0x6097 -- 2 bytes
local EB_X_HI           = 0x6099 -- 2 bytes
local EB_Y              = 0x609B -- 2 bytes
local AP_BOSS_STATE     = 0x60A1
local AP_BOSS_LOCK      = 0x60A2
local AP_BOSS_HP        = 0x60A3
local AP_BOSS_SHIELD    = 0x60A4
local AP_BOSS_X_LO      = 0x60A5
local AP_BOSS_X_HI      = 0x60A6
local AP_BOSS_DIR       = 0x60A8
local AP_KEY_COLLECTED  = 0x60AB

local digitZero = 0x1b

local function read(addr)
    return emu.read(addr, emu.memType.nesMemory)
end

local function hud(addr)
    return emu.read(addr, emu.memType.nesPpuMemory)
end

local function clock()
    return (hud(0x2035) - digitZero) * 100
        + (hud(0x2036) - digitZero) * 10
        + hud(0x2037) - digitZero
end

local function lives()
    return hud(0x202e) - digitZero
end

local function stageNum()
    return hud(0x2027) - digitZero
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

local function bossX()
    return read(AP_BOSS_X_HI) * 256 + read(AP_BOSS_X_LO)
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

-- Verify no scanline has more than 8 visible 8x16 sprites in OAM.
local function checkScanlineSpriteLimit()
    local counts = {}
    for slot = 0, 63 do
        local y = read(0x200 + slot * 4)
        if y < 0xef then
            for row = y + 1, y + 16 do
                local c = (counts[row] or 0) + 1
                counts[row] = c
                if c > 8 then
                    return false, row, c
                end
            end
        end
    end
    return true, 0, 0
end

local function traverseToCheckpoint(pf, killedFlag)
    local inp = { right = true }
    if pf < 140 and playerScreenX() >= 148 and playerScreenX() <= 154 and read(AP_STATE) == 0 then
        inp.a = true
    end
    if pf == 215 then
        inp.a = true
    end
    if not killedFlag and pf >= 275 and pf <= 310 and pf % 8 == 0 then
        inp.b = true
    end
    if pf == 412 then
        inp.a = true
    end
    if pf == 460 or pf == 476 then
        inp.b = true
    end
    return inp
end

local function dodgeBossBolts(inp)
    local pWorldX = 512 + playerScreenX() + 8
    for i = 0, 1 do
        if read(EB_ACTIVE + i) ~= 0 then
            local bx = read(EB_X_HI + i) * 256 + read(EB_X_LO + i)
            local by = read(EB_Y + i)
            local dx = bx - pWorldX
            if dx > 0 and dx < 36 then
                if by >= 162 then
                    if read(AP_STATE) == 0 then inp.a = true end
                else
                    if read(AP_STATE) == 0 then inp.down = true end
                end
                return true
            end
        end
    end
    return false
end

local killedEnemy1 = false
local sawBossBolt = false
local sawShieldDamage = false
local sawCoreDamage = false
local lockCheckMinX = 255
local prevClock = 120
local clockOneFrame = -1
local firedSimultaneous = false
local simultaneousTriggered = false

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
            nextPhase("enter1")
        end

    -- Route 1: Reach boss, verify lock + 8-sprite limit + bolts, die & retry.
    elseif phase == "enter1" then
        if phaseFrame == 1 then
            input = { right = true, start = true }
        elseif phaseFrame == 20 then
            check(read(AP_BOSS_STATE) == 0 and read(AP_BOSS_LOCK) == 0, 10, "boss state not idle at start")
            killedEnemy1 = false
            nextPhase("r1_reach_cp")
        end
    elseif phase == "r1_reach_cp" then
        input = traverseToCheckpoint(phaseFrame, killedEnemy1)
        if read(0x6018) == 0 then killedEnemy1 = true end
        if phaseFrame == 530 then
            check(read(AP_ARMOR) >= 1 and read(AP_STATE) == 0, 11, "r1 did not reach checkpoint")
            nextPhase("r1_board_hplat")
        end
    elseif phase == "r1_board_hplat" then
        local px = platX(1)
        if phaseFrame < 10 then
            input = { right = true }
        elseif px >= 502 and px <= 514 and read(AP_STATE) == 0 then
            input = { right = true, a = true }
            nextPhase("r1_cross_pit")
        elseif phaseFrame > 140 then
            check(false, 12, "r1 timed out waiting for hplat")
        end
    elseif phase == "r1_cross_pit" then
        if read(AP_RIDING) == 1 and platX(1) >= 530 then
            input = { right = true, a = true }
            nextPhase("r1_trigger_boss")
        elseif phaseFrame > 140 then
            check(false, 13, "r1 did not cross pit")
        end
    elseif phase == "r1_trigger_boss" then
        if phaseFrame >= 36 then
            input = { right = true }
        end
        if read(AP_BOSS_STATE) == 1 then
            check(read(AP_BOSS_LOCK) == 1, 14, "boss lock not set on trigger")
            check(read(AP_BOSS_HP) == 4 and read(AP_BOSS_SHIELD) == 2, 15, "boss initial HP/shield wrong")
            check(countEnemies() == 0, 16, "regular enemies not cleared on boss start")
            lockCheckMinX = 255
            sawBossBolt = false
            nextPhase("r1_verify_lock_and_die")
        elseif phaseFrame > 260 then
            check(false, 17, "boss did not trigger")
        end
    elseif phase == "r1_verify_lock_and_die" then
        local ok, row, cnt = checkScanlineSpriteLimit()
        check(ok, 18, string.format("scanline %d exceeded 8 sprites (%d)", row, cnt))
        -- Verify 4 boss sprites (16x32 head+body) are visible in OAM slots 11-14 ($22C..$238).
        if phaseFrame == 2 then
            for s = 11, 14 do
                check(read(0x200 + s * 4) < 0xef, 19, "boss OAM sprite missing")
            end
        end
        if countBolts() >= 1 then sawBossBolt = true end
        if phaseFrame <= 90 then
            -- Walk left to test arena left-edge lock; screen X must clamp at >= 4 (world X >= 520).
            input = { left = true }
            local sx = playerScreenX()
            if sx < lockCheckMinX then lockCheckMinX = sx end
        else
            -- Walk right into the boss to lose armor and die.
            input = { right = true }
            if read(AP_ARMOR) == 0 then
                check(sawBossBolt, 20, "boss never fired enemy bolt")
                nextPhase("r1_retry_cp")
            elseif phaseFrame > 420 then
                check(false, 21, "player did not die in boss arena")
            end
        end
    elseif phase == "r1_retry_cp" then
        if phaseFrame == 12 then
            input = { start = true }
        elseif phaseFrame == 28 then
            check(read(AP_BOSS_STATE) == 0 and read(AP_BOSS_LOCK) == 0, 22, "boss state not reset on retry")
            check(read(AP_BOSS_HP) == 0 and read(AP_BOSS_SHIELD) == 0, 23, "boss HP not reset on retry")
            check(read(AP_KEY_COLLECTED) == 0 and read(ITEM_TYPE + 3) == 0, 24, "key state not reset on retry")
            check(countBolts() == 0 and countEnemies() == 0, 25, "enemies/bolts not cleared on retry")
            check(lives() == 2, 26, "lives not decremented to 2")
            nextPhase("r2_board_hplat")
        end

    -- Route 2: Re-enter boss arena, chip shield + core HP, defeat boss, collect Key, transition to Stage 2.
    elseif phase == "r2_board_hplat" then
        local px = platX(1)
        if phaseFrame < 14 then
            input = { right = true }
        elseif px >= 502 and px <= 514 and read(AP_STATE) == 0 then
            input = { right = true, a = true }
            nextPhase("r2_cross_pit")
        elseif phaseFrame > 160 then
            check(false, 27, "r2 timed out waiting for hplat")
        end
    elseif phase == "r2_cross_pit" then
        if read(AP_RIDING) == 1 and platX(1) >= 530 then
            input = { right = true, a = true }
            nextPhase("r2_trigger_boss")
        elseif phaseFrame > 140 then
            check(false, 28, "r2 did not cross pit")
        end
    elseif phase == "r2_trigger_boss" then
        if phaseFrame >= 36 then
            input = { right = true }
            if phaseFrame % 28 == 0 then input.b = true end
            if playerScreenX() >= 164 and playerScreenX() <= 174 and read(AP_STATE) == 0 then
                input.a = true
            end
        end
        if read(AP_BOSS_STATE) == 1 then
            nextPhase("r2_step_back")
        elseif phaseFrame > 360 then
            check(false, 29, "r2 boss did not re-trigger")
        end
    elseif phase == "r2_step_back" then
        if phaseFrame <= 54 then
            input = { left = true }
        elseif phaseFrame == 55 then
            input = { left = true, a = true }
        elseif phaseFrame <= 96 then
            input = { left = true }
        elseif read(AP_STATE) == 0 then
            input = { right = true }
            sawShieldDamage = false
            sawCoreDamage = false
            nextPhase("r2_fight_boss")
        elseif phaseFrame > 140 then
            check(false, 28, "r2_step_back stuck")
        end
    elseif phase == "r2_fight_boss" then
        local ok, row, cnt = checkScanlineSpriteLimit()
        check(ok, 30, string.format("r2 scanline %d exceeded 8 sprites (%d)", row, cnt))
        check(read(AP_ARMOR) > 0, 34, "player died during r2_fight_boss")
        dodgeBossBolts(input)
        if phaseFrame % 12 == 1 then
            input.b = true
        end
        if read(AP_BOSS_SHIELD) < 2 then sawShieldDamage = true end
        if read(AP_BOSS_HP) < 4 then sawCoreDamage = true end
        if read(AP_BOSS_STATE) == 2 then
            check(sawShieldDamage and sawCoreDamage, 31, "multi-part shield/core HP not exercised")
            check(read(ITEM_TYPE + 3) == 5, 32, "key did not spawn on boss defeat")
            check(countBolts() == 0, 33, "enemy bolts not cleared on boss defeat")
            nextPhase("r2_collect_key")
        elseif phaseFrame > 320 then
            check(false, 35, "boss not defeated in time")
        end
    elseif phase == "r2_collect_key" then
        input = { right = true }
        if read(AP_KEY_COLLECTED) == 1 then
            nextPhase("r2_wait_clear")
        elseif phaseFrame > 200 then
            check(false, 35, "did not collect key")
        end
    elseif phase == "r2_wait_clear" then
        if phaseFrame == 16 then
            -- Press START on Stage 1 Clear screen to advance to Stage 2 (Bank 1).
            input = { start = true }
        elseif phaseFrame == 36 then
            check(read(0x8000) == 0x8d, 36, "stage 2 did not switch to Bank 13")
            check(stageNum() == 2, 37, "stage 2 HUD stage num mismatch")
            -- Route 1 lost 1 life (3->2), Route 2 boss+key crossed 2,000 pts for 1UP extend (2->3).
            check(lives() == 3, 54, "stage 2 HUD lives mismatch")
            check(clock() == 120, 55, "stage 2 HUD clock mismatch")
            check(read(AP_BOSS_STATE) == 0 and read(AP_KEY_COLLECTED) == 0, 38, "stage 2 inherited boss/key state")
            nextPhase("r3_return_title")
        end

    -- Route 3: Simultaneous timeout + final boss-killing blow on the exact same frame.
    elseif phase == "r3_return_title" then
        if phaseFrame == 4 then
            input = { select = true }
        elseif phaseFrame == 20 then
            input = { right = true, start = true }
        elseif phaseFrame == 40 then
            check(read(0x8000) == 0x8c and lives() == 3 and clock() == 120, 39, "r3 fresh start failed")
            killedEnemy1 = false
            nextPhase("r3_reach_cp")
        end
    elseif phase == "r3_reach_cp" then
        input = traverseToCheckpoint(phaseFrame, killedEnemy1)
        if read(0x6018) == 0 then killedEnemy1 = true end
        if phaseFrame == 530 then
            nextPhase("r3_board_hplat")
        end
    elseif phase == "r3_board_hplat" then
        local px = platX(1)
        if phaseFrame < 10 then
            input = { right = true }
        elseif px >= 502 and px <= 514 and read(AP_STATE) == 0 then
            input = { right = true, a = true }
            nextPhase("r3_cross_pit")
        elseif phaseFrame > 140 then
            check(false, 40, "r3 timed out waiting for hplat")
        end
    elseif phase == "r3_cross_pit" then
        if read(AP_RIDING) == 1 and platX(1) >= 530 then
            input = { right = true, a = true }
            nextPhase("r3_trigger_boss")
        elseif phaseFrame > 140 then
            check(false, 41, "r3 did not cross pit")
        end
    elseif phase == "r3_trigger_boss" then
        if phaseFrame >= 36 then
            input = { right = true }
            if phaseFrame % 28 == 0 then input.b = true end
            if playerScreenX() >= 164 and playerScreenX() <= 174 and read(AP_STATE) == 0 then
                input.a = true
            end
        end
        if read(AP_BOSS_STATE) == 1 then
            nextPhase("r3_step_back")
        elseif phaseFrame > 360 then
            check(false, 42, "r3 boss did not trigger")
        end
    elseif phase == "r3_step_back" then
        if phaseFrame <= 54 then
            input = { left = true }
        elseif phaseFrame == 55 then
            input = { left = true, a = true }
        elseif phaseFrame <= 96 then
            input = { left = true }
        elseif read(AP_STATE) == 0 then
            input = { right = true }
            nextPhase("r3_weaken_boss")
        elseif phaseFrame > 140 then
            check(false, 42, "r3_step_back stuck")
        end
    elseif phase == "r3_weaken_boss" then
        -- Weaken boss until shield == 0 and HP == 1, firing only when no player shot is active.
        dodgeBossBolts(input)
        if read(AP_BOSS_SHIELD) == 0 and read(AP_BOSS_HP) == 1 then
            prevClock = clock()
            clockOneFrame = -1
            firedSimultaneous = false
            simultaneousTriggered = false
            nextPhase("r3_wait_timeout")
        else
            local activeShots = read(PS_ACTIVE) + read(PS_ACTIVE + 1) + read(PS_ACTIVE + 2)
            if activeShots == 0 and phaseFrame % 8 == 1 then
                input.b = true
            end
            if phaseFrame > 360 then
                check(false, 43, "r3 failed to weaken boss to HP=1")
            end
        end
    elseif phase == "r3_wait_timeout" then
        -- Dodge incoming enemy bolts cleanly while waiting for clock to reach 1.
        local pWorldX = 512 + playerScreenX() + 8
        dodgeBossBolts(input)

        local curClock = clock()
        if curClock == 1 and prevClock == 2 then
            clockOneFrame = frame
        end
        prevClock = curClock

        -- Fire weapon (Lance=3 px/f, Dagger=4 px/f) so it hits boss (bossX - 14)
        -- on the exact timeout frame (clockOneFrame + 59).
        if clockOneFrame > 0 and not firedSimultaneous then
            local rem = (clockOneFrame + 59) - frame
            local vx = (read(AP_WEAPON) == 1) and 4 or 3
            local bDir = read(AP_BOSS_DIR) == 0 and -0.5 or 0.5
            local predBossX = bossX() + bDir * rem
            local dist = (predBossX - 14) - pWorldX
            local flight = math.floor((dist + vx - 1) / vx)
            if rem > 0 and rem <= flight then
                input = { b = true }
                firedSimultaneous = true
            end
        end

        -- Check if simultaneous timeout + boss defeat occurred on the same frame!
        if read(AP_BOSS_STATE) == 2 and read(AP_ARMOR) == 0 then
            simultaneousTriggered = true
            nextPhase("r3_verify_simultaneous")
        elseif read(AP_BOSS_STATE) == 2 and read(AP_ARMOR) > 0 then
            check(false, 44, "boss died before timeout frame")
        elseif curClock == 0 and clockOneFrame > 0 and frame > clockOneFrame + 64 then
            check(false, 45, "timeout occurred without simultaneous boss kill")
        elseif phaseFrame > 7400 then
            check(false, 53, "r3_wait_timeout exceeded 7400 frames")
        elseif read(AP_ARMOR) == 0 and curClock > 0 then
            check(false, 46, "player died before timeout while dodging")
        end
    elseif phase == "r3_verify_simultaneous" then
        if phaseFrame == 8 then
            check(simultaneousTriggered, 47, "simultaneous flag not set")
            check(clock() == 0 and lives() == 2, 48, "simultaneous timeout+kill did not deduct exactly 1 life")
            check(read(AP_BOSS_STATE) == 2 and read(ITEM_TYPE + 3) == 5, 49, "boss defeat did not register on timeout frame")
            check(read(AP_KEY_COLLECTED) == 0, 50, "key falsely collected on timeout frame")
        elseif phaseFrame == 16 then
            input = { start = true }
        elseif phaseFrame == 32 then
            check(lives() == 2 and clock() == 120, 51, "retry after simultaneous timeout+kill failed")
            check(read(AP_BOSS_STATE) == 0 and read(ITEM_TYPE + 3) == 0, 52, "boss/key not reset after simultaneous timeout+kill")
            emu.stop(0)
        end
    end

    if frame > 13000 then
        check(false, 99, "arcade-boss-smoke timed out")
    end
end, emu.eventType.endFrame)
