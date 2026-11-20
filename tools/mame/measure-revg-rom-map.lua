-- MAME 0.289, makaimurg, default DIP switches, cold boot.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local selected_bank

local function unique_key()
    local seen = {}
    return function(key)
        if seen[key] then return false end
        seen[key] = true
        return true
    end
end

local first_switch = unique_key()
local first_vector = unique_key()
local first_target = unique_key()
local post_entry_seen = false
print("event,notice,pc,address_or_value,target_or_bank")

revg_map_switch = mem:install_write_tap(0x3e00, 0x3e00, "bank select", function(_, value)
    local pc = cpu.state.PC.value
    selected_bank = value == 4 and 4 or (value & 3)
    local key = pc * 256 + value
    if first_switch(key) then
        print(string.format("bank_select,%d,%04x,%02x,%d", frame, pc, value, selected_bank))
    end
end)

revg_map_vector = mem:install_read_tap(0xfff2, 0xffff, "6809 vectors", function(addr)
    local pc = cpu.state.PC.value
    -- The POST also reads these bytes when summing the fixed ROM.
    if pc == 0x5662 or addr % 2 ~= 0 or not first_vector(addr) then return end
    print(string.format("vector_read,%d,%04x,%04x,", frame, pc, addr))
end)

revg_map_dispatch = mem:install_read_tap(0x65bc, 0x65c3, "IRQ dispatch table", function(addr, high)
    local pc = cpu.state.PC.value
    if pc ~= 0x65f5 or addr % 2 ~= 0 or not first_target(addr) then return end
    local target = high * 256 + mem:read_u8(addr + 1)
    print(string.format("irq_dispatch,%d,%04x,%04x,%04x", frame, pc, addr, target))
end)

revg_map_post = mem:install_read_tap(0x553a, 0x553a, "POST entry", function()
    if selected_bank ~= 3 or post_entry_seen then return end
    post_entry_seen = true
    print(string.format("post_entry,%d,%04x,553a,%d", frame, cpu.state.PC.value, selected_bank))
end)

local coin, start
for _, field in pairs(machine.ioport.ports[":SYSTEM"].fields) do
    if field.mask == 0x40 then coin = field end
    if field.mask == 0x01 then start = field end
end
assert(coin and start, "Missing makaimurg coin and start inputs")

revg_map_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    if frame == 1200 then machine:exit() end
end)
