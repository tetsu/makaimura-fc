-- Slot 8 places both markers on a platform at Y=112 and a hazard on the lower floor.
local frame = 0
local step = "select"
local nextInput = {}
local retryAt
local floorY, platformY = 143, 95
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
    check(frame < 1100, "timed out")
    local x = emu.read(0x203, cpu) + 4
    local y = emu.read(0x200, cpu)
    local hit = (emu.read(0x202, cpu) & 3) == 2
    local clear = emu.read(0x218a, ppu) == 0x04

    if step == "select" then
        if frame == 125 then
            nextInput.select = true
        elseif frame == 170 then
            nextInput.up, nextInput.b = true, true
        elseif frame >= 185 and frame <= 245 and (frame - 185) % 10 == 0 then
            nextInput.right = true
        elseif frame == 255 then
            nextInput.start = true
        elseif frame == 287 then
            check(emu.read(0x8000, cpu) == 0xbb and x == 32 and y == floorY,
                "wrong marker course or start position")
            step = "pass below both markers"
        end
    elseif step == "pass below both markers" then
        check(y == floorY and not clear, "course cleared while passing below the goal")
        if x == 176 then
            retryAt = frame + 12
            step = "wait below goal"
        else
            nextInput.right = true
        end
    elseif step == "wait below goal" then
        check(x == 176 and y == floorY and not clear, "standing below the goal cleared the course")
        if frame == retryAt then
            step = "hit before checkpoint"
        end
    elseif step == "hit before checkpoint" or step == "hit after checkpoint" then
        if hit then
            check(x == 31 and y == floorY, "damage did not occur on the lower floor hazard")
            nextInput.start = true
            retryAt = frame + 32
            step = step == "hit before checkpoint" and "retry from start" or "retry from checkpoint"
        else
            nextInput.left = true
        end
    elseif step == "retry from start" and frame == retryAt then
        check(x == 32 and y == floorY and not hit, "passing below the checkpoint activated it")
        step = "approach ladder"
    elseif step == "approach ladder" then
        nextInput.up = true
        if x == 64 then
            step = "climb"
        else
            nextInput.right = true
        end
    elseif step == "climb" then
        check(x == 64, "climb changed the player's column")
        if y == platformY then
            nextInput.right = true
            step = "reach checkpoint"
        else
            nextInput.up = true
        end
    elseif step == "reach checkpoint" then
        check(y == platformY, "approach to the checkpoint left the platform")
        if x == 96 then
            nextInput.left = true
            step = "return to ladder"
        else
            nextInput.right = true
        end
    elseif step == "return to ladder" then
        if x == 72 then
            retryAt = frame + 70
            step = "descend"
            nextInput.down = true
        else
            nextInput.left = true
        end
    elseif step == "descend" then
        if frame == retryAt then
            check(x == 72 and y == floorY, "descent did not return to the lower floor")
            step = "hit after checkpoint"
        else
            nextInput.down = true
        end
    elseif step == "retry from checkpoint" and frame == retryAt then
        check(x == 96 and y == platformY and not hit, "retry did not restore the elevated checkpoint")
        step = "reach goal"
    elseif step == "reach goal" then
        if clear then
            print("Both markers required the platform, and retry restored the elevated checkpoint")
            emu.stop(0)
        else
            check(y == platformY and not hit, "approach to the goal left the platform")
            nextInput.right = true
        end
    end
end, emu.eventType.endFrame)
