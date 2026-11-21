-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
-- Verifies Task 3-3: 16-bit SoA entity pools, weapon shot caps (Lance=2, Dagger=3),
-- Torch parabola + ground flame, tombstone obstacle/platform, pot-enemy weapon drop,
-- HUD weapon display, horizontal moving platform carrying player across X=512 page
-- boundary, and vertical moving platform carrying player.

local frame = 0
local phase = "boot"
local phaseFrame = 0
local input = {}

-- WRAM layout from src/arcade-player.inc
local AP_STATE = 0x6010
local AP_ARMOR = 0x6011
local AP_WEAPON = 0x601A
local AP_RIDING = 0x601B
local AP_POT_ENEMY = 0x601C
local PS_ACTIVE = 0x6026      -- 3 bytes
local PLAT_X_LO = 0x6042      -- 4 bytes
local PLAT_X_HI = 0x6046      -- 4 bytes
local PLAT_Y    = 0x604A      -- 4 bytes
local ITEM_TYPE = 0x6062      -- 4 bytes

local function read(addr)
    return emu.read(addr, emu.memType.nesMemory)
end

local function hud(addr)
    return emu.read(addr, emu.memType.nesPpuMemory)
end

local function check(cond, code, msg)
    if cond then return true end
    emu.log(string.format("FAIL(%d) frame=%d phase=%s: %s", code, frame, phase, msg))
    emu.stop(code)
    return false
end

local function nextPhase(name)
    phase = name
    phaseFrame = 0
end

local function countActiveShots()
    local n = 0
    for i = 0, 2 do
        if read(PS_ACTIVE + i) ~= 0 then n = n + 1 end
    end
    return n
end

local function platX(slot)
    return read(PLAT_X_HI + slot) * 256 + read(PLAT_X_LO + slot)
end

local function playerScreenX()
    return read(0x203)
end

local function playerScreenY()
    return read(0x200)
end

local sawTwoLances = false
local sawFlame = false
local sawPage1Carry = false
local sawPage2Carry = false
local vCarryStartY = 0
local sawVCarryMove = false
local sawThreeDaggers = false
local killedEnemy1 = false

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
            nextPhase("enter")
        end
    elseif phase == "enter" then
        if phaseFrame == 1 then
            input = { right = true, start = true }
        elseif phaseFrame == 24 then
            check(read(0x8000) == 0x8c, 10, "bank 12 not mapped")
            check(read(AP_WEAPON) == 0 and hud(0x203e) == 0x0c, 11, "HUD did not show initial L weapon")
            nextPhase("lance_cap")
        end
    elseif phase == "lance_cap" then
        -- Fire 3 times rapidly: Lance cap is 2 active shots max.
        if phaseFrame == 1 or phaseFrame == 5 or phaseFrame == 9 then
            input = { b = true }
        end
        local active = countActiveShots()
        check(active <= 2, 12, "lance exceeded 2-shot cap")
        if active == 2 then sawTwoLances = true end
        if phaseFrame == 16 then
            check(sawTwoLances, 13, "lance did not fire 2 simultaneous shots")
            nextPhase("reach_tombstone")
        end
    elseif phase == "reach_tombstone" then
        -- Traverse pit (col 10-11), wall+oneway (col 14-16), kill enemy 1 (X=312), reach col 25 (X~396).
        input = { right = true }
        -- Jump over pit at screen X ~ 148 (camera X=0)
        if phaseFrame < 140 and playerScreenX() >= 148 and playerScreenX() <= 154 and read(AP_STATE) == 0 then
            input.a = true
        end
        -- Jump onto one-way platform over col 15 wall
        if phaseFrame == 215 then
            input.a = true
        end
        -- Shoot enemy 1 at X=312 once past col 16
        if not killedEnemy1 and phaseFrame >= 275 and phaseFrame <= 310 and phaseFrame % 8 == 0 then
            input.b = true
        end
        if read(0x6018) == 0 then
            killedEnemy1 = true
        end
        -- Wait until camera+player reaches right before tombstone (X=416).
        -- When camera X ~ 220, screen X ~ 180 -> player world X ~ 400.
        if phaseFrame == 410 then
            check(killedEnemy1, 14, "did not clear enemy 1")
            check(read(AP_ARMOR) >= 1 and read(AP_STATE) == 0, 15, "player died before tombstone")
            nextPhase("test_tombstone_block")
        end
    elseif phase == "test_tombstone_block" then
        -- Wait for previous shots to clear, then fire lance at tombstone (X=416):
        -- tombstone blocks the shot so pot enemy (X=448) stays alive.
        if phaseFrame == 20 then
            input = { b = true }
        elseif phaseFrame == 50 then
            check(read(AP_POT_ENEMY) == 1, 16, "shot passed through tombstone and hit pot enemy")
            nextPhase("ride_tombstone")
        end
    elseif phase == "ride_tombstone" then
        -- Jump right onto tombstone (X=416, Y=160) -> ap_riding_plat == 0.
        if phaseFrame == 1 then
            input = { right = true, a = true }
        elseif phaseFrame <= 35 then
            if read(AP_RIDING) == 0 then
                nextPhase("kill_pot_and_get_torch")
            end
        else
            check(false, 17, "did not land on tombstone platform")
        end
    elseif phase == "kill_pot_and_get_torch" then
        -- Step off the tombstone to floor Y=176, then shoot pot enemy (X=448) -> drops Torch.
        if phaseFrame <= 14 then
            input = { right = true }
        elseif phaseFrame == 28 or phaseFrame == 36 then
            input = { b = true }
        elseif phaseFrame == 56 then
            check(read(AP_POT_ENEMY) == 0, 18, "pot enemy not defeated")
            check(read(ITEM_TYPE + 2) == 3, 19, "pot enemy did not drop Torch item")
        elseif phaseFrame > 56 and phaseFrame <= 130 then
            input = { right = true }
            if read(AP_WEAPON) == 2 then
                nextPhase("verify_torch_and_flame")
            end
        elseif phaseFrame > 130 then
            check(false, 20, "did not pick up dropped Torch weapon")
        end
    elseif phase == "verify_torch_and_flame" then
        if phaseFrame == 4 then
            check(hud(0x203e) == 0x13, 21, "HUD did not update to T for Torch")
            input = { b = true }
        elseif phaseFrame > 4 and phaseFrame <= 55 then
            if read(PS_ACTIVE) == 2 or read(PS_ACTIVE + 1) == 2 or read(PS_ACTIVE + 2) == 2 then
                sawFlame = true
            end
        elseif phaseFrame == 56 then
            check(sawFlame, 22, "Torch did not ignite ground flame on landing")
            nextPhase("approach_hplat")
        end
    elseif phase == "approach_hplat" then
        -- Walk right to edge of pit (col 30, X~490) and jump onto horizontal platform (slot 1, X=500..544).
        local px = platX(1)
        if phaseFrame < 40 then
            input = { right = true }
        elseif px >= 502 and px <= 514 and read(AP_STATE) == 0 then
            input = { right = true, a = true }
            nextPhase("ride_hplat_across_512")
        elseif phaseFrame > 160 then
            check(false, 23, "timed out waiting for horizontal platform")
        end
    elseif phase == "ride_hplat_across_512" then
        -- Stand still while carried across X=512 page boundary (page 1 -> page 2).
        if read(AP_RIDING) == 1 then
            local px = platX(1)
            if px < 512 then sawPage1Carry = true end
            if px >= 512 then sawPage2Carry = true end
            if sawPage1Carry and sawPage2Carry and px >= 530 then
                -- Jump right onto landing bank (col 34, X>=544).
                input = { right = true, a = true }
                nextPhase("reach_vplat")
            end
        end
        check(read(AP_ARMOR) >= 1, 24, "fell into pit while riding horizontal platform")
        if phaseFrame > 180 then
            check(false, 25, "horizontal platform did not carry player across X=512")
        end
    elseif phase == "reach_vplat" then
        -- Walk right onto vertical moving platform (slot 2, X=576).
        if phaseFrame > 36 then
            input = { right = true }
        end
        if read(AP_RIDING) == 2 then
            vCarryStartY = playerScreenY()
            nextPhase("ride_vplat")
        elseif phaseFrame > 160 then
            check(false, 26, "did not board vertical moving platform")
        end
    elseif phase == "ride_vplat" then
        -- Stand still and confirm vertical platform carries player vertically.
        check(read(AP_RIDING) == 2, 27, "lost footing on vertical moving platform")
        if math.abs(playerScreenY() - vCarryStartY) >= 8 then
            sawVCarryMove = true
            nextPhase("get_dagger")
        elseif phaseFrame > 60 then
            check(false, 28, "vertical platform did not carry player")
        end
    elseif phase == "get_dagger" then
        -- Walk right off vertical platform to col 40 (X=640) and collect Dagger item.
        input = { right = true }
        if read(AP_WEAPON) == 1 then
            nextPhase("test_dagger_cap")
        elseif phaseFrame > 140 then
            check(false, 29, "did not collect Dagger weapon item")
        end
    elseif phase == "test_dagger_cap" then
        if phaseFrame == 4 then
            check(hud(0x203e) == 0x05, 30, "HUD did not update to D for Dagger")
        end
        -- Fire 3 daggers rapidly (facing left so they stay on screen near X=640).
        if phaseFrame == 5 then
            input = { left = true }
        elseif phaseFrame == 7 or phaseFrame == 11 or phaseFrame == 15 then
            input = { b = true }
        end
        if countActiveShots() == 3 then
            sawThreeDaggers = true
        end
        if phaseFrame == 24 then
            check(sawThreeDaggers, 31, "Dagger did not allow 3 simultaneous shots")
            check(sawVCarryMove and sawPage1Carry and sawPage2Carry, 32, "carry flags incomplete")
            emu.stop(0)
        end
    end

    if frame > 2600 then
        check(false, 99, "arcade-systems-smoke timed out")
    end
end, emu.eventType.endFrame)
