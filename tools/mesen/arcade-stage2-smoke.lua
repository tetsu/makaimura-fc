-- Arcade Stage 1 (horizontal, Bank 12) -> Stage 2 (vertical climbing, Bank 13)
-- end-to-end 1P-input smoke test:
-- 1. Clears Stage 1 and verifies score carry-over into Stage 2 (Bank 13).
-- 2. Verifies Stage 2 distinct BG/OBJ palettes and custom CHR tiles.
-- 3. Defeats Stage 2 ground enemy to trigger 2,000-pt 1UP extend (lives 3 -> 4).
-- 4. Climbs vertical ladders across page 1 -> page 0 (vertical camera tracking),
--    defeats pot enemy for Dagger drop, reaches vertical checkpoint (Y <= 208).
-- 5. Triggers Stage 2 top-floor boss (Y <= 144), verifies <= 8 sprites/scanline,
--    defeats boss, collects Key, and verifies FINAL CLEAR transition.

local AP_STATE = 0x6010
local AP_ARMOR = 0x6011
local AP_WEAPON = 0x601A
local AP_RIDING = 0x601B
local AP_ENEMY1 = 0x6018
local AP_POT_ENEMY = 0x601C
local PLAT_X_LO = 0x6042
local PLAT_X_HI = 0x6046
local EB_ACTIVE = 0x6095
local EB_X_LO = 0x6097
local EB_X_HI = 0x6099
local EB_Y = 0x609B
local AP_BOSS_STATE = 0x60A1
local AP_BOSS_LOCK = 0x60A2
local AP_BOSS_HP = 0x60A3
local AP_BOSS_SHIELD = 0x60A4
local AP_KEY_COLLECTED = 0x60AB
local AP_PLAYER_Y_HI = 0x60AC
local AP_SCORE = 0x60C4
local SND_SONG_ID = 0x7800

local frame = 0
local phase = "boot"
local phaseFrame = 0
local input = {}

local function read(addr)
    return emu.read(addr, emu.memType.nesMemory)
end

local function readPpu(addr)
    return emu.read(addr, emu.memType.nesPpuDebug)
end

local function readNt(addr)
    return emu.read(addr, emu.memType.nesNametableRam)
end

local function check(cond, code, msg)
    if not cond then
        print(string.format("FAIL(%d) frame=%d phase=%s(%d): %s", code, frame, phase, phaseFrame, msg))
        emu.stop(code)
    end
end

local function nextPhase(name)
    phase = name
    phaseFrame = 0
end

local function playerScreenX()
    return read(0x203)
end

local function playerScreenY()
    return read(0x200)
end

local function platX(slot)
    return read(PLAT_X_HI + slot) * 256 + read(PLAT_X_LO + slot)
end

local function lives()
    return readNt(0x002e) - 0x1b
end

local function stageNum()
    return readNt(0x0027) - 0x1b
end

local function cpReached()
    return readNt(0x003c) == 0x17
end

local function scorePts()
    local d3 = read(AP_SCORE + 0)
    local d2 = read(AP_SCORE + 1)
    local d1 = read(AP_SCORE + 2)
    local d0 = read(AP_SCORE + 3)
    return (d3 * 1000 + d2 * 100 + d1 * 10 + d0) * 100
end

local function checkScanlineSpriteLimit()
    local counts = {}
    for i = 0, 63 do
        local y = read(0x200 + i * 4)
        if y < 0xef then
            for row = y + 1, y + 16 do
                if row < 240 then
                    counts[row] = (counts[row] or 0) + 1
                    if counts[row] > 8 then
                        return false, row, counts[row]
                    end
                end
            end
        end
    end
    return true, 0, 0
end

local function dodgeBolts(inp, pWorldX, lowThresh)
    for i = 0, 1 do
        if read(EB_ACTIVE + i) ~= 0 then
            local bx = read(EB_X_HI + i) * 256 + read(EB_X_LO + i)
            local by = read(EB_Y + i)
            local dx = bx - pWorldX
            if dx > 0 and dx < 36 and read(AP_STATE) == 0 then
                if by >= lowThresh then
                    inp.a = true
                else
                    inp.down = true
                end
                return true
            end
        end
    end
    return false
end

local killedS1Enemy1 = false
local sawExtend = false
local sawPage0 = false

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
            nextPhase("s1_start")
        end

    -- Stage 1: Clear horizontal stage to reach Stage 2 with >= 1900 pts.
    elseif phase == "s1_start" then
        if phaseFrame == 1 then
            input = { right = true, start = true }
        elseif phaseFrame == 20 then
            check(read(0x8000) == 0x8c, 10, "stage 1 did not load Bank 12")
            check(scorePts() == 0 and lives() == 3, 11, "initial score/lives mismatch")
            killedS1Enemy1 = false
            nextPhase("s1_reach_cp")
        end
    elseif phase == "s1_reach_cp" then
        input = { right = true }
        if phaseFrame < 140 and playerScreenX() >= 148 and playerScreenX() <= 154 and read(AP_STATE) == 0 then
            input.a = true
        end
        if phaseFrame == 215 then input.a = true end
        if not killedS1Enemy1 and phaseFrame >= 275 and phaseFrame <= 310 and phaseFrame % 8 == 0 then
            input.b = true
        end
        if phaseFrame == 412 then input.a = true end
        if phaseFrame == 460 or phaseFrame == 476 then input.b = true end
        if read(AP_ENEMY1) == 0 then killedS1Enemy1 = true end
        if phaseFrame == 530 then
            nextPhase("s1_board_hplat")
        end
    elseif phase == "s1_board_hplat" then
        local px = platX(1)
        if phaseFrame < 10 then
            input = { right = true }
        elseif px >= 502 and px <= 514 and read(AP_STATE) == 0 then
            input = { right = true, a = true }
            nextPhase("s1_cross_pit")
        elseif phaseFrame > 140 then
            check(false, 12, "s1 timed out waiting for hplat")
        end
    elseif phase == "s1_cross_pit" then
        if read(AP_RIDING) == 1 and platX(1) >= 530 then
            input = { right = true, a = true }
            nextPhase("s1_trigger_boss")
        elseif phaseFrame > 140 then
            check(false, 13, "s1 did not cross pit")
        end
    elseif phase == "s1_trigger_boss" then
        if phaseFrame >= 36 then
            input = { right = true }
            if phaseFrame % 28 == 0 then input.b = true end
            if playerScreenX() >= 164 and playerScreenX() <= 174 and read(AP_STATE) == 0 then
                input.a = true
            end
        end
        if read(AP_BOSS_STATE) == 1 then
            nextPhase("s1_step_back")
        elseif phaseFrame > 360 then
            check(false, 14, "s1 boss did not trigger")
        end
    elseif phase == "s1_step_back" then
        if phaseFrame <= 54 then
            input = { left = true }
        elseif phaseFrame == 55 then
            input = { left = true, a = true }
        elseif phaseFrame <= 96 then
            input = { left = true }
        elseif read(AP_STATE) == 0 then
            input = { right = true }
            nextPhase("s1_fight_boss")
        elseif phaseFrame > 140 then
            check(false, 15, "s1 step_back stuck")
        end
    elseif phase == "s1_fight_boss" then
        local pWorldX = 512 + playerScreenX() + 8
        dodgeBolts(input, pWorldX, 162)
        if phaseFrame % 12 == 1 then input.b = true end
        if read(AP_BOSS_STATE) == 2 then
            nextPhase("s1_collect_key")
        elseif phaseFrame > 360 then
            check(false, 16, "s1 boss not defeated")
        end
    elseif phase == "s1_collect_key" then
        input = { right = true }
        if read(AP_KEY_COLLECTED) == 1 then
            nextPhase("s1_advance_to_s2")
        elseif phaseFrame > 220 then
            check(false, 17, "s1 key not collected")
        end
    elseif phase == "s1_advance_to_s2" then
        if phaseFrame == 20 then
            input = { start = true }
        elseif phaseFrame == 44 then
            check(read(0x8000) == 0x8d, 18, "stage 2 did not load Bank 13")
            check(stageNum() == 2, 19, "stage 2 HUD number mismatch")
            check(scorePts() >= 1900, 20, "score did not carry into stage 2 (" .. scorePts() .. ")")
            check(read(AP_PLAYER_Y_HI) == 1, 21, "stage 2 did not start on vertical page 1")
            -- Verify Stage 2 distinct BG palette ($2C at $3F01 vs Stage 1's $2A)
            check(readPpu(0x3f01) == 0x2c, 22, "stage 2 custom BG palette not loaded")
            nextPhase("s2_tier0_fight_and_climb")
        end

    -- Stage 2 Tier 0 (Y=448): kill ground enemy (+200 -> >=2100 -> 1UP!), walk to right ladder (col 12, X=200)
    elseif phase == "s2_tier0_fight_and_climb" then
        if lives() >= 4 then sawExtend = true end
        local sx = playerScreenX()
        if read(AP_ENEMY1) ~= 0 then
            if sx < 92 then
                input = { right = true }
            elseif phaseFrame % 18 == 1 then
                input = { b = true }
            end
        elseif sx < 190 then
            input = { right = true }
        else
            input = { up = true }
            if read(AP_STATE) == 2 then
                nextPhase("s2_climb_to_tier1")
            end
        end
        if phaseFrame > 320 then
            check(false, 24, "s2 tier 0 did not reach right ladder")
        end
    elseif phase == "s2_climb_to_tier1" then
        if lives() >= 4 then sawExtend = true end
        input = { up = true }
        if read(AP_STATE) == 0 and phaseFrame > 20 then
            check(sawExtend, 25, "1UP extend did not trigger at 2000 pts (score=" .. scorePts() .. " lives=" .. lives() .. ")")
            nextPhase("s2_tier1_pot_and_left")
        elseif phaseFrame > 160 then
            check(false, 26, "s2 did not finish climbing to tier 1")
        end

    -- Stage 2 Tier 1 (Y=368): kill pot enemy (X=112) for Dagger, walk to left ladder (col 3, X=56)
    elseif phase == "s2_tier1_pot_and_left" then
        local sx = playerScreenX()
        if read(AP_POT_ENEMY) ~= 0 then
            if sx > 152 then
                input = { left = true }
            elseif phaseFrame % 18 == 1 then
                input = { b = true }
            end
        elseif sx > 52 then
            input = { left = true }
        else
            input = { up = true }
            if read(AP_STATE) == 2 then
                check(read(AP_WEAPON) == 1, 27, "stage 2 pot enemy did not grant Dagger")
                nextPhase("s2_climb_to_tier2")
            end
        end
        if phaseFrame > 320 then
            check(false, 28, "s2 tier 1 did not reach left ladder")
        end
    elseif phase == "s2_climb_to_tier2" then
        input = { up = true }
        if read(AP_STATE) == 0 and phaseFrame > 20 then
            nextPhase("s2_tier2_to_right_ladder")
        elseif phaseFrame > 160 then
            check(false, 29, "s2 did not finish climbing to tier 2")
        end

    -- Stage 2 Tier 2 (Y=288): walk right to col 12 (X=200), climb across page 1 -> page 0 to Y=208 (CP)
    elseif phase == "s2_tier2_to_right_ladder" then
        local sx = playerScreenX()
        if sx < 190 then
            input = { right = true }
        else
            input = { up = true }
            if read(AP_STATE) == 2 then
                nextPhase("s2_climb_across_page")
            end
        end
        if phaseFrame > 260 then
            check(false, 30, "s2 tier 2 did not reach right ladder")
        end
    elseif phase == "s2_climb_across_page" then
        input = { up = true }
        if read(AP_PLAYER_Y_HI) == 0 then sawPage0 = true end
        if read(AP_STATE) == 0 and phaseFrame > 20 then
            check(sawPage0, 31, "did not cross vertical page 1 -> page 0")
            check(cpReached(), 32, "stage 2 vertical checkpoint (Y<=208) not marked")
            nextPhase("s2_tier3_to_left_ladder")
        elseif phaseFrame > 200 then
            check(false, 33, "s2 did not finish climbing across page boundary")
        end

    -- Stage 2 Tier 3 (Y=208): walk left to col 3 (X=56), climb to top boss arena (Y=144)
    elseif phase == "s2_tier3_to_left_ladder" then
        local sx = playerScreenX()
        if sx > 52 then
            input = { left = true }
        else
            input = { up = true }
            if read(AP_STATE) == 2 then
                nextPhase("s2_climb_to_boss")
            end
        end
        if phaseFrame > 260 then
            check(false, 34, "s2 tier 3 did not reach left ladder")
        end
    elseif phase == "s2_climb_to_boss" then
        input = { up = true }
        if read(AP_BOSS_STATE) == 1 then
            check(read(AP_BOSS_LOCK) == 1, 35, "stage 2 boss lock not engaged")
            check(read(SND_SONG_ID) == 3, 36, "stage 2 boss BGM did not start")
            nextPhase("s2_fight_boss")
        elseif phaseFrame > 180 then
            check(false, 37, "stage 2 top-floor boss did not trigger")
        end

    -- Stage 2 Boss Fight: verify <= 8 sprites/scanline, dodge bolts, defeat boss, collect key
    elseif phase == "s2_fight_boss" then
        if phaseFrame == 1 then
            -- The head pair (tiles $77 and $79) is in the priority block, slots 4-12.
            local headLeft, headRight = 0, 0
            for s = 4, 12 do
                local tile = read(0x201 + s * 4)
                if tile == 0x77 then headLeft = tile elseif tile == 0x79 then headRight = tile end
            end
            check(headLeft % 2 == 1 and headRight % 2 == 1
                and readPpu(0x1000 + (headLeft & 0xfe) * 16) == 0x20
                and readPpu(0x1000 + (headRight & 0xfe) * 16) == 0x04,
                23, "stage 2 boss OAM does not reference the custom CHR")
        end
        local ok, row, cnt = checkScanlineSpriteLimit()
        check(ok, 38, string.format("stage 2 boss scanline %d exceeded 8 sprites (%d)", row, cnt))
        local pWorldX = playerScreenX() + 8
        if phaseFrame == 1 then
            input = { right = true }
        else
            dodgeBolts(input, pWorldX, 130)
            if phaseFrame % 12 == 1 then input.b = true end
        end
        if read(AP_BOSS_STATE) == 2 then
            nextPhase("s2_collect_key")
        elseif phaseFrame > 420 then
            check(false, 39, "stage 2 boss not defeated")
        end
    elseif phase == "s2_collect_key" then
        input = { right = true }
        if read(AP_KEY_COLLECTED) == 1 then
            nextPhase("s2_verify_stage3_advance")
        elseif phaseFrame > 260 then
            check(false, 40, "stage 2 key not collected")
        end
    elseif phase == "s2_verify_stage3_advance" then
        if phaseFrame == 12 then
            check(read(SND_SONG_ID) == 4, 41, "stage 2 clear fanfare did not play")
            -- Check COURSE CLEAR title on nametable ($218A -> offset $018A: C=$04, O=$0f, U=$14)
            check(readNt(0x018a) == 0x04 and readNt(0x018b) == 0x0f and readNt(0x018c) == 0x14, 42,
                "COURSE CLEAR title not displayed after Stage 2")
        elseif phaseFrame == 20 then
            input = { start = true }
        elseif phaseFrame == 44 then
            check(read(0x8000) == 0x8e, 43, "stage 3 did not load Bank 14")
            check(stageNum() == 3, 44, "stage 3 HUD number mismatch")
            check(readPpu(0x3f01) == 0x21, 45, "stage 3 custom BG palette not loaded")
            emu.stop(0)
        end
    end

    if frame > 5000 then
        check(false, 99, "arcade-stage2-smoke timed out")
    end
end, emu.eventType.endFrame)
