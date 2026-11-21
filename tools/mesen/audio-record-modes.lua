-- Start WAV recording before running this script on the audio test screen.
-- At NTSC speed there are about ten seconds between mode changes.
local frame = 0

emu.addEventCallback(function()
    emu.setInput({
        left = frame == 120,
        right = frame == 720,
        down = frame == 1320,
        up = frame == 1920,
    }, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
end, emu.eventType.endFrame)
