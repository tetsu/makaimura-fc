-- Stage 5 (Section 4: 5-page Castle Tower -> Satan), Stage 6 (Section 5: 5-page Inner Castle
-- -> 2x Satan + Cross weapon gate), Stage 7 (Section 6: Throne -> Astaroth), Loop 2, and
-- True Ending (FINAL CLEAR) end-to-end verification in Mesen 2.1.1.
local SCENE = 0x07
local PRG_BANK0 = 0x0d
local CAMPAIGN_STAGE = 0x14
local CAMERA_X = 0x25
local PAN_Y = 0x09
local PAN_X = 0x30
local PAN_Y_PAGE = 0x32
local PLAYER_X = 0x4c
local PLAYER_FEET = 0x4e
local AP_ARMOR = 0x6011
local AP_WEAPON = 0x601a
local KEY_COLLECTED = 0x60ab
local PLAYER_Y_HI = 0x60ac
local AP_CAM_Y_LO = 0x60b0
local AP_CAM_Y_HI = 0x60b1
local AP_LOOP = 0x60ce
local AP_BOUNCE_BACK = 0x60cf
local AP_CLEAR_SEQ = 0x6102
local REVG_BG_ACTIVE = 0x6118
local OB_KIND = 0x699b
local OB_XL = 0x6a27
local OB_XH = 0x6a3b
local OB_YL = 0x6a63
local OB_YH = 0x6a77
local OB_HP = 0x6b67
local OB_VAR = 0x6b7b
local OB_FLAGS = 0x6b8f

local TITLE, PLAY, CLEAR = 7, 4, 5
local OK_SATAN, OK_SKULL, OK_ASTAROTH, OK_KEY = 5, 7, 10, 17
local WPN_LANCE, WPN_CROSS = 0, 3

local frame = 0
local phase = "boot"
local input = {}
local phaseFrame = 0
local sawS5Skull, sawS5Satan, sawS5Key = false, false, false
local sawS6Bounce, sawS6TwoSatans, sawS6Key = false, false, false
local sawS7Astaroth, sawS7LanceBlocked, sawS7Loop2, sawS7FinalClear = false, false, false, false
local astarothHpBefore = 0

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readWord(addr) return read(addr) + 256 * read(addr + 1) end
local function write(addr, v) emu.write(addr, v, emu.memType.nesMemory) end

local function countKind(kind)
    local n, firstIdx = 0, nil
    for i = 0, 19 do
        if read(OB_KIND + i) == kind then
            n = n + 1
            if not firstIdx then firstIdx = i end
        end
    end
    return n, firstIdx
end

local function countArcadeBgTiles(ntBase)
    local n = 0
    for off = 128, 959 do
        if emu.read(ntBase + off, emu.memType.nesNametableRam) >= 0x25 then
            n = n + 1
        end
    end
    return n
end

local function check(cond, msg)
    if cond then return true end
    local kinds = {}
    for i = 0, 19 do
        local k = read(OB_KIND + i)
        if k ~= 0 then kinds[#kinds + 1] = string.format("%d:%d(v%d,hp%d,f%02x)", i, k, read(OB_VAR + i), read(OB_HP + i), read(OB_FLAGS + i)) end
    end
    print(string.format("FAIL frame=%d phase=%s stg=%d loop=%d bnc=%d wpn=%d bank0=%d bg=%d x=%d y=%d yp=%d camx=%d camy=%d 2sat=%s key=%s clr=%d objs=[%s]: %s",
        frame, phase, read(CAMPAIGN_STAGE), read(AP_LOOP), read(AP_BOUNCE_BACK), read(AP_WEAPON),
        read(PRG_BANK0), read(REVG_BG_ACTIVE), readWord(PLAYER_X),
        read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI), read(PAN_Y_PAGE),
        readWord(CAMERA_X), read(AP_CAM_Y_LO) + 256 * read(AP_CAM_Y_HI),
        tostring(sawS6TwoSatans), tostring(sawS6Key), read(AP_CLEAR_SEQ),
        table.concat(kinds, ","), msg))
    emu.stop(1)
    return false
end

local function fastClearStage(targetStage, nextPhase)
    if read(SCENE) == PLAY and read(CAMPAIGN_STAGE) == targetStage then
        write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
        phase = nextPhase
    end
end

emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    if not check(frame < 9000, "Stage 5-7 test timed out") then return end

    local scene = read(SCENE)
    if phase == "boot" then
        if scene == TITLE then phaseFrame = frame; phase = "title_wait" end
    elseif phase == "title_wait" then
        if frame == phaseFrame + 10 or frame == phaseFrame + 11 then
            input = {right = true, start = true}
        elseif scene == PLAY then
            phase = "skip_s1"
        end
    -- Skip Stages 1..4 to reach Stage 5
    elseif phase == "skip_s1" then
        if scene == PLAY and read(CAMPAIGN_STAGE) == 1 and readWord(PLAYER_X) == 96 then
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0); phase = "s1_clr"
        end
    elseif phase == "s1_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s1_w" end
    elseif phase == "s1_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "skip_s2" end
    elseif phase == "skip_s2" then
        if frame >= phaseFrame + 12 then fastClearStage(2, "s2_clr") end
    elseif phase == "s2_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s2_w" end
    elseif phase == "s2_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "skip_s3" end
    elseif phase == "skip_s3" then
        if frame >= phaseFrame + 12 then fastClearStage(3, "s3_clr") end
    elseif phase == "s3_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s3_w" end
    elseif phase == "s3_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "skip_s4" end
    elseif phase == "skip_s4" then
        if frame >= phaseFrame + 12 then fastClearStage(4, "s4_clr") end
    elseif phase == "s4_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s4_w" end
    elseif phase == "s4_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "s5_init" end

    -- === STAGE 5 (Section 4: 5-page Castle Tower -> Satan) ===
    elseif phase == "s5_init" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 5 then
            local y = read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI)
            if not check(read(PRG_BANK0) == 16, "Stage 5 must map bank 16") then return end
            if not check(read(PAN_Y_PAGE) == 4, "Stage 5 must start on bottom vertical page 4") then return end
            if not check(read(REVG_BG_ACTIVE) == 6, "Stage 5 must use revg_bg_active=6") then return end
            if not check(readWord(PLAYER_X) == 80 and y == 1136, "Stage 5 must start at (80, 1136)") then return end
            if not check(countArcadeBgTiles(0x000) > 100, "Stage 5 nametable must have arcade castle tiles") then return end
            phaseFrame = frame
            phase = "s5_skull"
        end
    elseif phase == "s5_skull" then
        write(AP_ARMOR, 2)
        -- Move right to spawn Floor Skull at lx=176, ly=136 (bottom page)
        input = {right = true}
        local nsk = countKind(OK_SKULL)
        if nsk > 0 then sawS5Skull = true end
        if sawS5Skull or frame > phaseFrame + 120 then
            if not check(sawS5Skull, "Stage 5 must spawn Floor Skull ($09)") then return end
            -- Warp to top floor (pan_y_page=0, X=560, Y=112) to fight Satan at (672, ly=1152)
            write(PLAYER_X, 560 % 256); write(PLAYER_X + 1, math.floor(560 / 256))
            write(CAMERA_X, 512 % 256); write(CAMERA_X + 1, math.floor(512 / 256))
            write(PAN_X, 512 % 256); write(PAN_X + 1, math.floor(512 / 256))
            write(PAN_Y, 0); write(PAN_Y_PAGE, 0)
            write(AP_CAM_Y_LO, 0); write(AP_CAM_Y_HI, 0)
            write(PLAYER_FEET, 112); write(PLAYER_Y_HI, 0)
            phaseFrame = frame
            phase = "s5_satan"
        end
    elseif phase == "s5_satan" then
        write(AP_ARMOR, 2)
        local nsat, sidx = countKind(OK_SATAN)
        if nsat > 0 then
            sawS5Satan = true
            -- Position Arthur to shoot Satan (once active and unshielded)
            local sx = read(OB_XL + sidx) + 256 * read(OB_XH + sidx)
            local sy = read(OB_YL + sidx) + 256 * read(OB_YH + sidx)
            local fcY = 1200 - sy
            if fcY < 64 then fcY = 64 end
            if fcY > 200 then fcY = 200 end
            local ax = math.max(528, sx - 64)
            write(PLAYER_X, ax % 256); write(PLAYER_X + 1, math.floor(ax / 256))
            write(PLAYER_FEET, fcY % 256); write(PLAYER_Y_HI, math.floor(fcY / 256))
            if (frame % 6) == 0 then input = {b = true} end
        end
        local nk, kidx = countKind(OK_KEY)
        if nk > 0 then
            sawS5Key = true
            local kx = read(OB_XL + kidx) + 256 * read(OB_XH + kidx)
            local ky = read(OB_YL + kidx) + 256 * read(OB_YH + kidx)
            local fcY = 1200 - ky
            write(PLAYER_X, kx % 256); write(PLAYER_X + 1, math.floor(kx / 256))
            write(PLAYER_FEET, fcY % 256); write(PLAYER_Y_HI, math.floor(fcY / 256))
            phaseFrame = frame
            phase = "s5_clear"
        end
        if not check(frame < phaseFrame + 480, "Stage 5 Satan fight timed out") then return end
    elseif phase == "s5_clear" then
        if scene == CLEAR then
            if not check(sawS5Satan and sawS5Key, "Stage 5 must defeat Satan and collect Key") then return end
            phaseFrame = frame
            phase = "s5_clear_w"
        end
    elseif phase == "s5_clear_w" then
        if frame == phaseFrame + 15 then input = {start = true}; phaseFrame = frame; phase = "s6_init" end

    -- === STAGE 6 (Section 5: 5-page Inner Castle -> 2x Satan + Cross Gate) ===
    elseif phase == "s6_init" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 6 then
            local y = read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI)
            if not check(read(PRG_BANK0) == 17, "Stage 6 must map bank 17") then return end
            if not check(read(PAN_Y_PAGE) == 4, "Stage 6 must start on bottom vertical page 4") then return end
            if not check(read(REVG_BG_ACTIVE) == 7, "Stage 6 must use revg_bg_active=7") then return end
            if not check(readWord(PLAYER_X) == 48 and y == 1120, "Stage 6 must start at (48, 1120)") then return end
            if not check(countArcadeBgTiles(0x000) > 100, "Stage 6 nametable must have arcade castle tiles") then return end
            -- Test 1: Clear Stage 6 WITHOUT Cross (weapon = Lance) -> must bounce back to Stage 5!
            write(AP_WEAPON, WPN_LANCE)
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
            phaseFrame = frame
            phase = "s6_bounce_clr"
        end
    elseif phase == "s6_bounce_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s6_bounce_w" end
    elseif phase == "s6_bounce_w" then
        if frame == phaseFrame + 15 then input = {start = true}; phaseFrame = frame; phase = "s6_bounce_chk" end
    elseif phase == "s6_bounce_chk" then
        if frame >= phaseFrame + 15 and scene == PLAY then
            if not check(read(CAMPAIGN_STAGE) == 5 and read(AP_BOUNCE_BACK) == 1,
                "Stage 6 without Cross must bounce back to Stage 5") then return end
            sawS6Bounce = true
            -- Re-clear Stage 5 to enter Stage 6 again WITH Cross!
            write(AP_WEAPON, WPN_CROSS)
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
            phase = "s5_reclr"
        end
    elseif phase == "s5_reclr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s5_reclr_w" end
    elseif phase == "s5_reclr_w" then
        if frame == phaseFrame + 15 then input = {start = true}; phaseFrame = frame; phase = "s6_satans_init" end
    elseif phase == "s6_satans_init" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 6 then
            write(AP_WEAPON, WPN_CROSS)
            -- Warp to top floor (pan_y_page=0, X=300, Y=112) to fight 2x Satan!
            write(PLAYER_X, 300 % 256); write(PLAYER_X + 1, math.floor(300 / 256))
            write(CAMERA_X, 256 % 256); write(CAMERA_X + 1, math.floor(256 / 256))
            write(PAN_X, 256 % 256); write(PAN_X + 1, math.floor(256 / 256))
            write(PAN_Y, 0); write(PAN_Y_PAGE, 0)
            write(AP_CAM_Y_LO, 0); write(AP_CAM_Y_HI, 0)
            write(PLAYER_FEET, 112); write(PLAYER_Y_HI, 0)
            phaseFrame = frame
            phase = "s6_satans"
        end
    elseif phase == "s6_satans" then
        write(AP_ARMOR, 2)
        local nsat, sidx = countKind(OK_SATAN)
        if nsat >= 2 then sawS6TwoSatans = true end
        if nsat > 0 then
            local sx = read(OB_XL + sidx) + 256 * read(OB_XH + sidx)
            local sy = read(OB_YL + sidx) + 256 * read(OB_YH + sidx)
            local fcY = 1200 - sy
            if fcY < 64 then fcY = 64 end
            if fcY > 200 then fcY = 200 end
            local ax = math.max(48, sx - 56)
            write(PLAYER_X, ax % 256); write(PLAYER_X + 1, math.floor(ax / 256))
            write(PLAYER_FEET, fcY % 256); write(PLAYER_Y_HI, math.floor(fcY / 256))
            if (frame % 6) == 0 then input = {b = true} end
        end
        local nk, kidx = countKind(OK_KEY)
        if nk > 0 then
            sawS6Key = true
            local kx = read(OB_XL + kidx) + 256 * read(OB_XH + kidx)
            local ky = read(OB_YL + kidx) + 256 * read(OB_YH + kidx)
            local fcY = 1200 - ky
            write(PLAYER_X, kx % 256); write(PLAYER_X + 1, math.floor(kx / 256))
            write(PLAYER_FEET, fcY % 256); write(PLAYER_Y_HI, math.floor(fcY / 256))
            phaseFrame = frame
            phase = "s6_clear"
        end
        if not check(frame < phaseFrame + 700, "Stage 6 2x Satan fight timed out") then return end
    elseif phase == "s6_clear" then
        if scene == CLEAR then
            if not check(sawS6TwoSatans and sawS6Key, "Stage 6 must defeat 2x Satan and collect Key") then return end
            phaseFrame = frame
            phase = "s6_clear_w"
        end
    elseif phase == "s6_clear_w" then
        if frame == phaseFrame + 15 then input = {start = true}; phaseFrame = frame; phase = "s7_init" end

    -- === STAGE 7 (Section 6: Throne -> Astaroth, Loop 1 -> Loop 2 -> Final Clear) ===
    elseif phase == "s7_init" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 7 then
            local y = read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI)
            if not check(read(PRG_BANK0) == 18, "Stage 7 must map bank 18") then return end
            if not check(read(PAN_Y_PAGE) == 0, "Stage 7 must be 1 vertical page") then return end
            if not check(read(REVG_BG_ACTIVE) == 3, "Stage 7 must use revg_bg_active=3") then return end
            if not check(countArcadeBgTiles(0x000) > 100, "Stage 7 nametable must have throne BG tiles") then return end
            -- First verify Lance CANNOT damage Astaroth
            write(AP_WEAPON, WPN_LANCE)
            phaseFrame = frame
            phase = "s7_lance_test"
        end
    elseif phase == "s7_lance_test" then
        write(AP_ARMOR, 2)
        local nas, aidx = countKind(OK_ASTAROTH)
        if nas > 0 then
            sawS7Astaroth = true
            local ax = read(OB_XL + aidx) - 48
            write(PLAYER_X, ax); write(PLAYER_X + 1, 0)
            write(PLAYER_FEET, 176); write(PLAYER_Y_HI, 0)
            if frame == phaseFrame + 5 then astarothHpBefore = read(OB_HP + aidx) end
            if (frame % 8) == 0 then input = {b = true} end
            if frame == phaseFrame + 60 then
                if not check(read(OB_HP + aidx) == astarothHpBefore, "Lance must NOT damage Astaroth") then return end
                sawS7LanceBlocked = true
                write(AP_WEAPON, WPN_CROSS)
                phaseFrame = frame
                phase = "s7_cross_kill"
            end
        end
    elseif phase == "s7_cross_kill" then
        write(AP_ARMOR, 2)
        local nas, aidx = countKind(OK_ASTAROTH)
        if nas > 0 then
            local ax = read(OB_XL + aidx) - 44
            write(PLAYER_X, ax); write(PLAYER_X + 1, 0)
            write(PLAYER_FEET, 176); write(PLAYER_Y_HI, 0)
            if (frame % 6) == 0 then input = {b = true} end
        end
        if scene == CLEAR then
            phaseFrame = frame
            phase = "s7_loop1_w"
        end
        if not check(frame < phaseFrame + 500, "Stage 7 Astaroth Cross kill timed out") then return end
    elseif phase == "s7_loop1_w" then
        if frame == phaseFrame + 15 then input = {start = true}; phaseFrame = frame; phase = "loop2_chk" end
    elseif phase == "loop2_chk" then
        if frame >= phaseFrame + 15 and scene == PLAY then
            if not check(read(AP_LOOP) == 1 and read(CAMPAIGN_STAGE) == 1,
                "Stage 7 Loop 1 clear must start Loop 2 at Stage 1") then return end
            sawS7Loop2 = true
            -- Verify Loop 2 Stage 7 FINAL CLEAR -> Title screen
            write(CAMPAIGN_STAGE, 7)
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
            phaseFrame = frame
            phase = "loop2_final"
        end
    elseif phase == "loop2_final" then
        if scene == CLEAR then
            phaseFrame = frame
            phase = "loop2_final_w"
        end
    elseif phase == "loop2_final_w" then
        if frame == phaseFrame + 15 then input = {start = true}; phaseFrame = frame; phase = "final_title" end
    elseif phase == "final_title" then
        if scene == 15 and frame >= phaseFrame + 25 then
            input = {start = true}
            phaseFrame = frame
        elseif scene == TITLE then
            sawS7FinalClear = true
            print(string.format("PASS frame=%d s5(skull=%s,satan=%s,key=%s) s6(bounce=%s,2satan=%s,key=%s) s7(ast=%s,lanceBlk=%s,loop2=%s,final=%s)",
                frame, tostring(sawS5Skull), tostring(sawS5Satan), tostring(sawS5Key),
                tostring(sawS6Bounce), tostring(sawS6TwoSatans), tostring(sawS6Key),
                tostring(sawS7Astaroth), tostring(sawS7LanceBlocked), tostring(sawS7Loop2), tostring(sawS7FinalClear)))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
