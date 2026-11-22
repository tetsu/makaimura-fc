-- Mesen 2.1.1, NTSC and the PPU's eight-sprite limit enabled.
-- Force a crowded scanline using the existing actor pools and verify selection
-- order as well as the item's appearance in the completed framebuffer. Then add
-- the boss, so that the player's shots, the enemy bolts and the boss overflow the
-- line by one, and verify that each of them is left out in turn.
local frame = 0
local memory = emu.memType.nesMemory
local seen, skipped = {}, {}
local baseline, itemVisible, itemHidden
local row
local actorX = { 104, 128, 152, 176, 216, 240 }
-- Slots 0..3 hold the player, 4..12 the priority block (shots, bolts, boss).
local prioFirst, prioEnd = 4, 12
local withBoss = false
local prioSeen, prioSkipped = {}, {}

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
    if withBoss then
        -- Active boss whose lower pair shares the row: with the player, nine sprites
        -- that come before the rotating groups.
        write(0x60a1, 1); write(0x60a3, 10); write(0x60a4, 0)
        write(0x60a5, 100); write(0x60a6, 0); write(0x60a7, 176); write(0x60b2, 0)
    end
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
    local count, slots, positions, prio = 0, {}, {}, {}
    for slot = 0, 63 do
        local base = 0x200 + slot * 4
        local top = emu.read(base, memory)
        if top < y and y <= top + 16 then
            count = count + 1
            if count <= 8 then slots[slot] = true end
            local x = emu.read(base + 3, memory)
            positions[x] = count <= 8
            if slot >= prioFirst and slot <= prioEnd then prio[x] = count <= 8 end
        end
    end
    return count, slots, positions, prio
end

local function countKeys(t)
    local n = 0
    for _ in pairs(t) do n = n + 1 end
    return n
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
    elseif frame >= 200 then
        -- Seven priority sprites and the player's two: one priority sprite is left
        -- out each frame, and the block's rotation hands that turn to each of them.
        local count, slots, _, prio = scanline(row)
        if not check(count == 20, "expected 20 sprites on the lower row with the boss, got " .. count) then return end
        if not check(slots[0] and slots[1], "the boss row lost the player") then return end
        if not check(countKeys(prio) == 7, "expected 7 priority sprites on the row, got " .. countKeys(prio)) then
            return
        end
        local shown = 0
        for x, selected in pairs(prio) do
            if selected then prioSeen[x] = true; shown = shown + 1 else prioSkipped[x] = true end
        end
        if not check(shown == 6, "expected 6 priority sprites drawn on the row, got " .. shown) then return end
        if frame == 240 then
            for x in pairs(prio) do
                if not check(prioSeen[x] and prioSkipped[x], "priority sprite never took its turn at X=" .. x) then
                    return
                end
            end
            emu.stop(0)
            return
        end
    elseif frame >= 160 then
        local count, slots, positions, prio = scanline(row)
        if not check(count == 18, "expected 18 sprites on the lower row, got " .. count) then return end
        if not check(slots[0] and slots[1], "lower row lost the player") then return end
        if not check(countKeys(prio) == 5, "expected 5 priority sprites on the lower row") then return end
        for x, selected in pairs(prio) do
            if not check(selected, "lower row lost the priority sprite at X=" .. x) then return end
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
            withBoss = true
        end
    end
    prepareCrowd()
end, emu.eventType.endFrame)
