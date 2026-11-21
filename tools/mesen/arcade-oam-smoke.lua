-- Mesen 2.1.1, NTSC and the PPU's eight-sprite limit enabled.
-- Force a crowded scanline using the existing actor pools and verify selection
-- order as well as the item's appearance in the completed framebuffer.
local frame = 0
local memory = emu.memType.nesMemory
local seen, skipped = {}, {}
local baseline, itemVisible, itemHidden
local row
local actorX = { 104, 128, 152, 176, 216, 240 }

local function write(addr, value)
    emu.write(addr, value, memory)
end

local function setSlot(slot, ...)
    for _, field in ipairs({ ... }) do write(field[1] + slot, field[2]) end
end

local function prepareCrowd()
    for i = 0, 3 do
        setSlot(i, {0x6079, 1}, {0x607d, actorX[i + 1] + 8}, {0x6081, 0},
            {0x6085, 176}, {0x608d, 0}, {0x60be, 0}) -- stationary zombie for this frame
    end
    for i = 0, 2 do
        setSlot(i, {0x6026, 1}, {0x6029, 0}, {0x602c, 64 + i * 9},
            {0x602f, 0}, {0x6032, 164}, {0x6035, 0}, {0x6038, 0},
            {0x603b, 60}, {0x60b3, 0}) -- player lances
    end
    for i = 0, 1 do
        setSlot(i, {0x6095, 1}, {0x6097, 196 + i * 10}, {0x6099, 0},
            {0x609b, 164}, {0x609d, 0}, {0x609f, 60}, {0x60c2, 0}) -- enemy bolts
    end
    setSlot(3, {0x603e, 1}, {0x6042, 224}, {0x6046, 0}, {0x604a, 160}, {0x60b6, 0})
    setSlot(2, {0x6062, 1}, {0x6066, 240}, {0x606a, 0}, {0x606e, 176}, {0x60ba, 0})
end

local function itemPixels()
    local image = emu.getScreenBuffer()
    local pixels = {}
    -- Only the item covers X=240..247. Sample rows shared by all actors.
    for y = row - 5, row + 7 do
        for x = 240, 247 do pixels[#pixels + 1] = image[y * 256 + x + 1] end
    end
    return table.concat(pixels, ",")
end

local function check(cond, message)
    if cond then return true end
    print(string.format("FAIL frame=%d: %s", frame, message))
    emu.stop(1)
    return false
end

local function scanline(y)
    local count, slots, positions = 0, {}, {}
    for slot = 0, 63 do
        local base = 0x200 + slot * 4
        local top = emu.read(base, memory)
        if top < y and y <= top + 16 then
            count = count + 1
            if count <= 8 then slots[slot] = true end
            positions[emu.read(base + 3, memory)] = count <= 8
        end
    end
    return count, slots, positions
end

emu.addEventCallback(function()
    emu.setInput({start = frame == 128}, 0)
end, emu.eventType.inputPolled)

emu.addEventCallback(function()
    frame = frame + 1
    if frame < 156 then return end
    if frame == 156 then
        if not check(emu.read(0x8000, memory) == 0x8c, "stage 1 did not start") then return end
        local playerY = emu.read(0x200, memory)
        row = playerY + 9
        baseline = itemPixels()
    elseif frame >= 160 then
        local count, slots, positions = scanline(row)
        if not check(count == 18, "expected 18 sprites on the lower row, got " .. count) then return end
        for _, slot in ipairs({0, 1, 4, 5, 6, 7, 8}) do
            if not check(slots[slot], "lower row lost priority sprite in slot " .. slot) then return end
        end
        local upperCount, upperSlots = scanline(row - 18)
        if not check(upperCount == 10 and upperSlots[2] and upperSlots[3],
            "crowded upper row lost the player") then return end
        for i, x in ipairs(actorX) do
            local selected = positions[x]
            -- A zombie may step one pixel left before the OAM buffer is published.
            if selected == nil and i <= 4 then selected = positions[x - 1] end
            if not check(selected ~= nil, "missing OAM entry at X=" .. x) then return end
            if selected then seen[i] = true else skipped[i] = true end
        end
        local rendered = itemPixels() ~= baseline
        itemVisible = itemVisible or rendered
        itemHidden = itemHidden or not rendered
        if frame == 199 then
            for i, x in ipairs(actorX) do
                if not check(seen[i] and skipped[i], "actor never took its turn at X=" .. x) then return end
            end
            if not check(itemVisible and itemHidden, "item did not alternate in the framebuffer") then return end
            emu.stop(0)
            return
        end
    end
    prepareCrowd()
end, emu.eventType.endFrame)
