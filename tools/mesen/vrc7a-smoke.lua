-- Run with Mesen 2.1.1 --testRunner and a standard controller on port 1.
local selected
local fm = {}
local started = 0
local sawSolo = {}
local apu = false
local frame = 0
local screen
local function fail(code, message)
    emu.log(message)
    emu.stop(code)
end
local function watch(address, callback)
    emu.addMemoryCallback(callback, emu.callbackType.write, address, address, emu.memType.nesMemory)
end
watch(0x9010,function(_, v) selected = v end)
watch(0x9030,function(_, v)
    if selected and selected >= 0x20 and selected <= 0x25 then
        local channel = selected - 0x20
        if (v & 16) ~= 0 and not fm[channel] then
            started = started + 1
            if started <= 6 then sawSolo[started] = channel end
        end
        fm[channel] = (v & 16) ~= 0
    end
end)
watch(0x4015,function(_, v) apu = (v & 1) ~= 0 end)
emu.addEventCallback(function()
    emu.setInput({left = frame >= 118 and frame <= 123, right = frame >= 128 and frame <= 133, down = frame >= 138 and frame <= 143}, 0)
end, emu.eventType.inputPolled)
emu.addEventCallback(function()
    frame = frame+1
    if frame == 115 then
        for channel = 0, 5 do
            if sawSolo[channel + 1] ~= channel then
                fail(8, "FM channels did not start in order")
                return
            end
        end
    end
    if frame == 115 or frame == 124 or frame == 134 or frame == 144 or frame == 270 then
        local active=0
        for _, playing in pairs(fm) do if playing then active=active+1 end end
        local expectedFM = frame == 134 and 0 or 6
        local expectedAPU = frame ~= 124
        if active ~= expectedFM or apu ~= expectedAPU then
            fail(frame, "unexpected audio at frame "..frame..": FM="..active..", APU="..tostring(apu))
            return
        end
        local buf = emu.getScreenBuffer()
        local bg = buf[1]
        local lit = 0
        local signature = 0
        for i, color in ipairs(buf) do
            if color ~= bg then lit = lit+1; signature=(signature+i)%2147483647 end
        end
        if lit < 300 or lit > 1500 then fail(9, "unexpected screen pixel count "..lit); return end
        if screen and screen ~= signature then fail(10, "screen changed"); return end
        screen = signature
        if frame == 144 then emu.reset() end
        if frame == 270 then emu.log("PASS: boot, fixed display, FM+APU, modes, reset"); emu.stop(0) end
    end
end, emu.eventType.endFrame)
