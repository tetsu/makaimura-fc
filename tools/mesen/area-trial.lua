-- Clear the flat trial, then follow the clear-screen action through to the menu.
local frame = 0
local step = "boot"
local nextInput = {}
local retryAt, returnAt, previousBolt
local hasSecondCourse
local sawAim, crossedPage = false, false
local player, fixed, ranged, bolt = 0, 2, 5, 7
local menu = {0x15, 0x11, 0x04, 0x01, 0x02, 0, 0x13, 0x06, 0x12, 0x13, 0, 0x0d, 0x06, 0x0e, 0x14}
local clear = {0x04, 0x0f, 0x14, 0x11, 0x12, 0x06, 0, 0x04, 0x0c, 0x06, 0x02, 0x11}
local rightB = {0x11, 0x0a, 0x08, 0x09, 0x13, 0x17, 0x03}
local startNext = {0x12, 0x13, 0x02, 0x11, 0x13, 0, 0x0e, 0x06, 0x18, 0x13}
local startPlay = {0x12, 0x13, 0x02, 0x11, 0x13, 0, 0x10, 0x0c, 0x02, 0x17}

local function oam(slot, field)
    return emu.read(0x200 + slot * 4 + field, emu.memType.nesMemory)
end

local function visible(slot)
    return oam(slot, 0) ~= 0xf8
end

local function gap(slot)
    -- Player part is 4 px left of its center, enemy part is 8 px left.
    return oam(slot, 3) - oam(player, 3) + 4
end

local function hasText(address, tiles)
    for i, tile in ipairs(tiles) do
        if emu.read(address + i - 1, emu.memType.nesPpuDebug) ~= tile then return false end
    end
    return true
end

local function check(condition, message)
    if condition then return end
    local detail = string.format("frame %d (%s): %s", frame, step, message)
    print(detail)
    emu.stop(1)
    error(detail)
end

emu.addEventCallback(function()
    emu.setInput(nextInput, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    nextInput = {}
    check(frame < 2100, "timed out")

    if step == "boot" then
        if frame == 125 then
            nextInput.select = true
        elseif frame == 170 then
            check(hasText(0x2108, menu), "SELECT did not open the test menu")
            hasSecondCourse = hasText(0x2328, rightB)
            nextInput.left, nextInput.b = true, true
        elseif frame == 190 then
            check(visible(player) and oam(player, 3) == 12 and oam(player, 1) == 0x11,
                "trial did not start with the generated pose at X=16")
            step = "approach fixed enemy"
        end
    elseif step == "approach fixed enemy" then
        nextInput.right = true
        if visible(fixed) and gap(fixed) == 38 then
            nextInput.b = true
            step = "shoot fixed enemy"
        end
    elseif step == "shoot fixed enemy" then
        nextInput.right = true
        if not visible(fixed) then step = "approach ranged enemy" end
    elseif step == "approach ranged enemy" then
        sawAim = sawAim or (visible(ranged) and (oam(ranged, 2) & 3) == 2)
        if visible(ranged) and gap(ranged) == 34 then
            step = "wait for bolt"
        else
            nextInput.right = true
        end
    elseif step == "wait for bolt" then
        sawAim = sawAim or (visible(ranged) and (oam(ranged, 2) & 3) == 2)
        if visible(bolt) then
            -- At world X=512 the bolt's left edge is 6 px left of the
            -- ranged enemy's left edge (524-8).
            local relativeBolt = oam(bolt, 3) - oam(ranged, 3)
            crossedPage = crossedPage or (previousBolt and previousBolt >= -6 and relativeBolt < -6)
            previousBolt = relativeBolt
        end
        if (oam(player, 2) & 3) == 2 then
            check(sawAim and crossedPage and visible(ranged), "missing windup or cross-page hit")
            retryAt = frame + 10
            step = "retry"
        end
    elseif step == "retry" then
        nextInput.start = frame == retryAt
        if frame == retryAt + 32 then
            check(visible(player) and visible(ranged) and gap(ranged) == 60,
                "START did not restore X=464 and the ranged enemy")
            check((oam(player, 2) & 3) == 0 and not visible(bolt), "retry retained hit or bolt")
            nextInput.b = true
            step = "shoot ranged enemy"
        end
    elseif step == "shoot ranged enemy" then
        if not visible(ranged) then step = "reach goal" end
    elseif step == "reach goal" then
        check((oam(player, 2) & 3) ~= 2, "player was hit after defeating the ranged enemy")
        if hasText(0x218a, clear) then
            check(hasText(0x21cb, hasSecondCourse and startNext or startPlay),
                "wrong START prompt for the first clear")
            nextInput.start = true
            retryAt = frame + 32
            step = "check start after clear"
        else
            nextInput.right = true
        end
    elseif step == "check start after clear" then
        if frame == retryAt then
            local tile = hasSecondCourse and 0x21 or 0x11
            check(visible(player) and oam(player, 3) == 12 and oam(player, 1) == tile
                and (oam(player, 2) & 3) == 0 and not visible(bolt),
                "START did not reset the player, camera and checkpoint from the first clear")
            if hasSecondCourse then
                check(hasText(0x224c, {0x64, 0x64}) and hasText(0x3f05, {0x07, 0x18, 0x37})
                    and hasText(0x3f11, {0x12, 0x2a, 0x3c}), "wrong terrain or palettes after advancing")
                step = "jump over step"
            else
                returnAt = frame + 10
                step = "return to menu"
            end
        end
    elseif step == "jump over step" then
        nextInput.right = true
        if oam(player, 3) == 91 then
            nextInput.a = true
            step = "approach second course enemy"
        end
    elseif step == "approach second course enemy" then
        check(not visible(ranged) and not visible(bolt), "second course inherited a ranged enemy or bolt")
        check((oam(player, 2) & 3) == 0, "player was hit before reaching the second course enemy")
        nextInput.right = true
        if visible(fixed) and gap(fixed) == 38 then
            check(oam(player, 1) ~= 0x27 and oam(player, 0) == oam(fixed, 0),
                "second course enemy is not on its lower floor")
            nextInput.b = true
            step = "shoot second course enemy"
        end
    elseif step == "shoot second course enemy" then
        if not visible(fixed) then step = "reach second goal" end
    elseif step == "reach second goal" then
        if hasText(0x218a, clear) then
            check(hasText(0x21cb, startPlay), "last course advertised another course")
            returnAt = frame + 10
            step = "return to menu"
        else
            nextInput.right = true
        end
    elseif step == "return to menu" then
        nextInput.select = frame == returnAt
        if frame == returnAt + 16 then
            check(hasText(0x2108, menu), "SELECT did not restore the test menu")
            print(hasSecondCourse and "Advanced from the first clear, cleared the second trial and returned to the menu"
                or "Replayed the single trial after clearing and returned to the menu")
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
