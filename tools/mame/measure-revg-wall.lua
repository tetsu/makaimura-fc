-- MAME 0.289, makaimurg, default DIP switches.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local direction = os.getenv("REVG_WALL") or "right"
local requested_x = os.getenv("REVG_WALL_X")
local local_x = requested_x and math.tointeger(tonumber(requested_x))
assert(direction == "right" or direction == "left", "Expected right or left")
assert(not requested_x or (local_x and local_x >= 0 and local_x < 16), "Expected a tile-local X in 0..15")

local test = direction == "right"
    and {frame = 1320, attr = 0x8b92, tile = 0x8b9a, depth = 0x8ba6, limit = 0x458e}
    or {frame = 1160, attr = 0x8bc4, tile = 0x8bcc, depth = 0x8bd6, limit = 0x468e}
local correction_pcs = {[0x9283] = true, [0x92a9] = true, [0x93f0] = true, [0x940f] = true}
local tile_id = 0x2e
local compared_tile
local checked, corrected = false, false

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start, left
if direction == "left" then
    coin = field(":SYSTEM", 0x40)
    start = field(":SYSTEM", 0x01)
    left = field(":P1", 0x02)
end

local function test_active()
    return frame == test.frame and cpu.state.X.value == 0x0510
end

revg_wall_tile = mem:install_read_tap(0x2800, 0x2fff, "wall tile", function(addr, value)
    if not test_active() then return end
    local pc = cpu.state.PC.value
    if pc ~= test.attr and pc ~= test.tile then return end
    checked = true
    local effective = value
    if local_x then
        effective = pc == test.attr and (value & 0x3f) or tile_id
    end
    if pc == test.tile then compared_tile = effective end
    print(string.format("tile,%d,%04x,%04x,%02x,%02x", frame, pc, addr, value, effective))
    if effective ~= value then return effective end
end)

revg_wall_depth = mem:install_read_tap(0x00e8, 0x00e8, "wall comparison", function(_, value)
    if not test_active() or cpu.state.PC.value ~= test.depth then return end
    local effective = local_x or value
    local limit = mem:read_u8(test.limit + compared_tile)
    print(string.format("compare,%d,%d,%d", frame, effective, limit))
    if effective ~= value then return effective end
end)

revg_wall_write = mem:install_write_tap(0x0507, 0x0507, "wall x correction", function(_, value)
    if test_active() and correction_pcs[cpu.state.PC.value] then
        corrected = true
        print(string.format("correct,%d,%d", frame, value))
    end
end)

print(string.format("trial,%s,%s", direction, local_x or "natural"))
revg_wall_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame == test.frame or frame == test.frame + 1 then
        local x = 256 * mem:read_u8(0x0506) + mem:read_u8(0x0507)
        print(string.format("sample,%d,%d,%d,%d", frame, x, mem:read_u8(0x0508), mem:read_u8(0x050a)))
    end
    if frame == test.frame + 1 then
        print("result," .. (checked and (corrected and "contact" or "clear") or "unchecked"))
        machine:exit()
        return
    end
    if direction == "left" then
        coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
        start:set_value(frame >= 500 and frame < 504 and 1 or 0)
        left:set_value(frame >= 1140 and 1 or 0)
    end
end)
