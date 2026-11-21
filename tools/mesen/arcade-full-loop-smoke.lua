-- Full 7-Stage + Required-Weapon Bounce-Back + 2-Loop + True Ending smoke test (pure 1P input):
-- 1. Plays Loop 1 from Title (Right+START) through Stages 1 -> 2 -> 3 -> 4 -> 5 -> 6.
-- 2. On first Stage 6 visit, skips the pot enemy (no Holy Shield), defeats Stage 6 boss,
--    and verifies bounce-back to Stage 5 (Bank 16, ap_bounce_back == 1).
-- 3. Re-clears Stage 5 -> Stage 6, kills pot enemy to collect Holy Shield (weapon 3, HUD 'S'),
--    defeats Stage 6 boss, and verifies advance to Stage 7 (Bank 18, retains Holy Shield).
-- 4. Defeats Stage 7 Demon King on Loop 1 (<= 8 sprites/scanline) -> COURSE CLEAR ->
--    verifies Loop 2 Stage 1 (Bank 12, ap_loop == 1, HUD LP 2, ap_rank >= 1, score/lives carried).
-- 5. Plays Loop 2 through Stages 1 -> 2 -> 3 -> 4 -> 5 -> 6 (with Holy Shield) -> Stage 7,
--    defeats Demon King on Loop 2 -> verifies FINAL CLEAR -> START returns to Title.

local AP_STATE = 0x6010
local AP_SHOOT_LOCK = 0x6014
local AP_WEAPON = 0x601A
local AP_RIDING = 0x601B
local AP_ENEMY1 = 0x6018
local AP_POT_ENEMY = 0x601C
local PLAT_X_LO = 0x6042
local PLAT_X_HI = 0x6046
local AP_RANK = 0x6072
local EB_ACTIVE = 0x6095
local EB_X_LO = 0x6097
local EB_X_HI = 0x6099
local EB_Y = 0x609B
local AP_BOSS_STATE = 0x60A1
local AP_BOSS_X_LO = 0x60A5
local AP_BOSS_X_HI = 0x60A6
local AP_KEY_COLLECTED = 0x60AB
local AP_SCORE = 0x60C4
local AP_LOOP = 0x60CE
local AP_BOUNCE_BACK = 0x60CF

local frame = 0
local phase = "boot"
local phaseFrame = 0
local input = {}

local didBounceTest = false
local killedEnemy1 = false
local killedPot = false
local walkDist = 0
local prevPlatX = 0
local loop1Score = 0
local loop1Lives = 0

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
        print(string.format("FAIL(%d) frame=%d phase=%s(%d) bank=$%02X stg=%d loop=%d: %s",
            code, frame, phase, phaseFrame, read(0x8000), readNt(0x0027) - 0x1b, read(AP_LOOP), msg))
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

local function hudWeaponGlyph()
    return readNt(0x003e)
end

local function hudLoopNum()
    return readNt(0x0056) - 0x1b
end

local function scorePts()
    return (read(AP_SCORE) * 1000 + read(AP_SCORE + 1) * 100 + read(AP_SCORE + 2) * 10 + read(AP_SCORE + 3)) * 100
end

local function checkScanlineSpriteLimit()
    local counts = {}
    for i = 0, 30 do
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

-- Vertical tower runner shared by Stage 2 (col 12/3), Stage 5 (col 11/4), Stage 6 (col 12/3).
local vCfg = {}
local function startVerticalStage(bankTag, stg, pal01, rightLadX, leftLadX, skipPot, expectShield, nextAfterKey)
    vCfg = {
        bankTag = bankTag,
        stg = stg,
        pal01 = pal01,
        rightLadX = rightLadX,
        leftLadX = leftLadX,
        skipPot = skipPot,
        expectShield = expectShield,
        nextAfterKey = nextAfterKey
    }
    nextPhase("v_tier0")
end

local function watch(address, callback)
    emu.addMemoryCallback(callback, emu.callbackType.write, address, address, emu.memType.nesMemory)
end

local playing = false
local displayChanged = false
local stablePlayFrames = 0
local dmaThisFrame = 0
local uploadsThisFrame = 0
local splitStep = nil
local overallWorstBudget = 0
local totalCheckedFrames = 0
local stageWorstBudget = {}
local stageMaxUploads = {}

watch(0x2001, function(_, value)
    playing = (value == 0x18)
    displayChanged = true
    stablePlayFrames = 0
end)

watch(0x2007, function()
    if not playing or displayChanged then return end
    local line = emu.getState()["ppu.scanline"]
    check(line >= 241 and line <= 260, 80, string.format("VRAM upload outside VBlank (scanline=%d)", line))
    uploadsThisFrame = uploadsThisFrame + 1
end)

watch(0x4014, function(_, value)
    if not playing or displayChanged then return end
    local state = emu.getState()
    local dots = (state["ppu.scanline"] - 241) * 341 + state["ppu.cycle"]
    local budget = math.ceil(dots / 3) + 514
    check(value == 2 and dots >= 0 and budget < 2273, 81,
        string.format("OAM DMA VBlank budget exceeded (val=$%02X dots=%d budget=%d)", value, dots, budget))
    if budget > overallWorstBudget then overallWorstBudget = budget end
    local stg = stageNum()
    if stg >= 1 and stg <= 7 then
        if budget > (stageWorstBudget[stg] or 0) then stageWorstBudget[stg] = budget end
        if uploadsThisFrame > (stageMaxUploads[stg] or 0) then stageMaxUploads[stg] = uploadsThisFrame end
    end
    dmaThisFrame = dmaThisFrame + 1
end)

watch(0xf010, function()
    if playing and not displayChanged then
        splitStep = 1
    end
end)

watch(0x2006, function()
    if splitStep == 1 then
        splitStep = 2
    elseif splitStep == 4 then
        local state = emu.getState()
        check(state["ppu.scanline"] == 31 and state["ppu.cycle"] < 255, 82,
            string.format("HUD split missed scanline 31 (sl=%d cyc=%d)", state["ppu.scanline"], state["ppu.cycle"]))
        splitStep = nil
    end
end)

watch(0x2005, function()
    if splitStep == 2 then
        splitStep = 3
    elseif splitStep == 3 then
        splitStep = 4
    end
end)

emu.addEventCallback(function()
    emu.setInput(input, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    phaseFrame = phaseFrame + 1
    input = {}

    if playing and not displayChanged then
        stablePlayFrames = stablePlayFrames + 1
        if stablePlayFrames >= 2 then
            totalCheckedFrames = totalCheckedFrames + 1
            check(dmaThisFrame == 1, 83,
                string.format("1-frame slowdown detected (dmaThisFrame=%d)", dmaThisFrame))
        end
    end
    displayChanged = false
    dmaThisFrame = 0
    uploadsThisFrame = 0
    splitStep = nil

    if phase == "boot" then
        if frame == 128 then
            if read(0xe000) ~= 0xbf then
                emu.stop(0)
                return
            end
            nextPhase("start_game")
        end

    elseif phase == "start_game" then
        if phaseFrame == 1 then
            input = { right = true, start = true }
        elseif phaseFrame == 20 then
            check(read(0x8000) == 0x8c and stageNum() == 1, 10, "did not start Stage 1 Bank 12")
            check(read(AP_LOOP) == 0 and hudLoopNum() == 1, 11, "initial loop != 1")
            killedEnemy1 = false
            nextPhase("s1_reach_cp")
        end

    -- ==================== STAGE 1 (Bank 12) ====================
    elseif phase == "s1_reach_cp" then
        input = { right = true }
        if phaseFrame < 140 and playerScreenX() >= 148 and playerScreenX() <= 154 and read(AP_STATE) == 0 then
            input.a = true
        end
        if phaseFrame == 215 then input.a = true end
        if not killedEnemy1 and phaseFrame >= 275 and phaseFrame <= 310 and phaseFrame % 8 == 0 then
            input.b = true
        end
        if phaseFrame == 412 then input.a = true end
        if phaseFrame == 460 or phaseFrame == 476 then input.b = true end
        if read(AP_ENEMY1) == 0 then killedEnemy1 = true end
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
        elseif phaseFrame > 160 then
            check(false, 12, "s1 timed out waiting for hplat")
        end
    elseif phase == "s1_cross_pit" then
        if read(AP_RIDING) == 1 and platX(1) >= 530 then
            input = { right = true, a = true }
            nextPhase("s1_trigger_boss")
        elseif phaseFrame > 160 then
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
            nextPhase("h_fight_boss")
        elseif phaseFrame > 140 then
            check(false, 15, "s1 step_back stuck")
        end

    -- Shared horizontal boss fight (Stage 1, 3, 4)
    elseif phase == "h_fight_boss" then
        local ok, row, cnt = checkScanlineSpriteLimit()
        check(ok, 16, string.format("horiz boss scanline %d exceeded 8 sprites (%d)", row, cnt))
        local pWorldX = 512 + playerScreenX() + 8
        dodgeBolts(input, pWorldX, 162)
        if phaseFrame % 12 == 1 then input.b = true end
        if read(AP_BOSS_STATE) == 2 then
            nextPhase("h_collect_key")
        elseif phaseFrame > 460 then
            check(false, 17, "horizontal boss not defeated")
        end
    elseif phase == "h_collect_key" then
        input = { right = true }
        if read(AP_KEY_COLLECTED) == 1 then
            nextPhase("advance_after_clear")
        elseif phaseFrame > 240 then
            check(false, 18, "horizontal key not collected")
        end

    -- ==================== STAGE 3 (Bank 14) ====================
    elseif phase == "s3_run_to_cp" then
        input = { right = true }
        local st = read(AP_STATE)
        local estX = 48 + walkDist
        -- Pit 1 at X=128..159: jump at estX=126
        if estX >= 125 and estX <= 128 and st == 0 then
            input.a = true
        -- Enemy 1 at X=200 & Pot enemy at X=384
        elseif read(AP_ENEMY1) ~= 0 and estX >= 164 and estX <= 192 and read(AP_SHOOT_LOCK) == 0 then
            input.b = true
        elseif read(AP_POT_ENEMY) ~= 0 and estX >= 336 and estX <= 376 and read(AP_SHOOT_LOCK) == 0 then
            input.b = true
        -- Pit 2 at X=416..447: jump at estX=414
        elseif estX >= 413 and estX <= 416 and st == 0 then
            input.a = true
        end
        if st == 1 or (st == 0 and read(AP_SHOOT_LOCK) == 0 and not input.b) then
            walkDist = walkDist + 1
        end
        if cpReached() then
            prevPlatX = platX(0)
            nextPhase("s3_board_ferry")
        elseif phaseFrame > 700 then
            check(false, 20, "s3 did not reach checkpoint (estX=" .. estX .. " st=" .. st .. ")")
        end
    elseif phase == "s3_board_ferry" then
        local px = platX(0)
        local movingLeft = px < prevPlatX
        prevPlatX = px
        if phaseFrame <= 12 then
            input = { right = true }
        elseif movingLeft and px >= 504 and px <= 514 and read(AP_STATE) == 0 then
            input = { right = true, a = true }
            nextPhase("s3_cross_water")
        elseif phaseFrame > 200 then
            check(false, 21, "s3 timed out waiting for ferry px=" .. px)
        end
    elseif phase == "s3_cross_water" then
        if read(AP_RIDING) == 0 and platX(0) >= 544 then
            input = { right = true, a = true }
            nextPhase("s34_trigger_boss")
        elseif phaseFrame > 180 then
            check(false, 22, "s3 did not cross waterway riding=" .. read(AP_RIDING) .. " px=" .. platX(0))
        end

    -- ==================== STAGE 4 (Bank 15) ====================
    elseif phase == "s4_to_chasm1" then
        local sx = playerScreenX()
        if sx < 98 then
            input = { right = true }
        else
            prevPlatX = platX(0)
            nextPhase("s4_board_chasm1")
        end
        if phaseFrame > 120 then check(false, 23, "s4 did not reach chasm 1") end
    elseif phase == "s4_board_chasm1" then
        local px = platX(0)
        local movingLeft = px < prevPlatX
        prevPlatX = px
        if movingLeft and px >= 120 and px <= 130 and read(AP_STATE) == 0 then
            input = { right = true, a = true }
            nextPhase("s4_cross_chasm1")
        elseif phaseFrame > 200 then
            check(false, 24, "s4 timed out waiting for chasm 1 platform px=" .. px)
        end
    elseif phase == "s4_cross_chasm1" then
        if read(AP_RIDING) == 0 and platX(0) >= 164 then
            input = { right = true, a = true }
            walkDist = 0
            nextPhase("s4_mid_bridge")
        elseif phaseFrame > 180 then
            check(false, 25, "s4 did not cross chasm 1 riding=" .. read(AP_RIDING) .. " px=" .. platX(0))
        end
    elseif phase == "s4_mid_bridge" then
        local st = read(AP_STATE)
        -- Started jump off platform at X~156 -> estX = 156 + walkDist
        local estX = 156 + walkDist
        if read(AP_ENEMY1) ~= 0 and estX >= 248 and st == 0 then
            if phaseFrame % 16 == 1 then input.b = true end
        elseif read(AP_POT_ENEMY) ~= 0 and estX >= 288 and st == 0 then
            if phaseFrame % 16 == 1 then input.b = true end
        else
            input = { right = true }
            -- Tombstone at X=224: jump at estX=208
            if estX >= 207 and estX <= 210 and st == 0 then
                input.a = true
            -- Lava fissure at X=352..383: jump at estX=350
            elseif estX >= 349 and estX <= 352 and st == 0 then
                input.a = true
            end
            if st == 1 or (st == 0 and read(AP_SHOOT_LOCK) == 0) then
                walkDist = walkDist + 1
            end
        end
        if cpReached() then
            prevPlatX = platX(2)
            nextPhase("s4_board_chasm2")
        elseif phaseFrame > 600 then
            check(false, 26, "s4 did not reach checkpoint (estX=" .. estX .. " st=" .. st .. ")")
        end
    elseif phase == "s4_board_chasm2" then
        local px = platX(2)
        local movingLeft = px < prevPlatX
        prevPlatX = px
        if phaseFrame <= 12 then
            input = { right = true }
        elseif movingLeft and px >= 504 and px <= 514 and read(AP_STATE) == 0 then
            input = { right = true, a = true }
            nextPhase("s4_cross_chasm2")
        elseif phaseFrame > 200 then
            check(false, 27, "s4 timed out waiting for chasm 2 platform px=" .. px)
        end
    elseif phase == "s4_cross_chasm2" then
        if read(AP_RIDING) == 2 and platX(2) >= 544 then
            input = { right = true, a = true }
            nextPhase("s34_trigger_boss")
        elseif phaseFrame > 180 then
            check(false, 28, "s4 did not cross chasm 2 riding=" .. read(AP_RIDING) .. " px=" .. platX(2))
        end

    elseif phase == "s34_trigger_boss" then
        if phaseFrame >= 36 then
            input = { right = true }
            local pWorldX = 512 + playerScreenX() + 8
            if not dodgeBolts(input, pWorldX, 162) then
                if phaseFrame % 36 == 0 then input.b = true end
            end
        end
        if read(AP_BOSS_STATE) == 1 then
            nextPhase("s34_step_back")
        elseif phaseFrame > 360 then
            check(false, 29, "s3/s4 boss did not trigger sx=" .. playerScreenX() .. " st=" .. read(AP_STATE))
        end
    elseif phase == "s34_step_back" then
        local pWorldX = 512 + playerScreenX() + 8
        if phaseFrame <= 75 then
            input = { left = true }
            dodgeBolts(input, pWorldX, 162)
        elseif read(AP_STATE) == 0 then
            input = { right = true }
            nextPhase("h_fight_boss")
        elseif phaseFrame > 140 then
            check(false, 30, "s3/s4 step_back stuck st=" .. read(AP_STATE) .. " sx=" .. playerScreenX())
        end

    -- ==================== SHARED VERTICAL TOWER (Stage 2, 5, 6) ====================
    elseif phase == "v_tier0" then
        local sx = playerScreenX()
        if read(AP_ENEMY1) ~= 0 then
            if sx < 84 then
                input = { right = true }
            elseif phaseFrame % 16 == 1 then
                input = { b = true }
            end
        elseif sx < vCfg.rightLadX - 2 then
            input = { right = true }
        elseif sx > vCfg.rightLadX + 2 then
            input = { left = true }
        else
            input = { up = true }
            if read(AP_STATE) == 2 then nextPhase("v_climb1") end
        end
        if phaseFrame > 320 then check(false, 31, "v_tier0 stuck sx=" .. sx) end
    elseif phase == "v_climb1" then
        input = { up = true }
        if read(AP_STATE) == 0 and phaseFrame > 20 then
            nextPhase("v_tier1")
        elseif phaseFrame > 160 then check(false, 32, "v_climb1 stuck") end
    elseif phase == "v_tier1" then
        local sx = playerScreenX()
        if vCfg.skipPot then
            if sx > vCfg.leftLadX + 2 then
                input = { left = true }
                -- Jump cleanly over pot enemy at X=112 (sx=104) without killing it
                if sx >= 116 and sx <= 120 and read(AP_STATE) == 0 then
                    input.a = true
                end
            elseif sx < vCfg.leftLadX - 2 then
                input = { right = true }
            else
                input = { up = true }
                if read(AP_STATE) == 2 then
                    check(read(AP_WEAPON) ~= 3, 33, "accidentally picked up Shield on skip run")
                    nextPhase("v_climb2")
                end
            end
        else
            if read(AP_POT_ENEMY) ~= 0 then
                if sx > 152 then
                    input = { left = true }
                elseif phaseFrame % 16 == 1 then
                    input = { b = true }
                end
            elseif sx > vCfg.leftLadX + 2 then
                input = { left = true }
            elseif sx < vCfg.leftLadX - 2 then
                input = { right = true }
            else
                input = { up = true }
                if read(AP_STATE) == 2 then
                    if vCfg.expectShield then
                        check(read(AP_WEAPON) == 3, 34, "Stage 6 pot enemy did not grant Holy Shield")
                        check(hudWeaponGlyph() == 0x12, 35, "HUD weapon glyph is not 'S'")
                    end
                    nextPhase("v_climb2")
                end
            end
        end
        if phaseFrame > 360 then check(false, 36, "v_tier1 stuck sx=" .. sx) end
    elseif phase == "v_climb2" then
        input = { up = true }
        if read(AP_STATE) == 0 and phaseFrame > 20 then
            nextPhase("v_tier2")
        elseif phaseFrame > 160 then check(false, 37, "v_climb2 stuck") end
    elseif phase == "v_tier2" then
        local sx = playerScreenX()
        if sx < vCfg.rightLadX - 2 then
            input = { right = true }
        elseif sx > vCfg.rightLadX + 2 then
            input = { left = true }
        else
            input = { up = true }
            if read(AP_STATE) == 2 then nextPhase("v_climb3") end
        end
        if phaseFrame > 260 then check(false, 38, "v_tier2 stuck sx=" .. sx) end
    elseif phase == "v_climb3" then
        input = { up = true }
        if read(AP_STATE) == 4 then
            check(false, 40, string.format("v_climb3 DIED at pf=%d cp=%s armor=%d sx=%d sy=%d yhi=%d camy=%d,%d",
                phaseFrame, tostring(cpReached()), read(0x6011), playerScreenX(), read(0x200), read(0x60AC), read(0x60B0), read(0x60B1)))
        end
        if read(AP_STATE) == 0 and phaseFrame > 20 and cpReached() then
            nextPhase("v_tier3")
        elseif phaseFrame > 200 then
            check(false, 40, string.format("v_climb3 stuck cp=%s st=%d armor=%d sx=%d sy=%d yhi=%d",
                tostring(cpReached()), read(AP_STATE), read(0x6011), playerScreenX(), read(0x200), read(0x60AC)))
        end
    elseif phase == "v_tier3" then
        local sx = playerScreenX()
        if sx > vCfg.leftLadX + 2 then
            input = { left = true }
        elseif sx < vCfg.leftLadX - 2 then
            input = { right = true }
        else
            input = { up = true }
            if read(AP_STATE) == 2 then nextPhase("v_climb_boss") end
        end
        if phaseFrame > 260 then check(false, 41, "v_tier3 stuck sx=" .. sx) end
    elseif phase == "v_climb_boss" then
        input = { up = true }
        if read(AP_BOSS_STATE) == 1 then
            nextPhase("v_fight_boss")
        elseif phaseFrame > 180 then check(false, 42, "vertical boss did not trigger") end
    elseif phase == "v_fight_boss" then
        local ok, row, cnt = checkScanlineSpriteLimit()
        check(ok, 43, string.format("vert boss scanline %d exceeded 8 sprites (%d)", row, cnt))
        local sx = playerScreenX()
        local pWorldX = sx + 8
        if sx < 88 then
            input = { right = true }
            dodgeBolts(input, pWorldX, 130)
        else
            dodgeBolts(input, pWorldX, 130)
            if phaseFrame % 12 == 1 then input.b = true end
        end
        if read(AP_BOSS_STATE) == 2 then
            nextPhase("v_collect_key")
        elseif phaseFrame > 720 then
            check(false, 44, string.format("vertical boss not defeated hp=%d sh=%d st=%d sx=%d",
                read(0x60A3), read(0x60A4), read(AP_STATE), sx))
        end
    elseif phase == "v_collect_key" then
        input = { right = true }
        if read(AP_KEY_COLLECTED) == 1 then
            nextPhase("advance_after_clear")
        elseif phaseFrame > 260 then
            check(false, 45, "vertical key not collected")
        end

    -- ==================== STAGE 7 (Bank 18: Demon King's Throne) ====================
    elseif phase == "s7_trigger_boss" then
        input = { right = true }
        if read(AP_BOSS_STATE) == 1 then
            nextPhase("s7_fight_boss")
        elseif phaseFrame > 120 then
            check(false, 50, "Stage 7 Demon King did not trigger")
        end
    elseif phase == "s7_fight_boss" then
        local ok, row, cnt = checkScanlineSpriteLimit()
        check(ok, 51, string.format("Stage 7 scanline %d exceeded 8 sprites (%d)", row, cnt))
        local sx = playerScreenX()
        local pWorldX = sx + 8
        if sx < 88 then
            input = { right = true }
            dodgeBolts(input, pWorldX, 162)
        else
            dodgeBolts(input, pWorldX, 162)
            if phaseFrame % 12 == 1 then input.b = true end
        end
        if read(AP_BOSS_STATE) == 2 then
            nextPhase("s7_collect_key")
        elseif phaseFrame > 800 then
            check(false, 52, string.format("Stage 7 Demon King not defeated hp=%d sh=%d st=%d sx=%d",
                read(0x60A3), read(0x60A4), read(AP_STATE), sx))
        end
    elseif phase == "s7_collect_key" then
        input = { right = true }
        if read(AP_KEY_COLLECTED) == 1 then
            if read(AP_LOOP) == 0 then
                loop1Score = scorePts()
                loop1Lives = lives()
                nextPhase("s7_loop1_clear")
            else
                nextPhase("s7_true_ending")
            end
        elseif phaseFrame > 260 then
            check(false, 53, "Stage 7 key not collected")
        end
    elseif phase == "s7_loop1_clear" then
        if phaseFrame == 12 then
            -- Loop 1 clear must show COURSE CLEAR (not FINAL CLEAR)
            check(readNt(0x018a) == 0x04 and readNt(0x018b) == 0x0f and readNt(0x018c) == 0x14, 54,
                "Loop 1 Stage 7 did not show COURSE CLEAR")
        elseif phaseFrame == 20 then
            input = { start = true }
        elseif phaseFrame == 44 then
            check(read(0x8000) == 0x8c, 55, "Loop 2 did not wrap to Stage 1 Bank 12")
            check(stageNum() == 1, 56, "Loop 2 HUD stage != 1")
            check(read(AP_LOOP) == 1 and hudLoopNum() == 2, 57, "Loop 2 flag/HUD not updated")
            check(read(AP_RANK) >= 1, 58, "Loop 2 did not retain/raise difficulty rank")
            check(scorePts() >= loop1Score and lives() >= loop1Lives, 59, "Loop 2 lost score or lives")
            killedEnemy1 = false
            nextPhase("s1_reach_cp")
        end
    elseif phase == "s7_true_ending" then
        if phaseFrame == 12 then
            -- Loop 2 clear must show FINAL CLEAR (F=$07, I=$0a, N=$0e)
            check(readNt(0x018a) == 0x07 and readNt(0x018b) == 0x0a and readNt(0x018c) == 0x0e, 60,
                "Loop 2 Stage 7 did not show FINAL CLEAR")
        elseif phaseFrame == 20 then
            input = { start = true }
        elseif phaseFrame == 44 then
            -- Returned to Title screen (MAKAIMURA FC at $20CA: M=$0d, A=$02, K=$0b)
            check(readNt(0x00ca) == 0x0d and readNt(0x00cb) == 0x02 and readNt(0x00cc) == 0x0b, 61,
                "START on True Ending did not return to Title")
            local summary = string.format("VBLANK_OK checkedFrames=%d overallMax=%d", totalCheckedFrames, overallWorstBudget)
            for s = 1, 7 do
                summary = summary .. string.format(" S%d=%d(%dB)", s, stageWorstBudget[s] or 0, stageMaxUploads[s] or 0)
            end
            print(summary)
            emu.stop(0)
        end

    -- ==================== STAGE TRANSITION DISPATCHER ====================
    elseif phase == "advance_after_clear" then
        if phaseFrame == 20 then
            input = { start = true }
        elseif phaseFrame == 44 then
            local bank = read(0x8000)
            local stg = stageNum()
            if stg == 2 then
                check(bank == 0x8d and readPpu(0x3f01) == 0x2c, 70, "Stage 2 bank/pal mismatch")
                startVerticalStage(0x8d, 2, 0x2c, 192, 48, false, false, "s3")
            elseif stg == 3 then
                check(bank == 0x8e and readPpu(0x3f01) == 0x21, 71, "Stage 3 bank/pal mismatch")
                walkDist = 0
                nextPhase("s3_run_to_cp")
            elseif stg == 4 then
                check(bank == 0x8f and readPpu(0x3f01) == 0x16, 72, "Stage 4 bank/pal mismatch")
                nextPhase("s4_to_chasm1")
            elseif stg == 5 then
                check(bank == 0x90 and readPpu(0x3f01) == 0x14, 73, "Stage 5 bank/pal mismatch")
                if didBounceTest and read(AP_LOOP) == 0 then
                    check(read(AP_BOUNCE_BACK) == 1, 74, "Stage 6 bounce-back flag not set")
                end
                startVerticalStage(0x90, 5, 0x14, 176, 64, false, false, "s6")
            elseif stg == 6 then
                check(bank == 0x91 and readPpu(0x3f01) == 0x06, 75, "Stage 6 bank/pal mismatch")
                if not didBounceTest then
                    didBounceTest = true
                    -- First visit: skip Shield to verify bounce-back to Stage 5!
                    startVerticalStage(0x91, 6, 0x06, 192, 48, true, false, "s5_bounce")
                else
                    -- Second visit (and Loop 2): collect Holy Shield to advance to Stage 7!
                    startVerticalStage(0x91, 6, 0x06, 192, 48, false, true, "s7")
                end
            elseif stg == 7 then
                check(bank == 0x92 and readPpu(0x3f01) == 0x28, 76, "Stage 7 bank/pal mismatch")
                check(read(AP_WEAPON) == 3, 77, "Stage 7 did not retain Holy Shield")
                check(read(AP_BOUNCE_BACK) == 0, 78, "Stage 7 bounce-back flag not cleared")
                nextPhase("s7_trigger_boss")
            else
                check(false, 79, "unexpected stage number " .. stg)
            end
        end
    end

    if frame > 35000 then
        check(false, 99, "arcade-full-loop-smoke timed out")
    end
end, emu.eventType.endFrame)
