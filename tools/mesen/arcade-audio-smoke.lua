-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
-- Verifies Task 4-1: Bank 8 VRC7a FM 6-voice + APU BGM/SE driver.
-- Checks:
-- 1. Hardware VRC7 register write spacing ($9010 -> $9030 >= 6 CPU cycles,
--    $9030 -> next write >= 42 CPU cycles) on every single write.
-- 2. Stage BGM start (song_id=2), shared FM custom patch 0 load ($00..$07),
--    and active FM 6-voice + APU updates.
-- 3. Shot SE (APU Pulse 2) trigger and clean release.
-- 4. Damage SE FM ch5 + APU Noise preemption (snd_se_fm_mask=$20) and
--    automatic BGM ch5 restoration upon SE completion.
-- 5. Boss BGM switch (song_id=3) with shared custom patch swap (0 -> 1).
-- 6. Boss defeat + Key pickup -> Clear fanfare (song_id=4, non-looping stop)
--    -> Stage 2 transition (song_id=2, patch 0 restored) -> SELECT title
--    return full audio state reset (song_id=0, playing=0).

local frame = 0
local phase = "boot"
local phaseFrame = 0
local input = {}

-- WRAM addresses
local AP_STATE         = 0x6010
local AP_ARMOR         = 0x6011
local AP_RIDING        = 0x601B
local PLAT_X_LO        = 0x6042
local PLAT_X_HI        = 0x6046
local AP_BOSS_STATE    = 0x60A1
local AP_KEY_COLLECTED = 0x60AB

-- WRAM_SND ($7800+) from src/sound-driver.inc
local SND_SONG_ID      = 0x7800
local SND_PLAYING      = 0x7801
local SND_CUSTOM_PATCH = 0x7802
local SND_SE_ID        = 0x7803
local SND_SE_TIMER     = 0x7804
local SND_SE_FM_MASK   = 0x7805
local SND_SE_APU_MASK  = 0x7806
local SND_TICK_COUNT   = 0x7807

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

-- Monitor VRC7 $9010 / $9030 writes for strict hardware wait-cycle compliance.
local lastRegSelectCycle = -1000
local lastDataWriteCycle = -1000
local lastFmReg = -1
local customPatchWrites = 0
local fmVoicesKeyed = {}
local sawFm5RestoreAfterDamage = false
local trackingFm5Restore = false

emu.addMemoryCallback(function(_, value)
    local cyc = emu.getState()["cpu.cycleCount"]
    if lastDataWriteCycle >= 0 then
        local dt = cyc - lastDataWriteCycle
        check(dt >= 42, 60, string.format("$9030 -> $9010 spacing too short: %d cycles", dt))
    end
    lastRegSelectCycle = cyc
    lastFmReg = value
end, emu.callbackType.write, 0x9010, 0x9010, emu.memType.nesMemory)

emu.addMemoryCallback(function(_, value)
    local cyc = emu.getState()["cpu.cycleCount"]
    if lastRegSelectCycle >= 0 then
        local dt = cyc - lastRegSelectCycle
        check(dt >= 6, 61, string.format("$9010 -> $9030 spacing too short: %d cycles", dt))
    end
    lastDataWriteCycle = cyc
    if lastFmReg >= 0x00 and lastFmReg <= 0x07 then
        customPatchWrites = customPatchWrites + 1
    elseif lastFmReg >= 0x20 and lastFmReg <= 0x25 and (value & 0x10) ~= 0 then
        fmVoicesKeyed[lastFmReg - 0x20] = true
    elseif trackingFm5Restore and lastFmReg == 0x35 then
        sawFm5RestoreAfterDamage = true
    end
end, emu.callbackType.write, 0x9030, 0x9030, emu.memType.nesMemory)

local startTicks = 0
local sawShotSe = false
local sawDamagePreempt = false
local sawDamageRestore = false

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
            check(read(SND_SONG_ID) == 1 and read(SND_PLAYING) == 1, 10, "title BGM not playing at boot")
            check(read(SND_CUSTOM_PATCH) == 0 and customPatchWrites >= 8, 12, "custom FM patch 0 not loaded")
            nextPhase("enter")
        end
    elseif phase == "enter" then
        if phaseFrame == 1 then
            fmVoicesKeyed = {}
            input = { start = true }
        elseif phaseFrame == 16 then
            check(read(SND_SONG_ID) == 2 and read(SND_PLAYING) == 1, 11, "stage BGM did not start")
            check(read(SND_CUSTOM_PATCH) == 0, 12, "custom FM patch 0 not retained")
            startTicks = read(SND_TICK_COUNT)
            nextPhase("test_shot_se")
        end
    elseif phase == "test_shot_se" then
        if phaseFrame == 2 then
            input = { b = true }
        end
        if read(SND_SE_ID) == 1 and read(SND_SE_APU_MASK) == 2 then
            sawShotSe = true
        end
        if phaseFrame == 48 then
            check(sawShotSe, 13, "shot SE did not trigger on APU Pulse 2")
            check(read(SND_SE_ID) == 0 and read(SND_SE_APU_MASK) == 0, 14, "shot SE did not release cleanly")
            -- Verify all 6 FM voices have been keyed by the authentic arcade stage BGM.
            local keyed = 0
            for ch = 0, 5 do
                if fmVoicesKeyed[ch] then keyed = keyed + 1 end
            end
            check(keyed == 6, 15, string.format("expected 6 FM voices active, got %d", keyed))
            check(read(SND_TICK_COUNT) ~= startTicks, 16, "sound driver tick counter not advancing")
            nextPhase("test_damage_se")
        end
    elseif phase == "test_damage_se" then
        -- Walk right to enemy 1 (X=312) and take armor hit to trigger FM ch5 preemption SE.
        input = { right = true }
        if phaseFrame < 140 and playerScreenX() >= 148 and playerScreenX() <= 154 and read(AP_STATE) == 0 then
            input.a = true
        end
        if phaseFrame == 215 then
            input.a = true
        end
        if read(SND_SE_ID) == 2 and read(SND_SE_FM_MASK) == 0x20 then
            sawDamagePreempt = true
            trackingFm5Restore = true
        elseif sawDamagePreempt and read(SND_SE_ID) == 0 and read(SND_SE_FM_MASK) == 0 then
            sawDamageRestore = true
            check(sawFm5RestoreAfterDamage, 17, "FM ch5 instrument not restored after damage SE")
            nextPhase("die_and_retry")
        elseif phaseFrame > 380 then
            check(sawDamagePreempt, 18, "damage SE did not preempt FM ch5")
            check(sawDamageRestore, 19, "damage SE did not restore FM ch5")
        end
    elseif phase == "die_and_retry" then
        -- Wait for invulnerability to expire, then walk right into enemy 1 (X=312) to die cleanly.
        if phaseFrame >= 110 then
            input = { right = true }
        end
        if read(AP_ARMOR) == 0 then
            nextPhase("retry_start")
        elseif phaseFrame > 220 then
            check(false, 20, "could not trigger death for retry")
        end
    elseif phase == "retry_start" then
        if phaseFrame == 12 then
            input = { start = true }
        elseif phaseFrame == 28 then
            check(read(SND_SONG_ID) == 2 and read(SND_PLAYING) == 1 and read(SND_SE_ID) == 0, 20,
                "stage BGM not cleanly reinitialized on retry")
            nextPhase("reach_cp")
        end
    elseif phase == "reach_cp" then
        -- Proven clean route from start to checkpoint (X=480) with full armor.
        input = { right = true }
        if phaseFrame < 140 and playerScreenX() >= 148 and playerScreenX() <= 154 and read(AP_STATE) == 0 then
            input.a = true
        end
        if phaseFrame == 215 then
            input.a = true
        end
        if read(0x6018) ~= 0 and phaseFrame >= 275 and phaseFrame <= 310 and phaseFrame % 8 == 0 then
            input.b = true
        end
        if phaseFrame == 412 then
            input.a = true
        end
        if phaseFrame == 460 or phaseFrame == 476 then
            input.b = true
        end
        if phaseFrame == 530 then
            nextPhase("board_hplat")
        end
    elseif phase == "board_hplat" then
        local px = platX(1)
        if phaseFrame < 10 then
            input = { right = true }
        elseif px >= 502 and px <= 514 and read(AP_STATE) == 0 then
            input = { right = true, a = true }
            nextPhase("cross_pit")
        elseif phaseFrame > 140 then
            check(false, 20, "timed out waiting for hplat")
        end
    elseif phase == "cross_pit" then
        if read(AP_RIDING) == 1 and platX(1) >= 530 then
            input = { right = true, a = true }
            nextPhase("trigger_boss")
        elseif phaseFrame > 140 then
            check(false, 21, "did not cross pit")
        end
    elseif phase == "trigger_boss" then
        if phaseFrame >= 36 then
            input = { right = true }
            if phaseFrame % 28 == 0 then input.b = true end
            if playerScreenX() >= 164 and playerScreenX() <= 174 and read(AP_STATE) == 0 then
                input.a = true
            end
        end
        if read(AP_BOSS_STATE) == 1 then
            check(read(SND_SONG_ID) == 3 and read(SND_PLAYING) == 1, 22, "boss BGM did not start")
            check(read(SND_CUSTOM_PATCH) == 1, 23, "boss custom FM patch 1 not loaded")
            nextPhase("step_back")
        elseif phaseFrame > 360 then
            check(false, 24, "boss did not trigger")
        end
    elseif phase == "step_back" then
        if phaseFrame <= 54 then
            input = { left = true }
        elseif phaseFrame == 55 then
            input = { left = true, a = true }
        elseif phaseFrame <= 96 then
            input = { left = true }
        elseif read(AP_STATE) == 0 then
            input = { right = true }
            nextPhase("fight_boss")
        elseif phaseFrame > 140 then
            check(false, 25, "step_back stuck")
        end
    elseif phase == "fight_boss" then
        -- Dodge bolts and defeat boss.
        local pWorldX = 512 + playerScreenX() + 8
        for i = 0, 1 do
            if read(0x6095 + i) ~= 0 then
                local bx = read(0x6099 + i) * 256 + read(0x6097 + i)
                local by = read(0x609B + i)
                local dx = bx - pWorldX
                if dx > 0 and dx < 36 and read(AP_STATE) == 0 then
                    if by >= 162 then input.a = true else input.down = true end
                end
            end
        end
        if phaseFrame % 12 == 1 then
            input.b = true
        end
        if read(AP_BOSS_STATE) == 2 then
            nextPhase("collect_key")
        elseif phaseFrame > 320 then
            check(false, 26, "boss not defeated in time")
        end
    elseif phase == "collect_key" then
        input = { right = true }
        if read(AP_KEY_COLLECTED) == 1 then
            nextPhase("verify_clear_jingle")
        elseif phaseFrame > 200 then
            check(false, 27, "did not collect key")
        end
    elseif phase == "verify_clear_jingle" then
        if phaseFrame == 4 then
            check(read(SND_SONG_ID) == 4 and read(SND_PLAYING) == 1, 28, "clear fanfare did not start")
            check(read(SND_CUSTOM_PATCH) == 0, 29, "clear fanfare did not restore patch 0")
        elseif phaseFrame == 380 then
            -- Authentic arcade Makaimura clear fanfare (~359 frames) should have stopped cleanly on its own.
            check(read(SND_PLAYING) == 0, 30, "clear fanfare did not auto-stop after completion")
            input = { start = true }
        elseif phaseFrame == 400 then
            check(read(0x8000) == 0x8d, 31, "did not transition to Stage 2 (Bank 13)")
            check(read(SND_SONG_ID) == 2 and read(SND_PLAYING) == 1, 32, "stage 2 BGM did not start cleanly")
            nextPhase("return_title")
        end
    elseif phase == "return_title" then
        if phaseFrame == 4 then
            input = { select = true }
        elseif phaseFrame == 20 then
            check(read(SND_SONG_ID) == 1 and read(SND_PLAYING) == 1 and read(SND_SE_ID) == 0, 33,
                "title BGM not restored on title return")
            input = { select = true }
        elseif phaseFrame == 36 then
            check(read(SND_SONG_ID) == 0 and read(SND_PLAYING) == 0 and read(SND_SE_ID) == 0, 34,
                "audio state not reset when entering test menu")
            emu.stop(0)
        end
    end

    if frame > 3500 then
        check(false, 99, "arcade-audio-smoke timed out")
    end
end, emu.eventType.endFrame)
