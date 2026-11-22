-- MAME 0.289, makaimurg, default DIP switches.
-- Checks a support type $00 trigger (variants 0-3) in section 0.
-- REVG_HIDDEN_VARIANT=0 (default): the hidden armour at (1440, 96), started from page $005.
-- REVG_HIDDEN_VARIANT=1: the hidden 10,000-point item at (2968, 112), started from page $00B.
-- Sets $AC (0 = armoured, 1 = armour lost) from REVG_ARMOUR_AC, then holds the player on the
-- trigger for a few frames and logs the support slot, the item pool, the score and sound requests.
-- If an item appears, the player is moved onto it once it has landed, to log the pickup.
-- With REVG_ARMOUR_RETRY=1, $AC is set to 1 after the first touch and the trigger is touched again.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local variant = tonumber(os.getenv("REVG_HIDDEN_VARIANT") or "0")
local ac = assert(tonumber(os.getenv("REVG_ARMOUR_AC")), "Expected REVG_ARMOUR_AC=0 or 1")
local retry = os.getenv("REVG_ARMOUR_RETRY") == "1"
-- Variants 2 and 3 are the 200- and 500-point discs at (368, 56) and (1408, 56).
local targets = { [0] = { 0x005, 1440, 96 }, [1] = { 0x00b, 2968, 112 }, [2] = { 0x001, 368, 56 },
    [3] = { 0x005, 1408, 56 } }
local target = assert(targets[variant], "Expected REVG_HIDDEN_VARIANT=0 to 3")
local section, page = 0, target[1]
local trigger_x, trigger_y = target[2], target[3]
local tag = string.format("hidden-v%d-ac%d%s", variant, ac, retry and "-retry" or "")

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local entry = 0x6cee + 4 * section

local function word(address)
    return mem:read_u8(address) << 8 | mem:read_u8(address + 1)
end

local function put_word(address, value)
    mem:write_u8(address, (value >> 8) & 0xff)
    mem:write_u8(address + 1, value & 0xff)
end

local frame, selected, playing_at = 0, false, nil
local trigger_slot, hold_from, hold_at, item_landed_at, picked = nil, nil, nil, nil, false
local last = {}

revg_armour_page_tap = mem:install_read_tap(entry, entry + 1, "stage start page", function(address)
    if not selected or cpu.state.PC.value ~= 0x6d39 then return end
    if address == entry then return 0x40 + (page >> 8) end
    return page & 0xff
end)

-- Sound requests: $10 is the armour pickup.
revg_armour_sound = mem:install_write_tap(0x3a00, 0x3a00, "sound", function(_, data)
    if playing_at then print(string.format("sound,%d,%02x", frame - playing_at, data)) end
end)

-- Every write to the disc bits $74-$77, from power-on.
revg_armour_once = mem:install_write_tap(0x74, 0x77, "once bits", function(address, data)
    print(string.format("once_write,%d,%04x,%02x,pc=%04x,e0=%02x", playing_at and frame - playing_at or -frame,
        address, data, cpu.state.PC.value, mem:read_u8(0xe0)))
end)

local function find_trigger()
    for i = 0, 29 do
        local x = 0x1010 + i * 0x20
        if mem:read_u8(x - 0x10) ~= 0 and mem:read_u8(x + 5) == 0 and mem:read_u8(x - 0x0e) == variant
            and word(x - 0x0a) == trigger_x and word(x - 0x07) == trigger_y then
            return x
        end
    end
end

local function log_changes(t)
    local values = {
        ac = mem:read_u8(0xac),
        armour = mem:read_u8(0x0500),
        score = string.format("%02x%02x%02x%02x", mem:read_u8(0x68), mem:read_u8(0x69), mem:read_u8(0x6a),
            mem:read_u8(0x6b)),
        -- One-time bits of the discs ($E3BA clears the bit for the slot index in $E0).
        once = string.format("%02x%02x%02x%02x", mem:read_u8(0x74), mem:read_u8(0x75), mem:read_u8(0x76),
            mem:read_u8(0x77)),
        trig = trigger_slot and string.format("%02x/%04x/%04x", mem:read_u8(trigger_slot - 0x10),
            word(trigger_slot - 0x0d), word(trigger_slot + 3)) or "-",
    }
    for i = 0, 1 do
        local x = 0x15e2 + i * 0x30
        values["item" .. i] = mem:read_u8(x - 0x10) == 0 and "-" or string.format("type=%02x kind=%02x state=%04x x=%d y=%d",
            mem:read_u8(x + 5), mem:read_u8(x - 0x0e), word(x - 0x0d), word(x - 0x0a), word(x - 0x07))
    end
    for _, key in ipairs({ "ac", "armour", "score", "once", "trig", "item0", "item1" }) do
        if last[key] ~= values[key] then
            print(string.format("state,%d,%s,%s,player=%d/%d", t, key, tostring(values[key]),
                word(0x0506), word(0x0509)))
            last[key] = values[key]
        end
    end
end

local function hold(x, y)
    put_word(0x0506, x)
    put_word(0x0509, y)
end

revg_armour_frame = emu.add_machine_frame_notifier(function()
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
    if not playing_at then
        assert(frame < 3000, "Play did not start")
        return
    end
    local t = frame - playing_at
    mem:write_u8(0xad, 0x20) -- hit timer: no knockback from enemies during the check
    if t == 30 then
        trigger_slot = assert(find_trigger(), "Hidden-armour trigger not found")
        print(string.format("trigger,%04x", trigger_slot))
        mem:write_u8(0xac, ac)
        hold_from = t
    end
    if hold_from and t >= hold_from and t < hold_from + 8 then hold(trigger_x, trigger_y) end
    if retry and hold_from == 30 and t == 60 then
        mem:write_u8(0xac, 1)
        hold_from = 70
    end
    -- Once an item waits to be picked up (state $0103, $E6AB), step onto it.
    local item = 0x1612
    if not picked and mem:read_u8(item - 0x10) ~= 0 and word(item - 0x0d) == 0x0103 then
        item_landed_at = item_landed_at or t
        if t >= item_landed_at + 30 then
            hold_at = hold_at or t
            if t < hold_at + 8 then hold(word(item - 0x0a), word(item - 0x07)) else picked = true end
        end
    end
    if t >= 0 then log_changes(t) end
    if t >= 400 then
        machine.screens[":screen"]:snapshot(tag .. ".png")
        machine:exit()
    end
    if hold_from and t == hold_from + 40 then
        machine.screens[":screen"]:snapshot(tag .. "-touch.png")
    end
end)
