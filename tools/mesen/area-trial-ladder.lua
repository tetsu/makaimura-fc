-- Slot 7 has a 64 px ladder that meets a one-way platform on its right.
local frame = 0
local step = "select"
local nextInput = {}
local pauseUntil, pauseY
local topY, floorY = 95, 143 -- OAM Y after the camera settles.
local cpu = emu.memType.nesMemory

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
    check(frame < 700, "timed out")
    local y = emu.read(0x200, cpu)
    local x = emu.read(0x203, cpu) + 4

    if step == "select" then
        if frame == 125 then
            nextInput.select = true
        elseif frame == 170 then
            nextInput.up, nextInput.b = true, true
        elseif frame >= 185 and frame <= 235 and (frame - 185) % 10 == 0 then
            nextInput.right = true
        elseif frame == 245 then
            nextInput.start = true
        elseif frame == 277 then
            check(emu.read(0x8000, cpu) == 0xba and x == 32 and y == floorY,
                "wrong ladder course or start position")
            step = "approach"
        end
    elseif step == "approach" then
        check(y == floorY, "approach left the floor")
        nextInput.up = true
        if x == 64 then
            step = "climb"
        else
            nextInput.right = true
        end
    elseif step == "climb" then
        check(x == 64, "climb changed the player's column")
        if y == 119 then
            pauseY = y
            pauseUntil = frame + 10
            step = "pause"
        else
            nextInput.up = true
        end
    elseif step == "pause" then
        check(x == 64 and y == pauseY, "player moved while paused on the ladder")
        if frame == pauseUntil then
            nextInput.up = true
            step = "reach top"
        else
            nextInput.right = true
        end
    elseif step == "reach top" then
        if y == topY then
            nextInput.right = true
            step = "cross platform"
        else
            nextInput.up = true
        end
    elseif step == "cross platform" then
        check(y == topY, "transfer to the adjacent platform changed height")
        if x == 96 then
            nextInput.left = true
            step = "return"
        else
            nextInput.right = true
        end
    elseif step == "return" then
        check(y == topY, "walking onto the ladder top changed height")
        if x == 72 then
            nextInput.down = true
            step = "descend"
        else
            nextInput.left = true
        end
    elseif step == "descend" then
        check(x == 72, "descent changed the player's column")
        if y == floorY then
            pauseUntil = frame + 22
            step = "floor"
        end
        nextInput.down = true
    elseif step == "floor" then
        check(x == 72 and y == floorY, "descended past the floor")
        if frame == pauseUntil then
            nextInput.right = true
            step = "leave"
        else
            nextInput.down = true
        end
    elseif step == "leave" then
        check(y == floorY, "walking away changed the floor height")
        if x == 96 then
            print("Climbed, paused, crossed the platform, returned and descended to the floor")
            emu.stop(0)
        else
            nextInput.right = true
        end
    end
end, emu.eventType.endFrame)
