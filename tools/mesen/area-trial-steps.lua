-- Exercise the generated step course using only controller input and video output.
local frame = 0
local step = "boot"
local nextInput = {}
local wallFrames = 0
local retryAt
local scrollX, previousScrollX
local seamColumns
local idleTile, jumpTile = 0x21, 0x27
local upperY = 127 -- Floor Y=144, 16 px sprite, OAM Y minus one.
local lowerY = 143 -- The camera settles with the lower floor at scanline 160.
local clear = {0x04, 0x0f, 0x14, 0x11, 0x12, 0x06, 0, 0x04, 0x0c, 0x06, 0x02, 0x11}

local function oam(slot, field)
    return emu.read(0x200 + slot * 4 + field, emu.memType.nesMemory)
end

local function enemyGap()
    if oam(2, 0) == 0xf8 then return nil end
    -- Slot 2 is the fixed enemy, drawn 8 px left of its center.
    return oam(2, 3) - oam(0, 3) + 4
end

local function ppuMatches(address, bytes)
    for i, byte in ipairs(bytes) do
        if emu.read(address + i - 1, emu.memType.nesPpuDebug) ~= byte then return false end
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

local function checkArea()
    -- Sample both bitplanes of the ledge and the idle sprite in CHR-RAM.
    check(ppuMatches(0x0640, {0xff, 0xff, 0, 0}) and ppuMatches(0x064a, {0xff, 0xff}),
        "missing second area's ledge CHR")
    check(ppuMatches(0x1200, {0, 0, 0xff, 0xff}) and ppuMatches(0x120c, {0x0e, 0x0e}),
        "missing second area's player CHR")
    check(ppuMatches(0x3f05, {0x07, 0x18, 0x37}), "wrong second-area background palette")
    check(ppuMatches(0x3f11, {0x12, 0x2a, 0x3c}), "wrong second-area player palette")
end

emu.addMemoryCallback(function(_, value)
    -- At the HUD split, the second $2005 write sets the playfield's low X byte.
    if step == "reach goal" and emu.getState()["ppu.scanline"] == 31 then
        scrollX = value
    end
end, emu.callbackType.write, 0x2005, 0x2005, emu.memType.nesMemory)

local function checkSeam()
    -- The marker at (512, 80) wraps to columns 0 and 1 in the 32-tile ring.
    if previousScrollX == 255 and scrollX == 0 then
        check(ppuMatches(0x2140, {0x60, 0x60}) and ppuMatches(0x2160, {0x60, 0x60}),
            "seam marker overwrote the ring before its columns entered the screen")
        seamColumns = 0
    elseif previousScrollX == 0 and scrollX == 1 and seamColumns == 0 then
        check(ppuMatches(0x2140, {0x61, 0x60}) and ppuMatches(0x2160, {0x62, 0x60}),
            "wrong left marker column at the horizontal seam")
        check(ppuMatches(0x23d0, {0x55}), "wrong palette attribute at the horizontal seam")
        seamColumns = 1
    elseif previousScrollX == 8 and scrollX == 9 and seamColumns == 1 then
        check(ppuMatches(0x2140, {0x61, 0x61}) and ppuMatches(0x2160, {0x62, 0x63}),
            "wrong right marker column at the horizontal seam")
        check(ppuMatches(0x23d0, {0x55}), "wrong palette attribute after the second column upload")
        seamColumns = 2
    end
    previousScrollX = scrollX
    scrollX = nil
end

emu.addEventCallback(function()
    emu.setInput(nextInput, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    nextInput = {}
    check(frame < 1200, "timed out")
    local y, tile, attributes, x = oam(0, 0), oam(0, 1), oam(0, 2), oam(0, 3)

    if step == "boot" then
        nextInput.select = frame == 125
        nextInput.left, nextInput.b = frame == 170, frame == 170
        if frame == 210 then
            check(y == lowerY and x == 12 and tile == idleTile and (attributes & 3) == 0,
                string.format("missing trial start at X=16 (OAM x=%d, y=%d, tile=%02x)", x, y, tile))
            -- World X=96, Y=144 is the first ledge tile in the initial nametable.
            check(ppuMatches(0x224c, {0x64, 0x64}), "terrain did not use the second area's tile IDs")
            check(ppuMatches(0x23e3, {0x55}), "terrain attributes did not select palette 1")
            checkArea()
            step = "approach wall"
        end
    elseif step == "approach wall" then
        nextInput.right = true
        if x == 91 then step = "walk into wall" end
    elseif step == "walk into wall" then
        check(x == 91 and y == lowerY and tile == idleTile, "walking crossed the step at X=96")
        nextInput.right = true
        wallFrames = wallFrames + 1
        if wallFrames == 8 then
            nextInput.a = true
            step = "start jump"
        end
    elseif step == "start jump" then
        check(tile == jumpTile and x == 91, "jump did not lift the player at the wall")
        nextInput.right = true
        step = "land on upper floor"
    elseif step == "land on upper floor" then
        nextInput.right = true
        if tile ~= jumpTile then
            check(x > 91 and y == upperY, "jump did not land on the raised floor")
            step = "cross upper floor"
        end
    elseif step == "cross upper floor" then
        nextInput.right = true
        if tile == jumpTile then
            check(y > upperY and enemyGap() == 60, "fall did not start at X=320")
            step = "land on lower floor"
        else
            check(y == upperY, "ground height changed before the drop")
        end
    elseif step == "land on lower floor" then
        nextInput.right = true
        if tile ~= jumpTile then
            check(y > upperY, "fall did not land below the raised floor")
            step = "touch enemy"
        end
    elseif step == "touch enemy" then
        if (attributes & 3) == 2 then
            check(y == lowerY and enemyGap(), "enemy contact did not occur on the lower floor")
            retryAt = frame + 10
            step = "retry at checkpoint"
        else
            nextInput.right = true
        end
    elseif step == "retry at checkpoint" then
        nextInput.start = frame == retryAt
        if frame == retryAt + 32 then
            check(y == upperY and tile == idleTile and (attributes & 3) == 0 and enemyGap() == 92,
                "START did not restore the upper checkpoint at X=288")
            checkArea()
            step = "approach enemy after retry"
        end
    elseif step == "approach enemy after retry" then
        if tile ~= jumpTile and enemyGap() == 38 then
            nextInput.b = true
            step = "shoot enemy"
        else
            nextInput.right = true
        end
    elseif step == "shoot enemy" then
        if not enemyGap() then step = "reach goal" end
    elseif step == "reach goal" then
        if ppuMatches(0x218a, clear) then
            check(seamColumns == 2, "did not verify both columns at the horizontal seam")
            print("Stepped area trial cleared after a checkpoint retry and a verified horizontal seam")
            emu.stop(0)
        else
            check((attributes & 3) ~= 2, "player was hit on the second descent")
            if seamColumns ~= 2 then checkSeam() end
            nextInput.right = true
        end
    end
end, emu.eventType.endFrame)
