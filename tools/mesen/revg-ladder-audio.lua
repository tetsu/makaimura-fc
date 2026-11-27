-- Regression suite for:
--   1. Naked vs. armored Arthur ladder animation (rf_9221/rf_9227 vs rf_9213/rf_9219,
--      immediate reload on armor transition, and no armor-palette sprites while naked).
--   2. Ladder damage & knockback behavior matching arcade Revision G ($F057/$F48C/$995F/$9A9F):
--      - Crouch cleared on ladder entry.
--      - Armored mid-ladder hit by enemy shot at dy=+10 (arcade $F4FE expanded shot Y box):
--        knocks Arthur off ladder, loses armor, clears ap_ladder_anim, starts 120F invuln,
--        triggers damage SE, and lands cleanly.
--      - Hit during ladder top-exit (ap_ladder_anim > 0) clears ap_ladder_anim to 0.
--      - Naked mid-ladder hit enters death flight (ap_kb_dying=1) and starts miss song ($31).
--      - Stage 2 multi-page ladder mid-climb hit at dy=+10 knocks Arthur off ladder.
--   3. BGM vs. SE volume balance:
--      - Stage 1 VRC7 FM BGM ($30..$35 attenuation nibble) stays >= 4 (-12 dB or quieter).
--      - APU weapon-throw SE and damage SE reach strong peak volume >= 13.

local SCENE = 0x07
local CAMPAIGN_STAGE = 0x14
local CAMERA_X = 0x25
local PAN_X = 0x30
local PLAYER_X = 0x4c
local PLAYER_FEET = 0x4e
local PLAYER_LEFT = 0x50
local OZ_PY = 0x00b2
local AP_STATE = 0x6010
local AP_ARMOR = 0x6011
local AP_INVULN = 0x6012
local AP_CROUCH = 0x6013
local AP_SHOOT_LOCK = 0x6014
local AP_KB_STEP = 0x6017
local KEY_COLLECTED = 0x60ab
local PLAYER_Y_HI = 0x60ac
local AP_LADDER_ANIM = 0x60fb
local AP_KB_DYING = 0x60fc
local AP_CLEAR_SEQ = 0x6102
local RP_CUR = 0x6990
local OB_KIND = 0x699b
local OB_XL = 0x6a27
local OB_XH = 0x6a3b
local OB_YL = 0x6a63
local OB_YH = 0x6a77
local OB_VAR = 0x6b7b
local OB_FLAGS = 0x6b8f
local SND_SONG_ID = 0x7800
local SND_SE_ID = 0x7803

local TITLE, PLAY, CLEAR = 7, 4, 5
local STATE_GROUND, STATE_LADDER, STATE_KNOCKBACK = 0, 2, 3
local OK_SHOT = 9
local OFL_HARMFUL = 2
local SONG_STAGE, SONG_MISS = 2, 8

local frame = 0
local phase = "boot"
local phaseFrame = 0
local input = {}

-- Audio telemetry
local lastFmReg = -1
local stageFmWrites = 0
local stageFmMinAtten = 15
local shotSePeakVol = 0
local damageSePeakVol = 0
local sawShotSe = false
local sawDamageSe = false

-- Ladder animation telemetry
local armorFrames = {}
local nakedFrames = {}
local armorFrameCount = 0
local nakedFrameCount = 0
local sawArmorPalOnArmored = false
local sawArmorPalOnNaked = false
local sawSkinPalOnNaked = false
local hitStartX = 0

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readWord(addr) return read(addr) + 256 * read(addr + 1) end
local function write(addr, v) emu.write(addr, v, emu.memType.nesMemory) end

local function check(cond, msg)
    if cond then return true end
    print(string.format("FAIL frame=%d phase=%s stg=%d x=%d y=%d st=%d ar=%d inv=%d la=%d dy=%d: %s",
        frame, phase, read(CAMPAIGN_STAGE), readWord(PLAYER_X),
        read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI),
        read(AP_STATE), read(AP_ARMOR), read(AP_INVULN),
        read(AP_LADDER_ANIM), read(AP_KB_DYING), msg))
    emu.stop(1)
    return false
end

local function clearObjs()
    for i = 0, 19 do
        write(OB_KIND + i, 0)
        write(OB_FLAGS + i, 0)
    end
end

-- Inspect Arthur's OAM sprites (drawn first starting at $0200, up to 8 sprites for 2x2 16x16 shape 3).
local function sampleArthurPalettes()
    local hasPal0, hasPal2 = false, false
    for s = 0, 7 do
        local y = read(0x0200 + s * 4)
        if y < 0xf0 then
            local pal = read(0x0202 + s * 4) & 3
            if pal == 0 then hasPal0 = true end
            if pal == 2 then hasPal2 = true end
        end
    end
    return hasPal0, hasPal2
end

-- Monitor VRC7 FM attenuation ($30..$35) during Stage 1 BGM.
emu.addMemoryCallback(function(_, value)
    lastFmReg = value
end, emu.callbackType.write, 0x9010, 0x9010, emu.memType.nesMemory)

emu.addMemoryCallback(function(_, value)
    if lastFmReg >= 0x30 and lastFmReg <= 0x35 then
        if read(SCENE) == PLAY and read(SND_SONG_ID) == SONG_STAGE then
            local atten = value & 0x0f
            if atten < 15 then
                stageFmWrites = stageFmWrites + 1
                if atten < stageFmMinAtten then stageFmMinAtten = atten end
            end
        end
    end
end, emu.callbackType.write, 0x9030, 0x9030, emu.memType.nesMemory)

-- Monitor APU Pulse 1 ($4000), Pulse 2 ($4004), and Noise ($400C) SE peak volumes.
local function onApuVolWrite(_, value)
    local vol = value & 0x0f
    local se = read(SND_SE_ID)
    if se == 1 then
        sawShotSe = true
        if vol > shotSePeakVol then shotSePeakVol = vol end
    elseif se == 2 then
        sawDamageSe = true
        if vol > damageSePeakVol then damageSePeakVol = vol end
    end
end
emu.addMemoryCallback(onApuVolWrite, emu.callbackType.write, 0x4000, 0x4000, emu.memType.nesMemory)
emu.addMemoryCallback(onApuVolWrite, emu.callbackType.write, 0x4004, 0x4004, emu.memType.nesMemory)
emu.addMemoryCallback(onApuVolWrite, emu.callbackType.write, 0x400c, 0x400c, emu.memType.nesMemory)

emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    if not check(frame < 4000, "revg-ladder-audio timed out") then return end

    local sc = read(SCENE)

    if phase == "boot" then
        if sc == TITLE then
            phaseFrame = frame
            phase = "title_wait"
        end
    elseif phase == "title_wait" then
        if frame == phaseFrame + 10 or frame == phaseFrame + 11 then
            input = {right = true, start = true}
        elseif sc == PLAY then
            phaseFrame = frame
            phase = "audio_shot_se"
        end
    elseif phase == "audio_shot_se" then
        clearObjs()
        -- Fire weapon to exercise snd_se_shot while Stage 1 BGM plays.
        local dt = frame - phaseFrame
        if dt == 10 or dt == 11 then
            input = {b = true}
        elseif dt >= 45 then
            if not check(stageFmWrites >= 3, string.format("expected Stage 1 FM writes, got %d", stageFmWrites)) then return end
            if not check(stageFmMinAtten >= 4,
                string.format("Stage 1 VRC7 BGM too loud: min atten=%d (expected >= 4 / -12 dB)", stageFmMinAtten)) then return end
            if not check(sawShotSe and shotSePeakVol >= 13,
                string.format("weapon throw SE too quiet: saw=%s peak=%d (expected >= 13)", tostring(sawShotSe), shotSePeakVol)) then return end
            phase = "crouch_diag_turn"
            phaseFrame = frame
        end
    elseif phase == "crouch_diag_turn" then
        clearObjs()
        local dt = frame - phaseFrame
        if dt < 4 then
            input = {down = true}
        elseif dt == 4 then
            if not check(read(AP_CROUCH) == 1 and read(PLAYER_LEFT) == 0,
                "expected Arthur crouching right before diagonal-turn check") then return end
            -- Fire while crouching right
            input = {down = true, b = true}
        elseif dt == 5 then
            input = {down = true}
        elseif dt == 6 then
            if not check(read(AP_CROUCH) == 1 and read(AP_SHOOT_LOCK) > 0 and read(PLAYER_LEFT) == 0,
                string.format("expected crouch-shoot right (crouch=%d lock=%d left=%d)",
                    read(AP_CROUCH), read(AP_SHOOT_LOCK), read(PLAYER_LEFT))) then return end
            -- Press diagonal Down+Left while crouch-shooting right -> must turn Left immediately while staying crouched!
            input = {down = true, left = true}
        elseif dt == 7 then
            input = {down = true, left = true, b = true}
        elseif dt == 8 then
            if not check(read(AP_CROUCH) == 1 and read(PLAYER_LEFT) == 1,
                string.format("crouch-shoot diagonal Down+Left failed to turn left (crouch=%d left=%d)",
                    read(AP_CROUCH), read(PLAYER_LEFT))) then return end
            input = {down = true, left = true}
        elseif dt == 9 then
            if not check(read(AP_CROUCH) == 1 and read(PLAYER_LEFT) == 1 and read(AP_SHOOT_LOCK) > 0,
                string.format("crouch-shoot left after diagonal turn failed (crouch=%d left=%d lock=%d)",
                    read(AP_CROUCH), read(PLAYER_LEFT), read(AP_SHOOT_LOCK))) then return end
            -- Press diagonal Down+Right while crouch-shooting left -> must turn Right immediately while staying crouched!
            input = {down = true, right = true}
        elseif dt == 10 then
            input = {down = true, right = true}
        else
            if not check(read(AP_CROUCH) == 1 and read(PLAYER_LEFT) == 0,
                string.format("crouch-shoot diagonal Down+Right failed to turn right (crouch=%d left=%d)",
                    read(AP_CROUCH), read(PLAYER_LEFT))) then return end
            phase = "walk_to_ladder"
            phaseFrame = frame
        end
    elseif phase == "walk_to_ladder" then
        clearObjs()
        write(AP_INVULN, 0)
        local px = readWord(PLAYER_X)
        local jump = (px >= 232 and px <= 248) or (px >= 392 and px <= 408) or (px >= 504 and px <= 520)
        input = {right = true, a = jump}
        if px >= 716 and read(AP_STATE) == STATE_GROUND then
            -- First crouch at bottom of ladder to test crouch-to-ladder entry.
            write(AP_ARMOR, 2)
            input = {down = true}
            phase = "crouch_before_ladder"
            phaseFrame = frame
        end
    elseif phase == "crouch_before_ladder" then
        clearObjs()
        local dt = frame - phaseFrame
        if dt < 4 then
            input = {down = true}
        else
            if not check(read(AP_CROUCH) == 1, "expected Arthur crouching before ladder entry") then return end
            input = {up = true}
            phase = "armored_climb"
            phaseFrame = frame
        end
    elseif phase == "armored_climb" then
        clearObjs()
        input = {up = true}
        local dt = frame - phaseFrame
        if read(AP_STATE) == STATE_LADDER then
            if not check(read(AP_CROUCH) == 0, "ap_crouch must clear immediately on entering ladder") then return end
            if dt >= 8 then
                local fid = readWord(RP_CUR)
                if not armorFrames[fid] then
                    armorFrames[fid] = true
                    armorFrameCount = armorFrameCount + 1
                end
                local _, hasPal2 = sampleArthurPalettes()
                if hasPal2 then sawArmorPalOnArmored = true end
            end
        end
        if dt >= 26 then
            if not check(armorFrameCount == 2 and sawArmorPalOnArmored,
                string.format("armored ladder anim should alternate 2 frames with armor pal (cnt=%d pal2=%s)",
                    armorFrameCount, tostring(sawArmorPalOnArmored))) then return end
            -- Switch to NAKED in place while still climbing the ladder!
            write(AP_ARMOR, 1)
            phase = "naked_climb"
            phaseFrame = frame
        end
    elseif phase == "naked_climb" then
        clearObjs()
        input = {up = true}
        local dt = frame - phaseFrame
        -- Wait 2 frames for rp_draw_player to reload and stream CHR tiles.
        if dt >= 3 and read(AP_STATE) == STATE_LADDER then
            local fid = readWord(RP_CUR)
            if not check(not armorFrames[fid],
                string.format("naked ladder drew armored frame $%04X!", fid)) then return end
            if not nakedFrames[fid] then
                nakedFrames[fid] = true
                nakedFrameCount = nakedFrameCount + 1
            end
            local hasPal0, hasPal2 = sampleArthurPalettes()
            if hasPal0 then sawSkinPalOnNaked = true end
            if hasPal2 then sawArmorPalOnNaked = true end
        end
        if dt >= 20 then
            if not check(nakedFrameCount == 2 and sawSkinPalOnNaked and not sawArmorPalOnNaked,
                string.format("naked ladder anim failed (cnt=%d pal0=%s pal2=%s)",
                    nakedFrameCount, tostring(sawSkinPalOnNaked), tostring(sawArmorPalOnNaked))) then return end
            -- Prepare Test B: Armored mid-ladder hit by shot at dy=+10!
            write(PLAYER_X, 720 & 0xff); write(PLAYER_X + 1, 720 >> 8)
            write(PLAYER_FEET, 164)
            write(AP_STATE, STATE_LADDER); write(AP_ARMOR, 2)
            write(AP_INVULN, 0); write(AP_LADDER_ANIM, 0)
            phase = "s1_mid_hit_arm"
            phaseFrame = frame
        end
    elseif phase == "s1_mid_hit_arm" then
        input = {up = true}
        local dt = frame - phaseFrame
        if dt == 1 then
            local px = readWord(PLAYER_X)
            local ozpy = readWord(OZ_PY)
            hitStartX = px
            write(OB_KIND + 0, OK_SHOT); write(OB_VAR + 0, 0); write(OB_FLAGS + 0, OFL_HARMFUL)
            write(OB_XL + 0, (px + 4) & 0xff); write(OB_XH + 0, (px + 4) >> 8)
            write(OB_YL + 0, (ozpy + 10) & 0xff); write(OB_YH + 0, (ozpy + 10) >> 8)
        elseif dt == 3 then
            if not check(read(AP_STATE) == STATE_KNOCKBACK and read(AP_ARMOR) == 1
                    and read(AP_LADDER_ANIM) == 0 and read(AP_KB_DYING) == 0,
                "armored mid-ladder hit at dy=+10 must trigger knockback and lose armor") then return end
        elseif dt >= 10 and dt <= 60 then
            if read(AP_STATE) == STATE_GROUND then
                if not check(readWord(PLAYER_X) ~= hitStartX and read(AP_INVULN) > 0,
                    "knockback must move Arthur off ladder and leave invuln active") then return end
                if not check(sawDamageSe and damageSePeakVol >= 13,
                    string.format("damage SE too quiet: saw=%s peak=%d (expected >= 13)", tostring(sawDamageSe), damageSePeakVol)) then return end
                -- Prepare Test C: Hit during ladder top-exit (ap_ladder_anim == 5)
                clearObjs()
                write(PLAYER_X, 720 & 0xff); write(PLAYER_X + 1, 720 >> 8)
                write(PLAYER_FEET, 126)
                write(AP_STATE, STATE_LADDER); write(AP_ARMOR, 2)
                write(AP_INVULN, 0); write(AP_LADDER_ANIM, 5)
                phase = "s1_top_exit_hit"
                phaseFrame = frame
            end
        elseif dt > 60 then
            check(false, "armored ladder knockback did not land cleanly")
            return
        end
    elseif phase == "s1_top_exit_hit" then
        input = {up = true}
        local dt = frame - phaseFrame
        if dt == 1 then
            local px = readWord(PLAYER_X)
            local ozpy = readWord(OZ_PY)
            write(OB_KIND + 0, OK_SHOT); write(OB_VAR + 0, 0); write(OB_FLAGS + 0, OFL_HARMFUL)
            write(OB_XL + 0, (px + 2) & 0xff); write(OB_XH + 0, (px + 2) >> 8)
            write(OB_YL + 0, ozpy & 0xff); write(OB_YH + 0, ozpy >> 8)
        elseif dt == 3 then
            if not check(read(AP_STATE) == STATE_KNOCKBACK and read(AP_ARMOR) == 1
                    and read(AP_LADDER_ANIM) == 0,
                "hit during ladder top-exit must knock Arthur off and clear ap_ladder_anim") then return end
            -- Prepare Test D: Naked mid-ladder hit -> death flight!
            clearObjs()
            write(PLAYER_X, 720 & 0xff); write(PLAYER_X + 1, 720 >> 8)
            write(PLAYER_FEET, 160)
            write(AP_STATE, STATE_LADDER); write(AP_ARMOR, 1)
            write(AP_INVULN, 0); write(AP_LADDER_ANIM, 0); write(AP_KB_DYING, 0)
            phase = "s1_naked_hit"
            phaseFrame = frame
        end
    elseif phase == "s1_naked_hit" then
        input = {up = true}
        local dt = frame - phaseFrame
        if dt == 1 then
            local px = readWord(PLAYER_X)
            local ozpy = readWord(OZ_PY)
            write(OB_KIND + 0, OK_SHOT); write(OB_VAR + 0, 0); write(OB_FLAGS + 0, OFL_HARMFUL)
            write(OB_XL + 0, (px + 2) & 0xff); write(OB_XH + 0, (px + 2) >> 8)
            write(OB_YL + 0, ozpy & 0xff); write(OB_YH + 0, ozpy >> 8)
        elseif dt == 3 then
            if not check(read(AP_STATE) == STATE_KNOCKBACK and read(AP_KB_DYING) == 1
                    and read(AP_LADDER_ANIM) == 0 and read(SND_SONG_ID) == SONG_MISS,
                string.format("naked mid-ladder hit must enter death flight & miss song (song=%d)", read(SND_SONG_ID))) then return end
            -- Restore alive state and fast-clear Stage 1 -> Stage 2 to verify 2-page ladder hit!
            clearObjs()
            write(AP_STATE, STATE_GROUND); write(AP_ARMOR, 2); write(AP_KB_DYING, 0)
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
            phase = "s1_clear"
            phaseFrame = frame
        end
    elseif phase == "s1_clear" then
        if sc == CLEAR then
            phaseFrame = frame
            phase = "s1_clear_wait"
        end
    elseif phase == "s1_clear_wait" then
        if frame == phaseFrame + 15 then
            input = {start = true}
            phaseFrame = frame
            phase = "s2_init"
        end
    elseif phase == "s2_init" then
        if frame >= phaseFrame + 15 and sc == PLAY and read(CAMPAIGN_STAGE) == 2 then
            -- Warp to Stage 2 Ladder 1 (X=1648, FC Y=432, 2-page vertical map).
            clearObjs()
            write(PLAYER_X, 1648 & 0xff); write(PLAYER_X + 1, 1648 >> 8)
            write(PLAYER_FEET, 432 & 0xff); write(PLAYER_Y_HI, 432 >> 8)
            write(CAMERA_X, 1536 & 0xff); write(CAMERA_X + 1, 1536 >> 8)
            write(PAN_X, 1536 & 0xff); write(PAN_X + 1, 1536 >> 8)
            write(AP_STATE, STATE_GROUND); write(AP_ARMOR, 2); write(AP_INVULN, 0)
            phase = "s2_ladder_hit"
            phaseFrame = frame
        end
    elseif phase == "s2_ladder_hit" then
        input = {up = true}
        local dt = frame - phaseFrame
        if dt < 15 then clearObjs() end
        if dt == 15 then
            if not check(read(AP_STATE) == STATE_LADDER, "expected Arthur climbing Stage 2 Ladder 1") then return end
            local px = readWord(PLAYER_X)
            local ozpy = readWord(OZ_PY)
            write(OB_KIND + 18, OK_SHOT); write(OB_FLAGS + 18, OFL_HARMFUL)
            write(OB_XL + 18, (px + 2) & 0xff); write(OB_XH + 18, (px + 2) >> 8)
            write(OB_YL + 18, (ozpy + 10) & 0xff); write(OB_YH + 18, (ozpy + 10) >> 8)
        elseif dt == 17 then
            if not check(read(AP_STATE) == STATE_KNOCKBACK and read(AP_ARMOR) == 1
                    and read(AP_LADDER_ANIM) == 0,
                "Stage 2 multi-page mid-ladder hit at dy=+10 must trigger knockback") then return end
            print(string.format("PASS: ladder anim (armor=%d naked=%d pal0=%s pal2=%s), S1/S2 ladder hits, audio mix (fmMinAtten=%d shotVol=%d dmgVol=%d) (frame %d)",
                armorFrameCount, nakedFrameCount, tostring(sawSkinPalOnNaked), tostring(sawArmorPalOnNaked),
                stageFmMinAtten, shotSePeakVol, damageSePeakVol, frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
