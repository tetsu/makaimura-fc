-- MAME 0.289, makaimurg, default DIP switches.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = tonumber(os.getenv("REVG_BOSS_SECTION"))
local sections = {
    [0] = { page = 0x0c, remaining = 1, enemies = { type08 = 1 } },
    [1] = { page = 0x1a, remaining = 2, enemies = { type08 = 2 } },
    [2] = { page = 0x4c, remaining = 1, enemies = { head = 1, body = 7, tail = 1 } },
    [3] = { page = 0x58, remaining = 1, enemies = { head = 1, body = 7, tail = 1 } },
    [4] = { remaining = 1, enemies = { type06 = 1 } },
    [5] = { remaining = 2, enemies = { type06 = 2, type08 = 1, head = 1, body = 7, tail = 1 } },
    [6] = { remaining = 1, enemies = { type0d = 1 } },
}
local trial = assert(sections[section], "Expected REVG_BOSS_SECTION=0 through 6")
local constructors = {
    [0xbd70] = { name = "type06", kind = 0x06, field = 0x14, hp = 4 },
    [0xc40b] = { name = "type08", kind = 0x08, field = 0x16, hp = 10 },
    [0xd4df] = { name = "head", kind = 0x0c, field = 0x10, hp = 6 },
    [0xd5c2] = { name = "body", kind = 0x0c, field = 0x10, hp = 1 },
    [0xd607] = { name = "tail", kind = 0x0c, field = 0x10, hp = 1 },
    [0xda68] = { name = "type0d", kind = 0x0d, field = 0x16, hp = 10 },
}
local frame, selected, count_loaded = 0, false, false
local seen, totals = {}, {}

local function log(event, format, ...)
    print(string.format("boss_roster,%d,%s,%d,", section, event, frame) .. string.format(format, ...))
end

local function field(mask)
    for _, item in pairs(machine.ioport.ports[":SYSTEM"].fields) do
        if item.mask == mask then return item end
    end
    error("Missing SYSTEM input")
end

local coin, start = field(0x40), field(0x01)
local page_address = 0x6cee + 4 * section
revg_boss_roster_taps = {
    remaining = mem:install_write_tap(0xb6, 0xb6, "initial boss count", function(_, value)
        if not selected or cpu.state.PC.value ~= 0x6ee1 then return end
        assert(value == trial.remaining, "Unexpected initial boss count")
        if not count_loaded then log("count", "%d", value) end
        count_loaded = true
    end),
    hp = mem:install_write_tap(0x0880, 0x0fff, "initial enemy hp", function(address, value)
        if not count_loaded then return end
        local pc = cpu.state.PC.value
        local constructor = constructors[pc]
        if not constructor then return end
        local slot = cpu.state.X.value
        if address ~= slot + constructor.field or seen[slot] then return end
        assert(value == constructor.hp and mem:read_u8(slot + 5) == constructor.kind, "Unexpected initial HP")
        seen[slot] = true
        local name = constructor.name
        totals[name] = (totals[name] or 0) + 1
        log("hp", "%04x,%04x,%02x,%02x,%02x,%d", pc, slot, constructor.kind,
            mem:read_u8(slot - 0x0e), constructor.field, value)
    end),
}
if trial.page then
    -- Select the encounter through the normal start-table read, then let the scheduler spawn it.
    revg_boss_roster_taps.page = mem:install_read_tap(page_address, page_address + 1, "encounter page",
        function(address)
            if not selected or cpu.state.PC.value ~= 0x6d39 then return end
            if address == page_address then return 0x40 + (trial.page >> 8) end
            return trial.page & 0xff
        end)
end

revg_boss_roster_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
    end
    if frame == 1100 then
        machine:exit()
        assert(count_loaded and mem:read_u8(0xb6) == trial.remaining, "Missing or changed boss count")
        for _, constructor in pairs(constructors) do
            local name = constructor.name
            assert((totals[name] or 0) == (trial.enemies[name] or 0), "Unexpected slot count for " .. name)
        end
        log("done", "%d", trial.remaining)
    end
end)
