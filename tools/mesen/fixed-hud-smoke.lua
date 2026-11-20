-- Mesen 2.1.1 with an NTSC framebuffer at 256x240 and a controller on port 1.
local frame = 0
local phase = "approach"
local irqWrites
local irqX, irqNametable
local irqCount, vramWrites = 0, 0
local previousX, previousGround
local header
local finishFrame
local checks = {forward = 0, reverse = 0}

local function fail(code, message)
    emu.log(message)
    emu.stop(code)
end

local function watch(address, callback)
    emu.addMemoryCallback(callback, emu.callbackType.write, address, address, emu.memType.nesMemory)
end

watch(0xf010, function()
    irqWrites = 0
    irqCount = irqCount + 1
end)
watch(0x2006, function(_, value)
    if irqWrites == 0 then
        irqNametable = (value & 4) ~= 0 and 256 or 0
        irqWrites = 1
    elseif irqWrites == 3 then
        local state = emu.getState()
        if state["ppu.scanline"] ~= 31 or state["ppu.cycle"] >= 255 then
            fail(61, "playfield address changed too late")
        end
        irqWrites = nil
    end
end)
watch(0x2005, function(_, value)
    if irqWrites == 1 then
        if value ~= 31 then fail(62, "unexpected split Y"); return end
        irqWrites = 2
    elseif irqWrites == 2 then
        irqX = irqNametable + value
        irqWrites = 3
    end
end)
watch(0x2007, function() vramWrites = vramWrites + 1 end)

emu.addEventCallback(function()
    local walking = frame >= 140 and phase ~= "done"
    emu.setInput({
        start = frame == 122,
        right = walking and phase ~= "reverse",
        left = walking and phase == "reverse",
        a = frame >= 260 and frame % 14 == 0 and phase ~= "done",
        b = frame == 216,
        select = finishFrame and frame == finishFrame + 3 or false,
    }, 0)
end, emu.eventType.inputPolled)

local function segment(buffer, firstRow, rowCount)
    local result = {}
    for i = 1, rowCount * 256 do result[i] = buffer[firstRow * 256 + i] end
    return result
end

emu.addEventCallback(function()
    frame = frame + 1
    if frame >= 138 and phase ~= "done" then
        if irqCount ~= 1 or irqWrites or not irqX then
            fail(63, "expected one complete HUD split per frame")
            return
        end
        if vramWrites ~= 0 and vramWrites ~= 59 then
            fail(64, "gameplay column changed the HUD rows")
            return
        end
        if not header or (irqX >= 247 and irqX <= 265) then
            local buffer = emu.getScreenBuffer()
            local current = segment(buffer, 0, 32)
            if not header then
                header = current
                local marked = false
                for _, pixel in ipairs(header) do
                    if pixel ~= header[1] then marked = true; break end
                end
                if not marked then fail(65, "HUD is blank"); return end
            else
                for i, pixel in ipairs(current) do
                    if pixel ~= header[i] then fail(66, "HUD pixels moved with the camera"); return end
                end
            end
            -- Terrain-only rows below the sprites. A one-pixel camera step
            -- must translate the entire interior exactly one pixel.
            local ground = segment(buffer, 224, 16)
            if previousGround and math.abs(irqX - previousX) == 1 then
                local delta = irqX - previousX
                for y = 0, 15 do
                    for x = 1, 254 do
                        local pos = y * 256 + x + 1
                        if ground[pos] ~= previousGround[pos + delta] then
                            fail(67, "playfield jumped or changed at the nametable seam")
                            return
                        end
                    end
                end
                if irqX == 256 or previousX == 256 then
                    local direction = delta > 0 and "forward" or "reverse"
                    checks[direction] = checks[direction] + 1
                end
            end
            previousGround, previousX = ground, irqX
        else
            previousGround = nil
        end
        if irqX >= 272 then phase = "reverse" end
        if phase == "reverse" and irqX <= 240 then
            if checks.forward < 2 or checks.reverse < 2 then
                fail(68, "camera did not cross the seam both ways")
                return
            end
            phase = "done"
            finishFrame = frame
        end
    end
    if phase == "done" and frame == finishFrame + 20 then
        if irqCount ~= 0 then fail(69, "HUD IRQ still active on audio screen"); return end
        emu.stop(0)
    end
    irqCount, vramWrites = 0, 0
    if frame > 1300 then fail(70, "route did not reach the seam"); return end
end, emu.eventType.endFrame)
