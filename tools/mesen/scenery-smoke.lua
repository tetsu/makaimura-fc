-- NTSC Mesen, standard NES controller on port 1.
local frame = 0
local firstFrame, duration = 120, 220
local original = {}
local white
local sawOcclusion, sawOpening, sawShot = false, false, false
local memory = emu.memType.nesMemory

local function fail(code, message)
    emu.log(message)
    emu.stop(code)
end

-- The second pass renders the same input sequence with only the player
-- priority bit cleared. Everything else in the frame should match.
local function removePriority(_, value)
    if frame >= firstFrame + duration then return value & ~0x20 end
end
emu.addMemoryCallback(removePriority, emu.callbackType.write, 0x0202, 0x0202, memory)
emu.addMemoryCallback(removePriority, emu.callbackType.write, 0x0206, 0x0206, memory)

emu.addEventCallback(function()
    local elapsed = frame - firstFrame
    if elapsed < 0 then return end
    local t = elapsed % duration
    emu.setInput({
        start = t == 2,
        right = t >= 18 and t < 204,
        b = t == 96 or t == 152,
    }, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    local elapsed = frame - firstFrame - 1
    if elapsed < 0 then return end
    local t = elapsed % duration
    local comparison = elapsed >= duration
    if t < 140 or t > 204 then return end

    local playerX = emu.read(0x0203, memory)
    local playerY = emu.read(0x0200, memory)
    if playerY ~= 175 or emu.read(0x0208, memory) ~= 0xf8 then
        fail(96, "player did not reach the lower terrace")
        return
    end
    local image = emu.getScreenBuffer()
    if not comparison then
        if t == 140 then white = image[(playerY + 5) * 256 + playerX + 5] end
        original[t] = image
        local shotY = emu.read(0x0210, memory)
        local shotX = emu.read(0x0213, memory)
        local priority = emu.read(0x0202, memory) & 0x20
        -- The second shot crosses the gate before the player does.
        if t >= 156 and t <= 162 and shotY < 240 then
            if (emu.read(0x0212, memory) & 0x20) ~= 0 then
                fail(97, "shot was placed behind the gate")
                return
            end
            for x = shotX - 4, shotX + 4 do
                if image[(shotY + 2) * 256 + x + 1] == white then sawShot = true end
            end
        end
        if priority ~= 0 and t >= 163 and t <= 174 then
            -- A visible white pixel in the sprite even with background priority
            -- means the player is still readable through at least one opening.
            for y = playerY + 1, playerY + 16 do
                for x = playerX, playerX + 15 do
                    if image[y * 256 + x + 1] == white then sawOpening = true end
                end
            end
        end
    else
        local reference = original[t]
        local differences = 0
        for i, pixel in ipairs(image) do
            if pixel ~= reference[i] then
                local x = (i - 1) % 256
                local y = (i - 1) // 256
                if x < playerX or x >= playerX + 16 or y <= playerY or y > playerY + 16 then
                    fail(98, "foreground priority changed unrelated pixels")
                    return
                end
                differences = differences + 1
            end
        end
        if differences > 0 then sawOcclusion = true end
        if t == 204 then
            if not (sawOcclusion and sawOpening and sawShot) then
                fail(99, "missing occlusion, visible player, or visible projectile")
                return
            end
            emu.stop(0)
        end
    end
end, emu.eventType.endFrame)
