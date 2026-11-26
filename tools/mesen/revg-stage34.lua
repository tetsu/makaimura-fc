-- Stage 3 (Revision G Section 2: 4-page Cave -> Dragon) & Stage 4 (Section 3: Bridge -> Dragon) smoke test.
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
local SND_SONG_ID = 0x7800

local TITLE, PLAY, CLEAR = 7, 4, 5
local OK_BAT, OK_DEVIL, OK_PILLAR, OK_FIRE, OK_DRAGON, OK_KEY = 2, 3, 7, 8, 9, 16

local frame = 0
local phase = "boot"
local input = {}
local phaseFrame = 0
local sawS3Bat, sawS3BatSwoop, sawS3Zombie, sawS3Pillar, sawS3Dragon, sawS3Key = false, false, false, false, false, false
local sawS4Fire, sawS4Dragon, sawS4Key = false, false, false

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readWord(addr) return read(addr) + 256 * read(addr + 1) end
local function write(addr, v) emu.write(addr, v, emu.memType.nesMemory) end

local function countKind(kind)
    local n, headIdx = 0, nil
    for i = 0, 19 do
        if read(OB_KIND + i) == kind then
            n = n + 1
            if not headIdx or read(OB_VAR + i) == 0 then headIdx = i end
        end
    end
    return n, headIdx
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
        if k ~= 0 then kinds[#kinds + 1] = string.format("%d:%d(v%d,f%02x)", i, k, read(OB_VAR + i), read(OB_FLAGS + i)) end
    end
    print(string.format("FAIL frame=%d phase=%s stage=%d bank0=%d bg=%d x=%d y=%d yp=%d objs=[%s]: %s",
        frame, phase, read(CAMPAIGN_STAGE), read(PRG_BANK0), read(REVG_BG_ACTIVE),
        readWord(PLAYER_X), read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI), read(PAN_Y_PAGE),
        table.concat(kinds, ","), msg))
    emu.stop(1)
    return false
end

emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    if not check(frame < 6000, "Stage 3-4 test timed out") then return end

    local scene = read(SCENE)
    if phase == "boot" then
        if scene == TITLE then phaseFrame = frame; phase = "title_wait" end
    elseif phase == "title_wait" then
        if frame == phaseFrame + 10 or frame == phaseFrame + 11 then
            input = {right = true, start = true}
        elseif scene == PLAY then
            phase = "skip_s1"
        end
    elseif phase == "skip_s1" then
        if scene == PLAY and read(CAMPAIGN_STAGE) == 1 and readWord(PLAYER_X) == 96 then
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
            phase = "s1_clr"
        end
    elseif phase == "s1_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s1_clr_w" end
    elseif phase == "s1_clr_w" then
        if frame == phaseFrame + 15 then input = {start = true}; phaseFrame = frame; phase = "skip_s2" end
    elseif phase == "skip_s2" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 2 then
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
            phase = "s2_clr"
        end
    elseif phase == "s2_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s2_clr_w" end
    elseif phase == "s2_clr_w" then
        if frame == phaseFrame + 15 then input = {start = true}; phaseFrame = frame; phase = "s3_init" end

    -- === STAGE 3 (Section 2: 4-page Cave) ===
    elseif phase == "s3_init" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 3 then
            local y = read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI)
            if not check(read(PRG_BANK0) == 14, "Stage 3 must map bank 14") then return end
            if not check(read(PAN_Y_PAGE) == 3, "Stage 3 must start on bottom vertical page 3") then return end
            if not check(read(REVG_BG_ACTIVE) == 5, "Stage 3 must use revg_bg_active=5") then return end
            if not check(readWord(PLAYER_X) == 80 and y == 896, "Stage 3 must start at (80, 896)") then return end
            if not check(countArcadeBgTiles(0x400) > 100, "Stage 3 nametable must have arcade cave tiles") then return end
            phaseFrame = frame
            phase = "s3_enemies"
        end
    elseif phase == "s3_enemies" then
        write(AP_ARMOR, 2)
        local nb, bidx = countKind(OK_BAT)
        if nb > 0 then
            sawS3Bat = true
            -- Verify Cave Bat swoop speed is +-1.25 px/F (vxi == 0x01 or 0xFE, never broken 0xFF)
            for i = 0, 19 do
                if read(OB_KIND + i) == OK_BAT and read(0x69af + i) == 2 then
                    sawS3BatSwoop = true
                    local vxi = read(0x6a9f + i) -- ob_vxi
                    if not check(vxi == 1 or vxi == 0xfe,
                            string.format("Stage 3 Cave Bat swoop vxi wrong (got $%02X)", vxi)) then return end
                end
            end
        end
        -- Verify Stage 3 Zombies are grounded on the floor (never spawning/walking in mid-air)
        for i = 0, 19 do
            if read(OB_KIND + i) == 6 and read(0x69af + i) >= 1 then
                sawS3Zombie = true
                local zy = read(OB_YL + i) + 256 * read(OB_YH + i)
                if not check(zy <= 144,
                        string.format("Stage 3 Zombie spawned in mid-air (y=%d)", zy)) then return end
            end
        end
        if frame == phaseFrame + 100 then
            -- Move to Stone Pillar at X=608
            write(CAMERA_X, 0); write(CAMERA_X + 1, 2)
            write(PLAYER_X, 560 & 255); write(PLAYER_X + 1, 560 >> 8)
        end
        local np, pidx = countKind(OK_PILLAR)
        if np > 0 and (read(OB_FLAGS + pidx) & 0x80) ~= 0 then sawS3Pillar = true end
        if (sawS3Bat and sawS3BatSwoop and sawS3Zombie and sawS3Pillar and frame >= phaseFrame + 105) or frame > phaseFrame + 240 then
            if not check(sawS3Bat and sawS3BatSwoop, "Stage 3 Bat ($01 var 2) should spawn and swoop") then return end
            if not check(sawS3Zombie, "Stage 3 Zombie ($0B) should spawn grounded") then return end
            if not check(sawS3Pillar, "Stage 3 Stone Pillar ($12) should spawn shielded") then return end
            -- Advance to Stage 3 Dragon boss arena (page 12, X=3072)
            write(CAMERA_X, 0); write(CAMERA_X + 1, 12)
            write(PLAYER_X, 3200 & 255); write(PLAYER_X + 1, 3200 >> 8)
            write(PLAYER_FEET, 880 & 255); write(PLAYER_Y_HI, 880 >> 8)
            phaseFrame = frame
            phase = "s3_boss"
        end
    elseif phase == "s3_boss" then
        write(AP_ARMOR, 2)
        local n, hidx = countKind(OK_DRAGON)
        if n == 9 then sawS3Dragon = true end
        if sawS3Dragon and hidx and read(OB_VAR + hidx) == 0 and frame > phaseFrame + 15 then
            -- Position Dragon head right in front of Arthur's lance and shoot it to defeat
            local px = readWord(PLAYER_X)
            write(OB_XL + hidx, (px + 28) & 255); write(OB_XH + hidx, (px + 28) >> 8)
            write(OB_HP + hidx, 0)
            input = {b = (frame % 4 == 0)}
        end
        local nk, kidx = countKind(OK_KEY)
        if nk > 0 then
            sawS3Key = true
            local kx = read(OB_XL + kidx) + 256 * read(OB_XH + kidx)
            write(PLAYER_X, kx & 255); write(PLAYER_X + 1, kx >> 8)
        end
        if scene == CLEAR then
            if not check(sawS3Dragon, "Stage 3 Dragon (9 segments) should spawn") then return end
            if not check(sawS3Key, "Stage 3 Dragon should drop key") then return end
            phaseFrame = frame
            phase = "s3_clr_w"
        end
    elseif phase == "s3_clr_w" then
        if frame == phaseFrame + 15 then input = {start = true}; phaseFrame = frame; phase = "s4_init" end

    -- === STAGE 4 (Section 3: 1-page Bridge) ===
    elseif phase == "s4_init" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 4 then
            local y = read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI)
            if not check(read(PRG_BANK0) == 15, "Stage 4 must map bank 15") then return end
            if not check(read(PAN_Y_PAGE) == 0, "Stage 4 must be 1-page (pan_y_page=0)") then return end
            if not check(read(REVG_BG_ACTIVE) == 2, "Stage 4 must use revg_bg_active=2") then return end
            if not check(readWord(PLAYER_X) == 48 and y == 176, "Stage 4 must start at (48, 176)") then return end
            if not check(countArcadeBgTiles(0x000) > 100, "Stage 4 nametable must have arcade bridge tiles") then return end
            phaseFrame = frame
            phase = "s4_enemies"
        end
    elseif phase == "s4_enemies" then
        write(AP_ARMOR, 2)
        if frame == phaseFrame + 5 then
            -- Move to fire bridge (local X=1520, page 5/6)
            write(CAMERA_X, 0); write(CAMERA_X + 1, 6)
            write(PLAYER_X, 1520 & 255); write(PLAYER_X + 1, 1520 >> 8)
            write(PLAYER_FEET, 192); write(PLAYER_Y_HI, 0)
        end
        if countKind(OK_FIRE) > 0 then sawS4Fire = true end
        if sawS4Fire or frame > phaseFrame + 90 then
            if not check(sawS4Fire, "Stage 4 Fire Pillar ($0E) should spawn on bridge") then return end
            -- Advance to Stage 4 Dragon boss arena (page 10, X=2560)
            write(CAMERA_X, 0); write(CAMERA_X + 1, 10)
            write(PLAYER_X, 2700 & 255); write(PLAYER_X + 1, 2700 >> 8)
            write(PLAYER_FEET, 176); write(PLAYER_Y_HI, 0)
            phaseFrame = frame
            phase = "s4_boss"
        end
    elseif phase == "s4_boss" then
        write(AP_ARMOR, 2)
        local n, hidx = countKind(OK_DRAGON)
        if n == 9 then sawS4Dragon = true end
        if sawS4Dragon and hidx and read(OB_VAR + hidx) == 0 and frame > phaseFrame + 15 then
            local px = readWord(PLAYER_X)
            write(OB_XL + hidx, (px + 28) & 255); write(OB_XH + hidx, (px + 28) >> 8)
            write(OB_YL + hidx, 80); write(OB_YH + hidx, 0)
            write(OB_HP + hidx, 0)
            input = {b = (frame % 4 == 0)}
        end
        local nk, kidx = countKind(OK_KEY)
        if nk > 0 then
            sawS4Key = true
            local kx = read(OB_XL + kidx) + 256 * read(OB_XH + kidx)
            write(PLAYER_X, kx & 255); write(PLAYER_X + 1, kx >> 8)
        end
        if scene == CLEAR then
            if not check(sawS4Dragon, "Stage 4 Dragon (9 segments) should spawn") then return end
            if not check(sawS4Key, "Stage 4 Dragon should drop key") then return end
            print(string.format("PASS Stage 3 & 4 (Cave + Bridge + Dragon x2) at frame %d", frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
