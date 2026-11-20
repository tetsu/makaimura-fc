-- MAME 0.289, makaimurg, default DIP switches, first credit after cold boot.
local machine = manager.machine
local mem = machine.devices[":maincpu"].spaces.program
local frame = 0
local first, last = 1050, 1340
local zombie = 0x0df0
local trial = os.getenv("REVG_ENEMY") or "idle"
local trials = {
    idle = {},
    early = {{0x01, 1140, 1200}},
    delayed = {{0x01, 1180, 1240}},
    wait_ahead = {{0x01, 1140, 1160}},
    wait_behind = {{0x02, 1140, 1190}},
    intro_left = {{0x02, 1040, 1190}},
    late_start = {},
    jump = {{0x20, 1260, 1261}},
    shot = {{0x10, 1220, 1221}},
}
local schedule = assert(trials[trial], "Unknown REVG_ENEMY: " .. trial)
local start_frame = trial == "late_start" and 560 or 500

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin = field(":SYSTEM", 0x40)
local start = field(":SYSTEM", 0x01)
local p1 = {}
for _, mask in ipairs({0x01, 0x02, 0x10, 0x20}) do
    p1[mask] = field(":P1", mask)
end

local scroll = {0, 0}
revg_enemy_scroll = mem:install_write_tap(0x3b08, 0x3b09, "scroll", function(addr, data)
    scroll[addr - 0x3b08 + 1] = data
end)
revg_enemy_sound = mem:install_write_tap(0x3a00, 0x3a00, "sound", function(_, data)
    if frame >= first then print(string.format("sound,%d,%02x", frame, data)) end
end)

local function x256(base)
    local x = mem:read_u8(base + 7) + 256 * mem:read_u8(base + 6)
    return 256 * x + mem:read_u8(base + 8)
end

local function zombie_sprite_in_view(x)
    for addr = 0x1e00, 0x1ffc, 4 do
        local tile = mem:read_u8(addr)
        local attr = mem:read_u8(addr + 1)
        local sy = mem:read_u8(addr + 2)
        local sx = mem:read_u8(addr + 3) - 256 * (attr & 1)
        -- Walking tiles and palette observed for this slot in the opening.
        if tile >= 0xa8 and tile <= 0xbd and (attr & 0xf0) == 0x30
            and sy >= 16 and sy < 240 and sx == x - 16 and sx >= -15 and sx < 256 then
            return 1
        end
    end
    return 0
end

print("trial," .. trial)
print("type,frame,player_x256,player_y,scroll_x,armor_state,player_state,zombie_x256,zombie_y,slot_state,phase,sprite_in_view")
revg_enemy_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame >= first then
        local sx = scroll[1] + 256 * scroll[2]
        local enemy_x = x256(zombie)
        print(string.format("sample,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", frame,
            x256(0x0500), mem:read_u8(0x050a), sx, mem:read_u8(0x0500), mem:read_u8(0x0503),
            enemy_x, mem:read_u8(zombie + 10), mem:read_u8(zombie), mem:read_u8(zombie + 3),
            zombie_sprite_in_view(enemy_x // 256 - sx)))
    end
    if frame == last then
        machine:exit()
        return
    end
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= start_frame and frame < start_frame + 4 and 1 or 0)
    local input = 0
    for _, entry in ipairs(schedule) do
        if frame >= entry[2] and frame < entry[3] then input = input | entry[1] end
    end
    for mask, item in pairs(p1) do item:set_value((input & mask) ~= 0 and 1 or 0) end
end)
