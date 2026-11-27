-- Regression test for:
--   1. Authentic arcade Revision G HUD font (gg1.bin 2-color drop shadow) & PLAYER 1 / TOP SCORE / TIME layout
--   2. Arcade 2-step double-door horizontal gate-opening animation ($75B8 half -> 50F -> $75C5 open) on Stage 1 & Stage 2
--   3. Arcade-accurate enemy projectile sprites (Stage 2 Petit Devil rf_FCE7, Big Man horizontal rf_FCF2/rf_FCFA)
local SCENE = 0x07
local CAMPAIGN_STAGE = 0x14
local CAMERA_X = 0x25
local PAN_Y_PAGE = 0x32
local PLAYER_X = 0x4c
local PLAYER_FEET = 0x4e
local AP_ARMOR = 0x6011
local AP_INVULN = 0x6012
local KEY_COLLECTED = 0x60ab
local PLAYER_Y_HI = 0x60ac
local AP_CLEAR_SEQ = 0x6102
local AP_CLEAR_TIMER = 0x6103
local OB_KIND = 0x699b
local OB_STATE = 0x69af
local OB_XL = 0x6a27
local OB_XH = 0x6a3b
local OB_YL = 0x6a63
local OB_YH = 0x6a77
local OB_CUR_L = 0x6adb
local OB_CUR_H = 0x6aef
local OB_AUX2 = 0x6bcb

local TITLE, PLAY, CLEAR = 7, 4, 5
local OK_DEVIL, OK_BIGMAN, OK_SHOT = 1, 3, 9

-- Expected frame IDs from src/revg-sprite-data.inc
local RF_FCE7 = 0x300D -- Petit Devil shot ($08)
local RF_FCF2 = 0x3019 -- Big Man horizontal Right ($03)
local RF_FCFA = 0x3031 -- Big Man horizontal Left ($03)
local RF_FD02 = 0x3049 -- Big Man downward ($03)

local frame = 0
local phase = "boot"
local phaseFrame = 0
local input = {}
local sawS1Closed, sawS1Half, sawS1Open = false, false, false
local sawS2Closed, sawS2Half, sawS2Open = false, false, false
local sawDevilShot, sawBigManHShot = false, false

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readWord(addr) return read(addr) + 256 * read(addr + 1) end
local function write(addr, v) emu.write(addr, v, emu.memType.nesMemory) end
local function readNt(addr) return emu.read(addr, emu.memType.nesPpuDebug) end
local function readChr(addr) return emu.read(addr, emu.memType.nesChrRam) end

local function saveShot(name)
    if io and os then
        local dir = os.getenv("REVG_CAPTURE_DIR")
        if dir and dir ~= "" then
            local png = emu.takeScreenshot()
            if png then
                local f = io.open(dir .. "/" .. name, "wb")
                if f then f:write(png); f:close() end
            end
        end
    end
end

local function check(cond, msg)
    if cond then return true end
    print(string.format("FAIL frame=%d phase=%s stage=%d x=%d y=%d clr=%d/%d: %s",
        frame, phase, read(CAMPAIGN_STAGE), readWord(PLAYER_X),
        read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI),
        read(AP_CLEAR_SEQ), read(AP_CLEAR_TIMER), msg))
    emu.stop(1)
    return false
end

-- Inspect 6x8 gate nametable region starting at topLeft (cols 0..5, rows 0..7).
local function checkGateState(topLeft, expectOpenCols)
    for r = 0, 7 do
        for c = 0, 5 do
            local v = readNt(topLeft + r * 32 + c)
            local shouldBeOpen = expectOpenCols[c] == true
            if shouldBeOpen and v ~= 0 then
                return false, string.format("row=%d col=%d expected 0 got $%02X at $%04X", r, c, v, topLeft + r * 32 + c)
            elseif (not shouldBeOpen) and v == 0 then
                return false, string.format("row=%d col=%d expected closed (>0) got 0 at $%04X", r, c, topLeft + r * 32 + c)
            end
        end
    end
    return true, "ok"
end

local function checkHud()
    -- Row 1: PLAYER 1 ($2021..$2028), TOP SCORE ($202B..$2033), TIME ($2036..$2039)
    local p1 = {0x10, 0x0c, 0x02, 0x18, 0x06, 0x11, 0x00, 0x1c}
    for i = 1, #p1 do
        if readNt(0x2020 + i) ~= p1[i] then
            return false, string.format("PLAYER 1 mismatch at $%04X: got $%02X exp $%02X", 0x2020 + i, readNt(0x2020 + i), p1[i])
        end
    end
    local top = {0x13, 0x0f, 0x10, 0x00, 0x12, 0x04, 0x0f, 0x11, 0x06}
    for i = 1, #top do
        if readNt(0x202a + i) ~= top[i] then
            return false, string.format("TOP SCORE mismatch at $%04X: got $%02X exp $%02X", 0x202a + i, readNt(0x202a + i), top[i])
        end
    end
    local tm = {0x13, 0x0a, 0x0d, 0x06}
    for i = 1, #tm do
        if readNt(0x2035 + i) ~= tm[i] then
            return false, string.format("TIME mismatch at $%04X: got $%02X exp $%02X", 0x2035 + i, readNt(0x2035 + i), tm[i])
        end
    end
    -- Row 2: leading-zero suppressed "     0" at $2042..$2047 and " 10000" at $204E..$2053
    for a = 0x2042, 0x2046 do
        if readNt(a) ~= 0x00 then
            return false, string.format("P1 leading zero not suppressed at $%04X: $%02X", a, readNt(a))
        end
    end
    if readNt(0x2047) ~= 0x1b then
        return false, string.format("P1 ones digit expected '0' ($1B), got $%02X", readNt(0x2047))
    end
    if readNt(0x204e) ~= 0x00 or readNt(0x204f) ~= 0x1c or readNt(0x2050) ~= 0x1b or readNt(0x2053) ~= 0x1b then
        return false, string.format("TOP SCORE row2 mismatch: %02X %02X %02X %02X %02X %02X",
            readNt(0x204e), readNt(0x204f), readNt(0x2050), readNt(0x2051), readNt(0x2052), readNt(0x2053))
    end
    -- Row 2 TIME "2:00" at $2056..$2059
    if readNt(0x2056) ~= 0x1d or readNt(0x2057) ~= 0x17 or readNt(0x2058) ~= 0x1b or readNt(0x2059) ~= 0x1b then
        return false, string.format("TIME row2 mismatch: %02X %02X %02X %02X",
            readNt(0x2056), readNt(0x2057), readNt(0x2058), readNt(0x2059))
    end
    -- Verify gg1.bin 2-color drop-shadow plane 1 is non-zero for 'P'($10), '0'($1B), '1'($1C), ':'($17)
    for _, tile in ipairs({0x10, 0x1b, 0x1c, 0x17}) do
        local p1sum = 0
        for row = 0, 7 do p1sum = p1sum + readChr(tile * 16 + 8 + row) end
        if p1sum == 0 then
            return false, string.format("CHR tile $%02X missing gg1.bin plane 1 drop shadow", tile)
        end
    end
    return true, "ok"
end

emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    if not check(frame < 4500, "revg-gate-hud-shots timed out") then return end
    local scene = read(SCENE)

    if phase == "boot" then
        if scene == TITLE then
            phaseFrame = frame
            phase = "title_wait"
        end
    elseif phase == "title_wait" then
        if frame == phaseFrame + 10 or frame == phaseFrame + 11 then
            input = {right = true, start = true}
        elseif scene == PLAY then
            phaseFrame = frame
            phase = "s1_hud"
        end
    elseif phase == "s1_hud" then
        if frame == phaseFrame + 10 then
            local ok, msg = checkHud()
            if not check(ok, "HUD check failed: " .. msg) then return end
            saveShot("verify_s1_hud.png")
            -- Move to Stage 1 boss gate (page 13: cam=13*256, player_x=3420)
            write(PLAYER_X, 3420 & 255); write(PLAYER_X + 1, 3420 >> 8)
            phaseFrame = frame
            phase = "s1_gate_scroll"
        end
    elseif phase == "s1_gate_scroll" then
        write(AP_INVULN, 20)
        input = {right = true}
        if readWord(CAMERA_X) >= 13 * 256 then
            -- Clear objects so boss doesn't interfere, verify closed gate, then trigger clear sequence
            for i = 0, 19 do write(OB_KIND + i, 0) end
            write(PLAYER_X, 3430 & 255); write(PLAYER_X + 1, 3430 >> 8)
            phaseFrame = frame
            phase = "s1_gate_prep"
        end
    elseif phase == "s1_gate_prep" then
        write(AP_INVULN, 20)
        if frame == phaseFrame + 12 then
            local ok, msg = checkGateState(0x2652, {})
            if not check(ok, "Stage 1 gate not closed before clear: " .. msg) then return end
            sawS1Closed = true
            saveShot("verify_s1_gate_closed.png")
            write(KEY_COLLECTED, 1)
            write(AP_CLEAR_SEQ, 1)
            write(AP_CLEAR_TIMER, 4)
            phase = "s1_gate_open"
        end
    elseif phase == "s1_gate_open" then
        local seq = read(AP_CLEAR_SEQ)
        local tmr = read(AP_CLEAR_TIMER)
        if seq == 2 and tmr == 55 and not sawS1Half then
            local ok, msg = checkGateState(0x2652, {[2] = true, [3] = true})
            if not check(ok, "Stage 1 gate Half-Open ($75B8) mismatch: " .. msg) then return end
            sawS1Half = true
            saveShot("verify_s1_gate_half.png")
        elseif seq == 2 and tmr == 5 and not sawS1Open then
            local ok, msg = checkGateState(0x2652, {[1] = true, [2] = true, [3] = true, [4] = true})
            if not check(ok, "Stage 1 gate Full-Open ($75C5) mismatch: " .. msg) then return end
            sawS1Open = true
            saveShot("verify_s1_gate_open.png")
        elseif scene == CLEAR then
            if not check(sawS1Closed and sawS1Half and sawS1Open, "Stage 1 gate steps incomplete") then return end
            phaseFrame = frame
            phase = "s1_to_s2"
        end
    elseif phase == "s1_to_s2" then
        if frame >= phaseFrame + 20 and (frame % 20) == 0 then
            input = {start = true}
        end
        if scene == PLAY and read(CAMPAIGN_STAGE) == 2 then
            phaseFrame = frame
            phase = "s2_devil"
        end
    elseif phase == "s2_devil" then
        write(AP_INVULN, 20)
        write(AP_ARMOR, 2)
        -- Advance slightly right into Village so Petit Devil emerges from window
        if frame < phaseFrame + 40 then
            input = {right = true}
        end
        -- Once a Petit Devil is in swoop state (state==2), align Arthur horizontally (dy=0, dx=80) to trigger its arcade $08 shot!
        for i = 0, 19 do
            if read(OB_KIND + i) == OK_DEVIL and read(OB_STATE + i) == 2 and read(OB_AUX2 + i) == 0 then
                local dx_obj = read(OB_XL + i) + 256 * read(OB_XH + i)
                local dy_obj = read(OB_YL + i) + 256 * read(OB_YH + i)
                -- Convert arcade local Y to FC feet Y for 2-page stage (480 - arcade_y)
                local fc_y = 480 - dy_obj
                if fc_y > 80 and fc_y < 440 then
                    local px = dx_obj - 70
                    if px < 64 then px = dx_obj + 70 end
                    write(PLAYER_X, px & 255); write(PLAYER_X + 1, px >> 8)
                    write(PLAYER_FEET, fc_y & 255); write(PLAYER_Y_HI, fc_y >> 8)
                end
            end
            if read(OB_KIND + i) == OK_SHOT then
                local fid = read(OB_CUR_L + i) + 256 * read(OB_CUR_H + i)
                if fid == RF_FCE7 then
                    sawDevilShot = true
                    saveShot("verify_s2_devil_shot.png")
                end
            end
        end
        if sawDevilShot or frame > phaseFrame + 300 then
            if not check(sawDevilShot, "Stage 2 Petit Devil did not fire authentic rf_FCE7 ($08) shot") then return end
            -- Scroll into Big Man arena (page 6, X=1664)
            write(PLAYER_X, 1620 & 255); write(PLAYER_X + 1, 1620 >> 8)
            write(PLAYER_FEET, 432 & 255); write(PLAYER_Y_HI, 432 >> 8)
            phaseFrame = frame
            phase = "s2_bigman_scroll"
        end
    elseif phase == "s2_bigman_scroll" then
        write(AP_INVULN, 20)
        write(AP_ARMOR, 2)
        write(0x6010, 0) -- keep grounded while camera smoothly streams to page 6
        write(PLAYER_X, 1680 & 255); write(PLAYER_X + 1, 1680 >> 8)
        write(PLAYER_FEET, 432 & 255); write(PLAYER_Y_HI, 432 >> 8)
        if readWord(CAMERA_X) >= 1536 then
            phaseFrame = frame
            phase = "s2_bigman"
        end
    elseif phase == "s2_bigman" then
        write(AP_INVULN, 20)
        write(AP_ARMOR, 2)
        write(0x6010, 0)
        for i = 0, 19 do
            if read(OB_KIND + i) == OK_BIGMAN then
                local bx = read(OB_XL + i) + 256 * read(OB_XH + i)
                local by = read(OB_YL + i) + 256 * read(OB_YH + i)
                local fc_y = 480 - by
                if fc_y > 80 and fc_y < 440 then
                    local px = bx - 48
                    write(PLAYER_X, px & 255); write(PLAYER_X + 1, px >> 8)
                    write(PLAYER_FEET, fc_y & 255); write(PLAYER_Y_HI, fc_y >> 8)
                end
            end
            if read(OB_KIND + i) == OK_SHOT then
                local fid = read(OB_CUR_L + i) + 256 * read(OB_CUR_H + i)
                if not check(fid ~= RF_FD02, "Big Man horizontal shot wrongly used downward rf_FD02") then return end
                if fid == RF_FCF2 or fid == RF_FCFA then
                    sawBigManHShot = true
                end
            end
        end
        if sawBigManHShot or frame > phaseFrame + 240 then
            if not check(sawBigManHShot, "Stage 2 Big Man did not fire horizontal rf_FCF2/rf_FCFA shot") then return end
            -- Scroll into Stage 2 boss gate (page 13, 2-axis nametable $2800 -> gate top-left $2A52)
            phaseFrame = frame
            phase = "s2_gate_scroll"
        end
    elseif phase == "s2_gate_scroll" then
        write(AP_INVULN, 20)
        write(AP_ARMOR, 2)
        write(0x6010, 0)
        write(PLAYER_X, 3460 & 255); write(PLAYER_X + 1, 3460 >> 8)
        write(PLAYER_FEET, 432 & 255); write(PLAYER_Y_HI, 432 >> 8)
        if readWord(CAMERA_X) >= 13 * 256 then
            for i = 0, 19 do write(OB_KIND + i, 0) end
            write(PLAYER_X, 3430 & 255); write(PLAYER_X + 1, 3430 >> 8)
            phaseFrame = frame
            phase = "s2_gate_prep"
        end
    elseif phase == "s2_gate_prep" then
        write(AP_INVULN, 20)
        if frame == phaseFrame + 16 then
            local ok, msg = checkGateState(0x2A52, {})
            if not check(ok, "Stage 2 gate ($2A52) not closed before clear: " .. msg) then return end
            sawS2Closed = true
            write(KEY_COLLECTED, 1)
            write(AP_CLEAR_SEQ, 1)
            write(AP_CLEAR_TIMER, 4)
            phase = "s2_gate_open"
        end
    elseif phase == "s2_gate_open" then
        local seq = read(AP_CLEAR_SEQ)
        local tmr = read(AP_CLEAR_TIMER)
        if seq == 2 and tmr == 55 and not sawS2Half then
            local ok, msg = checkGateState(0x2A52, {[2] = true, [3] = true})
            if not check(ok, "Stage 2 gate Half-Open ($75B8) mismatch: " .. msg) then return end
            sawS2Half = true
            saveShot("verify_s2_gate_half.png")
        elseif seq == 2 and tmr == 5 and not sawS2Open then
            local ok, msg = checkGateState(0x2A52, {[1] = true, [2] = true, [3] = true, [4] = true})
            if not check(ok, "Stage 2 gate Full-Open ($75C5) mismatch: " .. msg) then return end
            sawS2Open = true
            saveShot("verify_s2_gate_open.png")
            print(string.format("PASS: HUD (gg1.bin + PLAYER 1/TOP SCORE/TIME), Stage 1 & 2 2-step gate opening, Stage 2 Petit Devil (rf_FCE7) & Big Man (rf_FCF2/FA) shots (frame %d)", frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
