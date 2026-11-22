-- Feed every sound command to makaimurg's Z80 so a debugger trace covers its code.
-- Run with -debug -debugger none; the trace goes to build/revg-sound-trace/z80.tr.
local machine = manager.machine
local mem = machine.devices[":audiocpu"].spaces["program"]
local first_frame -- set when the Z80 leaves its start-up delay and polls the latch
local frames_per_command = tonumber(os.getenv("REVG_SOUND_FRAMES") or "30")
local frame = 0
local command = 0xff

revg_sound_latch = mem:install_read_tap(0xc800, 0xc800, "sound command", function()
  first_frame = first_frame or frame + 1
  return command
end)

revg_sound_frame = emu.add_machine_frame_notifier(function()
  frame = frame + 1
  if frame == 1 then
    machine.debugger:command("trace build/revg-sound-trace/z80.tr,audiocpu")
  end
  if not first_frame then return end
  local index = (frame - first_frame) // frames_per_command
  if index < 64 then
    if command ~= index then print(string.format("sound_command frame=%d value=%02X", frame, index)) end
    command = index
  else
    machine.debugger:command("trace off,audiocpu")
    machine:exit()
  end
end)
