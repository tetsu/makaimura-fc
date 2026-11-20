-- Mesen 2.1.1, NTSC 256x240 framebuffer, standard controller on port 1.
local frame, phase = 0, "warmup"
local control, nextIsX = 0, true
local rawX, rawY = 0, 0
local previousX, previousY = 0, 0
local previousRawX, previousRawY = 0, 0
local previousImage
local audioScreen
local writes, peak = 0, 0
local forwardYSeam, reverseYSeam = false, false
local sawXWrap, sawYWrap, sawRowsOnly = false, false, false
local finished

local function fail(code, message)
    emu.log(message)
    emu.stop(code)
end

local function watch(address, callback)
    emu.addMemoryCallback(callback, emu.callbackType.write, address, address, emu.memType.nesMemory)
end

watch(0x2000, function(_, value) control = value end)
watch(0x2005, function(_, value)
    if nextIsX then
        rawX = value
    else
        rawY = value + ((control & 2) ~= 0 and 240 or 0)
        if writes == 71 and phase ~= "warmup" and not finished then
            local state = emu.getState()
            local line = state["ppu.scanline"]
            if not line or line < 241 or line > 260 then
                fail(78, "scroll registers were not restored in VBlank")
            end
        end
    end
    nextIsX = not nextIsX
end)
watch(0x2007, function()
    writes = writes + 1
    if phase ~= "warmup" and not finished then
        local line = emu.getState()["ppu.scanline"]
        if not line or line < 241 or line > 260 then fail(71, "VRAM write outside VBlank") end
    end
end)

emu.addEventCallback(function()
    emu.setInput({
        a = frame == 122,
        select = finished and frame == finished + 4 or false,
        down = phase == "down" or phase == "forward",
        right = phase == "forward",
        up = phase == "reverse",
        left = phase == "reverse",
    }, 0)
end, emu.eventType.inputPolled)

local function wrappedDelta(now, previous, period)
    local delta = now - previous
    if delta == 1 - period then return 1 end
    if delta == period - 1 then return -1 end
    return delta
end

local function enteringTile(position, delta)
    return (delta == 1 and position % 8 == 1) or (delta == -1 and position % 8 == 7)
end

local function verifyImage(image, dx, dy)
    -- Exclude the masked left strip and the one-pixel edge newly entering view.
    for y = 1, 238 do
        local row = y * 256
        local source = (y + dy) * 256 + dx
        for x = 9, 254 do
            if image[row + x + 1] ~= previousImage[source + x + 1] then return false end
        end
    end
    return true
end

emu.addEventCallback(function()
    frame = frame + 1
    local count = writes
    writes = 0
    if frame == 118 then audioScreen = emu.getScreenBuffer() end
    if frame == 138 then
        previousImage = emu.getScreenBuffer()
        local colors = {}
        for _, pixel in ipairs(previousImage) do colors[pixel] = true end
        local distinct = 0
        for _ in pairs(colors) do distinct = distinct + 1 end
        if distinct < 4 then fail(79, "terrain is missing"); return end
        phase = "down"
        return
    end
    if finished and frame == finished + 29 then
        for i, pixel in ipairs(emu.getScreenBuffer()) do
            if pixel ~= audioScreen[i] then fail(80, "audio screen did not restore"); return end
        end
        emu.stop(0)
        return
    end
    if phase == "warmup" or finished then return end

    local dx = wrappedDelta(rawX, previousRawX, 256)
    local dy = wrappedDelta(rawY, previousRawY, 480)
    if math.abs(dx) > 1 or math.abs(dy) > 1 then fail(72, "camera jumped"); return end
    local x, y = previousX + dx, previousY + dy
    if x < 0 or x > 512 or y < 0 or y > 480 then fail(73, "camera moved out of bounds"); return end
    local expected = (enteringTile(x, dx) and 31 or 0) + (enteringTile(y, dy) and 40 or 0)
    if count ~= expected then fail(74, "incorrect row or column write count"); return end
    peak = math.max(peak, count)
    if count == 40 then sawRowsOnly = true end

    local image = emu.getScreenBuffer()
    if not verifyImage(image, dx, dy) then
        fail(75, "terrain or palette shifted incorrectly")
        return
    end
    if previousY == 239 and y == 240 then forwardYSeam = true end
    if previousY == 240 and y == 239 then reverseYSeam = true end
    if previousRawX == 255 and rawX == 0 then sawXWrap = true end
    if previousRawY == 479 and rawY == 0 then sawYWrap = true end
    previousImage, previousX, previousY = image, x, y
    previousRawX, previousRawY = rawX, rawY

    if phase == "down" and y == 32 then
        phase = "forward"
    elseif phase == "forward" and x == 512 and y == 480 then
        phase = "reverse"
    elseif phase == "reverse" and x == 0 and y == 0 then
        if not (forwardYSeam and reverseYSeam and sawXWrap and sawYWrap and sawRowsOnly and peak == 71) then
            fail(76, "missing scroll boundary or simultaneous row and column update")
            return
        end
        finished = frame
    end
    if frame > 1400 then fail(77, "camera stopped before completing the route") end
end, emu.eventType.endFrame)
