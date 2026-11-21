-- MAME 0.289, makaimurg, default DIP switches. Section 4 boss projectile allocation.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local trial = os.getenv("REVG_TYPE06_PROJECTILE") or "normal"
assert(trial == "normal" or trial == "pool", "Expected REVG_TYPE06_PROJECTILE=normal or pool")
local entry = 0x6cee + 4 * 4
local frame, attempts, shots, reloads = 0, 0, 0, 0
local selected, boss, activated, attempt_frame, effective_pool, failure

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
    print(string.format("type06_projectile,%s,%s,%d,", trial, event, frame) .. string.format(format, ...))
end

local coin, start = field(":SYSTEM", 0x40), field(":SYSTEM", 0x01)
revg_type06_projectile_taps = {
    page = mem:install_read_tap(entry, entry + 1, "section 4 start page", function(address)
        if selected and cpu.state.PC.value == 0x6d39 then return address == entry and 0x40 or 0xdc end
    end),
    pool = mem:install_read_tap(0xa7, 0xa7, "boss projectile allocation", function(_, value)
        if not activated or cpu.state.PC.value ~= 0xc101 or cpu.state.X.value ~= boss then return end
        attempts = attempts + 1
        attempt_frame = frame
        local blocked = trial == "pool" and attempts == 1
        effective_pool = blocked and 0 or value
        if value == 0 then failure = "Projectile pool was already exhausted" end
        log("attempt", "%d,%d,%d", attempts, value, effective_pool)
        if blocked then return 0 end
    end),
    enemy = mem:install_write_tap(0x0880, 0x0fff, "boss attack states", function(address, value)
        if not selected then return end
        local pc, slot = cpu.state.PC.value, cpu.state.X.value
        if pc == 0xbd70 and address == slot + 0x14 and mem:read_u8(slot + 5) == 0x06 then
            boss = slot
        end
        if slot ~= boss then return end
        if pc == 0xbddb and address == boss - 0x0d and value == 1 then
            activated = frame
            log("activate", "%04x", boss)
        elseif pc == 0xbe1d and address == boss + 0x1a then
            log("phase_timer", "%d", value)
        elseif pc == 0xbf15 and address == boss + 0x12 then
            log("shot_timer", "%d", value)
        elseif pc == 0xbece and address == boss + 0x12 then
            reloads = reloads + 1
            if frame ~= attempt_frame or reloads ~= attempts
                or (value ~= 65 and value ~= 60 and value ~= 50 and value ~= 45) then
                failure = "Unexpected projectile timer reload"
            end
            log("reload", "%d", value)
        end
    end),
    projectile = mem:install_write_tap(0x13d5, 0x1475, "boss projectile creation", function(address, value)
        if not activated or cpu.state.PC.value ~= 0xc120 or cpu.state.X.value ~= boss then return end
        local slot = cpu.state.Y.value
        local x, y = word(slot - 0x0a), word(slot - 7)
        if value ~= 0x0b or address ~= slot + 5 or frame ~= attempt_frame or effective_pool == 0
            or x ~= word(boss - 0x0a) or y ~= word(boss - 7) then
            failure = "Unexpected projectile creation"
        end
        shots = shots + 1
        log("shot", "%d,%04x,%d,%d,%d", shots, slot, x, y, mem:read_u8(slot - 2))
    end),
}

revg_type06_projectile_frame = emu.add_machine_frame_notifier(function()
    if failure or attempts >= 3 or frame >= 2300 then
        machine:exit()
        assert(not failure, failure)
        assert(attempts == 3 and reloads == attempts and shots == (trial == "pool" and 2 or 3),
            "Unexpected projectile or reload count")
        log("done", "%d,%d", attempts, shots)
        return
    end
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if not selected and mem:read_u8(0x05) == 1 and mem:read_u8(0x08) == 1
        and mem:read_u8(0x0b) == 0 and mem:read_u8(0x0e) == 0 then
        selected = true
        mem:write_u8(0x72, 4)
    end
end)
