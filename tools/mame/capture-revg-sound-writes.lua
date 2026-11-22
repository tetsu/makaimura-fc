-- Log every YM2203 register write makaimurg's Z80 makes for one sound command.
-- The latch returns REVG_SOUND_COMMAND (hex) from the first read after reset, so
-- the driver handles it on its first tick with a cleared RAM.
-- Writes REVG_SOUND_OUTPUT with lines
--   T                  the IRQ handler read the sound latch (one driver tick)
--   W <chip> <reg> <value>
-- and exits after REVG_SOUND_FRAMES frames (default 600) from that first read.
local machine = manager.machine
local mem = machine.devices[":audiocpu"].spaces["program"]
local command = tonumber(assert(os.getenv("REVG_SOUND_COMMAND")), 16)
local frames = tonumber(os.getenv("REVG_SOUND_FRAMES") or "600")
local out = assert(io.open(assert(os.getenv("REVG_SOUND_OUTPUT")), "wb"))
local frame, started = 0, nil
local selected = {0, 0}

revg_sound_latch = mem:install_read_tap(0xc800, 0xc800, "sound command", function()
  started = started or frame
  out:write("T\n")
  return command
end)

revg_sound_ym = mem:install_write_tap(0xe000, 0xe003, "ym writes", function(offset, data)
  local chip = (offset - 0xe000) // 2
  if offset % 2 == 0 then
    selected[chip + 1] = data
  elseif started then
    out:write(string.format("W %d %02X %02X\n", chip, selected[chip + 1], data))
  end
end)

revg_sound_frame = emu.add_machine_frame_notifier(function()
  frame = frame + 1
  if started and frame - started >= frames then
    out:close()
    machine:exit()
  end
end)
