-- MAME 0.289, makaimurg. Change only the Difficulty DIP.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local difficulty = os.getenv("REVG_RANK_DIFFICULTY") or "Normal"
local floors = { Easy = 0, Normal = 6, Difficult = 24, ["Very Difficult"] = 32 }
local floor = assert(floors[difficulty], "Expected Easy, Normal, Difficult or Very Difficult")
local trial = os.getenv("REVG_RANK_TRIAL")
local starts = {
    clear = floor,
    cross = (floor & 0x30) + 12,
    cap = 62,
    ["death-floor"] = floor,
    ["death-raised"] = floor + 1,
    loop = floor + 24,
    restart = floor + 24,
}
local initial = assert(starts[trial], "Expected clear, cross, cap, death-floor, death-raised, loop or restart")
local death = trial == "death-floor" or trial == "death-raised"
local final_section = trial == "loop" or trial == "restart"
local next_rank = death and floor or final_section and initial or math.min(initial + 4, 63)
local next_section = (death or final_section) and 0 or 1

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input %s/$%02x", port, mask))
end

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
local dip = field(":DSW2", 0x60)
for value, label in pairs(dip.settings) do
    if label == difficulty then dip.user_value = value end
end
assert(dip.settings[dip.user_value] == difficulty, "Difficulty DIP did not change")

local frame, rank_writes = 0, 0
local selected, active, loaded, failure
local configuration_source, copied_bytes = nil, 0

local function log(event, format, ...)
    print(string.format("rank,%s,%s,%s,%d,", difficulty, trial, event, frame) .. string.format(format, ...))
end

revg_rank_taps = {
    fall = mem:install_read_tap(0xa2, 0xa3, "Y on the offscreen boundary", function(address)
        if not death or not active or rank_writes ~= 0 or cpu.state.PC.value ~= 0xf6c0 then return end
        local y = word(0x9e) + 32
        return address == 0xa2 and y >> 8 or y & 0xff
    end),
    adjustment = mem:install_write_tap(0x61, 0x61, "rank after clear or death", function(_, value)
        local pc = cpu.state.PC.value
        if not active or (pc ~= 0x6c66 and pc ~= 0x711e) then return end
        rank_writes = rank_writes + 1
        log("adjust", "%04x,%d,%d", pc, mem:read_u8(0x61), value)
        local expected_pc = death and 0x711e or 0x6c66
        if final_section or pc ~= expected_pc or value ~= next_rank
            or rank_writes ~= 1 or mem:read_u8(0x61) ~= initial then
            failure = "Unexpected rank adjustment"
        end
    end),
    configuration = mem:install_read_tap(0x9a, 0x9a, "rank used to select enemy settings", function(_, value)
        if cpu.state.PC.value ~= 0x6f0e or not selected then return end
        local section, round = mem:read_u8(0x72), mem:read_u8(0x7f)
        local expected_rank = active and next_rank or floor
        local expected_section = active and next_section or (final_section and 6 or 0)
        local expected_round = active and trial == "loop" and 1 or 0
        local table_base = cpu.state.U.value
        local source = table_base + (value & 0x30)
        log("load", "%d,%d,%d,%d,%04x,%04x", section, round, mem:read_u8(0x61), value, table_base, source)
        if value ~= expected_rank or mem:read_u8(0x61) ~= expected_rank or section ~= expected_section
            or round ~= expected_round or table_base ~= word(0x4240 + 2 * section)
            or (active and copied_bytes ~= 16) then
            failure = "Unexpected rank or configuration table at section load"
        end
        configuration_source, copied_bytes = source, 0
    end),
    copy = mem:install_write_tap(0x30, 0x3f, "enemy settings copied to RAM", function(address, value)
        if not configuration_source or cpu.state.PC.value ~= 0x6f1d then return end
        if address ~= 0x30 + copied_bytes or value ~= mem:read_u8(configuration_source + copied_bytes) then
            failure = "Unexpected enemy configuration byte"
        end
        copied_bytes = copied_bytes + 1
        if copied_bytes == 16 and active then loaded = true end
    end),
}

revg_rank_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    local playing = mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
    local phase = mem:read_u8(0x0b)
    if not selected and playing and phase == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        assert(mem:read_u8(0x57) == floor and mem:read_u8(0x61) == floor, "Unexpected initial rank")
        log("dip", "%02x,%d", dip.user_value, floor)
        if final_section then mem:write_u8(0x72, 6) end
    elseif selected and not active and playing and phase == 1 and mem:read_u8(0x0503) == 1 then
        active = true
        if initial ~= floor then mem:write_u8(0x61, initial) end
        if final_section then
            -- Compare the first-clear sequence against the direct return to section 0.
            mem:write_u8(0x71, trial == "loop" and 0 or 1)
            mem:write_u8(0x08, 2)
            mem:write_u8(0x0b, 0)
            mem:write_u8(0x0e, 0)
        elseif not death then
            -- Enter the last frame of the post-key delay at $738B.
            mem:write_u8(0x08, 2)
            mem:write_u8(0x0b, 6)
            mem:write_u8(0x0520, 1)
        end
        log("start", "%d,%d", mem:read_u8(0x72), initial)
    end
    if failure then
        machine:exit()
        error(failure)
    elseif loaded then
        machine:exit()
        assert(rank_writes == (final_section and 0 or 1), "Missing rank adjustment")
        log("done", "%d", rank_writes)
    elseif frame >= 2200 then
        machine:exit()
        error("Rank trial did not complete")
    end
end)
