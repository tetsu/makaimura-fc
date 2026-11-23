-- MAME 0.289, makaimurg, default DIP switches, first credit after cold boot.
-- Plays the input schedule in REVG_PLAYER_INPUT from the first controllable update of
-- section 0 and prints the player record after every update, for
-- tools/compare-player-trace.py. Enemy spawns are held off by keeping the shared free
-- enemy slot count ($93) at 0, so only the player and the starting tombstone move.
--
-- REVG_PLAYER_INPUT is a list of "frames:bits" runs separated by commas, with the arcade
-- P1 bits: right $01, left $02, down $04, up $08, shot $10, jump $20. Two more bits hit the
-- player as the body check $F0A1 does, from an enemy on the right ($40) or the left
-- ($80): the hit lands before the next update, which runs state 3 ($995F). Bit $100 is
-- the time-out of $EC81 at the same point.
local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local mem = cpu.spaces.program
local frame = 0
-- The first update that reads controls with Arthur standing at the start (see
-- docs/revision-g-rom-map.md#1p-開始地点のプレイヤー更新).
local first = 1140

local schedule = {}
for frames, bits in string.gmatch(assert(os.getenv("REVG_PLAYER_INPUT"), "REVG_PLAYER_INPUT"),
        "(%d+):(%x+)") do
    for _ = 1, tonumber(frames) do schedule[#schedule + 1] = tonumber(bits, 16) end
end
local last = first + #schedule + 2

local function field(port, mask)
    for _, item in pairs(machine.ioport.ports[port].fields) do
        if item.mask == mask then return item end
    end
    error(string.format("Missing input field %s/$%02x", port, mask))
end

local coin = field(":SYSTEM", 0x40)
local start = field(":SYSTEM", 0x01)
local p1 = {}
for _, mask in ipairs({0x01, 0x02, 0x04, 0x08, 0x10, 0x20}) do p1[mask] = field(":P1", mask) end

local function word(addr) return mem:read_u8(addr) * 256 + mem:read_u8(addr + 1) end

-- The shot pose timer ($052A) and the four shot slots ($0530 + $40 * n): active flag,
-- state, X and height, as "active:state:x:y" with the X and height words. A hit fills
-- the slots with the four armor pieces ($99CE, type 5 in $0545 + $40 * n) for 20
-- updates; they are left out.
local function shots()
    local out = {}
    for slot = 0, 3 do
        local base = 0x0530 + 0x40 * slot
        local active = mem:read_u8(base)
        if mem:read_u8(base + 0x15) == 5 then active = 0 end
        out[#out + 1] = string.format("%d:%d:%d:%d", active, mem:read_u8(base + 3),
            word(base + 6), word(base + 9))
    end
    return table.concat(out, ";")
end

-- $F0A1: $AE = enemy X - player X, $0500 shifted left (no more hits), state 3, step 0,
-- and $0505 cleared.
local function hit(from_right)
    mem:write_u8(0x00ae, from_right and 0x00 or 0xff)
    mem:write_u8(0x00af, from_right and 0x01 or 0xff)
    mem:write_u8(0x0500, (mem:read_u8(0x0500) << 1) & 0xff)
    mem:write_u8(0x0503, 3)
    mem:write_u8(0x0504, 0)
    mem:write_u8(0x0505, 0)
end

-- $EC81, the time-out: $AC = 1, $0500 = 2, state 3 at sub-step 3 (the miss music of
-- $788C is left out).
local function time_up()
    mem:write_u8(0x00ac, 1)
    mem:write_u8(0x0500, 2)
    mem:write_u8(0x0503, 3)
    mem:write_u8(0x0504, 3)
end

print("type,step,input,x,x_fraction,y,y_fraction,mode,phase,direction,camera_x,facing,shot_timer,shots,crouch,ladder,blink,miss")
revg_frame = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame >= 1000 then
        mem:write_u8(0x0093, 0)
        -- The enemies placed with the section (the crows on the tombstones) are not held
        -- off by $93: free all 40 enemy slots ($0880 + $30 * n).
        for n = 0, 39 do mem:write_u8(0x0880 + 0x30 * n, 0) end
    end
    -- Clear the fractions left over from the attract demo so both traces start alike.
    if frame == first then
        mem:write_u8(0x0508, 0)
        mem:write_u8(0x050b, 0)
    end
    -- Rows read RAM after the update that used the input of the same step. A hit lands
    -- before the update of its own step, so the row before already shows state 3.
    local bits = frame >= first and schedule[frame - first + 1] or 0
    if (bits & 0xc0) ~= 0 then hit((bits & 0x40) ~= 0) end
    if (bits & 0x100) ~= 0 then time_up() end
    local step = frame - first - 1
    if step >= 0 and step <= #schedule then
        -- $051E is the facing (1 right, 2 left), $051F the crouch flag and $0526 the
        -- ladder flag. $AD is the blink timer after a hit, $90 the end of the death.
        print(string.format("arcade,%d,%02x,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%d,%d", step,
            schedule[step + 1] or 0, word(0x0506), mem:read_u8(0x0508), word(0x0509),
            mem:read_u8(0x050b), mem:read_u8(0x0503), mem:read_u8(0x0504), mem:read_u8(0x050e),
            word(0x009c), mem:read_u8(0x051e), mem:read_u8(0x052a), shots(),
            mem:read_u8(0x051f), mem:read_u8(0x0526), mem:read_u8(0x00ad), mem:read_u8(0x0090)))
    end
    if frame == last then
        machine:exit()
        return
    end
    coin:set_value(frame >= 400 and frame < 404 and 1 or 0)
    start:set_value(frame >= 500 and frame < 504 and 1 or 0)
    local input = frame >= first and schedule[frame - first + 1] or 0
    for mask, item in pairs(p1) do item:set_value((input & mask) ~= 0 and 1 or 0) end
end)
