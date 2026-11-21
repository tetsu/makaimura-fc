-- Mesen 2.1.1, NES controller on port 1, 256x240 NTSC framebuffer.
-- Verifies Bank 3 arcade player physics, 2D terrain (pit, wall, one-way platform,
-- ladder across nametable seam), two-stage armor, knockback, 120f invulnerability,
-- armor recovery item, and hazard instant death.

local frame = 0
local phase = "boot"
local phaseFrame = 0
local supportsArcade = false
local digitZero = 0x1b

-- Fixed WRAM addresses in src/arcade-player.inc (after 16-byte reserved header).
local AP_STATE = 0x6010
local AP_ARMOR = 0x6011
local AP_INVULN = 0x6012
local AP_CROUCH = 0x6013
local AP_SHOOT_LOCK = 0x6014
local AP_ARMOR_ITEM = 0x6019

local function read(addr)
    return emu.read(addr, emu.memType.nesMemory)
end

local function write(addr, val)
    emu.write(addr, val, emu.memType.nesMemory)
end

local function hud(addr)
    return emu.read(addr, emu.memType.nesPpuMemory)
end

local function lives()
    return hud(0x202e) - digitZero
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

local input = {}
local savedX = 0
local savedY = 0
local sawInvulnContact = false
local onLadder = false
local climbedTop = false

-- Player world X is read via OAM sprite X + camera, or directly from player_x?
-- Read camera & OAM for screen-visible verification, and WRAM for arcade state.
local function playerScreenX()
    return read(0x203)
end

local function playerScreenY()
    return read(0x200)
end

local function playerPalette()
    return read(0x202) & 3
end

emu.addEventCallback(function()
    emu.setInput(input, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    phaseFrame = phaseFrame + 1
    input = {}

    if phase == "boot" then
        if frame == 128 then
            supportsArcade = (read(0xe000) == 0xbf)
            if not supportsArcade then
                emu.stop(0)
                return
            end
            nextPhase("enter")
        end
    elseif phase == "enter" then
        if phaseFrame == 1 then
            input = { right = true, start = true }
        elseif phaseFrame == 28 then
            check(read(0x8000) == 0x8c, 10, "bank 12 not mapped")
            check(read(AP_ARMOR) == 2 and read(AP_STATE) == 0, 11, "initial armor/state wrong")
            check(lives() == 3 and playerPalette() == 0, 12, "initial HUD/palette wrong")
            savedY = playerScreenY()
            nextPhase("crouch")
        end
    elseif phase == "crouch" then
        -- Hold down: crouch lowers sprite by 4 px and blocks walking.
        if phaseFrame <= 6 then
            input = { down = true, right = true }
        elseif phaseFrame == 7 then
            check(read(AP_CROUCH) == 1, 13, "did not crouch")
            check(playerScreenY() == savedY + 4, 14, "crouch sprite Y did not drop 4px")
            savedX = playerScreenX()
            input = { down = true, b = true }
        elseif phaseFrame == 10 then
            check(playerScreenX() == savedX, 15, "moved while crouching")
            check(read(0x210) ~= 0xf8, 16, "crouch shot did not spawn")
            nextPhase("shoot_lock")
        end
    elseif phase == "shoot_lock" then
        -- Wait for previous shot to expire, then test 15-frame ground shoot lock.
        if phaseFrame == 52 then
            savedX = playerScreenX()
            input = { b = true }
        elseif phaseFrame >= 53 and phaseFrame <= 62 then
            input = { right = true }
            check(read(AP_SHOOT_LOCK) > 0, 17, "shoot lock ended too early")
            check(playerScreenX() == savedX, 18, "moved during 15f shoot lock")
        elseif phaseFrame >= 63 and phaseFrame <= 75 then
            input = { right = true }
        elseif phaseFrame == 76 then
            check(playerScreenX() > savedX + 4, 19, "did not resume walking after shoot lock")
            nextPhase("air_lock")
        end
    elseif phase == "air_lock" then
        -- Jump right, then hold left in mid-air: horizontal velocity must remain rightward.
        if phaseFrame <= 4 then
            input = { right = true }
        elseif phaseFrame == 5 then
            savedX = playerScreenX()
            input = { right = true, a = true }
        elseif phaseFrame >= 6 and phaseFrame <= 26 then
            input = { left = true }
            if phaseFrame == 20 then
                check(read(AP_STATE) == 1, 20, "expected air state")
                check(playerScreenX() > savedX + 10, 21, "air control was not locked rightward")
            end
        elseif phaseFrame == 48 then
            check(read(AP_STATE) == 0, 22, "did not land from jump")
            nextPhase("jump_pit")
        end
    elseif phase == "jump_pit" then
        -- Walk right up to pit edge (world X ~ 156, screen X ~ 148), jump across columns 10-11 (X=160..191).
        input = { right = true }
        if playerScreenX() >= 148 and playerScreenX() <= 154 and read(AP_STATE) == 0 then
            input.a = true
        end
        if phaseFrame > 180 then
            -- By now we should have cleared the pit and hit the wall at col 15 (X=240).
            check(read(AP_ARMOR) == 2 and read(AP_STATE) == 0, 23, "fell into pit instead of clearing")
            savedX = playerScreenX()
            nextPhase("check_wall")
        end
    elseif phase == "check_wall" then
        -- Keep pushing right against wall at col 15: X must stay blocked.
        input = { right = true }
        if phaseFrame == 12 then
            check(playerScreenX() == savedX, 24, "walked through solid wall at col 15")
            nextPhase("oneway_and_seam")
        end
    elseif phase == "oneway_and_seam" then
        -- Jump straight up / right onto one-way platform (Y=144) at col 14-16 across X=256 seam.
        if phaseFrame == 1 then
            savedY = playerScreenY()
            input = { right = true, a = true }
        elseif phaseFrame >= 2 and phaseFrame <= 36 then
            -- Air direction is already locked rightward.
        elseif phaseFrame == 37 then
            check(read(AP_STATE) == 0, 25, "did not land on one-way platform")
            -- World Y rose 32 px (176 -> 144) and vertical camera pan_y followed 16 -> 0.
            check(playerScreenY() == savedY - 16, 26, "landing Y did not match Y=144 platform")
            nextPhase("cross_seam_to_ladder")
        end
    elseif phase == "cross_seam_to_ladder" then
        -- Walk right across the X=256 nametable seam onto col 17 ladder (X=272..287).
        if phaseFrame <= 36 then
            input = { right = true }
        elseif phaseFrame == 37 then
            -- Now past col 16, dropped to floor at col 17 (Y=176) or still descending.
            nextPhase("climb_ladder")
        end
    elseif phase == "climb_ladder" then
        if phaseFrame <= 20 then
            -- Wait to settle on floor at col 17 if falling.
        elseif phaseFrame <= 100 then
            input = { up = true }
            if phaseFrame == 30 then
                check(read(AP_STATE) == 2, 27, "did not enter ladder state")
            end
        elseif phaseFrame == 101 then
            check(read(AP_STATE) == 0, 28, "did not finish climbing ladder to top")
            savedY = playerScreenY()
            nextPhase("descend_and_hit1")
        end
    elseif phase == "descend_and_hit1" then
        -- Climb down ladder back to lower floor, then walk right into enemy at X=312.
        if phaseFrame <= 80 then
            input = { down = true }
        elseif phaseFrame <= 140 then
            input = { right = true }
            if read(AP_ARMOR) == 1 then
                check(lives() == 3, 29, "armor loss deducted a life")
                check(read(AP_INVULN) >= 110, 30, "120f invulnerability not started")
                check(read(AP_STATE) == 3, 31, "did not enter knockback state")
                nextPhase("invuln_and_recover")
            end
        else
            check(false, 32, "did not collide with enemy for armor loss")
        end
    elseif phase == "invuln_and_recover" then
        -- Wait out knockback, walk into enemy during invulnerability, then climb ladder to armor item.
        if phaseFrame <= 36 then
            input = { right = true }
            if read(AP_INVULN) > 0 then
                sawInvulnContact = true
            end
            check(read(AP_ARMOR) == 1 and lives() == 3, 33, "took damage during invulnerability")
        elseif not climbedTop then
            if read(AP_STATE) == 2 then
                onLadder = true
                input = { up = true }
            elseif onLadder and read(AP_STATE) == 0 then
                climbedTop = true
                input = { right = true }
            else
                input = { left = true, up = true }
            end
            check(phaseFrame <= 200, 36, "could not climb ladder to upper tier")
        elseif phaseFrame <= 280 then
            -- On upper tier (Y=112): walk right to collect armor item at X=320.
            input = { right = true }
            if read(AP_ARMOR) == 2 then
                check(read(AP_ARMOR_ITEM) == 0, 34, "armor item not consumed")
                check(sawInvulnContact, 35, "never verified invulnerability window")
                nextPhase("second_armor_loss")
            end
        else
            check(false, 36, "did not collect armor recovery item on upper tier")
        end
    elseif phase == "second_armor_loss" then
        -- Walk left 32 px from armor item (X~310 -> col 17 ladder at X~278), then climb down to enemy.
        if phaseFrame <= 32 then
            input = { left = true }
        elseif phaseFrame <= 115 then
            input = { down = true }
        elseif phaseFrame <= 190 then
            input = { right = true }
            if read(AP_ARMOR) == 1 then
                nextPhase("wait_invuln_and_die")
            end
        else
            check(false, 37, "did not lose recovered armor on enemy")
        end
    elseif phase == "wait_invuln_and_die" then
        -- Wait until 120f invulnerability expires, then touch enemy naked -> death (lives 3 -> 2).
        if phaseFrame <= 130 then
            -- Wait in place.
        elseif phaseFrame <= 190 then
            input = { right = true }
            if read(AP_ARMOR) == 0 then
                nextPhase("verify_naked_death")
            end
        else
            check(false, 38, "naked contact did not kill player")
        end
    elseif phase == "verify_naked_death" then
        if phaseFrame == 8 then
            check(lives() == 2 and playerPalette() == 2, 39, "naked death did not reduce lives to 2")
            nextPhase("retry_for_pit")
        end
    elseif phase == "retry_for_pit" then
        if phaseFrame == 1 then
            input = { start = true }
        elseif phaseFrame == 16 then
            check(read(AP_ARMOR) == 2 and lives() == 2 and read(AP_STATE) == 0, 40, "retry did not restore armor")
            nextPhase("pit_instant_death")
        end
    elseif phase == "pit_instant_death" then
        -- Walk straight right into the pit with full armor -> instant death (lives 2 -> 1).
        input = { right = true }
        if read(AP_ARMOR) == 0 then
            nextPhase("verify_pit_death")
        elseif phaseFrame > 160 then
            check(false, 41, "walking into pit with armor did not kill player")
        end
    elseif phase == "verify_pit_death" then
        if phaseFrame == 8 then
            check(lives() == 1, 42, "pit death did not deduct exactly one life")
            emu.stop(0)
        end
    end

    if frame > 2400 then
        check(false, 99, "arcade-player-smoke timed out")
    end
end, emu.eventType.endFrame)
