-- MAME 0.289, makaimurg, default DIP switches.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local trial = os.getenv("REVG_SPAWN") or "play"
assert(trial == "play" or trial == "attract" or trial == "upper", "Expected REVG_SPAWN=play, attract or upper")
local upper = trial == "upper"
local support = upper and 0x10d0 or 0x1390
local frame, bank = 0, nil
local placement, pattern, motion, phase = {}, {}, {}, {}
local support_seen, phase_seen, enemy_seen = false, false, false
local taps = {}

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local function read_word(record, offset)
    return 256 * assert(record[offset]) + assert(record[offset + 1])
end

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local support_reads = {[0x6e91] = 0, [0x6e97] = 2, [0x6e9b] = 4}
local motion_reads = {[0xe2bc] = 0, [0xe2c1] = 2}
local enemy_reads = {[0xec01] = 0, [0xec05] = 0, [0xec0f] = 4, [0xec11] = 5}

taps.bank = mem:install_write_tap(0x3e00, 0x3e00, "spawn bank", function(_, value)
    bank = value == 4 and 4 or (value & 3)
end)

taps.rom = mem:install_read_tap(0x4000, 0x5fff, "spawn data", function(addr, value)
    local pc = cpu.state.PC.value
    if bank == 2 and support_reads[pc] and cpu.state.Y.value == support then
        if upper and mem:read_u8(0x72) ~= 1 then return end
        local offset = support_reads[pc] + 2 - (cpu.state.X.value - addr)
        if offset == 0 then placement = {address = addr} end
        placement[offset] = value
    elseif bank == 2 and not upper and not enemy_seen then
        if pc == 0x8e0e or pc == 0x8e36 then
            pattern = {selector_address = addr, selector = value}
        elseif pc == 0xe8e0 then
            pattern.group_address = addr
        elseif pc == 0xe9ea and cpu.state.X.value == 0x07f0 then
            pattern.sequence_address = addr
        elseif enemy_reads[pc] and cpu.state.Y.value == 0x0e00 then
            local offset = enemy_reads[pc]
            if offset == 0 then
                offset = 2 - (cpu.state.U.value - addr)
                if offset == 0 then pattern.entry = addr end
            end
            pattern[offset] = value
        end
    elseif bank == 4 and upper and not phase_seen and (motion_reads[pc] or pc == 0xe3d2)
        and cpu.state.X.value == support and mem:read_u8(0x72) == 1 then
        local offset = 2 - (cpu.state.U.value - addr)
        if pc == 0xe3d2 then
            if offset == 0 then phase = {address = addr} end
            phase[offset] = value
        elseif motion_reads[pc] == 0 then
            local table_address = 0x5970 + 2 * mem:read_u8(support + 5)
            if addr == table_address then
                motion.table_address, motion.table_high = addr, value
            elseif addr == table_address + 1 then
                motion.address = 256 * motion.table_high + value + 2 * mem:read_u8(support - 0xe)
            end
        elseif motion.address then
            if addr == motion.address then motion.high = value end
            if addr == motion.address + 1 then motion.expected = 256 * motion.high + value end
        end
    end
end)

taps.support = mem:install_write_tap(support - 0x10, support + 8, "support placement", function(addr, value)
    local pc = cpu.state.PC.value
    if addr == support - 0x10 and pc == 0x6ea6 and (not upper or mem:read_u8(0x72) == 1) then
        local kind, variant = mem:read_u8(support + 5), mem:read_u8(support - 0xe)
        local x, y = word(support - 0xa), word(support - 7)
        assert(value == 1 and kind == placement[0] and variant == placement[1]
            and x == read_word(placement, 2) and y == read_word(placement, 4), "Support placement differs from ROM")
        print(string.format("support,%d,%04x,%04x,%02x,%02x,%d,%d",
            frame, placement.address, support, kind, variant, x, y))
        support_seen = true
    elseif upper and pc == 0xe2c3 and addr == support + 7 and motion.expected and not motion.pointer then
        motion.pointer = 256 * mem:read_u8(support + 6) + value
        assert(motion.pointer == motion.expected, "Platform motion pointer differs from ROM")
        print(string.format("motion,%d,%04x,%04x,%04x", frame, motion.table_address, motion.address, motion.pointer))
    elseif upper and pc == 0xe3df and addr == support + 8 and motion.pointer and not phase_seen then
        assert(phase.address == motion.pointer and mem:read_u8(support - 2) == phase[0]
            and value == (phase[1] & 0x7f), "First platform phase differs from ROM")
        print(string.format("phase,%d,%04x,%02x,%d", frame, phase.address, phase[0], value))
        phase_seen = true
    end
end)

if not upper then
    taps.enemy = mem:install_write_tap(0x0df0, 0x0e05, "enemy placement", function(addr, value)
        local pc = cpu.state.PC.value
        if enemy_seen or pc ~= 0xeafd or addr ~= 0x0e05 or cpu.state.X.value ~= 0x07f0 then return end
        local camera, offset = word(0x009c), read_word(pattern, 0)
        local x, y = word(0x0df6), word(0x0df9)
        local sign = mem:read_u8(0x07f0) < 0x80 and 1 or -1
        assert(x == camera + 0x80 + sign * offset and y == word(0x00a2)
            and mem:read_u8(0x0df0) == 1 and mem:read_u8(0x0df2) == pattern[4]
            and mem:read_u8(0x07ef) == pattern[5], "Enemy placement differs from spawn sequence")
        print(string.format("pattern,%d,%04x,%02x,%04x,%04x,%04x", frame,
            pattern.selector_address, pattern.selector, pattern.group_address, pattern.sequence_address, pattern.entry))
        print(string.format("enemy,%d,%04x,%02x,%d,%d,%d,%d,%d", frame, 0x0e00, value, camera, offset, x, y, pattern[5]))
        enemy_seen = true
    end)
end

revg_spawn_taps = taps
print("trial," .. trial)
local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
revg_spawn_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if trial == "play" then
        coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
        start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    end
    if (not upper and frame == 1150) or (upper and phase_seen) then
        assert(support_seen and (upper or enemy_seen), "Incomplete spawn trace")
        print("result,complete")
        machine:exit()
    end
end)

if upper then
    assert(os.getenv("REVG_STAGE1_END") == "section1-upper-projectile", "Expected REVG_STAGE1_END=section1-upper-projectile")
    assert(os.getenv("REVG_UPPER_INPUT") == "ladder-exit-third-bank-wait",
        "Expected REVG_UPPER_INPUT=ladder-exit-third-bank-wait")
    dofile("tools/mame/measure-revg-stage1-end.lua")
end
