"""Compare the player's movement in MAME's makaimurg and in the FC build, update by update.

Each trial is one input schedule. MAME plays it from the first controllable update of
section 0 (tools/mame/record-revg-player.lua) and Mesen plays the same buttons in the
game build (./build.ps1 -Hardware) from the first update after the start (a generated Lua script). Both
traces are reduced to the change in X, in height and in camera X from the start, to
whether the player is in the air, the shot pose and shots, the facing and the crouch, and
must match on every update.

    python tools/compare-player-trace.py            # all trials
    python tools/compare-player-trace.py walk jump  # chosen trials
"""

import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
RIGHT, LEFT, DOWN, UP, SHOT, JUMP = 0x01, 0x02, 0x04, 0x08, 0x10, 0x20
# Not buttons: an enemy on the right or the left hits the player before the next update.
HIT_RIGHT, HIT_LEFT = 0x40, 0x80
# Also not a button: the clock runs out ($EC81) at the same point.
TIME_UP = 0x100

# Runs of (updates, arcade P1 bits). The start position is X 96 on flat ground and the
# first tombstone's left edge is X 256, so the other trials stay between them.
TRIALS = {
    "walk": [(10, 0), (40, RIGHT), (10, 0), (30, LEFT), (10, 0), (3, RIGHT), (2, LEFT),
             (20, RIGHT), (1, 0), (20, RIGHT), (10, 0)],
    "jump": [(10, 0), (1, JUMP), (50, 0), (40, JUMP), (20, 0), (1, JUMP), (35, 0),
             (1, JUMP), (45, 0)],
    "forward": [(10, 0), (20, RIGHT), (1, RIGHT | JUMP), (45, RIGHT), (10, 0), (20, LEFT),
                (1, LEFT | JUMP), (45, LEFT), (10, 0), (1, RIGHT | JUMP), (45, RIGHT),
                (10, 0), (10, RIGHT), (1, RIGHT | JUMP), (8, RIGHT), (40, LEFT), (10, 0)],
    # The camera follows at X - camera X = 128 both ways and stops at camera X 0. The
    # tombstone at X 48 keeps the player from the 10 px left edge, so that is not tried.
    "camera": [(10, 0), (120, RIGHT), (10, 0), (25, LEFT), (5, 0), (1, LEFT | JUMP),
               (45, LEFT), (10, 0), (1, RIGHT | JUMP), (45, RIGHT), (10, 0), (100, LEFT),
               (10, 0), (1, RIGHT | JUMP), (45, RIGHT), (10, 0), (20, RIGHT),
               (1, RIGHT | JUMP), (8, RIGHT), (40, LEFT), (10, 0)],
    # The lance ($8F71, $9DB7, $9B3A): one throw that runs out of range; presses while
    # the pose has more than 5 updates left (ignored) and 5 or less (thrown); all three
    # slots in use (the pose restarts, nothing is thrown); a throw that stops the walk;
    # turning out of the pose; throws to the left; throws while jumping and falling.
    "shot": [(10, 0), (1, SHOT), (55, 0), (1, SHOT), (6, 0), (1, SHOT), (3, 0), (1, SHOT),
             (9, 0), (1, SHOT), (9, 0), (1, SHOT), (9, 0), (1, SHOT), (60, 0),
             (20, RIGHT), (1, RIGHT | SHOT), (25, RIGHT), (1, RIGHT | SHOT), (4, RIGHT),
             (10, LEFT), (1, LEFT | SHOT), (20, LEFT), (10, 0), (1, JUMP), (5, 0), (1, SHOT),
             (40, 0), (1, RIGHT | JUMP), (20, RIGHT), (1, RIGHT | SHOT), (30, RIGHT),
             (60, 0)],
    # The crouch ($908B, $9017, $98AB): a one-update press (too short); crouching and
    # getting up with no direction (15 updates); getting up with a direction, which first
    # turns and then counts 10, later 7; crouching while walking; a crouch jump (straight
    # up); up to stand at once; pressing down again during the count; landing with down
    # held.
    "crouch": [(10, 0), (1, DOWN), (5, 0), (30, DOWN), (20, 0), (10, 0), (20, DOWN),
               (30, 0), (20, DOWN), (15, LEFT), (10, 0),
               (20, DOWN), (15, LEFT), (10, 0), (10, RIGHT), (20, RIGHT | DOWN),
               (10, RIGHT), (10, 0), (20, DOWN), (1, DOWN | JUMP), (40, 0), (20, DOWN),
               (1, UP), (10, 0), (20, DOWN), (5, 0), (3, DOWN), (20, 0), (10, 0), (1, JUMP),
               (30, DOWN), (20, 0)],
    # A crouch shot: the low lance, and the pose holding the crouch after down is let go.
    # The low lance would stop at a tombstone (not yet in the FC), so the player first
    # walks left to throw from further away, and the trial ends before the lance gets there.
    "crouch-shot": [(10, 0), (20, LEFT), (2, RIGHT), (10, 0), (20, DOWN), (1, DOWN | SHOT),
                    (3, DOWN), (30, 0)],
    # The first ladder (X 704-735, from the ground to the floor at height 142; $908B,
    # $9174, $91AD, $91CF, $90D2, $9101). Right-jumps clear the tombstones at X 256, 416
    # and 528. Then: up to get on and climb, hang, the shot button (no shot on a ladder),
    # down, up to the top and off (17 updates in place), down from the top (14 px lower,
    # 17 updates in place, then down), a stop, down to the ground and off, and up then
    # down to the ground again, which crouches because the ladder was taken with up.
    "ladder": [(10, 0), (145, RIGHT), (1, RIGHT | JUMP), (50, RIGHT), (100, RIGHT),
               (1, RIGHT | JUMP), (50, RIGHT), (70, RIGHT), (1, RIGHT | JUMP), (50, RIGHT),
               (125, RIGHT), (10, 0), (8, UP), (10, 0), (1, UP | SHOT), (5, UP), (1, SHOT),
               (5, 0), (6, DOWN), (80, UP), (20, 0), (40, DOWN), (5, 0), (70, DOWN), (20, 0),
               (4, UP), (12, DOWN), (30, 0)],
    # Walk into the first tombstone, jump onto it and walk off its far side.
    "tombstone": [(10, 0), (150, RIGHT), (10, 0), (1, RIGHT | JUMP), (60, RIGHT), (20, 0)],
    # The knockback and the death ($995F). A hit from the right while crouching throws the
    # player left toward the tombstone at X 48; walking during the blink; a hit from the
    # left without the armor is the death, up to the miss ($90).
    "hit": [(10, 0), (20, DOWN), (1, DOWN | HIT_RIGHT), (40, DOWN), (100, RIGHT), (20, 0),
            (1, HIT_LEFT), (100, 0)],
    # Hits in the air: during a forward jump, and the death during a straight jump.
    "hit-air": [(10, 0), (20, RIGHT), (1, RIGHT | JUMP), (10, RIGHT), (1, RIGHT | HIT_LEFT),
                (60, 0), (130, 0), (1, JUMP), (8, 0), (1, HIT_RIGHT), (100, 0)],
    # The time-out ($EC81) starts the death lying down: on the ground while crouching,
    # and in the air, where Arthur stays up to the miss.
    "time-up": [(10, 0), (20, DOWN), (1, DOWN | TIME_UP), (80, DOWN)],
    "time-up-air": [(10, 0), (20, RIGHT), (1, RIGHT | JUMP), (15, RIGHT), (1, TIME_UP),
                    (80, 0)],
    # Into the lake (X 1648-1791): the miss when the height is 32 or less ($F6BE). The
    # walk stops at each tombstone on the ground (X 256, 416, 528, 752, 960, 1104, 1264
    # and 1520) and jumps over it from there.
    "pit": [(10, 0), (160, RIGHT), (1, RIGHT | JUMP), (50, RIGHT), (150, RIGHT),
            (1, RIGHT | JUMP), (50, RIGHT), (110, RIGHT), (1, RIGHT | JUMP), (50, RIGHT),
            (210, RIGHT), (1, RIGHT | JUMP), (50, RIGHT), (190, RIGHT), (1, RIGHT | JUMP),
            (50, RIGHT), (140, RIGHT), (1, RIGHT | JUMP), (50, RIGHT), (150, RIGHT),
            (1, RIGHT | JUMP), (50, RIGHT), (240, RIGHT), (1, RIGHT | JUMP), (50, RIGHT),
            (57, RIGHT), (1, RIGHT | JUMP), (80, RIGHT), (10, 0)],
}

# Trials that show a known difference outside the player's movement. They run only when
# named on the command line.
KNOWN = {}

FC_BUTTONS = [(RIGHT, "right"), (LEFT, "left"), (DOWN, "down"), (UP, "up"),
              (SHOT, "b"), (JUMP, "a")]

MESEN_SCRIPT = """-- Generated by tools/compare-player-trace.py.
local inputs = {%(inputs)s}
local cpu = emu.memType.nesMemory
local function read(addr) return emu.read(addr, cpu) end
local frame, step, input = 0, nil, {}
emu.addEventCallback(function() emu.setInput(input, 0) end, emu.eventType.inputPolled)
emu.addEventCallback(function()
    frame = frame + 1
    input = {}
    -- Keep the zombies out, as MAME does with $93: hold the live count at the cap.
    emu.write(0x6077, 2, cpu)
    if frame > 4000 then print("timeout") emu.stop(2) return end
    if step == nil then
        -- Right+START skips the opening and the map.
        if frame == 128 then input = {start = true, right = true} end
        -- Stage 1 is running with the player standing and the raft placed.
        if frame > 128 and read(0x07) == 4 and read(0x8000) == 0x8c and read(0x6010) == 0
                and read(0x6042) ~= 0 then
            step = 0
        else
            return
        end
    else
        -- Shot slots: ps_active $6026, ps_x_lo $602C, ps_x_hi $602F, ps_y $6032,
        -- ps_y_hi $60B3 (src/arcade-player.inc). ap_shoot_lock is $6014.
        local shots = {}
        for slot = 0, 2 do
            shots[#shots + 1] = string.format("%%d:%%d:%%d", read(0x6026 + slot),
                read(0x602c + slot) + 256 * read(0x602f + slot),
                read(0x6032 + slot) + 256 * read(0x60b3 + slot))
        end
        -- player_left $50 (0 right, 1 left), ap_crouch $6013, ap_invuln $6012 and
        -- player_hit $4F (the miss).
        print(string.format("fc,%%d,%%d,%%d,%%d,%%d,%%d,%%s,%%d,%%d,%%d,%%d", step - 1,
            read(0x4c) + 256 * read(0x4d), read(0x4e) + 256 * read(0x60ac), read(0x6010),
            read(0x25) + 256 * read(0x26), read(0x6014), table.concat(shots, ";"),
            read(0x50), read(0x6013), read(0x6012), read(0x4f)))
    end
    if step >= #inputs + 2 then emu.stop(0) return end
    step = step + 1
    local bits = inputs[step] or 0
    -- A hit lands after the player update of the next frame (ap_test_hit $60FD), which
    -- the FC runs one step ahead of the MAME row of the same input.
    local hit = (inputs[step + 1] or 0) & %(hits)d
    if hit == %(time_up)d then
        emu.write(0x60fd, 3, cpu)
    elseif hit ~= 0 then
        emu.write(0x60fd, hit == %(hit_right)d and 2 or 1, cpu)
    end
%(buttons)s
end, emu.eventType.endFrame)
"""


def expand(runs):
    return [bits for count, bits in runs for _ in range(count)]


def runs_text(runs):
    return ",".join(f"{count}:{bits:x}" for count, bits in runs)


def record_arcade(args, name, runs):
    env = dict(os.environ, REVG_PLAYER_INPUT=runs_text(runs))
    command = [str(args.mame), "makaimurg", "-rompath", "reference", "-noreadconfig",
               "-video", "none", "-sound", "none", "-nothrottle", "-skip_gameinfo",
               "-autoboot_delay", "0", "-autoboot_script",
               "tools/mame/record-revg-player.lua",
               "-cfg_directory", "build/revg-player-cfg",
               "-nvram_directory", "build/revg-player-nvram", "-seconds_to_run", "60"]
    result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True)
    rows = []
    for line in result.stdout.splitlines():
        part = line.split(",")
        if part[0] != "arcade":
            continue
        step, _, x, _, y, _, mode, phase, _, camera = (int(v, 16) if i == 1 else int(v)
                                                       for i, v in enumerate(part[1:11]))
        timer = int(part[12])
        shots = []
        for slot, text in enumerate(part[13].split(";")):
            active, _, sx, sy = (int(v) for v in text.split(":"))
            if active:
                shots.append((slot, signed(sx), signed(sy)))
        # Mode 2 is the jump; phase 1 of mode 1 is the fall after walking off an edge.
        facing, crouch, ladder = int(part[11]) == 2, int(part[14]) != 0, int(part[15]) != 0
        # Mode 3 is the knockback and the death; $90 ends the death.
        rows.append((step, x, y, camera, mode == 2 or (mode == 1 and phase == 1), timer,
                     tuple(shots), facing, crouch, ladder, mode == 3, int(part[16]),
                     int(part[17]) != 0))
    if not rows:
        sys.exit(f"{name}: MAME printed no trace\n{result.stdout}\n{result.stderr}")
    return rows


def record_fc(args, name, runs):
    buttons = "\n".join(f"    input.{button} = bits & {mask} ~= 0"
                        for mask, button in FC_BUTTONS)
    inputs = ",".join(str(bits) for bits in expand(runs))
    script = ROOT / "build/player-trace" / f"{name}.lua"
    script.parent.mkdir(parents=True, exist_ok=True)
    script.write_text(MESEN_SCRIPT % {"inputs": inputs, "buttons": buttons,
                                      "hits": HIT_RIGHT | HIT_LEFT | TIME_UP,
                                      "hit_right": HIT_RIGHT, "time_up": TIME_UP},
                      encoding="utf-8")
    command = [str(args.mesen), "--doNotSaveSettings", "--testRunner", "--timeout=120",
               str(script), str(args.rom)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    rows = []
    for line in result.stdout.splitlines():
        part = line.strip().split(",")
        if part[0] != "fc":
            continue
        step, x, feet, state, camera, timer = (int(v) for v in part[1:7])
        shots = []
        for slot, text in enumerate(part[7].split(";")):
            active, sx, sy = (int(v) for v in text.split(":"))
            if active:
                # ps_y is down-positive like player_feet; the arcade height is up-positive.
                shots.append((slot, signed(sx), -signed(sy)))
        # ap_state 3 is the knockback and the death, 4 the miss after it (the arcade stays
        # in state 3 until the restart).
        rows.append((step, x, -feet, camera, state == 1, timer, tuple(shots),
                     int(part[8]) != 0, int(part[9]) != 0, state == 2, state >= 3,
                     int(part[10]), int(part[11]) != 0))
    if not rows:
        sys.exit(f"{name}: Mesen printed no trace (exit {result.returncode})\n{result.stdout}")
    return rows


def signed(value):
    return value - 65536 if value >= 32768 else value


def relative(rows):
    """Per update: dx, dy and dcam from the start, air, pose timer, each active shot slot
    with its X and height from the player's start, facing left, crouching, on a ladder,
    in state 3 (hit), the blink timer, and the miss."""
    _, x0, y0, c0, *_ = rows[0]
    # After the miss only the miss itself counts as the state: a fall into a pit misses
    # without state 3, and the FC turns to its own miss state either way.
    return [(x - x0, y - y0, c - c0, air and not miss, timer,
             tuple((slot, sx - x0, sy - y0) for slot, sx, sy in shots), left, crouch, ladder,
             hurt and not miss, blink, miss)
            for _, x, y, c, air, timer, shots, left, crouch, ladder, hurt, blink, miss in rows]


FC_PAN_MAX = 4  # px per frame (ap_follow_camera)


def toward(value, target, limit):
    return value + max(-limit, min(limit, target - value))


def compare(name, runs, arcade, fc):
    """Every field must match, except that the FC camera moves at most FC_PAN_MAX px per
    update: where the arcade camera jumps further (the ladder snaps X by up to 15 px),
    the FC camera must take the largest steps toward it."""
    inputs = expand(runs)
    a, f = relative(arcade), relative(fc)
    count = min(len(a), len(f), len(inputs))
    for step in range(count):
        previous = f[step - 1][2] if step else a[0][2]
        expected = a[step][:2] + (toward(previous, a[step][2], FC_PAN_MAX),) + a[step][3:]
        if expected != f[step]:
            print(f"FAIL {name}: first difference at update {step}")
            print("  update input  (dx, dy, dcam, air, pose, shots (slot, dx, dy), left, crouch, "
          "ladder, hit, blink, miss)")
            for i in range(max(0, step - 4), min(count, step + 6)):
                mark = ">" if i == step else " "
                print(f"{mark} {i:6d}  ${inputs[i]:02x}   arcade {a[i]}")
                print(f"                fc     {f[i]}")
            return False
    print(f"PASS {name}: {count} updates match")
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("trials", nargs="*", help=", ".join(TRIALS))
    parser.add_argument("--mame", type=Path,
                        default=Path.home() / "tools/mame/mame0289/mame.exe")
    parser.add_argument("--mesen", type=Path, default=ROOT / "build/mesen/Mesen.exe")
    parser.add_argument("--rom", type=Path,
                        default=ROOT / "build/vrc7a-test-512k.nes")
    args = parser.parse_args()
    for name in args.trials:
        if name not in TRIALS:
            parser.error(f"unknown trial {name}")
    ok = True
    for name in args.trials or TRIALS:
        if not args.trials and name in KNOWN:
            print(f"SKIP {name}: {KNOWN[name]}")
            continue
        runs = TRIALS[name]
        ok &= compare(name, runs, record_arcade(args, name, runs), record_fc(args, name, runs))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
