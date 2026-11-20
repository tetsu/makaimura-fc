-- MAME 0.289, makaimurg, default DIP switches, first attract sequence.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local first, test_frame, last = 1289, 1291, 1320
local platform = 0x1390
local mode = os.getenv("REVG_TERRAIN") or "natural"
local offset = math.tointeger(tonumber(os.getenv("REVG_OFFSET") or "0"))
assert(mode == "natural" or mode == "x" or mode == "y", "Unknown REVG_TERRAIN: " .. mode)
assert(offset and math.abs(offset) <= 64, "Expected an offset in -64..64")

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local comparisons = {
    [0x9e55] = {0x052b, "x"},
    [0x9e60] = {0x052d, "y"},
}
local probed, supported = false, false
revg_terrain_read = mem:install_read_tap(0x052b, 0x052d, "tombstone comparisons", function(addr, value)
    local pc = cpu.state.PC.value
    local entry = comparisons[pc]
    if frame < first or not entry or addr ~= entry[1] or cpu.state.U.value ~= platform then return end
    local axis = entry[2]
    local center = axis == "x" and word(0x00e1) or mem:read_u8(0x00e3)
    local effective = value
    if frame == test_frame and mode == axis then
        effective = (center + offset) & 0xff
        probed = true
    end
    local delta = ((effective - center + 128) & 0xff) - 128
    print(string.format("compare,%d,%s,%d,%d,%d,%d", frame, axis, value, center, effective, delta))
    if effective ~= value then return effective end
end)

revg_terrain_write = mem:install_write_tap(0x050a, 0x050b, "vertical position", function(addr, value)
    if frame < first then return end
    local pc = cpu.state.PC.value
    if addr == 0x050a and pc == 0x9e84 and cpu.state.U.value == platform and frame == test_frame then
        supported = true
    end
    print(string.format("write_y,%d,%04x,%04x,%04x,%d", frame, pc, cpu.state.U.value, addr, value))
end)

print(string.format("trial,%s,%d", mode, offset))
print("type,frame,player_x,player_y,airborne")
revg_terrain_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame >= first then
        print(string.format("sample,%d,%d,%d,%d", frame,
            word(0x0506), mem:read_u8(0x050a), mem:read_u8(0x0504)))
    end
    if mode ~= "natural" and frame == test_frame + 1 then
        print("result," .. (probed and (supported and "supported" or "not_supported") or "unchecked"))
        machine:exit()
    elseif frame == last then
        machine:exit()
    end
end)
