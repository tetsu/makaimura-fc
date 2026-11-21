-- The capacity fixture has 11 trials, with the short finish course third
-- and the steps course fourth. Exercise selection and continuation via pad.
local frame = 0
local nextInput = {}
local step = "select"
local retryAt
local zero = 0x1b
local ppu = emu.memType.nesPpuDebug
local cpu = emu.memType.nesMemory

local function check(condition, message)
    if condition then return end
    local detail = string.format("frame %d (%s): %s", frame, step, message)
    print(detail)
    emu.stop(1)
    error(detail)
end

local function checkNumber(tens, ones)
    check(emu.read(0x2148, ppu) == 0x13 and emu.read(0x2149, ppu) == 0x11,
        "selector heading missing")
    check(emu.read(0x2154, ppu) == zero + 1 and emu.read(0x2155, ppu) == zero + 1,
        "selector did not display a total of 11")
    check(emu.read(0x214e, ppu) == zero + tens and emu.read(0x214f, ppu) == zero + ones,
        string.format("selector did not display %d%d", tens, ones))
end

local function checkCourse(bankTag, x, tile)
    check(emu.read(0x8000, cpu) == bankTag, "wrong trial bank")
    check(emu.read(0x200, cpu) ~= 0xf8 and emu.read(0x203, cpu) == x
        and emu.read(0x201, cpu) == tile, "wrong initial player position or pose")
end

emu.addEventCallback(function()
    emu.setInput(nextInput, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    nextInput = {}
    check(frame < 800, "timed out")
    if step == "select" then
        if frame == 125 then
            nextInput.select = true
        elseif frame == 170 then
            check(emu.read(0x2348, ppu) == 0x14 and emu.read(0x234b, ppu) == 0x03,
                "test menu did not advertise Up+B")
        elseif frame >= 171 and frame <= 190 then
            nextInput.up, nextInput.b = true, true
        elseif frame == 200 then
            checkNumber(0, 1)
            nextInput.left = true
        elseif frame == 210 then
            checkNumber(1, 1)
            nextInput.start = true
        elseif frame == 242 then
            checkCourse(0xbe, 28, 0x11)
            nextInput.select = true
        elseif frame == 265 then
            nextInput.up, nextInput.b = true, true
        elseif frame == 280 then
            checkNumber(0, 1)
            nextInput.left = true
        elseif frame == 290 then
            checkNumber(1, 1)
            nextInput.right = true
        elseif frame == 300 then
            checkNumber(0, 1)
            nextInput.select = true
        elseif frame == 320 then
            check(emu.read(0x2108, ppu) == 0x15 and emu.read(0x8000, cpu) == 0x80
                and emu.read(0xa000, cpu) == 0x81, "cancel did not restore the test menu and its banks")
            nextInput.up, nextInput.b = true, true
        elseif frame == 335 or frame == 345 then
            if frame == 345 then checkNumber(0, 2) end
            nextInput.right = true
        elseif frame == 355 then
            checkNumber(0, 3)
            nextInput.b = true
        elseif frame == 387 then
            checkCourse(0xb6, 28, 0x11)
            check(emu.read(0x2142, ppu) == 0x41, "wrong third-course background")
            step = "clear third"
        end
    elseif step == "clear third" then
        if emu.read(0x218a, ppu) == 0x04 and emu.read(0x21d1, ppu) == 0x0e then
            nextInput.start = true
            retryAt = frame + 32
            step = "check fourth"
        else
            nextInput.right = true
            local enemyGap = emu.read(0x20b, cpu) - emu.read(0x203, cpu) + 4
            nextInput.b = emu.read(0x208, cpu) ~= 0xf8 and enemyGap == 38
        end
    elseif step == "check fourth" and frame == retryAt then
        checkCourse(0xb7, 12, 0x21)
        check(emu.read(0x224c, ppu) == 0x64 and emu.read(0x3f05, ppu) == 0x07,
            "fourth course did not load its own terrain and palette")
        print("Selected trial 11 with START, canceled, selected trial 3 with B and advanced to trial 4")
        emu.stop(0)
    end
end, emu.eventType.endFrame)
