-- NTSC Mesen 2.1.1, 256x240 framebuffer, NES controller on port 1.
local frame = 0
local irqStep, nextX, nextY
local oldX, oldY, oldPixels
local hud
local writes, irqCount, dmaCount = 0, 0, 0
local frameBudget, worstBudget = 0, 0
local worstBusyBudget = 0
local combined, rose, sank = false, false, false
local fullyLoaded = false
local musicDuringShot = false
local selected, fm, apu = nil, 0, false
local lastNote
local shotEnds, shotPeriod
local shotEnabled = false
local shotTriggers = 0

local function fail(code, message)
    emu.log(message)
    emu.stop(code)
end

local function watch(address, callback)
    emu.addMemoryCallback(callback, emu.callbackType.write, address, address, emu.memType.nesMemory)
end

watch(0x2007, function()
    writes = writes + 1
    if frame >= 159 and frame < 650 then
        local line = emu.getState()["ppu.scanline"]
        if line < 241 or line > 260 then fail(81, "playfield upload outside VBlank") end
    end
end)
watch(0x4014, function(_, value)
    dmaCount = dmaCount + 1
    if frame >= 159 and frame < 650 then
        if value ~= 2 then fail(82, "unexpected OAM buffer"); return end
        local state = emu.getState()
        local dots = (state["ppu.scanline"] - 241) * 341 + state["ppu.cycle"]
        frameBudget = math.ceil(dots / 3) + 514
        if dots < 0 or frameBudget >= 2273 then fail(83, "DMA would cross VBlank"); return end
    end
end)
watch(0xf010, function()
    irqStep = 1
    irqCount = irqCount + 1
end)
watch(0x2006, function(_, value)
    if irqStep == 1 then
        nextX = (value & 4) ~= 0 and 256 or 0
        irqStep = 2
    elseif irqStep == 4 then
        local state = emu.getState()
        if state["ppu.scanline"] ~= 31 or state["ppu.cycle"] >= 255 then
            fail(84, "split moved too late")
        end
        irqStep = nil
    end
end)
watch(0x2005, function(_, value)
    if irqStep == 2 then
        nextY = value - 31
        irqStep = 3
    elseif irqStep == 3 then
        nextX = nextX + value
        irqStep = 4
    end
end)
watch(0x9010, function(_, value) selected = value end)
watch(0x9030, function(_, value)
    if selected and selected >= 0x20 and selected <= 0x25 then
        local bit = 1 << (selected - 0x20)
        if (value & 16) ~= 0 then fm = fm | bit else fm = fm & ~bit end
    end
end)
watch(0x4015, function(_, value)
    apu = (value & 1) ~= 0
    shotEnabled = (value & 2) ~= 0
    if shotEnds and frame < shotEnds and frame < 650 and not shotEnabled then
        fail(100, "music cut off the shot sound")
    end
end)
watch(0x4007, function()
    shotTriggers = shotTriggers + 1
    shotEnds = frame + 6
    shotPeriod = 0
end)
watch(0x4006, function(_, value)
    if not shotEnabled or not shotEnds or frame >= shotEnds or value <= shotPeriod then
        fail(101, "shot pitch did not sweep down on pulse 2")
        return
    end
    shotPeriod = value
end)
watch(0x4003, function()
    if frame >= 160 and frame < 650 then
        if lastNote and frame - lastNote > 30 then fail(94, "audio phrase fell behind") end
        lastNote = frame
        if shotEnabled then musicDuringShot = true end
    end
end)

emu.addEventCallback(function()
    emu.setInput({
        a = frame == 122 or (frame >= 314 and frame < 620 and frame % 19 == 0),
        start = frame == 144,
        right = frame >= 160 and frame < 620,
        b = frame == 238 or (frame >= 285 and frame < 620 and frame % 13 == 0),
        up = frame == 170,
        select = frame == 650,
    }, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    if frame >= 160 and frame <= 650 then
        if irqCount ~= 1 or irqStep or dmaCount ~= 1 then
            fail(85, "expected one IRQ and DMA each frame")
            return
        end
        if fm ~= 0x3f or not apu then fail(86, "FM or APU became silent"); return end
        local image = emu.getScreenBuffer()
        if not hud then
            hud = {}
            for i = 1, 8192 do hud[i] = image[i] end
        else
            local dx, dy = nextX - oldX, nextY - oldY
            if math.abs(dx) > 1 or math.abs(dy) > 1 then fail(87, "camera jumped"); return end
            if dy > 0 then sank = true elseif dy < 0 then rose = true end
            if writes == 67 then
                if dx == 0 or dy == 0 then fail(88, "wrong frame for both uploads"); return end
                combined = true
                worstBudget = math.max(worstBudget, frameBudget)
                if shotEnabled and frame > 171 then
                    fullyLoaded = true
                    worstBusyBudget = math.max(worstBusyBudget, frameBudget)
                end
            end
            for i, pixel in ipairs(hud) do
                if image[i] ~= pixel then fail(89, "HUD moved"); return end
            end
            -- This strip lies between the stress markers and all moving actors.
            for y = 40, 236 do
                for x = 132, 154 do
                    if image[y * 256 + x + 1] ~= oldPixels[(y + dy) * 256 + x + dx + 1] then
                        fail(90, "terrain did not follow the camera")
                        return
                    end
                end
            end
        end
        oldPixels, oldX, oldY = image, nextX, nextY
        local playerY = emu.read(0x0200, emu.memType.nesMemory)
        local playerAttributes = emu.read(0x0202, emu.memType.nesMemory)
        if playerY < 32 or playerY >= 224 or playerAttributes == 2 then
            fail(91, "player sprite missing or encounter failed")
            return
        end
        if frame >= 172 then
            for offset = 32, 60, 4 do
                if emu.read(0x0200 + offset, emu.memType.nesMemory) ~= playerY then
                    fail(102, "stress markers left the player's scanline")
                    return
                end
            end
        end
        if frame == 160 then
            local playerX = emu.read(0x0203, emu.memType.nesMemory)
            if image[(playerY + 5) * 256 + playerX + 5] == image[1] then
                fail(95, "player sprite pixels are missing")
                return
            end
        end
    end
    if frame == 675 then
        if not combined or not rose or not sank then fail(92, "missing combined transfer or vertical travel"); return end
        if not fullyLoaded or not musicDuringShot or shotTriggers < 2 then
            fail(103, "no simultaneous transfer, OAM load and shot sound")
            return
        end
        if irqCount ~= 0 then fail(93, "IRQ remained active in audio screen"); return end
        emu.log("Combined transfer budget (CPU cycles): " .. worstBudget)
        emu.log("Combined transfer with shot sound (CPU cycles): " .. worstBusyBudget)
        emu.stop(0)
    end
    irqCount, dmaCount, writes = 0, 0, 0
end, emu.eventType.endFrame)
