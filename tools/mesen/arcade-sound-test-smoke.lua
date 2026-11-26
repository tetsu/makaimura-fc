-- Open the 512 KiB sound test with the controller and audition the driver.
local frame = 0
local songTile, seTile = 0x216c, 0x21ac
local zeroTile = 0x1b
local songId, playing, seId = 0x7800, 0x7801, 0x7803
local inputs = {
    [130] = {select = true},
    [160] = {down = true, a = true},
    [180] = {left = true, down = true},
    [190] = {a = true},
    [200] = {right = true, up = true},
    [210] = {b = true},
    [230] = {right = true, up = true, a = true, b = true},
    [255] = {up = true, b = true},
    [280] = {right = true, a = true},
    [290] = {right = true, a = true},
    [300] = {right = true, a = true},
    [310] = {right = true, a = true},
    [316] = {b = true},
    [320] = {start = true, b = true},
    [330] = {b = true},
    [335] = {select = true},
    [350] = {down = true, a = true},
    [370] = {select = true},
    [390] = {start = true},
}

local function read(address)
    return emu.read(address, emu.memType.nesMemory)
end

local function ppu(address)
    return emu.read(address, emu.memType.nesPpuDebug)
end

local function check(condition, message)
    if not condition then
        print(string.format("frame %d: %s", frame, message))
        emu.stop(1)
    end
    return condition
end

local mask, fmRegister, keyedChannels, apuChannels = 0, 0, 0, 0
emu.addMemoryCallback(function(_, value) mask = value end,
    emu.callbackType.write, 0x2001, 0x2001, emu.memType.nesMemory)
emu.addMemoryCallback(function()
    if frame >= 170 and frame < 335 and mask ~= 0 then
        local line = emu.getState()["ppu.scanline"]
        check(line >= 241 and line <= 260, "sound test wrote VRAM outside VBlank")
    end
end, emu.callbackType.write, 0x2007, 0x2007, emu.memType.nesMemory)
emu.addMemoryCallback(function(_, value) apuChannels = value end,
    emu.callbackType.write, 0x4015, 0x4015, emu.memType.nesMemory)
emu.addMemoryCallback(function(_, value) fmRegister = value end,
    emu.callbackType.write, 0x9010, 0x9010, emu.memType.nesMemory)
emu.addMemoryCallback(function(_, value)
    if fmRegister >= 0x20 and fmRegister <= 0x25 then
        local bit = 1 << (fmRegister - 0x20)
        if (value & 0x10) ~= 0 then
            keyedChannels = keyedChannels | bit
        else
            keyedChannels = keyedChannels & ~bit
        end
    end
end, emu.callbackType.write, 0x9030, 0x9030, emu.memType.nesMemory)

emu.addEventCallback(function()
    emu.setInput(inputs[frame] or {}, 0)
end, emu.eventType.inputPolled)

local function checkSelection(song, se)
    check(ppu(songTile) == zeroTile + song and ppu(seTile) == zeroTile + se,
        string.format("expected displayed IDs %d/%d", song, se))
end

emu.addEventCallback(function()
    frame = frame + 1
    if frame == 150 then
        check(ppu(0x22a8) == 0x05, "sound test shortcut missing from menu")
    elseif frame == 170 or frame == 360 then
        check(ppu(0x2108) == 0x12 and mask == 0x0a, "sound test did not open")
        checkSelection(1, 1)
        check(read(songId) == 0 and read(seId) == 0 and keyedChannels == 0 and apuChannels == 0,
            "entry did not start silent")
    elseif frame == 185 then
        checkSelection(7, 3)
    elseif frame == 195 then
        check(read(songId) == 7 and read(playing) == 1, "BGM 7 did not start")
    elseif frame == 205 then
        checkSelection(1, 1)
        check(read(songId) == 7, "changing selection changed playback")
    elseif frame == 213 then
        check(read(seId) == 1 and (apuChannels & 2) ~= 0 and read(playing) == 1 and keyedChannels ~= 0,
            "shot SE did not play over BGM")
    elseif frame == 235 then
        checkSelection(2, 2)
        check(read(songId) == 2 and read(seId) == 2 and (apuChannels & 9) == 9 and keyedChannels ~= 0,
            "damage SE did not play on APU over BGM 2")
    elseif frame == 250 then
        check(read(seId) == 0 and read(playing) == 1,
            "damage SE did not finish while BGM continued")
    elseif frame == 260 then
        checkSelection(2, 3)
        check(read(seId) == 3 and read(playing) == 1, "key SE did not play over BGM")
    elseif frame == 284 or frame == 294 or frame == 304 or frame == 314 then
        local song = 3 + (frame - 284) // 10
        checkSelection(song, 3)
        check(read(songId) == song and read(playing) == 1, "selected BGM did not start")
    elseif frame == 319 then
        check(read(seId) == 3 and read(playing) == 1, "stop test requires both BGM and SE to be playing")
    elseif frame == 325 then
        checkSelection(6, 3)
        check(read(songId) == 0 and read(seId) == 0 and keyedChannels == 0 and apuChannels == 0,
            "START did not silence both sound sources")
    elseif frame == 333 then
        check(read(songId) == 0 and read(seId) == 3 and (apuChannels & 1) ~= 0,
            "key SE did not play without BGM")
    elseif frame == 345 then
        check(ppu(0x2108) == 0x15 and read(songId) == 0 and read(seId) == 0,
            "SELECT did not reset audio and restore the menu")
    elseif frame == 420 then
        if check(mask == 0x1e, "START did not resume normal menu gameplay") then emu.stop(0) end
    end
end, emu.eventType.endFrame)
