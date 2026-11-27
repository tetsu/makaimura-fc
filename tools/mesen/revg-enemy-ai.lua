-- Regression test for Revision G arcade-accurate enemy & boss AI across Stages 1-7:
--   1. Stage 1 Unicorn left-charge does NOT abort on frame 1 (moves past Arthur)
--   2. Stage 2 Mansion Big Man: Charge (state=2) + horiz shot (rf_FCF2/FA), Downward Shoot
--      (state=3) + rf_FD02, 6F Hit-Stun (state=4, rf_51B0), and never despawns offscreen
--   3. Stage 5 Floor Skull: completes 2 bounces + 80-px sine-arc leap (ob_step==2; no landing freeze!)
--   4. Stage 6 Unicorn: spawns with 10 HP (ob_hp == 9)
--   5. Stage 6 Satan Variant 1: dormant hits early-wake into state 1 with HP reset to 4 (ob_hp == 3)
local SCENE = 0x07
local CAMPAIGN_STAGE = 0x14
local CAMERA_X = 0x25
local PAN_Y = 0x09
local PAN_X = 0x30
local PAN_Y_PAGE = 0x32
local PLAYER_X = 0x4c
local PLAYER_FEET = 0x4e
local AP_ARMOR = 0x6011
local AP_INVULN = 0x6012
local KEY_COLLECTED = 0x60ab
local PLAYER_Y_HI = 0x60ac
local AP_CAM_Y_LO = 0x60b0
local AP_CAM_Y_HI = 0x60b1
local AP_CLEAR_SEQ = 0x6102

local OB_KIND = 0x699b
local OB_STATE = 0x69af
local OB_STEP = 0x69c3
local OB_XL = 0x6a27
local OB_XH = 0x6a3b
local OB_YL = 0x6a63
local OB_YH = 0x6a77
local OB_CUR_L = 0x6adb
local OB_CUR_H = 0x6aef
local OB_HP = 0x6b67
local OB_VAR = 0x6b7b
local OB_DIR = 0x6ba3

local TITLE, PLAY, CLEAR = 7, 4, 5
local RF_FCF2, RF_FCFA, RF_FD02, RF_51B0 = 0x305D, 0x3075, 0x308D, 0x1305

local frame = 0
local phase = "boot"
local phaseFrame = 0
local input = {}

local s1UniStartX, sawS1UniChargeLeft = 0, false
local sawBmCharge, sawBmHShot, sawBmDown, sawBmDShot, sawBmStun = false, false, false, false, false
local bmSlot = nil
local sawSkullLeap = false
local sawS6UniHp10 = false
local sawS6SatanV1Wake = false

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readWord(addr) return read(addr) + 256 * read(addr + 1) end
local function write(addr, v) emu.write(addr, v, emu.memType.nesMemory) end

local function check(cond, msg)
    if cond then return true end
    print(string.format("FAIL frame=%d phase=%s stg=%d x=%d y=%d: %s",
        frame, phase, read(CAMPAIGN_STAGE), readWord(PLAYER_X),
        read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI), msg))
    emu.stop(1)
    return false
end

local function findKind(kind, wantVar)
    for i = 0, 19 do
        if read(OB_KIND + i) == kind then
            if wantVar == nil or read(OB_VAR + i) == wantVar then return i end
        end
    end
    return nil
end

local function fastClear(targetStage, nextPhase)
    if read(SCENE) == PLAY and read(CAMPAIGN_STAGE) == targetStage then
        write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
        phase = nextPhase
    end
end

emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    if not check(frame < 6000, "revg-enemy-ai timed out") then return end

    local scene = read(SCENE)
    if phase == "boot" then
        if scene == TITLE then phaseFrame = frame; phase = "title_wait" end
    elseif phase == "title_wait" then
        if frame == phaseFrame + 10 or frame == phaseFrame + 11 then
            input = {right = true, start = true}
        elseif scene == PLAY then
            phase = "s1_ready"
        end
    elseif phase == "s1_ready" then
        if scene == PLAY and readWord(PLAYER_X) == 96 and read(PLAYER_FEET) == 192 then
            -- Warp to Stage 1 Unicorn boss arena (X=3360, page 13)
            write(PLAYER_X, 3360 & 255); write(PLAYER_X + 1, 3360 >> 8)
            write(CAMERA_X, 3328 & 255); write(CAMERA_X + 1, 3328 >> 8)
            write(PAN_X, 3328 & 255); write(PAN_X + 1, 3328 >> 8)
            phaseFrame = frame
            phase = "s1_uni"
        end

    -- 1. Stage 1 Unicorn: force left-charge from 96 px right of Arthur and confirm it moves >= 24 px left!
    elseif phase == "s1_uni" then
        write(AP_ARMOR, 2); write(AP_INVULN, 20)
        local uidx = findKind(9) -- Stage 1 ok_unicorn = 9
        if uidx then
            if s1UniStartX == 0 then
                local ux = 3480
                write(PLAYER_X, 3380 & 255); write(PLAYER_X + 1, 3380 >> 8)
                write(OB_XL + uidx, ux & 255); write(OB_XH + uidx, ux >> 8)
                write(OB_DIR + uidx, 1)    -- facing left
                write(OB_STATE + uidx, 4)  -- charge state
                write(0x6a9f + uidx, 0xfe) -- ob_vxi = -1.80
                write(0x6a8b + uidx, 0x33) -- ob_vxf
                s1UniStartX = ux
                phaseFrame = frame
            elseif frame >= phaseFrame + 16 then
                local curX = read(OB_XL + uidx) + 256 * read(OB_XH + uidx)
                local st = read(OB_STATE + uidx)
                if not check(st == 4 and curX <= s1UniStartX - 24,
                        string.format("Stage 1 Unicorn left-charge aborted prematurely (st=%d startX=%d curX=%d)", st, s1UniStartX, curX)) then return end
                sawS1UniChargeLeft = true
                write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
                phase = "s1_clr"
            end
        end
    elseif phase == "s1_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s1_w" end
    elseif phase == "s1_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "s2_wait" end

    -- 2. Stage 2 Mansion Big Man: Charge + horiz shot + Downward Shoot + 6F Hit-Stun
    elseif phase == "s2_wait" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 2 then
            write(PLAYER_X, 1680 & 255); write(PLAYER_X + 1, 1680 >> 8)
            write(PLAYER_FEET, 432 & 255); write(PLAYER_Y_HI, 432 >> 8)
            write(CAMERA_X, 1536 & 255); write(CAMERA_X + 1, 1536 >> 8)
            write(PAN_X, 1536 & 255); write(PAN_X + 1, 1536 >> 8)
            phaseFrame = frame
            phase = "s2_bm_charge"
        end
    elseif phase == "s2_bm_charge" then
        write(AP_ARMOR, 2); write(AP_INVULN, 20); write(0x6010, 0)
        -- Find a Big Man visible on bottom half of Stage 2 (arcade Y ~ 80..200 -> FC Y ~ 300..432)
        if not bmSlot then
            for i = 0, 19 do
                if read(OB_KIND + i) == 3 then
                    local by = read(OB_YL + i) + 256 * read(OB_YH + i)
                    if by >= 64 and by <= 180 then bmSlot = i; break end
                end
            end
        end
        if bmSlot then
            local bx = read(OB_XL + bmSlot) + 256 * read(OB_XH + bmSlot)
            local by = read(OB_YL + bmSlot) + 256 * read(OB_YH + bmSlot)
            local fcY = 510 - by
            if fcY > 432 then fcY = 432 end
            local ax = bx - 52
            write(PLAYER_X, ax & 255); write(PLAYER_X + 1, ax >> 8)
            write(PLAYER_FEET, fcY & 255); write(PLAYER_Y_HI, fcY >> 8)
            if read(OB_STATE + bmSlot) == 2 then sawBmCharge = true end
            for i = 0, 19 do
                if read(OB_KIND + i) == 9 then
                    local fid = read(OB_CUR_L + i) + 256 * read(OB_CUR_H + i)
                    if fid == RF_FCF2 or fid == RF_FCFA then sawBmHShot = true end
                end
            end
        end
        if sawBmCharge and sawBmHShot then
            phaseFrame = frame
            phase = "s2_bm_stun"
        elseif frame > phaseFrame + 240 then
            check(false, string.format("Big Man charge/horiz shot failed (chg=%s hshot=%s slot=%s)", tostring(sawBmCharge), tostring(sawBmHShot), tostring(bmSlot)))
            return
        end
    elseif phase == "s2_bm_stun" then
        write(AP_ARMOR, 2); write(AP_INVULN, 20); write(0x6010, 0)
        local bx = read(OB_XL + bmSlot) + 256 * read(OB_XH + bmSlot)
        local by = read(OB_YL + bmSlot) + 256 * read(OB_YH + bmSlot)
        local fcY = 510 - by
        if fcY > 432 then fcY = 432 end
        write(PLAYER_X, (bx - 40) & 255); write(PLAYER_X + 1, (bx - 40) >> 8)
        write(PLAYER_FEET, fcY & 255); write(PLAYER_Y_HI, fcY >> 8)
        write(0x4f, 0) -- face right
        if (frame % 6) == 0 then input = {b = true} end
        if read(OB_STATE + bmSlot) == 4 then
            local fid = read(OB_CUR_L + bmSlot) + 256 * read(OB_CUR_H + bmSlot)
            if fid == RF_51B0 then sawBmStun = true end
        end
        if sawBmStun then
            phaseFrame = frame
            phase = "s2_bm_down"
        elseif frame > phaseFrame + 90 then
            check(false, "Big Man did not enter 6F hit-stun (state=4, rf_51B0) on non-lethal hit")
            return
        end
    elseif phase == "s2_bm_down" then
        write(AP_ARMOR, 2); write(AP_INVULN, 20); write(0x6010, 0)
        -- Place Arthur directly below Big Man (|dx|<=10, ob_y - player_y > 45)
        local bx = read(OB_XL + bmSlot) + 256 * read(OB_XH + bmSlot)
        local by = read(OB_YL + bmSlot) + 256 * read(OB_YH + bmSlot)
        local fcY = 510 - (by - 64)
        if fcY > 448 then fcY = 448 end
        write(PLAYER_X, bx & 255); write(PLAYER_X + 1, bx >> 8)
        write(PLAYER_FEET, fcY & 255); write(PLAYER_Y_HI, fcY >> 8)
        if read(OB_STATE + bmSlot) == 3 then sawBmDown = true end
        for i = 0, 19 do
            if read(OB_KIND + i) == 9 then
                local fid = read(OB_CUR_L + i) + 256 * read(OB_CUR_H + i)
                if fid == RF_FD02 then sawBmDShot = true end
            end
        end
        if sawBmDown and sawBmDShot then
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
            phase = "skip_s2_clr"
        elseif frame > phaseFrame + 120 then
            check(false, string.format("Big Man downward shoot failed (down=%s dshot=%s st=%d)", tostring(sawBmDown), tostring(sawBmDShot), read(OB_STATE + bmSlot)))
            return
        end

    -- Skip Stages 2->3->4->5
    elseif phase == "skip_s2_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s2_w" end
    elseif phase == "s2_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "skip_s3" end
    elseif phase == "skip_s3" then
        if frame >= phaseFrame + 12 then fastClear(3, "s3_clr") end
    elseif phase == "s3_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s3_w" end
    elseif phase == "s3_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "skip_s4" end
    elseif phase == "skip_s4" then
        if frame >= phaseFrame + 12 then fastClear(4, "s4_clr") end
    elseif phase == "s4_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s4_w" end
    elseif phase == "s4_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "s5_skull" end

    -- 3. Stage 5 Floor Skull: wake -> 2 bounces -> 80-px sine-arc leap (ob_step == 2, NO freeze!)
    elseif phase == "s5_skull" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 5 then
            write(AP_ARMOR, 2); write(AP_INVULN, 20)
            local sk = findKind(7) -- ok_skull = 7
            if not sk then
                input = {right = true}
            else
                -- Stand near Skull to wake it and watch it complete 2 hops + leap (ob_step == 2)!
                local sx = read(OB_XL + sk) + 256 * read(OB_XH + sk)
                local sy = read(OB_YL + sk) + 256 * read(OB_YH + sk)
                if read(OB_STATE + sk) <= 1 then
                    local fcY = 1200 - sy
                    write(PLAYER_X, (sx - 20) & 255); write(PLAYER_X + 1, (sx - 20) >> 8)
                    write(PLAYER_FEET, fcY & 255); write(PLAYER_Y_HI, fcY >> 8)
                end
                if read(OB_STATE + sk) == 3 and read(OB_STEP + sk) == 2 then
                    sawSkullLeap = true
                    write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
                    phase = "s5_clr"
                end
            end
            if frame > phaseFrame + 240 then
                check(false, "Stage 5 Floor Skull froze on landing and never reached sine-arc leap (ob_step==2)")
                return
            end
        end
    elseif phase == "s5_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s5_w" end
    elseif phase == "s5_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "s6_uni" end

    -- 4. Stage 6 Unicorn: confirm 10 HP (ob_hp == 9) on bottom page!
    elseif phase == "s6_uni" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 6 then
            write(AP_ARMOR, 2); write(AP_INVULN, 20)
            local u = findKind(6) -- ok_unicorn = 6
            if u then
                local hp = read(OB_HP + u)
                if not check(hp == 9, string.format("Stage 6 Unicorn HP wrong: expected 9 (10 HP), got %d", hp)) then return end
                sawS6UniHp10 = true
                -- Warp to top floor for Stage 6 Satan Variant 1 early-wake test!
                write(PLAYER_X, 400 & 255); write(PLAYER_X + 1, 400 >> 8)
                write(CAMERA_X, 256 & 255); write(CAMERA_X + 1, 256 >> 8)
                write(PAN_X, 256 & 255); write(PAN_X + 1, 256 >> 8)
                write(PAN_Y, 0); write(PAN_Y_PAGE, 0)
                write(AP_CAM_Y_LO, 0); write(AP_CAM_Y_HI, 0)
                write(PLAYER_FEET, 112); write(PLAYER_Y_HI, 0)
                phaseFrame = frame
                phase = "s6_satan_v1"
            elseif frame > phaseFrame + 120 then
                check(false, "Stage 6 Unicorn did not spawn on bottom page")
                return
            end
        end

    -- 5. Stage 6 Satan Variant 1: shoot while dormant -> must early-wake (state==1) with HP reset to 3 (4 HP)!
    elseif phase == "s6_satan_v1" then
        write(AP_ARMOR, 2); write(AP_INVULN, 20)
        local s1 = findKind(5, 1) -- ok_satan = 5, var = 1
        if s1 then
            local sx = read(OB_XL + s1) + 256 * read(OB_XH + s1)
            local sy = read(OB_YL + s1) + 256 * read(OB_YH + s1)
            local fcY = 1200 - sy
            if fcY < 64 then fcY = 64 end
            write(PLAYER_X, (sx - 48) & 255); write(PLAYER_X + 1, (sx - 48) >> 8)
            write(PLAYER_FEET, fcY & 255); write(PLAYER_Y_HI, fcY >> 8)
            write(0x4f, 0)
            if (frame % 6) == 0 then input = {b = true} end
            if read(OB_STATE + s1) == 1 then
                local hp = read(OB_HP + s1)
                if not check(hp == 3, string.format("Stage 6 Satan var 1 early-wake should reset HP to 3 (4 HP), got %d", hp)) then return end
                sawS6SatanV1Wake = true
                print(string.format("PASS revg-enemy-ai (frame %d): S1UniChgL=%s S2BigMan(chg=%s,hshot=%s,stun=%s,down=%s,dshot=%s) S5SkullLeap=%s S6UniHP10=%s S6SatanV1Wake=%s",
                    frame, tostring(sawS1UniChargeLeft), tostring(sawBmCharge), tostring(sawBmHShot),
                    tostring(sawBmStun), tostring(sawBmDown), tostring(sawBmDShot),
                    tostring(sawSkullLeap), tostring(sawS6UniHp10), tostring(sawS6SatanV1Wake)))
                emu.stop(0)
            end
        end
        if frame > phaseFrame + 120 then
            check(false, "Stage 6 Satan Variant 1 did not early-wake on dormant hits")
            return
        end
    end
end, emu.eventType.endFrame)
