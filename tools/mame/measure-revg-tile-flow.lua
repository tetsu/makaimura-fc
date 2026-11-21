-- MAME 0.289, makaimurg, default DIP switches, first credit after cold boot.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame, bank = 0, nil
local trial = os.getenv("REVG_TILE_FLOW") or "play"
assert(trial == "play" or trial == "attract", "Expected REVG_TILE_FLOW=play or attract")
local attract = trial == "attract"
local record, record_index
local seen, writers, taps = {}, {}, {}
local probe

local function log_once(key, format, ...)
    if seen[key] then return end
    seen[key] = true
    print(string.format(format, ...))
end

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local left, right = field(":P1", 0x02), field(":P1", 0x01)

taps.bank = mem:install_write_tap(0x3e00, 0x3e00, "tile flow bank", function(_, value)
    bank = value == 4 and 4 or (value & 3)
end)

local record_reads = {
    [0x635c] = 0, [0x635e] = 1, [0x6362] = 2, [0x6364] = 3,
    [0x6369] = 4, [0x636b] = 5, [0x6371] = 6, [0x6373] = 7,
}
local limits = {[0x8b3b] = 0x448e, [0x8b70] = 0x448e, [0x8ba2] = 0x458e, [0x8bd2] = 0x468e}
taps.rom = mem:install_read_tap(0x4000, 0x5fff, "tile flow ROM", function(addr, value)
    if frame < 490 then return end
    local pc = cpu.state.PC.value
    if pc == 0x631c and bank == 2 and addr % 2 == 0 then
        log_once("pointer", "pointer,%d,%d,%04x,%04x", frame, bank, addr, value * 256 + mem:read_u8(addr + 1))
    elseif pc == 0x6348 and bank == 0 then
        record_index, record = value, {}
        log_once("index", "index,%d,%d,%04x,%02x", frame, bank, addr, value)
    elseif record_reads[pc] and bank == 1 then
        record[record_reads[pc]] = value
        log_once(pc, "record,%d,%d,%04x,%02x,%02x", frame, bank, addr, record_index, value)
    elseif limits[pc] and probe and cpu.state.X.value == 0x0510 then
        local signed_tile = probe.tile < 0x80 and probe.tile or probe.tile - 0x100
        if addr ~= limits[pc] + signed_tile then
            error(string.format("Unexpected threshold address $%04x", addr))
        end
        log_once("limit_" .. probe.side, "limit,%d,%s,%04x,%02x,%d,%04x,%02x",
            frame, probe.side, probe.address, probe.tile, bank, addr, value)
    end
end)

local staged_record_offsets = {[0x80e7] = 4, [0x80eb] = 0, [0x81ad] = 6, [0x81b1] = 2}
taps.staging = mem:install_write_tap(0x0380, 0x03df, "tile staging", function(addr, value)
    if frame < 490 then return end
    local offset = staged_record_offsets[cpu.state.PC.value]
    if not offset then return end
    local source = cpu.state.X.value + offset + (addr % 2)
    if bank ~= 1 or mem:read_u8(source) ~= value then error("Unexpected bank 1 staging byte") end
    log_once("staging_" .. offset, "staging,%d,%d,%04x,%04x,%02x", frame, bank, source, addr, value)
end)

local initial_offsets = {[0x6360] = 1, [0x6367] = 3, [0x636f] = 5, [0x6377] = 7}
local staged_offsets = {[0x8519] = 0x30, [0x851d] = -1}
taps.writes = mem:install_write_tap(0x2800, 0x2fff, "tile flow writes", function(addr, value)
    if frame < 490 then return end
    local pc = cpu.state.PC.value
    writers[addr] = pc
    local offset = initial_offsets[pc]
    if offset then
        offset = offset - (addr % 2)
        if record[offset] ~= value then error("Unexpected byte from initial tile expansion") end
        log_once("initial_" .. offset, "initial,%d,%04x,%04x,%d,%02x", frame, pc, addr, offset, value)
    elseif staged_offsets[pc] then
        local source = cpu.state.U.value + staged_offsets[pc]
        if mem:read_u8(source) ~= value then error("Unexpected byte from tile staging buffer") end
        local phase = frame >= 1138 and "play" or "intro"
        log_once("stream_" .. phase .. pc, "stream,%d,%04x,%04x,%04x,%02x", frame, pc, source, addr, value)
    end
end)

local sides = {[0x8b2f] = "ground", [0x8b64] = "ground_other", [0x8b92] = "right", [0x8bc4] = "left"}
taps.attributes = mem:install_read_tap(0x2c00, 0x2fff, "tile flow collision", function(addr, attr)
    local side = sides[cpu.state.PC.value]
    if frame < 1138 or not side or cpu.state.X.value ~= 0x0510 then return end
    local x, y = word(0x00ec), word(0x00ee)
    local code_addr = addr - 0x400
    if side == "ground_other" then
        x = (x + word(0x8bdd + 4 * mem:read_u8(0x051f))) & 0xffff
    end
    local tile_offset = (((x >> 4) & 31) * 32) + (((y ~ 0xff) >> 4) & 31)
    if code_addr ~= 0x2800 + tile_offset then error("Unexpected tile buffer address") end
    probe = {side = side, address = code_addr, tile = mem:read_u8(code_addr)}
    local key = side .. ((attr & 0xc0) == 0 and "_lookup" or "_skip")
    log_once(key, "probe,%d,%s,%d,%d,%04x,%02x,%02x,%04x,%04x", frame, side, x, y,
        code_addr, probe.tile, attr, writers[code_addr] or 0, writers[addr] or 0)
end)

revg_tile_flow_taps = taps
print("trial," .. trial)
revg_tile_flow_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if not attract then
        coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
        start:set_value(frame >= 500 and frame < 504 and 1 or 0)
        left:set_value(frame >= 1140 and frame < 1160 and 1 or 0)
        right:set_value(frame >= 1165 and 1 or 0)
    end
    if frame == 1340 then
        assert(seen.initial_7 and seen.staging_0 and seen["stream_intro" .. 0x851d] and seen.limit_ground
            and seen[attract and "limit_right" or "limit_left"], "Incomplete tile flow trace")
        print("result,complete")
        machine:exit()
    end
end)
