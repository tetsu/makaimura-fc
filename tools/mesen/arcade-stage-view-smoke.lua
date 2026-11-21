-- 512 KiB stage viewer, driven through the title and test menu.
local frame = 0
local ctrl, mask, x, y = 0, 0, 0, 0
local nextIsX = true
local xWraps = 0
local checkingPreview = false
local savedTop
local nextStagePress = {[1440] = 3, [1460] = 4, [1480] = 5, [1500] = 6, [1520] = 7}

local function check(condition, message)
    if not condition then
        print(string.format("frame %d: %s", frame, message))
        emu.stop(1)
    end
    return condition
end

local function watch(address, callback)
    emu.addMemoryCallback(callback, emu.callbackType.write, address, address, emu.memType.nesMemory)
end

local function bankTag()
    return emu.read(0x8000, emu.memType.nesMemory)
end

local function ppu(address)
    return emu.read(address, emu.memType.nesPpuDebug)
end

watch(0x2000, function(_, value) ctrl = value end)
watch(0x2001, function(_, value) mask = value end)
watch(0x2005, function(_, value)
    if nextIsX then
        if checkingPreview and x == 255 and value == 0 then xWraps = xWraps + 1 end
        x = value
    else
        y = value + ((ctrl & 2) ~= 0 and 240 or 0)
    end
    nextIsX = not nextIsX
end)
watch(0x2007, function()
    if checkingPreview and mask ~= 0 then
        local line = emu.getState()["ppu.scanline"]
        check(line >= 241 and line <= 260, "preview wrote VRAM outside VBlank")
    end
end)
watch(0x4014, function()
    check(not checkingPreview, "preview still uploaded gameplay sprites")
end)

emu.addEventCallback(function()
    emu.setInput({
        select = frame == 130 or frame == 1550,
        a = nextStagePress[frame] ~= nil or frame == 160 or frame == 790 or frame == 810 or frame == 1410,
        b = frame == 770 or frame == 1390,
        up = frame == 160 or (frame >= 1110 and frame < 1370),
        down = frame >= 210 and frame < 750 or frame >= 830 and frame < 1090,
        right = frame >= 210 and frame < 750 or frame >= 830 and frame < 1090,
        start = frame == 190 or frame == 1580,
    }, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    if frame == 150 then
        check(bankTag() == 0x80 and ppu(0x2288) == 0x14, "stage view shortcut missing from test menu")
    elseif frame == 205 then
        check(bankTag() == 0x8c and x == 0 and y == 0 and mask == 0x08, "stage 1 preview did not open")
        checkingPreview = true
    elseif frame == 765 then
        check(xWraps == 2 and x == 0 and y == 0, "stage 1 camera did not stop at (512, 0)")
    elseif frame == 785 then
        check(bankTag() == 0x92 and x == 0 and y == 0, "B did not wrap to stage 7")
    elseif frame == 805 then
        check(bankTag() == 0x8c, "A did not wrap to stage 1")
    elseif frame == 825 then
        check(bankTag() == 0x8d and x == 0 and y == 0, "stage 2 preview did not open at the top")
        check(ppu(0x3f01) == 0x2c and ppu(0x0254) == 0xe7, "stage 2 BG palette or CHR did not load")
        savedTop = emu.getScreenBuffer()
    elseif frame == 1105 then
        check(x == 0 and y == 240, "stage 2 camera did not stop at (0, 240)")
        local different = false
        for i, pixel in ipairs(emu.getScreenBuffer()) do
            if pixel ~= savedTop[i] then
                different = true
                break
            end
        end
        check(different, "stage 2 terrain did not visibly scroll")
    elseif frame == 1380 then
        check(x == 0 and y == 0, "stage 2 camera did not return to the top edge")
    elseif frame == 1405 then
        check(bankTag() == 0x8c, "B did not return to stage 1")
    elseif frame == 1435 then
        check(bankTag() == 0x8d, "A did not return to stage 2")
    elseif nextStagePress[frame - 15] then
        local stage = nextStagePress[frame - 15]
        check(bankTag() == 0x8b + stage and x == 0 and y == 0 and mask == 0x08,
            "preview did not load stage " .. stage)
    elseif frame == 1540 then
        checkingPreview = false
    elseif frame == 1570 then
        check(bankTag() == 0x80 and ppu(0x2288) == 0x14, "SELECT did not restore the test menu")
    elseif frame == 1610 then
        if check(mask == 0x1e, "START did not restore normal menu gameplay") then emu.stop(0) end
    end
end, emu.eventType.endFrame)
