-- Mesen 2.1.1, NES controller on port 1, no horizontal overscan.
local frame = 0
local control, scroll, nextIsX = 0, 0, true
local selected, fmMask, apu = nil, 0, false
local ppuWrites = 0
local samples, reverse = {}, {}
local previousX
local phase = "start"
local wrap = 0
local finishFrame
local forwardColumns, reverseColumns = 0, 0
local points = {0, 1, 15, 16, 17, 31, 32, 127, 239, 255, 256, 257, 271, 383, 497, 511, 512}
local wanted = {}
for _, x in ipairs(points) do wanted[x] = true end
local function fail(code, message)
    emu.log(message)
    emu.stop(code)
end
local function watch(address, callback)
    emu.addMemoryCallback(callback, emu.callbackType.write, address, address, emu.memType.nesMemory)
end

watch(0x2000, function(_, value) control = value end)
watch(0x2005, function(_, value)
    if nextIsX then scroll = value + (control & 1) * 256 end
    nextIsX = not nextIsX
end)
watch(0x2007, function()
    ppuWrites = ppuWrites + 1
    if phase == "forward" or phase == "reverse" then
        local line = emu.getState()["ppu.scanline"]
        if not line or line < 241 or line > 260 then
            fail(20, "VRAM write outside NTSC VBlank")
        end
    end
end)
watch(0x9010, function(_, value) selected = value end)
watch(0x9030, function(_, value)
    if selected and selected >= 0x20 and selected <= 0x25 then
        local bit = 1 << (selected - 0x20)
        fmMask = (value & 16) ~= 0 and (fmMask | bit) or (fmMask & ~bit)
    end
end)
watch(0x4015, function(_, value) apu = (value & 1) ~= 0 end)

local lastNote
watch(0x4003, function()
    if lastNote and (phase == "forward" or phase == "reverse") and frame - lastNote > 30 then
        fail(21, "audio phrase fell behind")
    end
    lastNote = frame
end)

emu.addEventCallback(function()
    emu.setInput({
        select = (frame >= 120 and frame <= 122) or (finishFrame and frame >= finishFrame + 4 and frame <= finishFrame + 6) or false,
        right = phase == "forward",
        left = phase == "reverse",
    }, 0)
end, emu.eventType.inputPolled)

local function matchesWorld(screen, x)
    for i, pixel in ipairs(screen) do
        local column = (i - 1) % 256 + x
        local page = column // 256 * 256
        local source = i - (i - 1) % 256 + column % 256
        if samples[page][source] ~= pixel then return false end
    end
    return true
end

local initialScreen
emu.addEventCallback(function()
    frame = frame + 1
    local writes = ppuWrites
    ppuWrites = 0
    if frame == 118 then initialScreen = emu.getScreenBuffer() end
    if frame == 138 then
        lastNote = nil
        phase = "forward"
    end
    if phase == "forward" or phase == "reverse" then
        if previousX == 511 and scroll == 0 then wrap = 512 end
        if previousX == 512 and scroll == 511 then wrap = 0 end
        local x = scroll + wrap
        if fmMask ~= 0x3f or not apu then fail(11, "audio interrupted"); return end
        if not previousX and x == 0 then
            samples[0] = emu.getScreenBuffer()
        elseif previousX and x ~= previousX then
            local right = x > previousX
            if math.abs(x - previousX) ~= 1 then fail(12, "camera jumped"); return end
            local loadsColumn = (right and x % 16 == 1) or (not right and x % 16 == 15)
            if writes ~= (loadsColumn and 68 or 0) then fail(13, "unexpected VRAM write count"); return end
            if loadsColumn then
                if right then forwardColumns = forwardColumns + 1
                else reverseColumns = reverseColumns + 1 end
            end
            if wanted[x] then
                if right then samples[x] = emu.getScreenBuffer()
                else reverse[x] = emu.getScreenBuffer() end
            end
        elseif previousX and writes ~= 0 then
            fail(14, "stationary camera wrote VRAM")
            return
        end
        previousX = x
        if phase == "forward" and x == 512 then
            phase = "reverse"
        elseif phase == "reverse" and x == 0 then
            finishFrame = frame
            phase = "done"
        end
    end
    if frame == finishFrame then
        if forwardColumns ~= 32 or reverseColumns ~= 32 or previousX ~= 0 then
            fail(15, "incomplete traversal")
            return
        end
        for _, x in ipairs(points) do
            if not samples[x] or (x ~= 0 and x ~= 256 and x ~= 512 and not matchesWorld(samples[x], x)) then
                fail(16, "forward screen differs at x=" .. x)
                return
            end
            if x ~= 512 and (not reverse[x] or not matchesWorld(reverse[x], x)) then
                fail(17, "reverse screen differs at x=" .. x)
                return
            end
        end
    end
    if finishFrame and frame == finishFrame + 25 then
        local finalScreen = emu.getScreenBuffer()
        for i, pixel in ipairs(initialScreen) do
            if finalScreen[i] ~= pixel then fail(18, "audio screen failed to restore"); return end
        end
        emu.stop(0)
    end
    if frame > 1500 then fail(19, "camera did not reach both ends") end
end, emu.eventType.endFrame)
