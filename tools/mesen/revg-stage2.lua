-- Stage 2 (Revision G Section 1: Village -> Ice Palace -> 2x Unicorn) smoke test.
-- Verifies Stage 2 initialization (bank 13, 2-vertical-page map, start at (80, 400),
-- s2_boss_rem=2), Petit Devil ($02), Purple Flyer ($04), Big Man ($0A, HP 10),
-- 2x Unicorn simultaneous boss battle (s2_boss_rem 2 -> 1 -> 0), key drop & pickup,
-- clear sequence, STAGE2 CLEAR, and return to title.
local SCENE = 0x07
local PRG_BANK0 = 0x0d
local CAMPAIGN_STAGE = 0x14
local CAMERA_X = 0x25
local PAN_Y_PAGE = 0x32
local PLAYER_X = 0x4c
local PLAYER_FEET = 0x4e
local AP_ARMOR = 0x6011
local KEY_COLLECTED = 0x60ab
local PLAYER_Y_HI = 0x60ac
local OB_KIND = 0x699b
local OB_XL = 0x6a27
local OB_XH = 0x6a3b
local OB_HP = 0x6b67
local S2_BOSS_REM = 0x6c66
local SND_SONG_ID = 0x7800

local TITLE, PLAY, CLEAR = 7, 4, 5
local OK_DEVIL, OK_FLYER, OK_BIGMAN, OK_UNICORN, OK_KEY = 1, 2, 3, 8, 15

local frame = 0
local phase = "boot"
local input = {}
local phaseFrame = 0
local sawDevil, sawFlyer, sawBigMan, sawTwoUnicorns, sawOneRem, sawZeroRem, sawKey, sawClearSong =
    false, false, false, false, false, false, false, false
local initAttrCol0 = {}

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readWord(addr) return read(addr) + 256 * read(addr + 1) end
local function write(addr, v) emu.write(addr, v, emu.memType.nesMemory) end

local function check(cond, msg)
    if cond then return true end
    local kinds = {}
    for i = 0, 19 do
        local k = read(OB_KIND + i)
        if k ~= 0 then kinds[#kinds + 1] = string.format("%d:%d", i, k) end
    end
    print(string.format("FAIL frame=%d phase=%s stage=%d bank0=%d x=%d y=%d rem=%d seen=%d key=%d clr=%d objs=[%s]: %s",
        frame, phase, read(CAMPAIGN_STAGE), read(PRG_BANK0),
        readWord(PLAYER_X), read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI),
        read(S2_BOSS_REM), read(0x6c21), read(KEY_COLLECTED), read(0x6102),
        table.concat(kinds, ","), msg))
    emu.stop(1)
    return false
end

local function countKind(kind)
    local n, first = 0, nil
    for i = 0, 19 do
        if read(OB_KIND + i) == kind then
            n = n + 1
            if not first then first = i end
        end
    end
    return n, first
end

emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    if not check(frame < 4000, "Stage 2 test timed out") then return end

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
            phase = "enter_s1"
        end
    elseif phase == "enter_s1" then
        if scene == PLAY and readWord(PLAYER_X) == 96 and read(PLAYER_FEET) == 192 then
            -- Fast-forward Stage 1 clear so we enter Stage 2 through the real stage transition.
            write(KEY_COLLECTED, 1)
            write(0x6102, 0) -- ap_clear_seq = 0 -> immediate show_course_clear
            phase = "s1_clear"
        end
    elseif phase == "s1_clear" then
        if scene == CLEAR then
            phaseFrame = frame
            phase = "s1_clear_wait"
        end
    elseif phase == "s1_clear_wait" then
        if frame == phaseFrame + 20 then
            input = {start = true}
            phaseFrame = frame
            phase = "s2_init"
        end
    elseif phase == "s2_init" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 2 then
            local y = read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI)
            if not check(read(PRG_BANK0) == 13, "Stage 2 must map bank 13") then return end
            if not check(read(PAN_Y_PAGE) == 1, "Stage 2 starts on lower vertical page 1") then return end
            if not check(readWord(PLAYER_X) == 80 and y == 400, "Stage 2 starts at (80, 400)") then return end
            if not check(read(S2_BOSS_REM) == 2, "Stage 2 starts with 2 Unicorn bosses") then return end
            -- Verify Zone 0 Tower platforms: $03 var 0 vertical elevator (type 3, X=392, Y=318)
            -- and $03 var 1 fast-drop ledge (type 4, X=720, Y=360).
            local pt0, pt1 = read(0x603e), read(0x603f)
            local px0 = read(0x6042) + 256 * read(0x6046)
            local px1 = read(0x6043) + 256 * read(0x6047)
            if not check(pt0 == 3 and pt1 == 4 and px0 == 392 and px1 == 720,
                    string.format("Stage 2 tower platforms mismatch (t0=%d t1=%d px0=%d px1=%d)", pt0, pt1, px0, px1)) then return end
            -- Test high-tower fall from upper page 0 (y=80) down to ground floor page 1 (y=400):
            -- vertical camera must track at up to 5 px/f and Arthur must NOT pit-die mid-tower.
            write(0x09, 0)         -- pan_y = 0
            write(PAN_Y_PAGE, 0)   -- upper vertical page 0
            write(PLAYER_FEET, 80); write(PLAYER_Y_HI, 0)
            write(0x6010, 1)       -- ap_state_air
            write(0x6013, 2)       -- ap_jump_phase = falling
            phaseFrame = frame
            phase = "s2_tower_fall"
        end
    elseif phase == "s2_tower_fall" then
        write(AP_ARMOR, 2)
        if not check(read(0x6010) ~= 4 and read(0x15) == 3,
                string.format("Arthur died mid-air falling from Stage 2 tower (state=%d lives=%d y=%d yp=%d)",
                    read(0x6010), read(0x15), read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI), read(PAN_Y_PAGE))) then return end
        if frame == phaseFrame + 95 then
            local y = read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI)
            local camY = read(0x60b0) + 256 * read(0x60b1)
            if not check(camY >= 200 and y >= 380,
                    string.format("Vertical camera did not track tower fall (camY=%d y=%d)", camY, y)) then return end
            -- Restore camera to bottom page 1 for subsequent Stage 2 enemy checks.
            write(0x09, 0); write(PAN_Y_PAGE, 1)
            phaseFrame = frame
            phase = "s2_devil"
        end
    elseif phase == "s2_devil" then
        write(AP_ARMOR, 2)
        if countKind(OK_DEVIL) > 0 then sawDevil = true end
        if sawDevil or frame > phaseFrame + 180 then
            if not check(sawDevil, "Petit Devil ($02) should spawn on page 0") then return end
            -- Snapshot attribute column 0 of $2800 (rows 1..7) at pan_x=0, then scroll right
            -- across pan_x=1..24 and verify the left-edge $CC bits (cols 2..3) stay intact.
            for r = 1, 7 do
                initAttrCol0[r] = emu.read(0x7c0 + r * 8, emu.memType.nesNametableRam)
            end
            phaseFrame = frame
            phase = "s2_scroll_attr"
        end
    elseif phase == "s2_scroll_attr" then
        write(AP_ARMOR, 2); write(0x6012, 30)
        local dt = frame - phaseFrame
        local px = 128 + dt
        write(PLAYER_X, px & 255); write(PLAYER_X + 1, px >> 8)
        write(PLAYER_FEET, 400 & 255); write(PLAYER_Y_HI, 400 >> 8)
        local cx = readWord(CAMERA_X)
        if not check(read(CAMPAIGN_STAGE) == 2 and dt < 60,
            string.format("Stage 2 scroll attr check stalled (stg=%d cx=%d)", read(CAMPAIGN_STAGE), cx)) then return end
        if cx >= 1 and cx <= 8 then
            for r = 1, 7 do
                local cur = emu.read(0x7c0 + r * 8, emu.memType.nesNametableRam)
                if not check(cur == initAttrCol0[r], string.format(
                    "Stage 2 left-edge attr clobbered at cx=%d row=%d (got $%02X want $%02X)",
                    cx, r, cur, initAttrCol0[r])) then return end
            end
        elseif cx >= 9 and cx <= 24 then
            for r = 1, 7 do
                local cur = emu.read(0x7c0 + r * 8, emu.memType.nesNametableRam)
                if not check((cur & 0xcc) == (initAttrCol0[r] & 0xcc), string.format(
                    "Stage 2 left-edge $CC attr clobbered at cx=%d row=%d (got $%02X want $%02X)",
                    cx, r, cur & 0xcc, initAttrCol0[r] & 0xcc)) then return end
            end
        end
        if cx >= 18 then
            -- Advance camera & player to page 4 (X=1152, building windows for Purple Flyer $04)
            write(CAMERA_X, 0); write(CAMERA_X + 1, 4)
            write(PLAYER_X, 1152 & 255); write(PLAYER_X + 1, 1152 >> 8)
            write(PLAYER_FEET, 432 & 255); write(PLAYER_Y_HI, 432 >> 8)
            phaseFrame = frame
            phase = "s2_flyer"
        end
    elseif phase == "s2_flyer" then
        write(AP_ARMOR, 2)
        if countKind(OK_FLYER) > 0 then sawFlyer = true end
        if sawFlyer or frame > phaseFrame + 360 then
            if not check(sawFlyer, "Purple Flyer ($04) should spawn on page 4") then return end
            -- Advance to page 6 (X=1664, Big Man $0A)
            write(CAMERA_X, 0); write(CAMERA_X + 1, 6)
            write(PLAYER_X, 1620 & 255); write(PLAYER_X + 1, 1620 >> 8)
            phaseFrame = frame
            phase = "s2_bigman"
        end
    elseif phase == "s2_bigman" then
        write(AP_ARMOR, 2)
        local n, idx = countKind(OK_BIGMAN)
        if n > 0 and read(OB_HP + idx) == 9 then sawBigMan = true end
        if sawBigMan or frame > phaseFrame + 120 then
            if not check(sawBigMan, "Big Man ($0A, HP 10) should spawn on page 6") then return end
            -- Verify Ice Palace Zone 1 moving lifts (camera_high = 9 -> 4 vertical lifts at X=2448..2608)
            write(CAMERA_X, 0); write(CAMERA_X + 1, 9)
            write(PLAYER_X, 2400 & 255); write(PLAYER_X + 1, 2400 >> 8)
            write(PLAYER_FEET, 432 & 255); write(PLAYER_Y_HI, 432 >> 8)
            phaseFrame = frame
            phase = "s2_ice_plats"
        end
    elseif phase == "s2_ice_plats" then
        write(AP_ARMOR, 2)
        if frame == phaseFrame + 2 then
            local t0, t1, t2, t3 = read(0x603e), read(0x603f), read(0x6040), read(0x6041)
            local px0 = read(0x6042) + 256 * read(0x6046)
            if not check(t0 == 3 and t1 == 3 and t2 == 3 and t3 == 3 and px0 == 2448,
                    string.format("Stage 2 Ice Palace Zone 1 lifts missing (t=%d,%d,%d,%d px0=%d)", t0, t1, t2, t3, px0)) then return end
            icePlatY0 = read(0x604a) + 256 * read(0x60b6)
        elseif frame == phaseFrame + 12 then
            local curY0 = read(0x604a) + 256 * read(0x60b6)
            if not check(curY0 ~= icePlatY0,
                    string.format("Stage 2 Ice Palace lift did not move vertically (y0=%d -> %d)", icePlatY0, curY0)) then return end
            -- Advance to boss arena (page 12, X=3328 -> Zone 4 Channel 4 lifts + 2x Unicorn)
            write(CAMERA_X, 0); write(CAMERA_X + 1, 12)
            write(PLAYER_X, 3310 & 255); write(PLAYER_X + 1, 3310 >> 8)
            write(PLAYER_FEET, 432 & 255); write(PLAYER_Y_HI, 432 >> 8)
            phaseFrame = frame
            phase = "s2_boss"
        end
    elseif phase == "s2_boss" then
        write(AP_ARMOR, 2)
        if frame == phaseFrame + 2 then
            local t1, t2 = read(0x603f), read(0x6040)
            local px1 = read(0x6043) + 256 * read(0x6047)
            if not check(t1 == 3 and t2 == 3 and px1 == 2992,
                    string.format("Stage 2 Ice Palace Zone 4 Channel 4 lifts missing (t1=%d t2=%d px1=%d)", t1, t2, px1)) then return end
        end
        local n, idx = countKind(OK_UNICORN)
        local rem = read(S2_BOSS_REM)
        if n >= 2 then sawTwoUnicorns = true end
        if rem == 1 then sawOneRem = true end
        if rem == 0 then sawZeroRem = true end
        if n > 0 then
            -- Fight the Unicorns with real lance shots
            local bx = read(OB_XL + idx) + 256 * read(OB_XH + idx)
            local x = readWord(PLAYER_X)
            if bx > x then
                input = {right = (frame % 16 == 1), b = (frame % 10 == 0)}
            else
                input = {left = (frame % 16 == 1), b = (frame % 10 == 0)}
            end
        elseif sawZeroRem then
            if countKind(OK_KEY) > 0 then
                sawKey = true
                phase = "s2_key"
            else
                input = {right = true}
            end
        end
    elseif phase == "s2_key" then
        if read(SND_SONG_ID) == 4 then sawClearSong = true end
        local n, kslot = countKind(OK_KEY)
        if n > 0 then
            local kx = read(OB_XL + kslot) + 256 * read(OB_XH + kslot)
            local x = readWord(PLAYER_X)
            if x < kx - 4 then input = {right = true}
            elseif x > kx + 4 then input = {left = true} end
        end
        if scene == CLEAR then
            if not check(sawTwoUnicorns and sawOneRem and sawZeroRem and sawKey and sawClearSong,
                    "Stage 2 clear after defeating 2x Unicorn and picking up key") then return end
            phaseFrame = frame
            phase = "s2_clear"
        end
    elseif phase == "s2_clear" then
        if frame == phaseFrame + 20 then
            input = {start = true}
        elseif frame == phaseFrame + 35 then
            if not check(scene == PLAY and read(CAMPAIGN_STAGE) == 3, "START after Stage 2 clear advances to Stage 3") then return end
            print(string.format("PASS: Stage 2 (bank 13, 2-page map, Petit Devil, Purple Flyer, Big Man HP10, 2x Unicorn, key, clear -> Stage 3) (frame %d)", frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
