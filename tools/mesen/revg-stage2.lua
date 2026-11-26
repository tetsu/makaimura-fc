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
local OB_KIND = 0x698a
local OB_XL = 0x6a16
local OB_XH = 0x6a2a
local OB_HP = 0x6b56
local S2_BOSS_REM = 0x6c55
local SND_SONG_ID = 0x7800

local TITLE, PLAY, CLEAR = 7, 4, 5
local OK_DEVIL, OK_FLYER, OK_BIGMAN, OK_UNICORN, OK_KEY = 1, 2, 3, 8, 15

local frame = 0
local phase = "boot"
local input = {}
local phaseFrame = 0
local sawDevil, sawFlyer, sawBigMan, sawTwoUnicorns, sawOneRem, sawZeroRem, sawKey, sawClearSong =
    false, false, false, false, false, false, false, false

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
            input = {start = true}
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
            phaseFrame = frame
            phase = "s2_devil"
        end
    elseif phase == "s2_devil" then
        write(AP_ARMOR, 2)
        if countKind(OK_DEVIL) > 0 then sawDevil = true end
        if sawDevil or frame > phaseFrame + 180 then
            if not check(sawDevil, "Petit Devil ($02) should spawn on page 0") then return end
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
        if sawFlyer or frame > phaseFrame + 180 then
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
            -- Advance to boss arena (page 12, X=3328 -> 2x Unicorn)
            write(CAMERA_X, 0); write(CAMERA_X + 1, 12)
            write(PLAYER_X, 3310 & 255); write(PLAYER_X + 1, 3310 >> 8)
            write(PLAYER_FEET, 432 & 255); write(PLAYER_Y_HI, 432 >> 8)
            phaseFrame = frame
            phase = "s2_boss"
        end
    elseif phase == "s2_boss" then
        write(AP_ARMOR, 2)
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
            if not check(scene == TITLE, "START after Stage 2 clear returns to title") then return end
            print(string.format("PASS: Stage 2 (bank 13, 2-page map, Petit Devil, Purple Flyer, Big Man HP10, 2x Unicorn, key, clear -> title) (frame %d)", frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
