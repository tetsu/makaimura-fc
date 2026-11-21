-- MAME 0.289, makaimurg, default DIP switches, first attract sequence.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local first, test_frame = 1313, 1317
local frame = 0
local player = 0x0510
local mode = os.getenv("REVG_GROUND") or "natural"
local edge_tile = ({edge_left = 0x2a0d, edge_right = 0x2a2d})[mode]
local probe_x = math.tointeger(tonumber(os.getenv("REVG_GROUND_X") or "269"))
assert(mode == "natural" or mode == "above" or mode == "right" or mode == "neither" or edge_tile,
    "Unknown REVG_GROUND: " .. mode)
assert(not edge_tile or (probe_x and probe_x >= 256 and probe_x <= 287),
    "Expected REVG_GROUND_X in 256..287 for an edge trial")

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local function is_player_check()
    return frame >= first and cpu.state.X.value == player
end

-- At notification 1317 the demo falls onto the flat background tile.
-- Raise the integer Y operand by one pixel only for the ground test.
revg_ground_y = mem:install_read_tap(0x050a, 0x050a, "ground input y", function(_, value)
    if not is_player_check() or cpu.state.PC.value ~= 0x8b1d then return end
    local effective = frame == test_frame and mode == "above" and value + 1 or value
    print(string.format("ground_y,%d,%d,%d", frame, value, effective))
    if effective ~= value then return effective end
end)

if edge_tile then
    revg_ground_x = mem:install_read_tap(0x0507, 0x0507, "ground input x", function(_, value)
        if frame ~= test_frame or not is_player_check() or cpu.state.PC.value ~= 0x8b17 then return end
        print(string.format("ground_x,%d,%d,%d", frame, 256 * mem:read_u8(0x0506) + value, probe_x))
        return probe_x & 0xff
    end)
end

local attribute_side = {[0x8b2f] = "left", [0x8b64] = "right"}
local tiles = {}
local tested = false
revg_ground_attributes = mem:install_read_tap(0x2c00, 0x2fff, "ground tile attributes", function(addr, value)
    local side = attribute_side[cpu.state.PC.value]
    if not is_player_check() or not side then return end
    local reject = frame == test_frame and
        (mode == "neither" or (mode == "right" and side == "left") or (edge_tile and addr - 0x400 ~= edge_tile))
    local effective = reject and (value | 0xc0) or value
    if frame == test_frame then tested = true end
    local tile = mem:read_u8(addr - 0x400)
    tiles[side] = tile
    print(string.format("tile,%d,%s,%04x,%02x,%02x,%02x", frame, side, addr - 0x400, tile, value, effective))
    if effective ~= value then return effective end
end)

local compare_side = {[0x8b41] = "left", [0x8b76] = "right"}
revg_ground_depth = mem:install_read_tap(0x00e9, 0x00e9, "ground depth", function(_, value)
    local side = compare_side[cpu.state.PC.value]
    if not is_player_check() or not side then return end
    local tile = tiles[side]
    local threshold = mem:read_u8(0x448e + (tile < 0x80 and tile or tile - 0x100))
    print(string.format("depth,%d,%s,%d,%d", frame, side, value, threshold))
end)

local landed = false
revg_ground_landing = mem:install_write_tap(0x050a, 0x050a, "ground landing", function(_, value)
    if not is_player_check() or cpu.state.PC.value ~= 0x9054 then return end
    if frame == test_frame then landed = true end
    print(string.format("ground_write,%d,%d", frame, value))
end)

print("trial," .. mode .. (edge_tile and "," .. probe_x or ""))
print("type,frame,player_x,player_y,airborne")
revg_ground_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame >= first then
        print(string.format("sample,%d,%d,%d,%d", frame,
            word(0x0506), mem:read_u8(0x050a), mem:read_u8(0x0504)))
    end
    if frame == test_frame + 1 then
        print("result," .. (tested and (landed and "landed" or "airborne") or "unchecked"))
        machine:exit()
    end
end)
