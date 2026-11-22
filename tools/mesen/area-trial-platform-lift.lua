-- Take a vertical mover to a ledge beyond jump height from the floor.
local cpu, ppu = emu.memType.nesMemory, emu.memType.nesPpuDebug
local frame, nextInput, step = 0, {}, "boot"
local splitY, previousLift, riderX, resumeFrame
local liftX, topY, bottomY = 96, 104, 160
local floorY, ledgeY = 176, 112

local function check(ok, detail)
    if ok then return end
    local message = string.format("frame %d (%s): %s", frame, step, detail)
    print(message)
    emu.stop(1)
    error(message)
end

emu.addMemoryCallback(function(_, value)
    -- The first scroll write at the HUD split sets playfield Y at scanline 31.
    if splitY == nil and emu.getState()["ppu.scanline"] == 31 then splitY = value end
end, emu.callbackType.write, 0x2005, 0x2005, cpu)

emu.addEventCallback(function()
    emu.setInput(nextInput, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    nextInput = {}
    if step == "done" then return end
    local cameraY = splitY and splitY - 31
    splitY = nil
    check(frame < 850, "timed out")

    if step == "boot" then
        nextInput.select = frame == 125
        nextInput.right, nextInput.b = frame == 170, frame == 170
        if frame ~= 210 then return end
        check(emu.read(0x8000, cpu) == 0xb5, "wrong trial course")
        step = "take damage"
    end

    local x = emu.read(0x203, cpu) + 4
    local hit = (emu.read(0x202, cpu) & 3) == 2
    local playerY = emu.read(0x200, cpu)
    local liftY = emu.read(0x240, cpu)
    local airborne = emu.read(0x201, cpu) == 0x27
    if step == "reach goal" and emu.read(0x218a, ppu) == 0x04 then
        print("Retried at the bottom, rode up and down, and cleared from the upper ledge")
        step = "done"
        emu.stop(0)
        return
    end
    if step == "retry" then
        if hit or x ~= 32 then return end
        -- The first reset frame draws OAM before the HUD split resumes.
        check(playerY + 16 - liftY == floorY - bottomY,
            "retry did not restore the lift to its bottom stop")
        step = "approach"
        resumeFrame = frame
        return
    end
    -- The split resumes one or two frames after the reset OAM, depending on whether the
    -- redraw ends inside vertical blank (enable_screen polls $2002 for the next one).
    if not cameraY and resumeFrame and frame <= resumeFrame + 2 then return end
    check(cameraY, "missing playfield scroll")
    local feet = playerY + 17 + cameraY
    local lift = liftY + 1 + cameraY
    check(emu.read(0x247, cpu) == liftX and emu.read(0x243, cpu) == liftX - 8
        and emu.read(0x244, cpu) == liftY, "lift sprites moved sideways or separated")
    check(lift >= topY and lift <= bottomY, "lift moved beyond its stops")
    check(not hit or step == "take damage", "unexpected hit")

    if step == "take damage" then
        check(feet == floorY and lift < bottomY, "lift did not rise while the player stayed on the floor")
        if hit then
            check(x == 31, "expected damage on the hazard behind the start")
            nextInput.start = true
            step = "retry"
        else
            nextInput.left = true
        end
    elseif step == "approach" then
        -- While the camera settles after retry, prepared OAM can be one
        -- frame ahead of the split scroll. Check height once it settles.
        check(not airborne and (cameraY < 16 or feet == floorY), "approach left the lower floor")
        if x < 78 then
            nextInput.right = true
        elseif lift == 147 and previousLift == 146 then
            nextInput.right, nextInput.a = true, true
            step = "board"
        end
    elseif step == "board" then
        if not airborne and feet == lift and math.abs(x - liftX) < 8 and lift >= 150 then
            riderX = x
            step = "ride up"
        else
            nextInput.right = x < liftX
        end
    elseif step == "ride up" or step == "ride down" or step == "ride back up" then
        check(not airborne and feet == lift and x == riderX, "lift lost or shifted the rider")
        local delta = lift - previousLift
        local direction = step == "ride down" and 1 or -1
        check(delta == 0 or delta == direction, "lift moved in the wrong direction")
        if step == "ride up" and lift == topY then
            step = "ride down"
        elseif step == "ride down" and lift == bottomY then
            step = "ride back up"
        elseif step == "ride back up" and lift == 108 then
            nextInput.right, nextInput.a = true, true
            step = "land on ledge"
        end
    elseif step == "land on ledge" then
        nextInput.right = true
        if x >= 112 and feet == ledgeY and not airborne then
            step = "reach goal"
        end
    elseif step == "reach goal" then
        check(feet == ledgeY, "player left the upper ledge before the goal")
        nextInput.right = true
    end
    previousLift = lift
end, emu.eventType.endFrame)
