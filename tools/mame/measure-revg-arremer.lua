-- MAME 0.289, makaimurg, default DIP switches, first credit from cold boot.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local first, last = 2450, 2750
local enemy = 0x0eb0
local actor_x = enemy + 16
local mode = os.getenv("REVG_ARREMER") or "baseline"
local pauses = {
    baseline = {0, 0},
    wait = {2520, 500},
    pause = {2520, 64},
    early_timer = {0, 0},
    no_slot = {0, 0},
}
local pause = assert(pauses[mode], "Unknown REVG_ARREMER: " .. mode)

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local right, jump, fire = field(":P1", 0x01), field(":P1", 0x20), field(":P1", 0x10)

local function word(addr)
    return mem:read_u8(addr) * 256 + mem:read_u8(addr + 1)
end

local function is_enemy_at(pc)
    return frame >= first and cpu.state.X.value == actor_x and cpu.state.PC.value == pc
end

revg_arremer_timer = mem:install_read_tap(enemy + 22, enemy + 22, "arremer shot timer", function(_, value)
    if not is_enemy_at(0xb7b2) then return end
    local effective = mode == "early_timer" and frame == 2624 and 0 or value
    if value <= 8 or effective ~= value then
        print(string.format("timer,%d,%d,%d", frame, value, effective))
    end
    if effective ~= value then return effective end
end)

revg_arremer_stage = mem:install_write_tap(enemy + 4, enemy + 5, "arremer launch stage", function(addr, value)
    if addr == enemy + 4 and value == 2 and is_enemy_at(0xb7bd) then
        print(string.format("ready,%d,%d,%d,%d,%d,%d", frame,
            word(0x0506), word(0x0509), word(enemy + 6), word(enemy + 9), mem:read_u8(enemy + 4)))
    elseif addr == enemy + 5 and value == 1 and is_enemy_at(0xb9c8) then
        print(string.format("windup,%d,%d", frame, mem:read_u8(enemy + 22)))
    end
end)

revg_arremer_slots = mem:install_read_tap(0x00a7, 0x00a7, "arremer projectile capacity", function(_, value)
    if not is_enemy_at(0xb9d3) then return end
    local effective = mode == "no_slot" and frame >= 2635 and frame < 2638 and 0 or value
    print(string.format("capacity,%d,%d,%d", frame, value, effective))
    if effective ~= value then return effective end
end)

revg_arremer_launch = mem:install_write_tap(0x13c0, 0x1460, "arremer shot", function(addr, value)
    if value == 1 and (addr - 0x13c0) % 32 == 0 and is_enemy_at(0xb9ed) then
        print(string.format("launch,%d,%04x,%d,%d,%d,%d,%d", frame, addr,
            word(0x0506), word(0x0509), word(enemy + 6), word(enemy + 9), mem:read_u8(addr + 21)))
    end
end)

print("trial," .. mode)
print("type,frame,player_x,player_y,enemy_x,enemy_y,phase,subphase,timer")
revg_arremer_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame >= first and (frame % 20 == 0 or (frame >= 2623 and frame <= 2639)) then
        print(string.format("sample,%d,%d,%d,%d,%d,%d,%d,%d", frame,
            word(0x0506), word(0x0509), word(enemy + 6), word(enemy + 9),
            mem:read_u8(enemy + 4), mem:read_u8(enemy + 5), mem:read_u8(enemy + 22)))
    end
    if frame == last then
        print("end," .. frame)
        machine:exit()
        return
    end
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    local resume = pause[1] + pause[2]
    local moving = frame > 1140 and (frame < pause[1] or frame >= resume)
    local progress = frame >= resume and frame - pause[2] or frame
    right:set_value(moving and 1 or 0)
    jump:set_value(moving and progress % 49 == 0 and 1 or 0)
    fire:set_value(moving and progress % 22 == 0 and 1 or 0)
end)
