-- MAME 0.289, makaimurg, default DIP switches.
-- Reuse the inputs and, for section1-end-normal-hp, the restart-page override.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local trial = os.getenv("REVG_STAGE1_END")
assert(trial == "controller" or trial == "section1-end-normal-hp",
    "Expected REVG_STAGE1_END=controller or section1-end-normal-hp")
local frame, restart, key_at = 0, nil, nil
local camera_ready, reached_x_limit = false, false
local taps = {}

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local function log(event, format, ...)
    print(string.format("flow,%s,%d,%d,", event, frame, mem:read_u8(0x72)) .. string.format(format, ...))
end

taps.restart = mem:install_read_tap(0x6cee, 0x6d25, "section restart record", function()
    if cpu.state.PC.value == 0x6d39 then restart = cpu.state.X.value end
end)

taps.spawn = mem:install_write_tap(0x050a, 0x050a, "section restart coordinates", function(_, value)
    if cpu.state.PC.value ~= 0x6d75 then return end
    local section, checkpoint = mem:read_u8(0x72), mem:read_u8(0x81)
    local index, page = word(0x91), word(0x6c)
    local x, y = word(0x0506), 256 * mem:read_u8(0x0509) + value
    local x_offset, y_offset = word(0xe2), word(0xe4)
    assert(restart == 0x6cee + 4 * section + 28 * checkpoint, "Unexpected restart table entry")
    assert(index == page - 0x4000 and x == (index & 31) * 256 + x_offset
        and y == (index >> 5) * 256 + y_offset, "Restart position differs from the selected record")
    log("restart", "%d,%04x,%04x,%d,%d,%04x", checkpoint, restart, page, x, y, word(0xaa))
end)

taps.camera_setup = mem:install_write_tap(0x005f, 0x005f, "section camera bounds", function(_, value)
    if cpu.state.PC.value ~= 0x6cdc then return end
    local entry = 0x6cac + 4 * mem:read_u8(0x72)
    local min_x, max_x, max_y = word(0x58), word(0x5a), word(0x5c)
    local min_y = 256 * mem:read_u8(0x5e) + value
    assert(min_x == 256 * mem:read_u8(entry) and max_x == 256 * mem:read_u8(entry + 1)
        and max_y == 256 * mem:read_u8(entry + 2) and min_y == 256 * mem:read_u8(entry + 3), "Unexpected camera bounds")
    log("camera_setup", "%04x,%d,%d,%d,%d", entry, min_x, max_x, max_y, min_y)
    camera_ready = true
end)

taps.camera = mem:install_write_tap(0x009d, 0x009d, "section horizontal camera limit", function(_, value)
    if not camera_ready or reached_x_limit or cpu.state.PC.value ~= 0x7fc4 then return end
    local camera_x = 256 * mem:read_u8(0x9c) + value
    local limit = word(0x5a)
    if camera_x >= limit then
        log("camera_limit", "%d,%d", camera_x, limit)
        reached_x_limit = true
    end
end)

taps.checkpoint = mem:install_write_tap(0x0081, 0x0081, "section checkpoint", function(_, value)
    if cpu.state.PC.value ~= 0xecdd then return end
    local section, camera_x = mem:read_u8(0x72), word(0x9c)
    local threshold = word(0xece4 + 2 * section)
    assert(value == 1 and camera_x > threshold, "Checkpoint set before camera passed threshold")
    log("checkpoint", "%d,%d,%04x", camera_x, threshold, word(0xaa))
end)

taps.timeout = mem:install_write_tap(0x00ac, 0x00ac, "section timeout", function(_, value)
    if cpu.state.PC.value == 0xec88 and value == 1 then
        assert(word(0xaa) == 0, "Timeout signalled with time remaining")
        log("timeout", "%d,%d", word(0x0506), word(0x0509))
    end
end)

taps.boss = mem:install_write_tap(0x00b6, 0x00b6, "section bosses remaining", function(_, value)
    if cpu.state.PC.value == 0xe78e then log("boss_remaining", "%d", value) end
end)

taps.key = mem:install_write_tap(0x0553, 0x0555, "section key", function(addr, value)
    local pc = cpu.state.PC.value
    if addr == 0x0553 and pc == 0x72ef and value == 1 then
        local player_x, x_limit = word(0x0506), word(0x5a)
        assert(player_x >= x_limit - 48 and word(0x0521) == x_limit + 37, "Unexpected key request")
        log("key_request", "%d,%d", player_x, word(0x0521))
    elseif addr == 0x0555 and pc == 0x733e then
        local dx, dy = word(0x0506) - word(0x0521), word(0x0509) - word(0x0550)
        assert(math.abs(dx) <= 8 and math.abs(dy) <= 8, "Key collected outside its bounds")
        log("key_collected", "%d,%d", dx, dy)
        key_at = frame
    end
end)

taps.section = mem:install_write_tap(0x0072, 0x0072, "section transition", function(_, value)
    if cpu.state.PC.value ~= 0x73a3 then return end
    local previous = mem:read_u8(0x72)
    assert(key_at and value == (previous + 1) % 7, "Unexpected section transition")
    log("advance", "%d,%d", value, frame - key_at)
    camera_ready, reached_x_limit, key_at = false, false, nil
end)

revg_section_flow_taps = taps
revg_section_flow_frame = emu.add_machine_frame_notifier(function() frame = frame + 1 end)
dofile("tools/mame/measure-revg-stage1-end.lua")
