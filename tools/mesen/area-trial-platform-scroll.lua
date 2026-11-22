-- Ride a platform across X=256 in both directions using controller input.
local cpu, ppu = emu.memType.nesMemory, emu.memType.nesPpuDebug
local frame, nextInput, step = 0, {}, "boot"
local cameraX, previousCameraX, previousPlatform, boardingCameraX, riderOffset
local hudPixels, streamedColumns = {}, 0
local leftEnd, rightEnd = 247, 304
local floorY = 143

local function check(ok, detail)
    if ok then return end
    local message = string.format("frame %d (%s): %s", frame, step, detail)
    print(message)
    emu.stop(1)
    error(message)
end

local function checkHud()
    local pixels = emu.getScreenBuffer()
    for i, pixel in ipairs(hudPixels) do
        check(pixels[i] == pixel, "HUD changed while the camera followed the rider")
    end
end

local function checkTerrain()
    -- World X=336, Y=80 wraps to columns 10 and 11 of the nametable ring.
    local left = emu.read(0x214a, ppu)
    local right = emu.read(0x214b, ppu)
    if cameraX == 80 then
        check(left == 0x60 and right == 0x60, "marker replaced the initial terrain too early")
    elseif previousCameraX == 80 and cameraX == 81 then
        check(step == "ride right", "marker entered the viewport before the rider boarded")
        check(left == 0x61 and right == 0x60 and emu.read(0x216a, ppu) == 0x62,
            "wrong first marker column at the right edge")
        streamedColumns = 1
    elseif previousCameraX == 88 and cameraX == 89 and streamedColumns == 1 then
        check(left == 0x61 and right == 0x61 and emu.read(0x216b, ppu) == 0x63,
            "wrong second marker column at the right edge")
        check(emu.read(0x23d2, ppu) == 0x55, "wrong palette attribute for the streamed marker")
        streamedColumns = 2
    end
end

emu.addMemoryCallback(function(_, value)
    -- The final $2005 write at the HUD split is the playfield's X scroll.
    -- This course keeps the camera below X=256 even as the rider crosses it.
    if emu.getState()["ppu.scanline"] == 31 then cameraX = value end
end, emu.callbackType.write, 0x2005, 0x2005, cpu)

emu.addEventCallback(function()
    emu.setInput(nextInput, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    nextInput = {}
    check(frame < 900, "timed out")
    if step == "boot" then
        nextInput.select = frame == 125
        nextInput.left, nextInput.b = frame == 170, frame == 170
        if frame ~= 210 then return end
        check(emu.read(0x8000, cpu) == 0xb4 and cameraX == 0
            and emu.read(0x203, cpu) + 4 == 160, "wrong course or start position")
        local pixels = emu.getScreenBuffer()
        local visible = false
        for i = 1, 32 * 256 do
            hudPixels[i] = pixels[i]
            visible = visible or pixels[i] ~= pixels[1]
        end
        check(visible, "HUD is blank")
        step = "find right edge"
    end

    checkHud()
    checkTerrain()
    local x = emu.read(0x203, cpu) + 4 + cameraX
    local y = emu.read(0x200, cpu)
    local leftY, rightY = emu.read(0x240, cpu), emu.read(0x244, cpu)
    local leftX, rightX = emu.read(0x243, cpu), emu.read(0x247, cpu)
    local platform
    if leftY ~= 0xf8 then platform = leftX + 8 + cameraX end
    if rightY ~= 0xf8 then
        check(platform == rightX + cameraX and leftY == rightY, "platform halves are misaligned")
    end
    local onPlatform = platform and y + 16 == leftY and x >= platform - 8 and x < platform + 8
    check((emu.read(0x202, cpu) & 3) ~= 2, "player was hit")

    if step == "find right edge" and platform == 255 and previousPlatform == 254 then
        check(rightY == leftY and rightX == 255, "right half disappeared before the edge")
        step = "check clipped half"
    elseif step == "check clipped half" and platform ~= 255 then
        check(platform == 256 and leftX == 248 and rightY == 0xf8,
            "right half wrapped on screen when its X crossed 255")
        step = "check hidden platform"
    elseif step == "check hidden platform" and platform == nil then
        check(leftY == 0xf8 and rightY == 0xf8 and previousPlatform == 263,
            "platform left half did not disappear at the right edge")
        step = "approach"
    elseif step == "approach" then
        check(y == floorY, "player left the near bank too early")
        if x < 234 then
            nextInput.right = true
        elseif platform == 262 and previousPlatform == 263 then
            nextInput.right, nextInput.a = true, true
            step = "board"
        end
    elseif step == "board" then
        if onPlatform then
            check(x < 256 and platform < 256, "rider boarded after the page boundary")
            boardingCameraX = cameraX
            riderOffset = x - platform
            step = "ride right"
        else
            nextInput.right = x < 250
        end
    elseif step == "ride right" or step == "ride left" then
        check(onPlatform and x - platform == riderOffset and rightY == leftY,
            "platform lost the rider")
        local delta = platform - previousPlatform
        if step == "ride right" then
            check(delta == 0 or delta == 1, "platform did not advance smoothly to the right")
            if platform == rightEnd then
                check(streamedColumns == 2 and cameraX > boardingCameraX,
                    "camera did not stream both marker columns and follow the rider")
                step = "ride left"
            end
        else
            check(delta == 0 or delta == -1, "platform did not advance smoothly to the left")
            if platform == leftEnd then
                print("Checked right-edge clipping, rode the platform across X=256 and back, and verified terrain and HUD")
                emu.stop(0)
            end
        end
    end

    previousCameraX, previousPlatform = cameraX, platform
end, emu.eventType.endFrame)
