-- MAME 0.289, makaimurg, default DIP switches, first credit after cold boot.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local first = 1140
local trial = os.getenv("REVG_TRIAL") or "idle"
local last = trial == "contact" and 1280 or 1190
-- The flat-ground jump lands during the update after notification 1177.
local land = 1177
local trials = {
    idle = {},
    contact = {},
    walk = {{0x01, first, first + 15}},
    reverse = {{0x01, first, first + 15}, {0x02, first + 15, first + 24}},
    crouch = {{0x04, first, first + 6}},
    shot = {{0x10, first, first + 1}},
    jump = {{0x20, first, first + 1}},
    jump_hold = {{0x20, first, last}},
    jump_before_land = {{0x20, first, first + 1}, {0x20, land - 2, land - 1}},
    jump_on_land = {{0x20, first, first + 1}, {0x20, land - 1, land}},
    jump_after_land = {{0x20, first, first + 1}, {0x20, land, land + 1}},
    forward = {{0x01, first, last}, {0x20, first + 4, first + 5}},
    simultaneous = {{0x01, first, last}, {0x20, first, first + 1}},
}
local schedule = assert(trials[trial], "Unknown REVG_TRIAL: " .. trial)

if os.getenv("REVG_INPUT_TRACE") == "1" then
    local entries = {
        [0x6f22] = "play_irq", [0x6f54] = "first_section", [0x7097] = "section_dispatch",
        [0x70f3] = "play_update", [0x691c] = "select_input", [0x8f0e] = "player_update",
        [0x8fdf] = "ground_update", [0x9233] = "horizontal_input", [0x9265] = "walk",
        [0x8b0a] = "ground_probe", [0x8b7d] = "right_probe", [0x8baf] = "left_probe",
        [0x907b] = "jump_test", [0x9084] = "jump_accept", [0x93ca] = "air_update",
        [0x9415] = "jump_init", [0x942b] = "air_step", [0x946a] = "air_down",
        [0x952d] = "land", [0xee73] = "combat_update",
        [0xf0a1] = "body_hit", [0xf4e2] = "projectile_hit", [0x995f] = "hit_update",
    }
    local player_only = {[0x8b0a] = true, [0x8b7d] = true, [0x8baf] = true}
    revg_input_trace = {}
    print("trace_type,notice,address,routine,current_input,previous_input,player_mode,air_phase,move_direction")
    for addr, routine in pairs(entries) do
        revg_input_trace[#revg_input_trace + 1] = mem:install_read_tap(addr, addr, routine, function()
            if frame < first - 2 or cpu.state.PC.value ~= addr + 1 then return end
            if player_only[addr] and cpu.state.X.value ~= 0x0510 then return end
            print(string.format("trace,%d,%04x,%s,%02x,%02x,%02x,%02x,%02x", frame, addr, routine,
                mem:read_u8(0x004b), mem:read_u8(0x004c), mem:read_u8(0x0503),
                mem:read_u8(0x0504), mem:read_u8(0x050e)))
        end)
    end
end

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin = field(":SYSTEM", 0x40)
local start = field(":SYSTEM", 0x01)
local p1 = {}
for _, mask in ipairs({0x01, 0x02, 0x04, 0x10, 0x20}) do
    p1[mask] = field(":P1", mask)
end

-- Tap timestamps name the last frame notification. Sample rows read RAM
-- at the notification itself, before assigning the input for the next poll.
local last_input = 0xff
local scroll = {0, 0}
revg_input_tap = mem:install_read_tap(0x3001, 0x3001, "p1", function(_, data)
    if frame >= first - 2 and data ~= last_input then
        print(string.format("input,%d,%02x", frame, data))
        last_input = data
    end
end)
revg_scroll_tap = mem:install_write_tap(0x3b08, 0x3b09, "scroll", function(addr, data)
    scroll[addr - 0x3b08 + 1] = data
end)
revg_sound_tap = mem:install_write_tap(0x3a00, 0x3a00, "sound", function(_, data)
    if frame >= first - 2 then print(string.format("sound,%d,%02x", frame, data)) end
end)

local function shot_x()
    for addr = 0x1e00, 0x1ffc, 4 do
        -- Confirmed against the idle trial for this initial weapon and direction.
        if mem:read_u8(addr) == 0x54 and mem:read_u8(addr + 1) == 0 then
            return mem:read_u8(addr + 3)
        end
    end
    return -1
end

print("trial," .. trial)
print("type,frame,x_integer,x_fraction,y_integer,scroll_x,move_state,jump_state,crouch,shot_sprite_x")
revg_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame >= first - 2 then
        print(string.format("sample,%d,%d,%d,%d,%d,%d,%d,%d,%d", frame,
            mem:read_u8(0x0507), mem:read_u8(0x0508), mem:read_u8(0x050a),
            scroll[1] + 256 * scroll[2], mem:read_u8(0x050e),
            mem:read_u8(0x0504), mem:read_u8(0x051f), shot_x()))
    end
    if frame == last then
        machine:exit()
        return
    end
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    local input = 0
    for _, entry in ipairs(schedule) do
        if frame >= entry[2] and frame < entry[3] then input = input | entry[1] end
    end
    for mask, item in pairs(p1) do item:set_value((input & mask) ~= 0 and 1 or 0) end
end)
