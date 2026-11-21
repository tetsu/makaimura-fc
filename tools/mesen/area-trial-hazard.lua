-- Select the enemy-free hazard course in slot 5 of the capacity ROM.
local frame = 0
local step = "select"
local nextInput = {}
local hitAt, retryAt
local crossedHazard = false
local floorY = 143 -- OAM Y for the standing sprite on this course.
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
    check(frame < 660, "timed out")
    local y = emu.read(0x200, cpu)
    local hit = (emu.read(0x202, cpu) & 3) == 2
    local x = emu.read(0x203, cpu) + 4 -- The sprite is drawn 4 px left of center.

    if step == "select" then
        if frame == 125 then
            nextInput.select = true
        elseif frame == 170 then
            nextInput.up, nextInput.b = true, true
        elseif frame == 185 or frame == 195 or frame == 205 or frame == 215 then
            nextInput.right = true
        elseif frame == 225 then
            nextInput.start = true
        elseif frame == 257 then
            check(emu.read(0x8000, cpu) == 0xb8 and x == 32 and y == floorY and not hit,
                "wrong hazard course or start position")
            check(emu.read(0x208, cpu) == 0xf8 and emu.read(0x20c, cpu) == 0xf8,
                "hazard course spawned an enemy")
            check(emu.read(0x2290, ppu) == 0x48 and emu.read(0x2291, ppu) == 0x48,
                "hazard graphic missing at X=128, Y=160")
            step = "walk"
        end
    elseif step == "walk" then
        nextInput.right = true
        check(hit == (x == 128), "hit did not coincide with stepping onto the hazard at X=128")
        check(y == floorY, "hazard changed the floor height")
        if hit then
            hitAt = frame
            step = "hit"
        end
    elseif step == "hit" then
        check(hit and x == 128, "hit state did not stop movement")
        if frame < hitAt + 8 then
            nextInput.right = true
        else
            nextInput.start = true
            retryAt = frame + 32
            step = "retry"
        end
    elseif step == "retry" and frame == retryAt then
        check(x == 96 and y == floorY and not hit, "START did not restore the checkpoint")
        step = "jump"
    elseif step == "jump" then
        if crossedHazard and emu.read(0x218a, ppu) == 0x04 then
            print("Took damage at X=128, retried at X=96, jumped the hazard and cleared the course")
            emu.stop(0)
            return
        end
        check(not hit, "jumping across the hazard caused a hit")
        nextInput.right = true
        nextInput.a = x == 115
        if x >= 128 and x < 144 then
            check(emu.read(0x201, cpu) == 0x17, "player crossed the hazard without the jump pose")
        elseif x == 144 then
            crossedHazard = true
        end
    end
end, emu.eventType.endFrame)
