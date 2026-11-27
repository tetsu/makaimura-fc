-- Stage 5 (Section 4) and Stage 6 (Section 5) 5-page nibble-map terrain, ladder, wall,
-- overhang, and ball-platform regression test in Mesen 2.1.1.
local SCENE = 0x07
local CAMPAIGN_STAGE = 0x14
local CAMERA_X = 0x25
local PAN_Y = 0x09
local PAN_X = 0x30
local PAN_Y_PAGE = 0x32
local PLAYER_X = 0x4c
local PLAYER_FEET = 0x4e
local AP_STATE = 0x6010
local AP_ARMOR = 0x6011
local AP_INVULN = 0x6012
local AP_WEAPON = 0x601a
local KEY_COLLECTED = 0x60ab
local PLAYER_Y_HI = 0x60ac
local AP_CAM_Y_LO = 0x60b0
local AP_CAM_Y_HI = 0x60b1
local AP_CLEAR_SEQ = 0x6102
local PLAT_TYPE = 0x603e
local PLAT_X_LO = 0x6042
local PLAT_X_HI = 0x6046
local PLAT_Y_LO = 0x604a
local PLAT_SPAN = 0x605e
local PLAT_Y_HI = 0x60b6

local TITLE, PLAY, CLEAR = 7, 4, 5
local STATE_GROUND, STATE_AIR, STATE_LADDER = 0, 1, 2

local frame = 0
local phase = "boot"
local phaseFrame = 0
local input = {}

local function read(addr) return emu.read(addr, emu.memType.nesMemory) end
local function readWord(addr) return read(addr) + 256 * read(addr + 1) end
local function write(addr, v) emu.write(addr, v, emu.memType.nesMemory) end
local function playerY() return read(PLAYER_FEET) + 256 * read(PLAYER_Y_HI) end

local function placeArthur(x, y, camX, yPage)
    write(PLAYER_X, x % 256); write(PLAYER_X + 1, math.floor(x / 256))
    write(PLAYER_FEET, y % 256); write(PLAYER_Y_HI, math.floor(y / 256))
    write(AP_STATE, STATE_GROUND)
    write(CAMERA_X, camX % 256); write(CAMERA_X + 1, math.floor(camX / 256))
    write(PAN_X, camX % 256); write(PAN_X + 1, math.floor(camX / 256))
    write(PAN_Y, 0); write(PAN_Y_PAGE, yPage)
    local camY = yPage * 240
    write(AP_CAM_Y_LO, camY % 256); write(AP_CAM_Y_HI, math.floor(camY / 256))
end

local function check(cond, msg)
    if cond then return true end
    print(string.format("FAIL frame=%d phase=%s stg=%d state=%d x=%d y=%d: %s",
        frame, phase, read(CAMPAIGN_STAGE), read(AP_STATE), readWord(PLAYER_X), playerY(), msg))
    emu.stop(1)
    return false
end

-- Stage 5 bottom-floor non-ladder columns (all previously corrupted by col*40 carry bug).
local noLadderCols = {
    {x = 120, cam = 0,   label = "col 7"},
    {x = 232, cam = 128, label = "col 14"},
    {x = 248, cam = 128, label = "col 15"},
    {x = 264, cam = 160, label = "col 16"},
    {x = 312, cam = 192, label = "col 19"},
    {x = 392, cam = 256, label = "col 24"},
    {x = 408, cam = 256, label = "col 25"},
}
local nlIdx = 1

emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    if not check(frame < 4500, "Stage 5-6 terrain test timed out") then return end

    local scene = read(SCENE)
    if scene == PLAY then
        write(AP_ARMOR, 2)
        write(AP_INVULN, 20)
    end

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
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0); phase = "s1_clr"
        end
    elseif phase == "s1_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s1_w" end
    elseif phase == "s1_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "skip_s2" end
    elseif phase == "skip_s2" then
        if frame >= phaseFrame + 12 and scene == PLAY and read(CAMPAIGN_STAGE) == 2 then
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0); phase = "s2_clr"
        end
    elseif phase == "s2_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s2_w" end
    elseif phase == "s2_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "skip_s3" end
    elseif phase == "skip_s3" then
        if frame >= phaseFrame + 12 and scene == PLAY and read(CAMPAIGN_STAGE) == 3 then
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0); phase = "s3_clr"
        end
    elseif phase == "s3_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s3_w" end
    elseif phase == "s3_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "skip_s4" end
    elseif phase == "skip_s4" then
        if frame >= phaseFrame + 12 and scene == PLAY and read(CAMPAIGN_STAGE) == 4 then
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0); phase = "s4_clr"
        end
    elseif phase == "s4_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s4_w" end
    elseif phase == "s4_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "s5_ready" end

    -- 1. Verify Stage 5 ball platform geometry (arcade support $05 var 5, 6, 4)
    elseif phase == "s5_ready" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 5 then
            local p0y = read(PLAT_Y_LO + 0) + 256 * read(PLAT_Y_HI + 0)
            local p1y = read(PLAT_Y_LO + 1) + 256 * read(PLAT_Y_HI + 1)
            local p2y = read(PLAT_Y_LO + 2) + 256 * read(PLAT_Y_HI + 2)
            if not check(p0y == 579 and read(PLAT_SPAN + 0) == 208, string.format("S5 plat 0 expected y=579 span=208, got y=%d span=%d", p0y, read(PLAT_SPAN + 0))) then return end
            if not check(p1y == 659 and read(PLAT_SPAN + 1) == 48,  string.format("S5 plat 1 expected y=659 span=48, got y=%d span=%d", p1y, read(PLAT_SPAN + 1))) then return end
            if not check(p2y == 499 and read(PLAT_SPAN + 2) == 172, string.format("S5 plat 2 expected y=499 span=172, got y=%d span=%d", p2y, read(PLAT_SPAN + 2))) then return end
            nlIdx = 1
            placeArthur(noLadderCols[1].x, 1136, noLadderCols[1].cam, 4)
            phaseFrame = frame
            phase = "s5_no_phantom_ladder"
        end

    -- 2. Verify NO phantom ladders exist in empty air on previously-corrupted columns
    -- Release Up for 3 frames before each probe so ap_stick resets to 0 (fresh Up press every column).
    elseif phase == "s5_no_phantom_ladder" then
        local dt = frame - phaseFrame
        if dt >= 3 then input = {up = true} end
        if dt >= 5 then
            local entry = noLadderCols[nlIdx]
            if not check(read(AP_STATE) ~= STATE_LADDER and playerY() == 1136,
                "Phantom ladder triggered at Stage 5 bottom floor " .. entry.label) then return end
        end
        if dt >= 12 then
            nlIdx = nlIdx + 1
            if nlIdx <= #noLadderCols then
                placeArthur(noLadderCols[nlIdx].x, 1136, noLadderCols[nlIdx].cam, 4)
                phaseFrame = frame
            else
                -- Move to real ladder 1 right half (col 13, X=216, Y=1136)
                placeArthur(216, 1136, 96, 4)
                phaseFrame = frame
                phase = "s5_climb_col13"
            end
        end

    -- 3. Verify climbing real ladder right half at col 13 (snaps to center X=208, climbs to Y=1056)
    elseif phase == "s5_climb_col13" then
        local dt = frame - phaseFrame
        if dt >= 3 then input = {up = true} end
        if dt == 10 then
            if not check(read(AP_STATE) == STATE_LADDER and readWord(PLAYER_X) == 208,
                "Stage 5 ladder col 13 must snap to center X=208 and enter ladder state") then return end
        end
        if dt >= 14 and read(AP_STATE) == STATE_GROUND and playerY() == 1056 then
            -- Next: verify right-side ladder right half at col 39 (X=632, Y=1136 -> snaps to 624)
            placeArthur(632, 1136, 512, 4)
            phaseFrame = frame
            phase = "s5_climb_col39"
        elseif not check(dt < 130, "Stage 5 ladder col 13 failed to climb to Y=1056") then
            return
        end

    elseif phase == "s5_climb_col39" then
        local dt = frame - phaseFrame
        if dt >= 3 then input = {up = true} end
        if dt == 12 then
            if not check(read(AP_STATE) == STATE_LADDER and readWord(PLAYER_X) == 624 and playerY() < 1136,
                "Stage 5 ladder col 39 must snap to X=624 and climb up") then return end
            -- Next: bottom-right wall test (must pass col 44 overhang at r=75, stop at col 46 wall!)
            placeArthur(680, 1136, 512, 4)
            phaseFrame = frame
            phase = "s5_wall_col46"
        end

    -- 4. Verify bottom-right wall (col 46): passes r=75 col 44 overhang, stops at col 46 (X <= 728)
    elseif phase == "s5_wall_col46" then
        input = {right = true}
        local dt = frame - phaseFrame
        if dt >= 60 then
            local x = readWord(PLAYER_X)
            if not check(x > 704, string.format("Stage 5 col 44 overhang falsely blocked Arthur at X=%d", x)) then return end
            if not check(x <= 728, string.format("Stage 5 col 46 outer wall failed to block Arthur (X=%d)", x)) then return end
            -- Next: 4x4 stone pillar left side on floor r=72 (Y=1056, col 16 wall at X=256)
            placeArthur(224, 1056, 128, 4)
            phaseFrame = frame
            phase = "s5_pillar_left"
        end

    -- 5. Verify 4x4 stone pillar left wall (col 16, X=256 -> blocks at X <= 248)
    elseif phase == "s5_pillar_left" then
        input = {right = true}
        if frame - phaseFrame >= 40 then
            local x = readWord(PLAYER_X)
            if not check(x >= 244 and x <= 248, string.format("Stage 5 pillar left wall (col 16) expected stop 244..248, got X=%d", x)) then return end
            -- Next: 4x4 stone pillar right side (col 19 wall at X=304..319 -> blocks moving left at X >= 328)
            placeArthur(348, 1056, 224, 4)
            phaseFrame = frame
            phase = "s5_pillar_right"
        end

    -- 6. Verify 4x4 stone pillar right wall (col 19 -> blocks at X >= 328)
    elseif phase == "s5_pillar_right" then
        input = {left = true}
        if frame - phaseFrame >= 40 then
            local x = readWord(PLAYER_X)
            if not check(x >= 328 and x <= 332, string.format("Stage 5 pillar right wall (col 19) expected stop 328..332, got X=%d", x)) then return end
            -- Next: top boss-floor left castle wall (col 13 at r=11, Y=96 -> blocks at X >= 232)
            placeArthur(260, 96, 128, 0)
            phaseFrame = frame
            phase = "s5_top_wall_col13"
        end

    -- 7. Verify top boss floor left wall (col 13 -> blocks at X >= 232)
    elseif phase == "s5_top_wall_col13" then
        input = {left = true}
        if frame - phaseFrame >= 40 then
            local x = readWord(PLAYER_X)
            if not check(x >= 232 and x <= 236, string.format("Stage 5 boss floor left wall (col 13) expected stop 232..236, got X=%d", x)) then return end
            -- Advance to Stage 6 to verify Stage 6 5-page nibble map too!
            write(AP_WEAPON, 3)
            write(KEY_COLLECTED, 1); write(AP_CLEAR_SEQ, 0)
            phase = "s5_clr"
        end

    elseif phase == "s5_clr" then
        if scene == CLEAR then phaseFrame = frame; phase = "s5_w" end
    elseif phase == "s5_w" then
        if frame == phaseFrame + 12 then input = {start = true}; phaseFrame = frame; phase = "s6_ready" end

    -- 8. Verify Stage 6 5-page nibble map: no phantom ladder at col 7, solid right wall at col 24
    elseif phase == "s6_ready" then
        if frame >= phaseFrame + 15 and scene == PLAY and read(CAMPAIGN_STAGE) == 6 then
            placeArthur(120, 1120, 0, 4)
            phaseFrame = frame
            phase = "s6_no_phantom"
        end
    elseif phase == "s6_no_phantom" then
        input = {up = true}
        local dt = frame - phaseFrame
        if dt >= 2 then
            if not check(read(AP_STATE) ~= STATE_LADDER and playerY() == 1120,
                "Stage 6 col 7 triggered phantom ladder") then return end
        end
        if dt >= 10 then
            placeArthur(348, 1120, 256, 4)
            phaseFrame = frame
            phase = "s6_wall_col24"
        end
    elseif phase == "s6_wall_col24" then
        input = {right = true}
        if frame - phaseFrame >= 45 then
            local x = readWord(PLAYER_X)
            if not check(x >= 372 and x <= 376, string.format("Stage 6 right wall (col 24) expected stop 372..376, got X=%d", x)) then return end
            print(string.format("PASS frame=%d (Stage 5/6 5-page map stride, ladders, walls, overhangs, ball platforms)", frame))
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
