-- MAME 0.289, makaimurg, default DIP switches, first credit after cold boot.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local test_frame = 1159
local recovery_first, recovery_last = 1272, 1395
local player, zombie = 0x0500, 0x0df0
local pose = os.getenv("REVG_CONTACT") or "stand"
local dx = math.tointeger(tonumber(os.getenv("REVG_DX") or "0"))
local dy = math.tointeger(tonumber(os.getenv("REVG_DY") or "0"))
assert(dx and dy and math.abs(dx) <= 64 and math.abs(dy) <= 64, "Expected offsets in -64..64")
assert(pose == "stand" or pose == "crouch" or pose == "air" or pose == "shot" or pose == "recovery",
    "Unknown REVG_CONTACT: " .. pose)

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin = field(":SYSTEM", 0x40)
local start = field(":SYSTEM", 0x01)
local crouch = field(":P1", 0x04)
local shot = field(":P1", 0x10)
local jump = field(":P1", 0x20)

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

-- Each entry names the CPU PC reported for an operand fetch, the offset
-- within the zombie record, and the axis to vary.
local axes = pose == "shot"
    and {[0xeefa] = {6, "x", dx}, [0xef05] = {9, "y", dy}}
    or {[0xf094] = {6, "x", dx}, [0xf089] = {9, "y", dy}}
local probed = {}
local hit = false
revg_contact_read = mem:install_read_tap(zombie + 6, zombie + 10, "zombie comparison", function(addr)
    local pc = cpu.state.PC.value
    local axis = axes[pc]
    if not axis or cpu.state.U.value ~= zombie + 16 then return end
    local offset, name, delta = table.unpack(axis)
    local byte = addr - zombie - offset
    if byte ~= 0 and byte ~= 1 then return end
    if pose == "recovery" then
        if byte == 0 and name == "y" and frame >= recovery_first and frame <= recovery_last then
            print(string.format("checked,%d,%d", frame, mem:read_u8(player)))
        end
        return
    end
    if frame ~= test_frame then return end
    local reference = cpu.state.Y.value - 16
    local anchor = word(reference + offset)
    if byte == 0 then
        local table_entry = cpu.state.X.value + (name == "x" and 0 or 4)
        print(string.format("probe,%d,%s,%d,%d,%04x,%d,%d", frame, name, delta, anchor,
            table_entry, word(table_entry), word(table_entry + 2)))
        probed[name] = true
    end
    -- Substitute only the two operand bytes fetched by this comparison.
    local target = (anchor + delta) & 0xffff
    return byte == 0 and target >> 8 or target & 0xff
end)

revg_contact_armor = mem:install_write_tap(player, player, "armor", function(_, value)
    local pc = cpu.state.PC.value
    if pose == "recovery" and frame >= recovery_first and frame <= recovery_last then
        print(string.format("armor,%d,%d,%04x", frame, value, pc))
    elseif frame == test_frame and value == 2 and pc == 0xf0a3 and cpu.state.U.value == zombie + 16 then
        hit = true
    end
end)
if pose == "recovery" then
    revg_contact_gate = mem:install_read_tap(player, player, "body check gate", function(_, value)
        if frame >= recovery_first and frame <= recovery_last and cpu.state.PC.value == 0xf061 then
            print(string.format("gate,%d,%d", frame, value))
        end
    end)
end
revg_contact_shot = mem:install_write_tap(zombie + 3, zombie + 3, "zombie phase", function(_, value)
    if pose == "shot" and frame == test_frame and value == 2 and cpu.state.PC.value == 0xef43 then
        hit = true
    end
end)

print(string.format("trial,%s,%d,%d", pose, dx, dy))
revg_contact_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if pose == "recovery" then
        if frame == recovery_last + 1 then
            machine:exit()
            return
        end
    elseif frame == test_frame then
        print(string.format("state,%d,%d,%d", frame, mem:read_u8(player + 31), mem:read_u8(player + 4)))
    elseif frame == test_frame + 1 then
        local outcome = next(probed) and (hit and "hit" or "miss") or "unchecked"
        print("result," .. outcome)
        machine:exit()
        return
    end
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    crouch:set_value(pose == "crouch" and frame >= 1140 and 1 or 0)
    shot:set_value(pose == "shot" and frame == 1140 and 1 or 0)
    jump:set_value(pose == "air" and frame == 1135 and 1 or 0)
end)
