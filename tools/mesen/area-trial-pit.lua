-- Slot 9 has a 16 px pit beyond the checkpoint.
local frame = 0
local step = "select"
local nextInput = {}
local retryAt
local floorY = 143
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
    check(frame < 650, "timed out")
    local x = emu.read(0x203, cpu) + 4
    local y = emu.read(0x200, cpu)
    local hit = (emu.read(0x202, cpu) & 3) == 2

    if step == "select" then
        if frame == 125 then
            nextInput.select = true
        elseif frame == 170 then
            nextInput.up, nextInput.b = true, true
        elseif frame >= 185 and frame <= 255 and (frame - 185) % 10 == 0 then
            nextInput.right = true
        elseif frame == 265 then
            nextInput.start = true
        elseif frame == 297 then
            check(emu.read(0x8000, cpu) == 0xbc and x == 32 and y == floorY,
                "wrong pit course or start position")
            step = "walk into pit"
        end
    elseif step == "walk into pit" then
        check(y == floorY and not hit, "player fell or took damage before the pit")
        nextInput.right = true
        if x == 127 then
            step = "fall"
        end
    elseif step == "fall" then
        check(x >= 128 and x < 144, "player escaped the pit without jumping")
        if hit then
            check(y > floorY + 20, "fall ended before the player descended into the pit")
            nextInput.start = true
            retryAt = frame + 32
            step = "retry"
        else
            nextInput.right = true
        end
    elseif step == "retry" and frame == retryAt then
        check(x == 96 and y == floorY and not hit, "retry did not restore the checkpoint")
        step = "jump the pit"
    elseif step == "jump the pit" then
        check(not hit, "jump into the pit failed")
        nextInput.right = true
        nextInput.a = x == 114
        if x >= 128 and x < 144 then
            check(y < floorY, "player crossed the gap below floor level")
        elseif x >= 150 and y == floorY then
            step = "finish"
        end
    elseif step == "finish" then
        if emu.read(0x218a, ppu) == 0x04 then
            print("Fell in the pit, retried at the checkpoint, then jumped across and cleared")
            emu.stop(0)
        else
            nextInput.right = true
        end
    end
end, emu.eventType.endFrame)
