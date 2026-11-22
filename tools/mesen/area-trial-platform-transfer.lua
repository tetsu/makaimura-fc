-- Cross a wide gap on one mover, then transfer to a lift to reach the goal.
local cpu, ppu = emu.memType.nesMemory, emu.memType.nesPpuDebug
local frame, nextInput, step = 0, {}, "select"
local splitY, lastShuttle, lastLift, riderOffset, resumeFrame
local shuttleStart, liftX, liftBottom = 71, 144, 176

local function check(ok, detail)
    if ok then return end
    local message = string.format("frame %d (%s): %s", frame, step, detail)
    print(message)
    emu.stop(1)
    error(message)
end

emu.addMemoryCallback(function(_, value)
    if splitY == nil and emu.getState()["ppu.scanline"] == 31 then splitY = value end
end, emu.callbackType.write, 0x2005, 0x2005, cpu)

emu.addEventCallback(function()
    emu.setInput(nextInput, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    if step == "done" then return end
    frame = frame + 1
    nextInput = {}
    local cameraY = splitY and splitY - 31
    splitY = nil
    check(frame < 950, "timed out")
    if step == "select" then
        nextInput.select = frame == 125
        nextInput.up, nextInput.b = frame == 170, frame == 170
        nextInput.right = frame == 185 or frame == 195
        nextInput.start = frame == 205
        if frame ~= 240 then return end
        check(emu.read(0x8000, cpu) == 0xb6, "wrong trial course")
        step = "take damage"
    end

    local x = emu.read(0x203, cpu) + 4
    local y = emu.read(0x200, cpu)
    local hit = (emu.read(0x202, cpu) & 3) == 2
    local airborne = emu.read(0x201, cpu) == 0x27
    local shuttleX, shuttleY = emu.read(0x247, cpu), emu.read(0x244, cpu)
    local liftY = emu.read(0x24c, cpu)
    if step == "reach goal" and emu.read(0x218a, ppu) == 0x04 then
        print("Both movers reset, then the player transferred from the shuttle to the lift and cleared")
        step = "done"
        emu.stop(0)
        return
    end
    if step == "retry" then
        if hit or x ~= 32 then return end
        -- The first OAM frame after retry is drawn before the HUD split.
        check(shuttleX == shuttleStart and y + 16 - shuttleY == 16
            and y + 16 == liftY, "both movers did not return to their starting positions")
        step = "approach"
        resumeFrame = frame
        return
    end

    -- The split resumes one or two frames after the reset OAM, depending on whether the
    -- redraw ends inside vertical blank (enable_screen polls $2002 for the next one).
    if not cameraY and resumeFrame and frame <= resumeFrame + 2 then return end
    check(cameraY, "missing playfield scroll")
    check(emu.read(0x240, cpu) == shuttleY and emu.read(0x243, cpu) == shuttleX - 8
        and emu.read(0x248, cpu) == liftY and emu.read(0x24b, cpu) == liftX - 8
        and emu.read(0x24f, cpu) == liftX, "platform sprites missing or misaligned")
    local feet = y + 17 + cameraY
    local liftFeet = liftY + 1 + cameraY
    local ridingShuttle = not airborne and y + 16 == shuttleY
        and x >= shuttleX - 8 and x < shuttleX + 8
    local ridingLift = not airborne and y + 16 == liftY
        and x >= liftX - 8 and x < liftX + 8
    check(not hit or step == "take damage", "unexpected hit")

    if step == "take damage" then
        check(shuttleX > shuttleStart and liftFeet < liftBottom, "both movers should be away from their stops")
        if hit then
            check(x == 31, "expected damage on the hazard behind the start")
            nextInput.start = true
            step = "retry"
        else
            nextInput.left = true
        end
    elseif step == "approach" then
        check(not airborne, "approach left the near bank")
        if x < 58 then
            nextInput.right = true
        elseif shuttleX == 86 and lastShuttle == 87 then
            nextInput.right, nextInput.a = true, true
            step = "board shuttle"
        end
    elseif step == "board shuttle" then
        if ridingShuttle and x == 74 then
            riderOffset = x - shuttleX
            step = "ride shuttle"
        else
            nextInput.right = x < 74
        end
    elseif step == "ride shuttle" then
        check(ridingShuttle and x - shuttleX == riderOffset, "shuttle lost its rider")
        if shuttleX == 124 then
            nextInput.right, nextInput.a = true, true
            step = "board lift"
        end
    elseif step == "board lift" then
        if ridingLift and x == liftX then
            check(feet < 160, "transfer did not land on the rising lift")
            step = "ride lift"
        else
            nextInput.right = x < liftX
        end
    elseif step == "ride lift" then
        check(ridingLift and x == liftX, "lift lost its rider")
        check(liftFeet == lastLift or liftFeet == lastLift - 1, "lift did not carry the rider up")
        if liftFeet == 108 then
            nextInput.right, nextInput.a = true, true
            step = "land on ledge"
        end
    elseif step == "land on ledge" then
        nextInput.right = x < 168
        if x >= 160 and not airborne and feet == 112 then
            step = "reach goal"
        end
    elseif step == "reach goal" then
        check(feet == 112, "player left the upper ledge before the goal")
        nextInput.right = true
    end
    lastShuttle, lastLift = shuttleX, liftFeet
end, emu.eventType.endFrame)
