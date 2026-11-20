-- MAME 0.289, makaimurg, default DIP switches, first attract sequence.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
local scroll = {0, 0}
local columns = {}

-- Keep subscriptions alive for the duration of the run.
probe_scroll = mem:install_write_tap(0x3b08, 0x3b09, "scroll_x", function(addr, data)
    scroll[addr - 0x3b08 + 1] = data
end)

probe_tile = mem:install_write_tap(0x2800, 0x2fff, "background", function(addr)
    if frame >= 1190 and frame < 1330 then
        local tile = (addr - 0x2800) % 1024
        local col = tile // 32
        columns[col] = (columns[col] or 0) | (1 << (tile % 32))
    end
end)

probe_y = mem:install_write_tap(0x050a, 0x050a, "vertical_motion", function(_, data)
    if frame >= 1289 and frame <= 1292 then
        print(string.format("write_y,%d,%04x,%d", frame, cpu.state.PC.value, data))
        if cpu.state.PC.value == 0x9e84 then
            local dp = cpu.state.DP.value << 8
            print(string.format("contact,%d,%04x,%04x,%d,%d,%d,%d", frame,
                cpu.state.X.value, cpu.state.U.value, mem:read_u8(dp + 0xe4),
                mem:read_u8(dp + 0xe5), mem:read_u8(dp + 0xe6), mem:read_u8(dp + 0xe7)))
        end
    end
end)

probe_frame = emu.add_machine_frame_notifier(function()
    -- Writes occurred after the preceding sample.
    for col, rows in pairs(columns) do
        print(string.format("background,%d,%d,%08x", frame, col, rows))
    end
    columns = {}
    frame = frame + 1
    if frame >= 1160 and frame <= 1350 then
        local x, y, count = nil, nil, 0
        for addr = 0x1e00, 0x1ffc, 4 do
            local tile = mem:read_u8(addr)
            local attr = mem:read_u8(addr + 1)
            local sy = mem:read_u8(addr + 2)
            -- Empirical selector for four player pieces in this short window.
            if attr == 0 and tile <= 0x2b and sy > 16 and sy < 240 then
                local sx = mem:read_u8(addr + 3)
                x = math.min(x or sx, sx)
                y = math.min(y or sy, sy)
                count = count + 1
            end
        end
        print(string.format("motion,%d,%d,%d,%d,%d,%d", frame,
            scroll[1] + 256 * scroll[2], x or -1, y or -1, count, mem:read_u8(0x050a)))
    end
    if frame == 1350 then machine:exit() end
end)
