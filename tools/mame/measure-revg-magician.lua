-- MAME 0.289, makaimurg, default DIP switches.
-- Crouches and shoots the first tombstone of section 0 (support slot type $01 at (256, 62)) until
-- the magician (enemy type $07) appears, then stands still so its spell can reach the player.
-- Standing shots pass over the tombstone's hit box. The player is kept invulnerable with the hit
-- timer $AD while shooting, and not afterwards.
-- Logs the tombstone hit counter (+$0A), the scheduler slot $0770, the magician's enemy slot,
-- enemy projectiles, the active flags of the four player weapon slots, the player state, sprite
-- and $DF, and sound requests.
-- REVG_MAGICIAN_FRAMES sets the play length (default 1500).
-- REVG_MAGICIAN_SHIELD=1 keeps the player invulnerable again once the spell has hit, to measure how
-- long the transformation lasts without enemy contact.
-- REVG_MAGICIAN_SNAP=<t>,<t>,... saves magician-<t>.png at those frames of play.
-- REVG_MAGICIAN_COUNTER (optional) overwrites the tombstone's counter at t = 10, for example 1.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frames = tonumber(os.getenv("REVG_MAGICIAN_FRAMES") or "1500")
local counter = tonumber(os.getenv("REVG_MAGICIAN_COUNTER") or "")
local shield = os.getenv("REVG_MAGICIAN_SHIELD") == "1"
local spell_hit
local snaps = {}
for value in (os.getenv("REVG_MAGICIAN_SNAP") or ""):gmatch("%d+") do snaps[tonumber(value)] = true end

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local fire, down = field(":P1", 0x10), field(":P1", 0x04)
local right, jump = field(":P1", 0x01), field(":P1", 0x20)
-- REVG_MAGICIAN_CONTROL=1 drives the transformed player, counted from the frame the player
-- state becomes $0400: right is held for frames 40-135, jump is pressed at 160 and 240, fire
-- at 200, 208 and 216.
local control = os.getenv("REVG_MAGICIAN_CONTROL") == "1"
local hit_at
local frame, selected, playing_at = 0, false, nil
local tombstone, magician, appeared
local last = {}

local function word(address)
    return mem:read_u8(address) << 8 | mem:read_u8(address + 1)
end

revg_magician_sound = mem:install_write_tap(0x0140, 0x017f, "sound queue", function(_, value)
    local pc = cpu.state.PC.value
    if pc < 0x7778 or pc > 0x777a or not playing_at or value == 0xff then return end
    print(string.format("sound,%d,%02x,x=%04x", frame - playing_at, value, cpu.state.X.value))
end)

-- The spell: $C33E takes a slot from the enemy projectile free list ($A8) and writes type 0 at $C35F.
local spell
revg_magician_spell = mem:install_write_tap(0x0200, 0x1fff, "spell", function(address)
    local pc = cpu.state.PC.value
    if pc < 0xc35f or pc > 0xc361 or not playing_at then return end
    spell = address - 5
    print(string.format("spell,%d,slot=%04x,free=%d", frame - playing_at, spell, mem:read_u8(0xa7)))
end)

local function find_tombstone()
    for i = 0, 29 do
        local x = 0x1010 + i * 0x20
        if mem:read_u8(x - 0x10) ~= 0 and mem:read_u8(x + 5) == 1 and word(x - 0x0a) == 256 then return x end
    end
end

local function log_changes(t)
    local values = {
        tomb = tombstone and string.format("%d", mem:read_u8(tombstone + 0x0a)) or "-",
        sched = string.format("%02x/%04x/%d,%d", mem:read_u8(0x0760), word(0x0763), word(0x0776), word(0x0778)),
        limit = string.format("%d", mem:read_u8(0x37)),
        player = string.format("state=%04x sub=%02x sprite=%04x df=%02x ac=%02x x=%d y=%d", word(0x0503),
            mem:read_u8(0x0505), word(0x0513), mem:read_u8(0xdf), mem:read_u8(0xac), word(0x0506), word(0x0509)),
    }
    magician = nil
    for i = 0, 39 do
        local x = 0x0890 + i * 0x30
        if mem:read_u8(x - 0x10) ~= 0 and mem:read_u8(x + 5) == 7 then magician = x end
    end
    values.magician = magician and string.format("%04x state=%04x%02x x=%d y=%d", magician, word(magician - 0x0d),
        mem:read_u8(magician - 0x0b), word(magician - 0x0a), word(magician - 0x07)) or "-"
    local weapons = {}
    for i = 0, 3 do weapons[#weapons + 1] = string.format("%02x", mem:read_u8(0x0530 + i * 0x40)) end
    values.weapons = table.concat(weapons, " ")
    values.spell = spell and string.format("active=%02x type=%02x state=%04x x=%d y=%d", mem:read_u8(spell - 0x10),
        mem:read_u8(spell + 5), word(spell - 0x0d), word(spell - 0x0a), word(spell - 0x07)) or "-"
    -- Leave out the blank sprite $A4AB that blinks while the player is invulnerable.
    if word(0x0513) == 0xa4ab then values.player = last.player end
    for _, key in ipairs({ "tomb", "sched", "limit", "magician", "spell", "weapons", "player" }) do
        if last[key] ~= values[key] then
            print(string.format("state,%d,%s,%s", t, key, values[key]))
            last[key] = values[key]
        end
    end
end

revg_magician_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, 0)
    end
    if selected and not playing_at and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 1 and mem:read_u8(0x0e) == 0 then playing_at = frame end
    if not playing_at then
        assert(frame < 3000, "Play did not start")
        return
    end
    local t = frame - playing_at
    if t == 10 then
        tombstone = assert(find_tombstone(), "Tombstone at X 256 not found")
        print(string.format("tombstone,%04x,counter=%d", tombstone, mem:read_u8(tombstone + 0x0a)))
        if counter then mem:write_u8(tombstone + 0x0a, counter) end
    end
    -- Fire until the magician appears, then hold still.
    if magician then appeared = true end
    local shooting = not appeared
    if word(0x0503) == 0x0400 and not spell_hit then
        spell_hit = true
        hit_at = t
        print(string.format("transformed,%d", t))
    end
    local c = control and hit_at and t - hit_at
    local function pressed(...)
        for _, at in ipairs({ ... }) do
            if c and c >= at and c < at + 2 then return true end
        end
    end
    fire:set_value((shooting and t % 8 < 2 or pressed(200, 208, 216)) and 1 or 0)
    down:set_value(shooting and 1 or 0)
    right:set_value(c and c >= 40 and c < 136 and 1 or 0)
    jump:set_value(pressed(160, 240) and 1 or 0)
    if shooting or shield and spell_hit then mem:write_u8(0xad, 0x20) end
    if t >= 0 then log_changes(t) end
    if snaps[t] then machine.screens[":screen"]:snapshot(string.format("magician-%d.png", t)) end
    if t >= frames then machine:exit() end
end)
