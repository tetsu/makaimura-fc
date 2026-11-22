-- MAME 0.289, makaimurg, default DIP switches.
-- Starts a section (optionally at another page of it), keeps the player invulnerable and idle,
-- and takes a snapshot the first time each enemy or support type is on screen, so
-- capture-revg-types.py can crop and label what each type number looks like.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = assert(tonumber(os.getenv("REVG_TYPES_SECTION")), "Expected REVG_TYPES_SECTION=0 through 6")
local page = os.getenv("REVG_TYPES_PAGE")
page = page and tonumber(page, 16)
local frames = tonumber(os.getenv("REVG_TYPES_FRAMES") or "900")
local walk = os.getenv("REVG_TYPES_WALK") == "1"
-- A type is captured once it has stayed on screen this many frames (past its spawn animation).
local age = 24

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

local frame, selected, playing_at = 0, false, nil
local seen, alive = {}, {}
local shot_slot, nudge = nil, nil

if page then
    -- Same start-page override as dump-revg-stage.lua.
    revg_types_page_tap = mem:install_read_tap(entry, entry + 1, "stage start page", function(address)
        if not selected or cpu.state.PC.value ~= 0x6d39 then return end
        if address == entry then return 0x40 + (page >> 8) end
        return page & 0xff
    end)
end

-- Pools: enemies at $0890 (40 slots, stride $30), support objects at $1010 (30 slots, stride $20).
-- Slot fields as in dump-revg-stage.lua: active x-$10, type x+5, variant x-$0e, X x-$0a, Y x-$07.
local pools = { { "e", 0x0890, 40, 0x30 }, { "s", 0x1010, 30, 0x20 } }

local function scan()
    local camera_x, camera_y = word(0x9c), word(0x9e)
    local shot = nil
    for _, pool in ipairs(pools) do
        local name, first, count, stride = pool[1], pool[2], pool[3], pool[4]
        for i = 0, count - 1 do
            local x = first + i * stride
            local key = name .. i
            if mem:read_u8(x - 0x10) == 0 then
                alive[key] = nil
            else
                local kind, variant = mem:read_u8(x + 5), mem:read_u8(x - 0x0e)
                local id = string.format("%s%02x", name, kind)
                local sx = word(x - 0x0a) - camera_x
                local sy = camera_y + 239 - word(x - 0x07) - 16
                if sx >= 0x8000 then sx = sx - 0x10000 end
                local slot = alive[key]
                if not slot or slot.id ~= id then
                    slot = { id = id, since = frame }
                    alive[key] = slot
                end
                local visible = sx >= 16 and sx < 240 and sy >= 16 and sy < 208
                if visible and frame - slot.since >= age and not seen[id] and not shot then
                    seen[id] = true
                    shot_slot = x
                    shot = string.format("types-%d-%s-%s.png", section, page and string.format("%x", page) or "start", id)
                    print(string.format("seen,%d,%s,%s,%02x,%02x,%d,%d,%d,%04x,%s", section,
                        page and string.format("%x", page) or "-", name, kind, variant, sx, sy, frame - playing_at,
                        x, shot))
                end
            end
        end
    end
    if shot then
        machine.screens[":screen"]:snapshot(shot)
        -- Lift the object 24 px a moment later, so the pair of snapshots shows which sprite is its.
        nudge = { slot = shot_slot, at = frame + 1, file = shot:gsub("%.png$", "-up.png") }
    end
end

local function nudge_step()
    if not nudge then return end
    if frame == nudge.at then
        local y = word(nudge.slot - 0x07) + 24
        mem:write_u8(nudge.slot - 0x07, (y >> 8) & 0xff)
        mem:write_u8(nudge.slot - 0x06, y & 0xff)
    elseif frame == nudge.at + 2 then
        machine.screens[":screen"]:snapshot(nudge.file)
        nudge = nil
    end
end

revg_types_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
    end
    if selected and not playing_at and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 1 and mem:read_u8(0x0e) == 0 then playing_at = frame end
    if playing_at then
        -- The hit timer keeps the player from being knocked back or killed.
        mem:write_u8(0xad, 0x20)
        right:set_value(walk and (frame // 8) % 4 == 0 and 1 or 0)
        nudge_step()
        if not nudge then scan() end
        if frame >= playing_at + frames then machine:exit() end
    end
    assert(frame < 3000 + frames, "Play did not start")
end)
