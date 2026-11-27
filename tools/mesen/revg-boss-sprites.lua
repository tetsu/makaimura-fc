-- Regression test for boss sprite visibility, 32-piece Astaroth rendering,
-- per-object CHR-RAM fallback (ob_last_*), alternating piece OAM order,
-- and NTSC VBlank safety under 4-tile/frame unrolled sprite streaming.
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
local AP_WEAPON = 0x601a
local KEY_COLLECTED = 0x60ab
local PLAYER_Y_HI = 0x60ac
local AP_CAM_Y_LO = 0x60b0
local AP_CAM_Y_HI = 0x60b1
local AP_CLEAR_SEQ = 0x6102

local OB_KIND = 0x699b
local OB_XL = 0x6a27
local OB_XH = 0x6a3b
local OB_YL = 0x6a63
local OB_YH = 0x6a77
local OB_CUR_L = 0x6adb
local OB_CUR_H = 0x6aef

local TITLE, PLAY, CLEAR = 7, 4, 5
local OK_S1_UNICORN = 7
local OK_S5_SATAN = 5
local OK_S7_ASTAROTH = 10
local WPN_CROSS = 3

local frame = 0
local phase = "boot"
local phaseFrame = 0
local input = {}

local ppuMask = 0
local vblankOverruns = 0
local worstOverrunSl = -1
local maxVblankSl = -1

local uniFrames, uniMinOam, uniFlips = 0, 99, 0
local satFrames, satMinOam = 0, 99
local astFrames, astMinOam, astMinBg, astMaxPerLine = 0, 99, 99, 0
local prevFirstX = nil

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readWord(addr) return read(addr) + 256 * read(addr + 1) end
local function write(addr, v) emu.write(addr, v, emu.memType.nesMemory) end

-- Track PPU mask ($2001) and check every $2007 write during active rendering
emu.addMemoryCallback(function(addr, val)
    ppuMask = val
end, emu.callbackType.write, 0x2001)

emu.addMemoryCallback(function(addr, val)
    if (ppuMask & 0x18) ~= 0 then
        local st = emu.getState()
        local sl = st["ppu.scanline"]
        if sl ~= nil then
            if sl >= 241 and sl <= 261 then
                if sl > maxVblankSl then maxVblankSl = sl end
            elseif sl >= 0 and sl <= 239 then
                vblankOverruns = vblankOverruns + 1
                if worstOverrunSl < 0 then worstOverrunSl = sl end
            end
        end
    end
end, emu.callbackType.write, 0x2007)

local function check(cond, msg)
    if cond then return true end
    print(string.format("FAIL frame=%d phase=%s stg=%d x=%d overruns=%d(sl=%d): %s",
        frame, phase, read(CAMPAIGN_STAGE), readWord(PLAYER_X),
        vblankOverruns, worstOverrunSl, msg))
    emu.stop(1)
    return false
end

local function findKind(kind)
    for i = 0, 19 do
        if read(OB_KIND + i) == kind then return i end
    end
    return nil
end

-- Count visible OAM sprites near boss screen X (radius px) and return (count, firstX).
local function countBossOam(bossIdx, radius)
    local bx = read(OB_XL + bossIdx) + 256 * read(OB_XH + bossIdx)
    local camX = readWord(CAMERA_X)
    local sx = bx - camX
    local count = 0
    local firstX = nil
    for s = 0, 63 do
        local sy = read(0x200 + s * 4)
        local ox = read(0x200 + s * 4 + 3)
        if sy < 239 and sy >= 23 and math.abs(ox - sx) <= radius then
            count = count + 1
            if firstX == nil then firstX = ox end
        end
    end
    return count, firstX, sx
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
    if not check(frame < 7500, "revg-boss-sprites timed out") then return end

    local scene = read(SCENE)
    if phase == "boot" then
        if scene == TITLE then phaseFrame = frame; phase = "title_wait" end
    elseif phase == "title_wait" then
        if frame == phaseFrame + 10 or frame == phaseFrame + 11 then
            input = {right = true, start = true}
        elseif scene == PLAY then
            phase = "s1_ready"
        end

    -- 1. Stage 1 Unicorn (16-18 pieces)
    elseif phase == "s1_ready" then
        if scene == PLAY and readWord(PLAYER_X) == 96 and read(PLAYER_FEET) == 192 then
            write(PLAYER_X, 3420 & 255); write(PLAYER_X + 1, 3420 >> 8)
            write(CAMERA_X, 3328 & 255); write(CAMERA_X + 1, 3328 >> 8)
            write(PAN_X, 3328 & 255); write(PAN_X + 1, 3328 >> 8)
            phaseFrame = frame
            phase = "s1_uni_wait"
        end
    elseif phase == "s1_uni_wait" then
        write(AP_ARMOR, 2); write(AP_INVULN, 0)
        write(PLAYER_X, 3408 & 255); write(PLAYER_X + 1, 3408 >> 8)
        local uidx = findKind(OK_S1_UNICORN)
        local ucur = uidx and (read(OB_CUR_L + uidx) + 256 * read(OB_CUR_H + uidx)) or 0
        if uidx and ucur ~= 0 then
            local cnt = countBossOam(uidx, 36)
            if cnt >= 15 then
                phaseFrame = frame
                prevFirstX = nil
                phase = "s1_uni_check"
            else
                if not check(frame < phaseFrame + 40,
                    string.format("Stage 1 Unicorn failed to stream in within 40 frames (oam=%d)", cnt)) then return end
            end
        else
            phaseFrame = frame
        end
    elseif phase == "s1_uni_check" then
        write(AP_ARMOR, 2); write(AP_INVULN, 0)
        -- Keep Arthur on left side of boss arena so camera stays locked & Unicorn stays visible
        write(PLAYER_X, 3408 & 255); write(PLAYER_X + 1, 3408 >> 8)
        local uidx = findKind(OK_S1_UNICORN)
        if not check(uidx ~= nil, "Stage 1 Unicorn vanished during visibility check") then return end
        local cnt, fx, sx = countBossOam(uidx, 36)
        if cnt < uniMinOam then uniMinOam = cnt end
        if prevFirstX ~= nil and fx ~= nil and math.abs(fx - prevFirstX) >= 24 then
            uniFlips = uniFlips + 1
        end
        prevFirstX = fx
        uniFrames = uniFrames + 1
        if not check(cnt >= 15, string.format(
            "Stage 1 Unicorn dropped pieces (oam=%d, sx=%d, obsFrame=%d)", cnt, sx, uniFrames)) then return end
        if uniFrames >= 60 then
            if not check(uniFlips >= 20, string.format(
                "Stage 1 Unicorn piece order did not alternate (flips=%d)", uniFlips)) then return end
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
            phase = "s1_clr"
        end

    -- Skip Stages 2, 3, 4 -> Stage 5
    elseif phase == "s1_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s1_w" end
    elseif phase == "s1_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "skip_s2" end
    elseif phase == "skip_s2" then
        if frame >= phaseFrame + 12 then fastClear(2, "s2_clr") end
    elseif phase == "s2_clr" then
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
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "s5_init" end

    -- 2. Stage 5 Satan (13-18 pieces, including dur=5 fast-animating frames)
    elseif phase == "s5_init" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 5 then
            write(PLAYER_X, 624 & 255); write(PLAYER_X + 1, 624 >> 8)
            write(CAMERA_X, 512 & 255); write(CAMERA_X + 1, 512 >> 8)
            write(PAN_X, 512 & 255); write(PAN_X + 1, 512 >> 8)
            write(PAN_Y, 0); write(PAN_Y_PAGE, 0)
            write(AP_CAM_Y_LO, 0); write(AP_CAM_Y_HI, 0)
            write(PLAYER_FEET, 112); write(PLAYER_Y_HI, 0)
            phaseFrame = frame
            phase = "s5_sat_wait"
        end
    elseif phase == "s5_sat_wait" then
        write(AP_ARMOR, 2); write(AP_INVULN, 0)
        write(PLAYER_X, 624 & 255); write(PLAYER_X + 1, 624 >> 8)
        local sidx = findKind(OK_S5_SATAN)
        local scur = sidx and (read(OB_CUR_L + sidx) + 256 * read(OB_CUR_H + sidx)) or 0
        local sy = sidx and (read(OB_YL + sidx) + 256 * read(OB_YH + sidx)) or 1200
        if sidx and scur ~= 0 then
            local cnt = countBossOam(sidx, 40)
            -- Wait until vertical camera settles at top floor (camy >= 16) and Satan is visible
            if read(AP_CAM_Y_LO) >= 16 and cnt >= 12 then
                phaseFrame = frame
                phase = "s5_sat_check"
            else
                if not check(frame < phaseFrame + 90,
                    string.format("Stage 5 Satan failed to stream in within 90 frames (oam=%d, sy=%d)", cnt, sy)) then return end
            end
        else
            phaseFrame = frame
        end
    elseif phase == "s5_sat_check" then
        write(AP_ARMOR, 2); write(AP_INVULN, 0)
        write(PLAYER_X, 624 & 255); write(PLAYER_X + 1, 624 >> 8)
        local sidx = findKind(OK_S5_SATAN)
        if not check(sidx ~= nil, "Stage 5 Satan vanished during visibility check") then return end
        local cnt, _, sx = countBossOam(sidx, 40)
        if cnt < satMinOam then satMinOam = cnt end
        satFrames = satFrames + 1
        if not check(cnt >= 12, string.format(
            "Stage 5 Satan dropped pieces (oam=%d, sx=%d, obsFrame=%d)", cnt, sx, satFrames)) then return end
        if satFrames >= 60 then
            write(AP_WEAPON, WPN_CROSS)
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
            phase = "s5_clr"
        end

    -- Skip Stage 6 (with Cross so no bounce-back) -> Stage 7 Astaroth
    elseif phase == "s5_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s5_w" end
    elseif phase == "s5_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "skip_s6" end
    elseif phase == "skip_s6" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 6 then
            write(AP_WEAPON, WPN_CROSS)
            fastClear(6, "s6_clr")
        end
    elseif phase == "s6_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s6_w" end
    elseif phase == "s6_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "s7_wait" end

    -- 3. Stage 7 Astaroth (Hybrid BG center 32 tiles $A0..$EB + 16 outer OAM sprites = 4/line!)
    elseif phase == "s7_wait" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 7 then
            write(AP_ARMOR, 2); write(AP_INVULN, 0)
            local aidx = findKind(OK_S7_ASTAROTH)
            if aidx then
                phaseFrame = frame
                phase = "s7_ast_stream"
            end
        end
    elseif phase == "s7_ast_stream" then
        write(AP_ARMOR, 2); write(AP_INVULN, 0)
        write(PLAYER_X, 40); write(PLAYER_X + 1, 0)
        local aidx = findKind(OK_S7_ASTAROTH)
        local cnt = aidx and countBossOam(aidx, 48) or 0
        -- Count Stage 7 Astaroth BG center tiles ($A0..$EF) in nametable rows 16..23 ($200..$2FF)
        local bgCnt = 0
        for off = 0x200, 0x2ff do
            local t = emu.read(off, emu.memType.nesNametableRam)
            if t >= 0xa0 and t <= 0xef then bgCnt = bgCnt + 1 end
        end
        if cnt >= 16 and bgCnt >= 32 then
            phaseFrame = frame
            phase = "s7_ast_check"
        else
            -- Pre-loaded during forced blank -> must appear within 4 frames!
            if not check(frame < phaseFrame + 6,
                string.format("Stage 7 Astaroth failed instant preload (oam=%d, bg=%d)", cnt, bgCnt)) then return end
        end
    elseif phase == "s7_ast_check" then
        write(AP_ARMOR, 2); write(AP_INVULN, 0)
        -- Keep Arthur to the left so he does not overlap Astaroth's OAM X radius
        write(PLAYER_X, 40); write(PLAYER_X + 1, 0)
        local aidx = findKind(OK_S7_ASTAROTH)
        if not check(aidx ~= nil, "Stage 7 Astaroth vanished during visibility check") then return end
        local cnt, _, sx = countBossOam(aidx, 48)
        local bgCnt = 0
        for off = 0x200, 0x2ff do
            local t = emu.read(off, emu.memType.nesNametableRam)
            if t >= 0xa0 and t <= 0xef then bgCnt = bgCnt + 1 end
        end
        -- Check max Astaroth OAM sprites on any single scanline (must be <= 4 -> zero flicker!)
        local lineCnt = {}
        local bx = read(OB_XL + aidx) + 256 * read(OB_XH + aidx)
        local camX = readWord(CAMERA_X)
        local bsx = bx - camX
        for s = 0, 63 do
            local sy = read(0x200 + s * 4)
            local ox = read(0x200 + s * 4 + 3)
            if sy < 239 and sy >= 23 and math.abs(ox - bsx) <= 48 then
                lineCnt[sy] = (lineCnt[sy] or 0) + 1
                if lineCnt[sy] > astMaxPerLine then astMaxPerLine = lineCnt[sy] end
            end
        end
        if cnt < astMinOam then astMinOam = cnt end
        if bgCnt < astMinBg then astMinBg = bgCnt end
        astFrames = astFrames + 1
        if not check(cnt >= 16 and bgCnt >= 32, string.format(
            "Stage 7 Astaroth hybrid BG+sprite incomplete (oam=%d, bg=%d, sx=%d, obsFrame=%d)", cnt, bgCnt, sx, astFrames)) then return end
        if astFrames >= 60 then
            if not check(astMaxPerLine <= 4, string.format(
                "Stage 7 Astaroth exceeded 4 OAM sprites/scanline (max=%d)", astMaxPerLine)) then return end
            if not check(vblankOverruns == 0, string.format(
                "VBlank overrun detected: %d active-render $2007 writes (first at scanline %d)",
                vblankOverruns, worstOverrunSl)) then return end
            print(string.format(
                "PASS revg-boss-sprites frame=%d uniMin=%d(flips=%d) satMin=%d astOam=%d(maxLine=%d) astBg=%d maxVblankSl=%d overruns=0",
                frame, uniMinOam, uniFlips, satMinOam, astMinOam, astMaxPerLine, astMinBg, maxVblankSl))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
