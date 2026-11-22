-- Slot 6 has two stacked platforms above the solid floor.
local frame = 0
local step = "select"
local nextInput = {}
local crossedTop = false
local standUntil
local floorY, platformY = 143, 127 -- OAM Y after the camera settles.
local cpu, ppu = emu.memType.nesMemory, emu.memType.nesPpuDebug

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
    check(frame < 600, "timed out")
    local y = emu.read(0x200, cpu)
    local airborne = emu.read(0x201, cpu) == 0x17
    local x = emu.read(0x203, cpu) + 4

    if step == "select" then
        if frame == 125 then
            nextInput.select = true
        elseif frame == 170 then
            nextInput.up, nextInput.b = true, true
        elseif frame >= 185 and frame <= 225 and (frame - 185) % 10 == 0 then
            nextInput.right = true
        elseif frame == 235 then
            nextInput.start = true
        elseif frame == 267 then
            check(emu.read(0x8000, cpu) == 0xb9 and x == 32 and y == floorY,
                "wrong platform course or start position")
            step = "walk under"
        end
    elseif step == "walk under" then
        check(y == floorY and not airborne, "platform blocked walking underneath")
        if x < 64 then
            nextInput.right = true
        else
            nextInput.a = true
            step = "lift"
        end
    elseif step == "lift" then
        if airborne then step = "jump" end
    elseif step == "jump" then
        crossedTop = crossedTop or y < platformY
        if not airborne then
            check(crossedTop, "jump never passed through both platforms")
            check(x == 64 and y == platformY, "jump did not land on the upper platform")
            standUntil = frame + 8
            step = "stand"
        end
    elseif step == "stand" then
        check(y == platformY and not airborne, "player did not stay on the platform")
        if frame == standUntil then
            nextInput.right = true
            step = "walk off"
        end
    elseif step == "walk off" then
        check(airborne == (x == 80), "fall did not start at the platform edge")
        if airborne then
            step = "fall"
        else
            nextInput.right = true
        end
    elseif step == "fall" and not airborne then
        standUntil = frame + 16 -- Let the camera catch up to the landing.
        step = "floor"
    elseif step == "floor" then
        check(x == 80 and not airborne, "player did not stay on the solid floor")
        if frame == standUntil then
            check(y == floorY, "player did not return to the lower floor height")
            print("Walked under both platforms, jumped through them, landed on top and walked off")
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
