-- MAME 0.289, makaimurg, default DIP switches, first credit from cold boot.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local shot = 0x1460
local pose = os.getenv("REVG_PROJECTILE") or "air"
local followup = pose == "followup"
local target = followup and 2640 or 1874
local source = followup and 0x0eb0 or 0x0fa0
local requested_dx, requested_dy = os.getenv("REVG_DX"), os.getenv("REVG_DY")
local dx = requested_dx and math.tointeger(tonumber(requested_dx))
local dy = requested_dy and math.tointeger(tonumber(requested_dy))
assert(pose == "stand" or pose == "crouch" or pose == "air" or pose == "recovery" or followup,
    "Unknown REVG_PROJECTILE: " .. pose)
assert((not requested_dx and not requested_dy) or
    (dx and dy and math.abs(dx) <= 64 and math.abs(dy) <= 64 and pose ~= "recovery"),
    "Supply both offsets in -64..64 for an overlap trial")

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end
local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local right, down = field(":P1", 0x01), field(":P1", 0x04)
local jump, fire = field(":P1", 0x20), field(":P1", 0x10)

local function word(addr)
    return mem:read_u8(addr) * 256 + mem:read_u8(addr + 1)
end

local function fixed(addr)
    return word(addr) * 256 + mem:read_u8(addr + 2)
end

revg_projectile_launch = mem:install_write_tap(0x13c0, 0x1460, "projectile launch", function(addr, value)
    local pc = cpu.state.PC.value
    if frame >= 1140 and (addr - 0x13c0) % 32 == 0 and value == 1 and (pc == 0xdedd or pc == 0xb9ed) then
        print(string.format("launch,%d,%04x,%04x", frame, cpu.state.X.value - 16, addr))
    end
end)
local table_address, hit
local observed = {}
local axes = {[0xf4bd] = {9, "y", dy}, [0xf4c8] = {6, "x", dx}}
revg_projectile_compare = mem:install_read_tap(shot + 6, shot + 10, "projectile comparison", function(addr, data)
    if frame ~= target or cpu.state.U.value ~= shot + 16 then return end
    local axis = axes[cpu.state.PC.value]
    if not axis then return end
    local offset, name, delta = table.unpack(axis)
    local byte = addr - shot - offset
    if byte ~= 0 and byte ~= 1 then return end
    table_address = cpu.state.X.value
    local anchor = word(0x0500 + offset)
    if byte == 0 then
        observed[offset] = data * 256
    else
        local original = observed[offset] + data
        print(string.format("compare,%d,%s,%d,%d,%d", frame, name, original, anchor, delta or original - anchor))
    end
    if delta then
        local operand = (anchor + delta) & 0xffff
        return byte == 0 and operand >> 8 or operand & 0xff
    end
end)

revg_projectile_damage = mem:install_write_tap(0x0500, 0x0500, "armor state", function(_, data)
    local pc = cpu.state.PC.value
    if frame == target and cpu.state.U.value == shot + 16 and pc == 0xf4e4 and data == 2 then hit = true end
    if frame >= target and pose == "recovery" then
        print(string.format("armor,%d,%d,%04x", frame, data, pc))
    end
end)
if pose == "recovery" then
    revg_projectile_timer = mem:install_read_tap(0x00ad, 0x00ad, "projectile immunity timer", function(_, data)
        if frame >= target and cpu.state.PC.value == 0xf48e then
            print(string.format("timer,%d,%d", frame, data))
        end
    end)
    revg_projectile_gate = mem:install_read_tap(0x0500, 0x0500, "projectile armor gate", function(_, data)
        if frame >= target and cpu.state.PC.value == 0xf496 then
            print(string.format("gate,%d,%d", frame, data))
        end
    end)
end

print(string.format("trial,%s,%s,%s", pose, dx or "native", dy or "native"))
print("type,frame,enemy_x,enemy_y,player_x,player_y,shot_x256,shot_y256,airborne,crouch,shot_type")
revg_projectile_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    local moving = frame > 1140 and (frame < 1854 or pose == "air" or pose == "recovery" or followup)
    right:set_value(moving and 1 or 0)
    fire:set_value(moving and frame % 22 == 0 and 1 or 0)
    jump:set_value(moving and frame % 49 == 0 and 1 or 0)
    down:set_value(pose == "crouch" and frame >= 1854 and 1 or 0)
    if (followup and frame >= 2636 and frame <= 2644)
        or (not followup and ((frame >= 1846 and frame <= 1850) or frame == target)) then
        print(string.format("sample,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", frame,
            word(source + 6), word(source + 9), word(0x0506), word(0x0509), fixed(shot + 6), fixed(shot + 9),
            mem:read_u8(0x0504), mem:read_u8(0x051f), mem:read_u8(shot + 21)))
    end
    if frame == target + 1 then
        if table_address then
            print(string.format("table,%04x,%d,%d,%d,%d", table_address,
                word(table_address), word(table_address + 2), word(table_address + 4), word(table_address + 6)))
        end
        print("result," .. (table_address and (hit and "hit" or "miss") or "unchecked"))
        if pose ~= "recovery" and not followup then machine:exit() end
    elseif frame == target + 130 and pose == "recovery" then
        machine:exit()
    elseif followup and frame == 2645 then
        machine:exit()
    end
end)
