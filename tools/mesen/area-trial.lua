-- Play the generated course using controller input and inspect OAM and nametable output.
local frame = 0
local step = "boot"
local nextInput = {}
local retryAt, returnAt, previousBolt
local sawAim, crossedPage = false, false
local player, fixed, ranged, bolt = 0, 2, 5, 7
local menu = {0x15, 0x11, 0x04, 0x01, 0x02, 0, 0x13, 0x06, 0x12, 0x13, 0, 0x0d, 0x06, 0x0e, 0x14}
local clear = {0x04, 0x0f, 0x14, 0x11, 0x12, 0x06, 0, 0x04, 0x0c, 0x06, 0x02, 0x11}

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
    check(frame < 1100, "timed out")

    if step == "boot" then
        if frame == 125 then
            nextInput.select = true
        elseif frame == 170 then
            check(hasText(0x2108, menu), "SELECT did not open the test menu")
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
            returnAt = frame + 10
            step = "return to menu"
        else
            nextInput.right = true
        end
    elseif step == "return to menu" then
        nextInput.select = frame == returnAt
        if frame == returnAt + 16 then
            check(hasText(0x2108, menu), "SELECT did not restore the test menu after clearing")
            print("Area trial cleared and returned to the test menu")
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
