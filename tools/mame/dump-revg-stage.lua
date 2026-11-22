-- MAME 0.289, makaimurg, default DIP switches.
-- Starts a section (optionally at another page of it), then dumps the camera, background RAM,
-- BG palette and object slots so verify-revg-stages.py can compare them with the ROM tables.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = assert(tonumber(os.getenv("REVG_STAGE_SECTION")), "Expected REVG_STAGE_SECTION=0 through 6")
local page = os.getenv("REVG_STAGE_PAGE")
page = page and tonumber(page, 16)
local walk = tonumber(os.getenv("REVG_STAGE_WALK") or "0")
-- Dump 3 frames after play starts ($05/$08/$0B/$0E = 1/1/1/0) following the section's initial
-- page expansion ($633F-$6377), and
-- again after holding right for REVG_STAGE_WALK frames with the hit timer $AD kept non-zero.
local settle = 3
local still = 120
local base, moved_cells, moved_colors, camera_moved = nil, {}, {}, false

local frame, selected = 0, false
local expanded_at, playing_at, first_dump, expand_dumped = nil, nil, nil, false
local scroll = { [0x3b08] = 0, [0x3b09] = 0, [0x3b0a] = 0, [0x3b0b] = 0 }
revg_stage_scroll_tap = mem:install_write_tap(0x3b08, 0x3b0b, "bg scroll", function(address, value)
    scroll[address] = value
end)

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start, right = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01), field(":P1", 0x01)
local entry = 0x6cee + 4 * section

local function word(address)
    return mem:read_u8(address) << 8 | mem:read_u8(address + 1)
end

local function hex_range(first, last)
    local parts = {}
    for address = first, last do parts[#parts + 1] = string.format("%02x", mem:read_u8(address)) end
    return table.concat(parts)
end

local function slots(first, count, stride)
    local parts = {}
    for i = 0, count - 1 do
        local x = first + i * stride
        if mem:read_u8(x - 0x10) ~= 0 then
            parts[#parts + 1] = string.format("%04x:%02x:%02x:%04x:%04x", x, mem:read_u8(x + 5),
                mem:read_u8(x - 0x0e), word(x - 0x0a), word(x - 0x07))
        end
    end
    return table.concat(parts, " ")
end

-- Record only the bytes the page expansion itself writes, so later column or row streaming
-- cannot hide what $62AB produced. Expansion writes are 16-bit, so taps see both bytes.
local expanded = {}
revg_stage_expand_tap = mem:install_write_tap(0x2800, 0x2fff, "initial expansion", function(address, value, mask)
    local pc = cpu.state.PC.value
    if selected and not first_dump and pc >= 0x633f and pc <= 0x6378 then
        expanded_at = frame
        expanded[address] = value
    end
end)

local function expanded_hex()
    local parts = {}
    for address = 0x2800, 0x2fff do
        local value = expanded[address]
        parts[#parts + 1] = value and string.format("%02x", value) or "--"
    end
    return table.concat(parts)
end

if page then
    -- Replace only the page of the normal start-table read; the in-page X/Y stay as in ROM.
    revg_stage_page_tap = mem:install_read_tap(entry, entry + 1, "stage start page", function(address)
        if not selected or cpu.state.PC.value ~= 0x6d39 then return end
        if address == entry then return 0x40 + (page >> 8) end
        return page & 0xff
    end)
end

revg_stage_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if expanded_at and not expand_dumped and frame >= expanded_at + 3 then
        -- What $62AB wrote: page p, then p-1, then the first 4 block columns of p+1 ("--" = unwritten).
        expand_dumped = true
        print(string.format("expand_dump,%d,%04x", section, word(0x91)))
        print("expanded," .. expanded_hex())
    end
    if expanded_at and not playing_at and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 1 and mem:read_u8(0x0e) == 0 then playing_at = frame end
    if not first_dump and playing_at and frame == playing_at + settle then first_dump = frame end
    -- Dumps: on settling (0) and after walking right for REVG_STAGE_WALK frames. In between,
    -- `still` idle frames list the ring cells and colours the game changes on its own
    -- (tile animation, colour cycling).
    if first_dump and frame > first_dump and frame <= first_dump + still then
        if word(0x9c) ~= base.camera_x or word(0x9e) ~= base.camera_y then camera_moved = true end
        for i = 0, 0x3ff do
            if mem:read_u8(0x2800 + i) ~= base.bg[i] or mem:read_u8(0x2c00 + i) ~= base.bg[0x400 + i] then
                moved_cells[i] = true
            end
        end
        for i = 0, 63 do
            if mem:read_u8(0x1632 + i) ~= base.palette[i]
                or (mem:read_u8(0x1672 + i) >> 4) ~= (base.palette[64 + i] >> 4) then moved_colors[i] = true end
        end
        if frame == first_dump + still then
            local cells, colors = {}, {}
            for i = 0, 0x3ff do if moved_cells[i] then cells[#cells + 1] = string.format("%x", i) end end
            for i = 0, 63 do if moved_colors[i] then colors[#colors + 1] = tostring(i) end end
            print(string.format("motion,%d,%s,%s", camera_moved and 1 or 0, table.concat(cells, " "),
                table.concat(colors, " ")))
        end
    end
    local walking = first_dump and frame > first_dump + still and frame <= first_dump + still + walk
    right:set_value(walking and 1 or 0)
    if walking then mem:write_u8(0xad, 0x20) end
    local dump_now = first_dump and (frame == first_dump or (walk > 0 and frame == first_dump + still + walk))
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
        -- REVG_STAGE_CHECKPOINT=1 restarts from the checkpoint table, as after passing it.
        if os.getenv("REVG_STAGE_CHECKPOINT") == "1" then mem:write_u8(0x81, 1) end
    end
    if dump_now then
        -- Mode bytes $05/$08/$0B/$0E and player state $0500/$0503 show whether play continued.
        print(string.format("stage_dump,%d,%s,%d,%d,%04x,%04x,%04x,%02x%02x,%02x%02x,%02x%02x%02x%02x,%02x%02x",
            section, page and string.format("%x", page) or "-", frame - first_dump, mem:read_u8(0x72), word(0x91),
            word(0x9c), word(0x9e), scroll[0x3b09], scroll[0x3b08], scroll[0x3b0b], scroll[0x3b0a],
            mem:read_u8(0x05), mem:read_u8(0x08), mem:read_u8(0x0b), mem:read_u8(0x0e),
            mem:read_u8(0x0500), mem:read_u8(0x0503)))
        print("bg," .. hex_range(0x2800, 0x2fff))
        -- Palette RAM is write-only in MAME; $1632-$16B1 is the copy the IRQ sends to it.
        print("palette," .. hex_range(0x1632, 0x16b1))
        print("enemies," .. slots(0x0890, 40, 0x30))
        print("support," .. slots(0x1010, 30, 0x20))
        if frame == first_dump then
            base = { camera_x = word(0x9c), camera_y = word(0x9e), bg = {}, palette = {} }
            for i = 0, 0x7ff do base.bg[i] = mem:read_u8(0x2800 + i) end
            for i = 0, 127 do base.palette[i] = mem:read_u8(0x1632 + i) end
        end
    end
    if dump_now and os.getenv("REVG_STAGE_SNAPSHOT") then
        -- The snapshot directory must exist; the file name is relative to it.
        machine.screens[":screen"]:snapshot(string.format("stage-%d-%s-%d.png", section,
            page and string.format("%x", page) or "start", frame - first_dump))
    end
    if first_dump and frame == first_dump + still + walk + 1 then machine:exit() end
    assert(frame < 3000, "Section expansion was not observed")
end)
