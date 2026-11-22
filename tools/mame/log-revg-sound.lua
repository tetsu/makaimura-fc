-- Log every sound command the 6809 queues, with the code that requested it.
-- Set REVG_SOUND_SCENARIO to another measurement script to drive the game.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces["program"]
local frame = 0

-- $776D/$7775 store the command in the ring buffer at $0140-$017F with the
-- instruction at $7778. The power-on RAM test writes here from other code.
revg_sound_queue = mem:install_write_tap(0x0140, 0x017f, "sound queue", function(_, value)
  local pc = cpu.state["PC"].value
  if pc < 0x7778 or pc > 0x777a then return end
  local s = cpu.state["S"].value
  print(string.format("sound_request frame=%d value=%02X stack=%04X,%04X,%04X section=%02X",
    frame, value, mem:read_u16(s), mem:read_u16(s + 2), mem:read_u16(s + 4), mem:read_u8(0x72)))
end)

revg_sound_latch = mem:install_write_tap(0x3a00, 0x3a00, "sound latch", function(_, value)
  print(string.format("sound_latch frame=%d value=%02X", frame, value))
end)

revg_sound_log_frame = emu.add_machine_frame_notifier(function()
  frame = frame + 1
end)

local scenario = os.getenv("REVG_SOUND_SCENARIO")
if scenario then dofile(scenario) end
