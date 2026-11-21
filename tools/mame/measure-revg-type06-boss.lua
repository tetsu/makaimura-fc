-- MAME 0.289, makaimurg, default DIP switches. Controlled 1P projectile hits.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = tonumber(os.getenv("REVG_TYPE06_SECTION"))
assert(section == 4 or section == 5, "Expected REVG_TYPE06_SECTION=4 or 5")

local boss_count = section == 4 and 1 or 2
local page = section == 4 and 0x40dc or 0x415e
local entry = 0x6cee + 4 * section
local bosses = {}
local frame, removals, delay_ticks = 0, 0, 0
local selected, target, last_delay_tick, completed, failure

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input %s/$%02x", port, mask))
end

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local function log(event, format, ...)
    print(string.format("type06_boss,%d,%s,%d,", section, event, frame) .. string.format(format, ...))
end

local function aiming()
    return target and target.active and target.hits < 4 and frame >= 1064
end

local coin, start, fire = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01), field(":P1", 0x10)
revg_type06_boss_taps = {
    page = mem:install_read_tap(entry, entry + 2, "late section start position", function(address)
        if not selected then return end
        local pc = cpu.state.PC.value
        if pc == 0x6d39 then return address == entry and page >> 8 or page & 0xff end
        if section == 5 and pc == 0x6d42 and address == entry + 2 then return 0xc0 end
    end),
    enemy = mem:install_write_tap(0x0880, 0x0fff, "type 06 boss states", function(address, value)
        if not selected then return end
        local pc, slot = cpu.state.PC.value, cpu.state.X.value
        if pc == 0xef2a and aiming() and address == target.slot + 0x14 then
            target.hits = target.hits + 1
            if value ~= 4 - target.hits then failure = "Unexpected HP after hit" end
            log("hit", "%04x,%d", target.slot, value)
            if value == 0 then target.last_hit = frame end
            return
        end

        if pc == 0xbd70 and address == slot + 0x14 then
            local variant = mem:read_u8(slot - 0x0e)
            if value ~= 4 or mem:read_u8(slot + 5) ~= 0x06 or variant >= boss_count or bosses[variant] then
                failure = "Unexpected boss initialization"
                return
            end
            local boss = { slot = slot, hits = 0 }
            bosses[variant] = boss
            if variant == 0 then target = boss end
            log("init", "%04x,%d,%d", slot, variant, value)
            return
        end
        local delayed = bosses[1]
        if delayed and slot == delayed.slot and pc == 0xbdcf and address == slot + 0x11 then
            delay_ticks = delay_ticks + 1
            if value ~= 6 - delay_ticks or (last_delay_tick and frame - last_delay_tick ~= 64) then
                failure = "Unexpected activation countdown"
            end
            last_delay_tick = frame
            log("delay", "%04x,%d", slot, value)
        end
        local boss = bosses[0] and bosses[0].slot == slot and bosses[0]
            or delayed and delayed.slot == slot and delayed
        if not boss then return end
        if pc == 0xbdd6 and address == slot + 0x14 then
            if value ~= 4 or boss.active or (boss == delayed and delay_ticks ~= 6) then
                failure = "Unexpected attack phase entry"
            end
            boss.active = true
            log("activate", "%04x", slot)
        elseif pc == 0xc222 and address == slot + 0x0a then
            if boss.death_timer or not boss.last_hit or frame ~= boss.last_hit + 1 or value ~= 63 then
                failure = "Unexpected death timer"
            end
            boss.death_timer = true
            log("death_timer", "%04x,%d", slot, value)
        elseif pc == 0xc231 and address == slot + 4 and mem:read_u8(slot + 0x0a) == 63 then
            local pose = 256 * mem:read_u8(slot + 3) + value
            if boss.pose or not boss.death_timer or frame ~= boss.last_hit + 24 or pose ~= 0xe880 then
                failure = "Unexpected death pose"
            end
            boss.pose = true
            log("pose", "%04x,%04x", slot, pose)
        elseif pc == 0xc248 and address == slot - 0x10 then
            if boss ~= target or not boss.pose or frame ~= boss.last_hit + 87 or value ~= 0 then
                failure = "Unexpected slot removal"
            end
            boss.removed = frame
            log("remove", "%04x,%d", slot, mem:read_u8(0xb6))
        end
    end),
    eligibility = mem:install_read_tap(0x0880, 0x0fff, "controlled projectile target", function(address)
        if cpu.state.PC.value == 0xeeaa and address == cpu.state.X.value - 0x0f and aiming() then
            return address == target.slot - 0x0f and 1 or 0
        end
    end),
    projectile = mem:install_read_tap(0x0530, 0x05ff, "controlled projectile overlap", function(address)
        if not aiming() or cpu.state.U.value ~= target.slot then return end
        local pc = cpu.state.PC.value
        local offset = pc == 0xeefc and -0x0a or pc == 0xef07 and -0x07
        if not offset then return end
        local byte = address - cpu.state.Y.value - offset
        if byte == 0 or byte == 1 then return mem:read_u8(target.slot + offset + byte) end
    end),
    remaining = mem:install_write_tap(0xb6, 0xb6, "type 06 boss remaining count", function(_, value)
        if not selected or cpu.state.PC.value ~= 0xe78e then return end
        removals = removals + 1
        if not target or target.removed ~= frame or value ~= boss_count - removals
            or mem:read_u8(0xb6) ~= value + 1 or mem:read_u8(0x08) ~= 1 then
            failure = "Unexpected boss count update"
        end
        log("remaining", "%d", value)
    end),
    progress = mem:install_write_tap(0x08, 0x08, "type 06 section completion", function(_, value)
        if not selected or cpu.state.PC.value ~= 0xe79d then return end
        if removals ~= boss_count or mem:read_u8(0xb6) ~= 0 or value ~= 2 then
            failure = "Expected completion only after the final removal"
        end
        completed = true
        log("progress", "%d", value)
    end),
}

revg_type06_boss_frame = emu.add_machine_frame_notifier(function()
    if failure or completed or frame >= 1700 then
        machine:exit()
        assert(not failure, failure)
        assert(completed and removals == boss_count, "Type 06 boss trial did not complete")
        log("done", "%d", removals)
        return
    end
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
    end
    if target and target.removed then target = removals < boss_count and bosses[1] or nil end
    fire:set_value(aiming() and frame % 8 < 4 and 1 or 0)
end)
