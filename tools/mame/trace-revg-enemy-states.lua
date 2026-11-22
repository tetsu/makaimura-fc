-- MAME 0.289, makaimurg, default DIP switches.
-- Logs the state changes of chosen enemy types during play of one section, with how far each
-- enemy moved during the state it is leaving, so speeds and timers can be read from the log.
-- REVG_STATES_SECTION picks the section (default 0), REVG_STATES_PAGE (hex, optional) the start
-- page, REVG_STATES_FRAMES the play length (default 3600), REVG_STATES_TYPES the enemy types as
-- hex numbers separated by commas (default all).
-- The player holds right and jumps every 64 frames (REVG_STATES_STILL=1 stands still instead,
-- REVG_STATES_SEED=<n> moves at random as in log-revg-sound-sources.lua) and is kept
-- invulnerable with the hit timer $AD. REVG_STATES_FIRE=1 fires every 8 frames (default no fire,
-- so enemies live through their whole cycle). REVG_STATES_RANK=<n> sets the difficulty counter $61
-- when the section is chosen, so $9A starts at n when the section loads (rows: 0-15, 16-31, 32-47,
-- 48-63).
-- Output lines (t counts frames of play; positions are the slot's X -$0A and Y -$07 words, Y up):
--   spawn,<t>,slot=,type=,variant=,state=,x=,y=,px=,py=
--   state,<t>,slot=,type=,variant=,from=<ssss.bb>,to=<ssss.bb>,frames=,dx=,dy=,x=,y=,px=,py=[,hp=]
--   free,<t>,slot=,type=,variant=,state=,frames=,dx=,dy=,x=,y=
--   shot,<t>,slot=,type=,x=,y=,dx16=,dy16=  (enemy projectile, its type, and how far it went in 16 frames)
--   hp,<t>,slot=,type=,hp=,state=  (hit points changed)
--   weapon,<t>,slot=,state=<ssss.bb>,x=,y=  (player weapon slot $0540 + n * $40 changed to a $02xx state)
--   weapon-free,<t>,slot=,state=,frames=,dx=,dy=  (that weapon slot freed: time and movement since its last $02xx state)
--   sound,<t>,<request>  (sound requests)
--   rank,<t>,<$9A word>  (at the start of play)
-- "ssss.bb" is the state word -$0D/-$0C and the step -$0B.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = tonumber(os.getenv("REVG_STATES_SECTION") or "0")
local page = os.getenv("REVG_STATES_PAGE") and tonumber(os.getenv("REVG_STATES_PAGE"), 16)
local frames = tonumber(os.getenv("REVG_STATES_FRAMES") or "3600")
local still = os.getenv("REVG_STATES_STILL") == "1"
local seed = tonumber(os.getenv("REVG_STATES_SEED") or "")
local firing = os.getenv("REVG_STATES_FIRE") == "1"
local rank = tonumber(os.getenv("REVG_STATES_RANK") or "")
local types
for value in (os.getenv("REVG_STATES_TYPES") or ""):gmatch("%x+") do
    types = types or {}
    types[tonumber(value, 16)] = true
end
-- Slot offset of the hit points for the types that have them ($EF7A).
local hp_offset = { [0x05] = 0x1e, [0x06] = 0x14, [0x08] = 0x16, [0x0a] = 0x16, [0x0c] = 0x10, [0x0d] = 0x16,
    [0x12] = 0x16 }

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local right, jump, fire = field(":P1", 0x01), field(":P1", 0x20), field(":P1", 0x10)
local left, down, up = field(":P1", 0x02), field(":P1", 0x04), field(":P1", 0x08)
local directions = { right, left, up, down }
local choice, jumping = 1, false

local function next_random()
    seed = (seed * 1103515245 + 12345) % 0x80000000
    return seed >> 16
end

local frame, selected, playing_at = 0, false, nil
local entry = 0x6cee + 4 * section

local function word(address)
    return mem:read_u8(address) << 8 | mem:read_u8(address + 1)
end

local function signed(value)
    return value >= 0x8000 and value - 0x10000 or value
end

if page then
    revg_states_page = mem:install_read_tap(entry, entry + 1, "start page", function(address)
        if not selected or cpu.state.PC.value ~= 0x6d39 then return end
        if address == entry then return 0x40 + (page >> 8) end
        return page & 0xff
    end)
end

local enemies, shots, weapons = {}, {}, {}

revg_states_sound = mem:install_write_tap(0x0140, 0x017f, "sound queue", function(_, value)
    local pc = cpu.state.PC.value
    if pc < 0x7778 or pc > 0x777a or not playing_at or value == 0xff then return end
    print(string.format("sound,%d,%02x", frame - playing_at, value))
end)

local function state_of(x)
    return string.format("%04x.%02x", word(x - 0x0d), mem:read_u8(x - 0x0b))
end

local function player()
    return string.format("px=%d,py=%d", word(0x0506), word(0x0509))
end

local function track_enemies(t)
    for i = 0, 39 do
        local x = 0x0890 + i * 0x30
        local active = mem:read_u8(x - 0x10) ~= 0
        local kind = mem:read_u8(x + 5)
        local e = enemies[x]
        if e and (not active or kind ~= e.type) then
            print(string.format("free,%d,slot=%04x,type=%02x,variant=%02x,state=%s,frames=%d,dx=%d,dy=%d,x=%d,y=%d",
                t, x, e.type, e.variant, e.state, t - e.since, e.x - e.sx, e.y - e.sy, e.x, e.y))
            enemies[x], e = nil, nil
        end
        if active and (not types or types[kind]) then
            local px, py, state = signed(word(x - 0x0a)), signed(word(x - 0x07)), state_of(x)
            if not e then
                e = { type = kind, variant = mem:read_u8(x - 0x0e), state = state, since = t, sx = px, sy = py }
                enemies[x] = e
                print(string.format("spawn,%d,slot=%04x,type=%02x,variant=%02x,state=%s,x=%d,y=%d,%s",
                    t, x, kind, e.variant, state, px, py, player()))
            elseif state ~= e.state then
                local hp = hp_offset[kind] and string.format(",hp=%d", mem:read_u8(x + hp_offset[kind])) or ""
                print(string.format("state,%d,slot=%04x,type=%02x,variant=%02x,from=%s,to=%s,frames=%d,dx=%d,dy=%d,x=%d,y=%d,%s%s",
                    t, x, kind, e.variant, e.state, state, t - e.since, px - e.sx, py - e.sy, px, py, player(), hp))
                e.state, e.since, e.sx, e.sy = state, t, px, py
            end
            local hp = hp_offset[kind] and mem:read_u8(x + hp_offset[kind])
            if hp and e.hp and hp ~= e.hp then
                print(string.format("hp,%d,slot=%04x,type=%02x,hp=%d,state=%s", t, x, kind, hp, state))
            end
            e.x, e.y, e.hp = px, py, hp
        end
    end
end

-- Enemy projectiles: 6 slots of $20 bytes from $13D0.
local function track_shots(t)
    for i = 0, 5 do
        local x = 0x13d0 + i * 0x20
        local active = mem:read_u8(x - 0x10) ~= 0
        local s = shots[x]
        if not active then
            shots[x] = nil
        elseif not s then
            shots[x] = { since = t, x = signed(word(x - 0x0a)), y = signed(word(x - 0x07)) }
        elseif t - s.since == 16 then
            print(string.format("shot,%d,slot=%04x,type=%02x,x=%d,y=%d,dx16=%d,dy16=%d", s.since, x,
                mem:read_u8(x + 5), s.x, s.y, signed(word(x - 0x0a)) - s.x, signed(word(x - 0x07)) - s.y))
        end
    end
end

local function track_weapons(t)
    for i = 0, 3 do
        local x = 0x0540 + i * 0x40
        local w = weapons[i]
        local state = mem:read_u8(x - 0x10) ~= 0 and state_of(x) or nil
        if w and state ~= w.state then
            if not state then
                print(string.format("weapon-free,%d,slot=%04x,state=%s,frames=%d,dx=%d,dy=%d", t, x, w.state,
                    t - w.since, w.x - w.sx, w.y - w.sy))
                weapons[i], w = nil, nil
            elseif state:sub(1, 2) ~= "02" and state:sub(1, 2) ~= "03" then
                weapons[i], w = nil, nil
            end
        end
        if state and state:sub(1, 2) == "02" and (not w or state ~= w.state) then
            local px, py = signed(word(x - 0x0a)), signed(word(x - 0x07))
            print(string.format("weapon,%d,slot=%04x,state=%s,x=%d,y=%d", t, x, state, px, py))
            w = { state = state, since = t, sx = px, sy = py }
            weapons[i] = w
        end
        if w then w.x, w.y = signed(word(x - 0x0a)), signed(word(x - 0x07)) end
    end
end

revg_states_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
        if rank then mem:write_u8(0x61, rank) end
    end
    if selected and not playing_at and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 1 and mem:read_u8(0x0e) == 0 then playing_at = frame end
    assert(frame < 3000 + frames, "Play did not start")
    if not playing_at then return end
    local t = frame - playing_at
    if t == 0 then print(string.format("rank,0,%04x", word(0x9a))) end
    mem:write_u8(0xad, 0x20)
    if seed then
        if t % 16 == 0 then
            local r = next_random()
            choice, jumping = r % 6, (r >> 4) % 3 == 0
        end
        for i, item in ipairs(directions) do item:set_value(choice == i and 1 or 0) end
        jump:set_value(jumping and t % 16 < 4 and 1 or 0)
    else
        right:set_value(still and 0 or 1)
        jump:set_value(not still and t % 64 < 4 and 1 or 0)
    end
    fire:set_value(firing and t % 8 < 2 and 1 or 0)
    track_enemies(t)
    track_shots(t)
    track_weapons(t)
    if t >= frames then
        print(string.format("end,%d,%s", t, player()))
        machine:exit()
    end
end)
