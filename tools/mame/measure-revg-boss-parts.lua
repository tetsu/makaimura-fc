-- MAME 0.289, makaimurg, default DIP switches.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local section = tonumber(os.getenv("REVG_BOSS_SECTION"))
assert(section == 2 or section == 5, "Expected REVG_BOSS_SECTION=2 or 5")

local frame = 0
local selected, head, target, phase, old_tail, new_tail
local parts, removed = {}, {}
local hits, removal_count, remaining_writes = 0, 0, 0

local function log(event, format, ...)
    print(string.format("boss_parts,%d,%s,%d,", section, event, frame) .. string.format(format, ...))
end

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error("Missing input")
end

local function word(address)
    return 256 * mem:read_u8(address) + mem:read_u8(address + 1)
end

local function aiming()
    return target and hits < (phase == "body" and 1 or 6)
end

local coin, start, fire = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01), field(":P1", 0x10)
local page_address = 0x6cee + 4 * section
revg_boss_parts_taps = {
    enemy = mem:install_write_tap(0x0880, 0x0fff, "multipart boss state", function(address, value)
        if not selected then return end
        local pc = cpu.state.PC.value
        if pc == 0xd4df and not head then
            head = cpu.state.X.value
        elseif target and address == target + 0x10 and pc == 0xef2a then
            hits = hits + 1
            assert(value == (phase == "body" and 0 or 6 - hits), "Unexpected hit points after projectile hit")
            log("hit", "%s,%04x,%d", phase, target, value)
        elseif phase and pc == 0xda03 then
            local slot = cpu.state.X.value
            if address == slot - 0x10 and parts[slot] then
                local counted = mem:read_u8(slot + 0x0b)
                assert(value == 0 and counted == (slot == head and 1 or 0), "Unexpected removal flag")
                assert(not removed[slot], "Slot removed twice")
                removed[slot] = true
                removal_count = removal_count + 1
                log("remove", "%04x,%d,%d", slot, counted, mem:read_u8(0xb6))
            end
        end
    end),
    eligibility = mem:install_read_tap(0x0880, 0x0fff, "controlled projectile target",
        function(address)
            -- Include the offscreen target in the collision pass and exclude other slots.
            if aiming() and cpu.state.PC.value == 0xeeaa and address == cpu.state.X.value - 0x0f then
                return address == target - 0x0f and 1 or 0
            end
        end),
    projectile = mem:install_read_tap(0x0530, 0x05ff, "controlled projectile overlap",
        function(address)
            if not aiming() then return end
            local pc = cpu.state.PC.value
            local offset = pc == 0xeefc and -0x0a or pc == 0xef07 and -0x07
            if not offset then return end
            local projectile = cpu.state.Y.value
            local byte = address - projectile - offset
            if byte ~= 0 and byte ~= 1 then return end
            if cpu.state.U.value == target then return mem:read_u8(target + offset + byte) end
        end),
    remaining = mem:install_write_tap(0xb6, 0xb6, "multipart boss remaining count", function(_, value)
        if cpu.state.PC.value ~= 0xe78e or not phase then return end
        assert(section == 2 and phase == "head" and removed[head] and value == 0,
            "Unexpected remaining count update")
        remaining_writes = remaining_writes + 1
        log("remaining", "%d", value)
    end),
}
if section == 2 then
    revg_boss_parts_taps.page = mem:install_read_tap(page_address, page_address + 1, "encounter page",
        function(address)
            if selected and cpu.state.PC.value == 0x6d39 then
                return address == page_address and 0x40 or 0x4c
            end
        end)
end

revg_boss_parts_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    fire:set_value(aiming() and frame % 8 < 4 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, section)
    end
    if head and not phase and mem:read_u8(head + 0x1b) == 8 then
        local slot, previous = head, 0
        for index = 8, 0, -1 do
            assert(slot ~= 0 and mem:read_u8(slot + 0x1b) == index and word(slot + 0x1c) == previous,
                "Unexpected part linkage")
            parts[slot] = true
            if index == 4 then target = slot end
            if index == 1 then new_tail = slot end
            if index == 0 then old_tail = slot end
            previous, slot = slot, word(slot + 0x1e)
        end
        assert(slot == 0, "Expected the chain to end at the tail")
        phase = section == 2 and "body" or "head"
        if phase == "head" then target = head end
        log("target", "%s,%04x", phase, target)
    elseif phase == "body" and removed[old_tail] then
        assert(removal_count == 1 and hits == 1 and mem:read_u8(target + 0x10) == 1
            and mem:read_u8(new_tail + 0x1b) == 0 and word(new_tail + 0x1e) == 0
            and mem:read_u8(0xb6) == 1, "Expected only the old tail to be removed")
        log("new_tail", "%04x", new_tail)
        phase, target, hits = "head", head, 0
        log("target", "%s,%04x", phase, target)
    elseif removed[head] and (section == 2 or removal_count == 9) then
        machine:exit()
        assert(hits == 6, "Expected six hits on the head")
        local remaining, progress = mem:read_u8(0xb6), mem:read_u8(0x08)
        if section == 2 then
            assert(remaining_writes == 1 and remaining == 0 and progress == 2 and removal_count == 2,
                "Expected transition after head removal")
            for slot in pairs(parts) do
                if not removed[slot] then
                    assert(word(slot - 0x0d) == 0x0206 and mem:read_u8(slot + 0x0a) > 0,
                        "Expected the remaining linked parts to be in their death animation")
                end
            end
        else
            assert(remaining_writes == 0 and remaining == 2 and progress == 1,
                "Expected play to continue after all removals")
        end
        log("done", "%d,%d,%d", removal_count, remaining, progress)
        return
    end
    if frame >= 1800 then
        machine:exit()
        error("Multipart boss trial did not complete")
    end
end)
