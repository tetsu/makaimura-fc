local cpu, ppu = emu.memType.nesMemory, emu.memType.nesPpuDebug
local frame, nextInput = 0, {}
local step = "select"
local lastPlatform, riderOffset
local jumped, landed, nextCourseAt
local leftEnd, rightEnd = 119, 176
local floorY = 143

local function check(ok, detail)
    if ok then return end
    print(string.format("FAIL frame=%d step=%s: %s", frame, step, detail))
    emu.stop(1)
    error(detail)
end

emu.addEventCallback(function()
    emu.setInput(nextInput, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    nextInput = {}
    check(frame < 1000, "timed out")
    local x = emu.read(0x203, cpu) + 4
    local y = emu.read(0x200, cpu)
    local hit = (emu.read(0x202, cpu) & 3) == 2
    local platformY = emu.read(0x240, cpu)
    local platform = emu.read(0x243, cpu) + 8
    local onPlatform = y + 16 == platformY and x >= platform - 8 and x < platform + 8

    if step == "select" then
        if frame == 125 then
            nextInput.select = true
        elseif frame == 170 then
            nextInput.up, nextInput.b = true, true
        elseif frame >= 185 and frame <= 265 and (frame - 185) % 10 == 0 then
            nextInput.right = true
        elseif frame == 275 then
            nextInput.start = true
        elseif frame == 307 then
            check(emu.read(0x8000, cpu) == 0xbd and x == 32 and y == floorY,
                "wrong platform course or start position")
            check(platform >= leftEnd and platform < rightEnd and platformY == floorY,
                "platform missing or in the wrong place")
            check(emu.read(0x244, cpu) == platformY and emu.read(0x247, cpu) == platform,
                "platform sprites are not aligned")
            check(emu.read(0x1100, ppu) == 0x7f and emu.read(0x1120, ppu) == 0xfe,
                "platform CHR art was not loaded")
            step = "walk into pit"
        end
    elseif step == "walk into pit" then
        check(not onPlatform, "player boarded the raised platform from below")
        if hit then
            check(x >= 112 and x < 176, "expected a fall into the wide pit")
            nextInput.start = true
            step = "retry"
        else
            nextInput.right = true
        end
    elseif step == "retry" then
        if not hit and x == 80 then
            check(y == floorY and platform == leftEnd, "retry did not reset player and platform")
            nextInput.right = true
            step = "approach"
        end
    elseif step == "approach" then
        check(y == floorY and not hit, "player left the near bank too early")
        if x < 106 then
            nextInput.right = true
        elseif platform == 134 and lastPlatform == 135 then
            nextInput.right, nextInput.a = true, true
            step = "jump aboard"
        end
    elseif step == "jump aboard" then
        check(not hit, "jump onto the platform failed")
        if onPlatform and x == 122 then
            riderOffset = x - platform
            step = "ride right"
        else
            nextInput.right = x < 122
        end
    elseif step == "ride right" then
        check(onPlatform and not hit and x - platform == riderOffset,
            "platform did not carry the rider across the pit")
        check(platform >= lastPlatform and platform - lastPlatform <= 1,
            "platform moved incorrectly while traveling right")
        if platform == rightEnd then
            step = "ride the reversal"
        end
    elseif step == "ride the reversal" then
        check(onPlatform and not hit and x - platform == riderOffset,
            "reverse motion did not carry the rider")
        if platform <= rightEnd - 3 then
            nextInput.right, nextInput.a = true, true
            step = "jump off"
        end
    elseif step == "jump off" then
        check(not hit, "jump to the far bank failed")
        if platformY < 0xf8 and y + 16 < platformY then jumped = true end
        if x >= 176 and y == floorY then landed = true end
        if emu.read(0x218a, ppu) == 0x04 then
            check(jumped and landed, "player did not jump off and land on the far bank")
            nextInput.start = true
            nextCourseAt = frame + 32
            step = "next course"
        else
            nextInput.right = true
        end
    elseif step == "next course" and frame == nextCourseAt then
        check(emu.read(0x8000, cpu) == 0xbe and x == 32, "next course did not start")
        check(platformY == 0xf8 and emu.read(0x244, cpu) == 0xf8,
            "platform sprites leaked into the next course")
        print("Reset, boarded and rode the platform through a reversal, then cleared the wide pit")
        emu.stop(0)
    end
    lastPlatform = platform
end, emu.eventType.endFrame)
