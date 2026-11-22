"""Decode the Revision G animation frames that tools/mame/log-revg-sprites.lua recorded.

The 6809 builds the sprite list in $F6F2. Each object has a shape (-$01) and a frame pointer
(+$03). A frame is one code byte per 16x16 sprite, with one attribute byte after the first code,
and then a duration byte; a duration with bit 7 set is followed by the address of the next frame.
Frames below $6000 are read from bank 4, which $F6E4 selects before the list is built.

  check LOG...   rebuild the logged sprite lists from ROM and compare them with sprite RAM, and
                 compare the sprite layer drawn from sprite RAM with the MAME snapshots
  dump LOG...    follow every logged frame pointer to its animation, and write sprites.json and
                 one PNG sheet per pool, type and shape
  load LOG...    count the objects and sprites of every list a REVG_SPRITES_LOAD=1 log recorded, and
                 the FC 8x16 sprites on the busiest line under each thinning plan (TODO C-2)
"""

import argparse
import glob
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile

from PIL import Image

ROM_SHA1 = {
    "mj04g.bin": "07f7cf788810a1425016e016ce3579adb3253ac7",
    "mj03g.bin": "23a0a17d0abc4b084ffeba90266ef455361771cc",
    "gng13.n4": "13e5a38a134bd7cfa16c63a18fa332c6d66b9345",
    "gg16.bin": "9e06012bcd82f98fad43de666ef9a75979d940ab",
    "gg15.bin": "e3a1421d465b87148ffa94f5673b2307f0246afe",
    "gng16.l4": "af207f9ee2f93a0cf9cf25cfe72b0fdfe55481b8",
    "gg13.bin": "cb641c25bb04b970b2cbeca41adb792bbe142fb5",
    "gg12.bin": "3f129ca6d695548b659955fe538584bd9ac2ff17",
}

# $F7C0: offset of the bottom-left sprite from the object point (X right, Y up), and
# $F7D6: the routine for each shape. Sprites are listed column by column, bottom to top.
SHAPES = {
    0: (-8, -8, 1, 1), 1: (-8, -16, 1, 2), 2: (-16, -8, 2, 1), 3: (-16, -16, 2, 2),
    4: (-24, -24, 3, 3), 5: (-24, -8, 3, 1), 6: (-32, -32, 4, 4), 7: (-32, -8, 4, 1),
    8: (-8, -24, 1, 3), 9: (-24, -16, 3, 2), 10: (-8, -32, 1, 4),
}
# $8EC3: bytes per frame (codes, attribute and duration).
FRAME_LENGTH = [3, 4, 4, 6, 11, 5, 18, 6, 5, 8, 6]
NORMAL_BUILDER = 0xF6F4  # return address of $F6E4 when $F6F2 builds the list from every pool
BLANK_FRAME = 0xA4AB  # $8EFD shows this every other frame while the hit timer $AD runs.
POOLS = ["extra", "weapon", "enemyshot", "enemy", "support", "player", "final"]


class Rom:
    def __init__(self, path):
        with ZipFile(path) as rom:
            images = {name: rom.read(name) for name in ROM_SHA1}
        for name, expected in ROM_SHA1.items():
            if hashlib.sha1(images[name]).hexdigest() != expected:
                raise SystemExit(f"{name} does not match MAME's makaimurg manifest")
        self.cpu = bytearray(0x10000)
        self.cpu[0x4000:0x6000] = images["mj04g.bin"][:0x2000]  # bank 4
        self.cpu[0x6000:0x8000] = images["mj04g.bin"][0x2000:]
        self.cpu[0x8000:0x10000] = images["mj03g.bin"]
        # "sprites" region: planes 1-2 in the first half, planes 3-4 (the high bits) in the second.
        self.low = images["gng13.n4"] + images["gg16.bin"] + images["gg15.bin"]
        self.high = images["gng16.l4"] + images["gg13.bin"] + images["gg12.bin"]
        self.tiles = {}

    def byte(self, address):
        return self.cpu[address]

    def word(self, address):
        return self.cpu[address] << 8 | self.cpu[address + 1]

    def tile(self, code):
        """16x16 pens of one sprite, as MAME's spritelayout decodes them (15 is transparent)."""
        if code in self.tiles:
            return self.tiles[code]
        pens = [[15] * 16 for _ in range(16)]
        if code < len(self.low) // 64:
            base = code * 64
            for y in range(16):
                for x in range(16):
                    offset = base + (32 if x >= 8 else 0) + y * 2 + (x & 7) // 4
                    bit = 3 - (x & 3)
                    low, high = self.low[offset], self.high[offset]
                    # MAME counts bit offsets from bit 7, so plane offset +4 is the low nibble.
                    pens[y][x] = ((high >> bit) & 1) << 3 | ((high >> (bit + 4)) & 1) << 2 \
                        | ((low >> bit) & 1) << 1 | (low >> (bit + 4)) & 1
        self.tiles[code] = pens
        return pens

    def frame(self, pointer, shape):
        """The frame at pointer: sprite codes, attribute, duration and the next frame."""
        length = FRAME_LENGTH[shape]
        raw = self.cpu[pointer:pointer + length]
        codes = [raw[0]] + list(raw[2:length - 1])
        attribute, duration = raw[1], raw[length - 1]
        jump = bool(duration & 0x80)
        following = self.word(pointer + length) if jump else pointer + length
        return {
            "codes": [code + ((attribute << 2) & 0x300) for code in codes],
            "attribute": attribute, "colour": (attribute >> 4) & 3,
            "flip_x": bool(attribute & 4), "flip_y": bool(attribute & 8),
            "duration": duration & 0x7F, "jump": jump, "next": following,
        }


def sprites_of(frame, shape):
    """(code, left, top) of each sprite, relative to the object point with Y down."""
    dx, dy, columns, rows = SHAPES[shape]
    placed = []
    for index, code in enumerate(frame["codes"]):
        column, row = divmod(index, rows)
        placed.append((code, dx + 16 * column, -dy - 16 * (row + 1)))
    return placed


def build_list(rom, objects, cam_x, cam_y, budget=0x40):
    """Rebuild the sprite RAM entries $F79D writes, from $1E3C on."""
    entries = []
    for shape, pointer, x, y_low in objects:
        dx, dy, columns, rows = SHAPES[shape]
        count = columns * rows
        if budget < count:
            continue
        budget -= count
        frame = rom.frame(pointer, shape)
        left = (dx + x - cam_x) & 0x1FF
        bottom = (dy + y_low - cam_y) & 0xFF
        attribute = frame["attribute"]
        raw_codes = [rom.byte(pointer)] + [rom.byte(pointer + 2 + i) for i in range(count - 1)]
        for index, code in enumerate(raw_codes):
            column, row = divmod(index, rows)
            sx = (left + 16 * column) & 0x1FF
            sy = (-bottom - 16 * (row + 1)) & 0xFF
            entries.append(bytes([code, attribute | (sx >> 8), sy, sx & 0xFF]))
    return entries, budget


def parse_log(path):
    frames, checks = [], []
    raw = Path(path).read_bytes()
    text = raw.decode("utf-16") if raw[:2] in (b"\xff\xfe", b"\xfe\xff") else raw.decode("utf-8-sig")
    for line in text.splitlines():
        if line.startswith("frame,"):
            fields = dict(item.split("=", 1) for item in line.split(",")[2:])
            frames.append({
                "t": int(line.split(",")[1]), "pool": fields["pool"], "type": int(fields["type"], 16),
                "variant": int(fields["variant"], 16), "state": int(fields["state"], 16),
                "shape": int(fields["shape"]), "ptr": int(fields["ptr"], 16),
                "section": int(fields["section"]),
            })
        elif line.startswith("check,"):
            parts = line.split(",")
            fields = dict(item.split("=", 1) for item in parts[2:] if "=" in item)
            cam_y = int(parts[3])
            objects = []
            for item in fields["objs"].split():
                pool, shape, pointer, x, y_low = item.split(":")
                objects.append((int(shape), int(pointer, 16), int(x), int(y_low)))
            palette = []
            for i in range(0, len(fields["pal"]), 4):
                b, rg = int(fields["pal"][i:i + 2], 16), int(fields["pal"][i + 2:i + 4], 16)
                palette.append(((rg >> 4) * 17, (rg & 15) * 17, (b >> 4) * 17))
            checks.append({
                "t": int(parts[1]), "cam_x": int(fields["cam"]), "cam_y": cam_y,
                "budget": int(fields["budget"]), "objects": objects,
                "builder": int(fields["builder"], 16) if "builder" in fields else None,
                "ram": bytes.fromhex(fields["ram"]), "palette": palette,
            })
    return frames, checks


def draw_layer(rom, ram, palette):
    """The sprite layer as MAME draws it from sprite RAM (index 0 on top), 256x224."""
    layer = {}
    for offset in range(len(ram) - 4, -1, -4):
        code_low, attribute, sy, sx = ram[offset:offset + 4]
        sx -= 0x100 * (attribute & 1)
        code = code_low + ((attribute << 2) & 0x300)
        pens = rom.tile(code)
        for y in range(16):
            for x in range(16):
                pen = pens[15 - y if attribute & 8 else y][15 - x if attribute & 4 else x]
                if pen == 15:
                    continue
                px, py = sx + x, sy + y - 16
                if 0 <= px < 256 and 0 <= py < 224:
                    layer[(px, py)] = palette[((attribute >> 4) & 3) * 16 + pen]
    return layer


def check(args, rom):
    total = matched = special = 0
    for log in args.logs:
        _, checks = parse_log(log)
        for item in checks:
            if item["builder"] not in (None, NORMAL_BUILDER):
                special += 1  # $714E, $7257, $792D, $7D4D draw only a few objects, in their own order.
                continue
            entries, budget = build_list(rom, item["objects"], item["cam_x"], item["cam_y"])
            ram = item["ram"][0x3C:]
            expected = b"".join(entries)
            total += 1
            ok = ram[:len(expected)] == expected and budget == item["budget"]
            matched += ok
            if not ok:
                print(f"{log} t={item['t']}: rebuilt list differs from sprite RAM")
            if args.snap:
                layer = draw_layer(rom, item["ram"], item["palette"])
                results = []
                for later in (1, 2):
                    shot = args.snap / f"{args.snap_prefix}-{item['t']}-{later}.png"
                    if not shot.exists() or not layer:
                        continue
                    image = Image.open(shot).convert("RGB")
                    same = sum(image.getpixel(p) == c for p, c in layer.items())
                    results.append(f"+{later}: {same}/{len(layer)}")
                if results:
                    print(f"{log} t={item['t']}: sprite pixels equal to the snapshot {', '.join(results)}")
    print(f"{matched}/{total} sprite lists rebuilt exactly ({special} lists of special scenes skipped)")
    return 0 if matched == total else 1


def follow(rom, pointer, shape, observed):
    """The frames from pointer until the animation repeats or falls into an unlogged frame."""
    path, seen = [], set()
    while pointer not in seen and len(path) < 64:
        seen.add(pointer)
        frame = rom.frame(pointer, shape)
        path.append((pointer, frame))
        if not frame["jump"] and frame["next"] not in observed:
            break
        pointer = frame["next"]
    return path, pointer if pointer in seen else None


def render(rom, frame, shape, palette):
    placed = sprites_of(frame, shape)
    image = Image.new("RGBA", (64 + 16, 64 + 16), (0, 0, 0, 0))
    for code, left, top in placed:
        pens = rom.tile(code)
        for y in range(16):
            for x in range(16):
                pen = pens[15 - y if frame["flip_y"] else y][15 - x if frame["flip_x"] else x]
                if pen != 15:
                    colour = palette[frame["colour"] * 16 + pen] if palette else (pen * 17,) * 3
                    image.putpixel((left + x + 40, top + y + 40), colour + (255,))
    return image


def dump(args, rom):
    observed, palettes = {}, {}
    for log in args.logs:
        frames, checks = parse_log(log)
        # The palette of a section is taken from the first check of a log named s<section>-<run>.log.
        section = int(Path(log).stem.split("-")[0][1:])
        if checks and section not in palettes:
            palettes[section] = checks[0]["palette"]
        for item in frames:
            if item["ptr"] == BLANK_FRAME:
                continue
            # The player's +$05 is not a type: both of its values point at the same frames.
            kind = 0 if item["pool"] == "player" else item["type"]
            key = (item["pool"], kind, item["shape"])
            entry = observed.setdefault(key, {"pointers": {}, "sections": set()})
            entry["sections"].add(item["section"])
            seen = entry["pointers"].setdefault(item["ptr"], {"variants": set(), "states": set()})
            seen["variants"].add(item["variant"])
            seen["states"].add(item["state"])

    args.output.mkdir(parents=True, exist_ok=True)
    result = []
    for (pool, kind, shape), entry in sorted(observed.items(), key=lambda i: (POOLS.index(i[0][0]), i[0][1:])):
        pointers = entry["pointers"]
        animations, covered = [], set()
        for start in sorted(pointers):
            if start in covered:
                continue
            path, loop = follow(rom, start, shape, pointers)
            covered.update(p for p, _ in path)
            animations.append({
                "start": f"${start:04X}", "loop": f"${loop:04X}" if loop is not None else None,
                "frames": [{
                    "ptr": f"${p:04X}", "logged": p in pointers,
                    "codes": [f"${c:03X}" for c in f["codes"]], "colour": f["colour"],
                    "flip_x": f["flip_x"], "flip_y": f["flip_y"], "duration": f["duration"],
                    "variants": sorted(pointers[p]["variants"]) if p in pointers else [],
                    "states": [f"${s:04X}" for s in sorted(pointers[p]["states"])] if p in pointers else [],
                } for p, f in path],
            })
        # Drop an animation when another one runs into its first frame (and so contains it).
        for animation in animations:
            inside = {int(f["ptr"][1:], 16) for f in animation["frames"]}
            for other in animations:
                if other is not animation and int(other["start"][1:], 16) in inside:
                    other["merged"] = True
        animations = [a for a in animations if not a.get("merged")]
        sections = sorted(entry["sections"])
        result.append({"pool": pool, "type": f"${kind:02X}", "shape": shape,
                       "size": f"{SHAPES[shape][2] * 16}x{SHAPES[shape][3] * 16}",
                       # Top-left of each sprite in the order of "codes", from the object point, Y down.
                       "offsets": [[left, top] for _, left, top in sprites_of(
                           {"codes": [0] * SHAPES[shape][2] * SHAPES[shape][3]}, shape)],
                       "sections": sections, "animations": animations})

        palette = palettes.get(sections[0])
        rows = []
        for animation in animations:
            row = [render(rom, rom.frame(int(f["ptr"][1:], 16), shape), shape, palette)
                   for f in animation["frames"]]
            rows.append(row)
        width = max(len(r) for r in rows) * 80
        sheet = Image.new("RGBA", (width, 80 * len(rows)), (40, 40, 48, 255))
        for r, row in enumerate(rows):
            for c, image in enumerate(row):
                sheet.alpha_composite(image, (c * 80, r * 80))
        sheet.save(args.output / f"{pool}-{kind:02x}-shape{shape}.png")

    (args.output / "sprites.json").write_text(json.dumps(result, indent=1))
    frames = sum(len(a["frames"]) for r in result for a in r["animations"])
    print(f"{len(result)} (pool, type, shape) groups, {frames} frames -> {args.output}")
    return 0


def parse_load(path):
    """The load lines of a REVG_SPRITES_LOAD=1 log."""
    raw = Path(path).read_bytes()
    text = raw.decode("utf-16") if raw[:2] in (b"\xff\xfe", b"\xfe\xff") else raw.decode("utf-8-sig")
    for line in text.splitlines():
        if not line.startswith("load,"):
            continue
        parts = line.split(",")
        fields = dict(item.split("=", 1) for item in parts[2:] if "=" in item)
        objects = []
        for item in fields["objs"].split():
            pool, kind, shape, pointer, x, y_low = item.split(":")
            objects.append({"pool": pool, "type": int(kind, 16), "shape": int(shape),
                            "ptr": int(pointer, 16), "x": int(x), "y": int(y_low)})
        yield {"t": int(parts[1]), "cam_x": int(fields["cam"]), "cam_y": int(parts[3]),
               "builder": int(fields["builder"], 16), "budget": int(fields["budget"]),
               "free": int(fields["free"]), "difficulty": int(fields["difficulty"], 16), "objects": objects}


def half_opaque(rom, code, flip_x):
    """Whether the left and the right 8-pixel half of a sprite, as drawn, have an opaque pen."""
    pens = rom.tile(code)
    left = any(pens[y][x] != 15 for y in range(16) for x in range(8))
    right = any(pens[y][x] != 15 for y in range(16) for x in range(8, 16))
    return (right, left) if flip_x else (left, right)


def placed(rom, frame):
    """The drawn objects with their on-screen sprites, as (object, [(top, column, FC sprites)]).

    The arcade sprite is 16x16. On the FC each one becomes two 8x16 sprites, one for each 8-pixel half,
    and a half with no opaque pen needs no FC sprite. Objects that do not fit the budget are dropped,
    as $F6F2 drops them."""
    budget, result = 0x40, []
    for item in frame["objects"]:
        dx, dy, columns, rows = SHAPES[item["shape"]]
        if budget < columns * rows:
            continue
        budget -= columns * rows
        data = rom.frame(item["ptr"], item["shape"])
        left = (dx + item["x"] - frame["cam_x"]) & 0x1FF
        bottom = (dy + item["y"] - frame["cam_y"]) & 0xFF
        sprites = []
        for index, code in enumerate(data["codes"]):
            column, row = divmod(index, rows)
            sx = (left + 16 * column) & 0x1FF
            sy = (-bottom - 16 * (row + 1)) & 0xFF
            x = (sx & 0xFF) - 0x100 * (sx >> 8)  # MAME: the low byte, minus 256 when the high bit is set
            if -16 < x < 256:
                halves = sum(opaque and -8 < hx < 256
                             for opaque, hx in zip(half_opaque(rom, code, data["flip_x"]), (x, x + 8)))
                sprites.append((sy - 16, column, halves))
        result.append((item, sprites))
    return result, budget


GRAVESTONE = 0x01  # support type of the gravestones and rocks
UNICORN, ZOMBIE = 0x08, 0x0B


def is_gravestone(item):
    return item["pool"] == "support" and item["type"] == GRAVESTONE


def plan_sprites(item, sprites, narrow_weapons):
    """(top, FC sprites) of an object; a narrow weapon keeps only its first 16-pixel column."""
    if narrow_weapons and item["pool"] == "weapon":
        return [(top, min(n, 2)) for top, column, n in sprites if column == 0]
    return [(top, n) for top, _, n in sprites]


def busiest(objects, keep, narrow_weapons=False, zombies=None, enemies=None):
    """FC sprites on the busiest line, counting the objects keep() accepts."""
    lines, zombie_count, enemy_count = [0] * 224, 0, 0
    for item, sprites in objects:
        if item["pool"] == "enemy" and sprites:
            zombie_count += item["type"] == ZOMBIE
            enemy_count += 1
            if (zombies is not None and item["type"] == ZOMBIE and zombie_count > zombies) or \
                    (enemies is not None and enemy_count > enemies):
                continue
        if not keep(item):
            continue
        for top, n in plan_sprites(item, sprites, narrow_weapons):
            for line in range(max(top, 0), min(top + 16, 224)):
                lines[line] += n
    return max(lines)


def everything(item):
    return True


def not_gravestone(item):
    return not is_gravestone(item)


def player_only(item):
    return item["pool"] == "player"


def player_and_shots(item):
    return item["pool"] in ("player", "weapon", "enemyshot")


def unicorn_and_player(item):
    return item["pool"] == "player" or (item["pool"] == "enemy" and item["type"] == UNICORN)


# Each plan keeps the changes of the plans above it (TODO C-2).
PLANS = [
    ("as on the arcade", everything, {}),
    ("gravestones and rocks in BG", not_gravestone, {}),
    ("+ weapons 16 px wide", not_gravestone, {"narrow_weapons": True}),
    ("+ at most 2 zombies", not_gravestone, {"narrow_weapons": True, "zombies": 2}),
    ("+ at most 4 enemies on screen", not_gravestone, {"narrow_weapons": True, "zombies": 2, "enemies": 4}),
    ("drawn first: the player", player_only, {}),
    ("drawn first: player, 16 px weapons, enemy shots", player_and_shots, {"narrow_weapons": True}),
    ("drawn first: player, arcade weapons, enemy shots", player_and_shots, {}),
]
BOSS_PLAN = ("unicorn fight: unicorn and player", unicorn_and_player, {})


def load(args, rom):
    """Count objects and sprites per frame and per scanline, for the FC thinning rule (TODO C-2)."""
    frames = mismatched = special = gaps = skipped = 0
    max_type, max_pool, on_screen = {}, {}, {}
    used = 0
    plans = {name: {} for name, _, _ in PLANS + [BOSS_PLAN]}
    boss_frames = 0
    worst = []
    for log in args.logs:
        last = None
        for frame in parse_load(log):
            if last is not None:
                gaps += frame["t"] - last - 1
            last = frame["t"]
            if frame["builder"] != NORMAL_BUILDER:
                special += 1  # $714E draws a few objects in its own order after the boss
                continue
            frames += 1
            objects, budget = placed(rom, frame)
            mismatched += budget != frame["budget"]
            skipped += len(objects) < len(frame["objects"])
            used = max(used, 40 - frame["free"])
            counts_type, counts_pool = {}, {}
            for item, sprites in objects:
                if not sprites:
                    continue
                counts_pool[item["pool"]] = counts_pool.get(item["pool"], 0) + 1
                if item["pool"] == "enemy":
                    counts_type[item["type"]] = counts_type.get(item["type"], 0) + 1
            for key, value in counts_type.items():
                max_type[key] = max(max_type.get(key, 0), value)
            for key, value in counts_pool.items():
                max_pool[key] = max(max_pool.get(key, 0), value)
            enemies = counts_pool.get("enemy", 0)
            on_screen[enemies] = on_screen.get(enemies, 0) + 1
            for name, keep, options in PLANS:
                value = busiest(objects, keep, **options)
                plans[name][value] = plans[name].get(value, 0) + 1
                if name == PLANS[2][0]:
                    worst.append((value, Path(log).stem, frame["t"], " ".join(
                        f"{i['pool']}:{i['type']:02x}" for i, s in objects
                        if s and i["pool"] != "player" and not is_gravestone(i))))
            if counts_type.get(UNICORN):
                boss_frames += 1
                name, keep, options = BOSS_PLAN
                value = busiest(objects, keep, **options)
                plans[name][value] = plans[name].get(value, 0) + 1

    def share(count, total):
        return f"{100 * count / total:.1f}%" if total else "-"

    print(f"{frames} lists built by $F6F2 ({special} lists of special scenes skipped, "
          f"{gaps} frames without a new list); {mismatched} budgets differ from the rebuilt lists")
    print(f"enemy slots in use at most: {used}; lists with an object left out: {skipped}")
    print("most objects on screen at once, by pool:", ", ".join(f"{k} {v}" for k, v in sorted(max_pool.items())))
    print("most enemies on screen at once, by type:",
          ", ".join(f"${k:02X} {v}" for k, v in sorted(max_type.items())))
    print("enemies on screen:", ", ".join(f"{k}: {share(v, frames)}" for k, v in sorted(on_screen.items())))
    print("FC 8x16 sprites on the busiest line:")
    for name, histogram in plans.items():
        total = boss_frames if name == BOSS_PLAN[0] else frames
        bins = [sum(c for v, c in histogram.items() if low <= v <= high)
                for low, high in ((0, 8), (9, 16), (17, 24), (25, 999))]
        print(f"  {name:50s} " + " ".join(f"{label} {share(b, total):>6s}" for label, b in
                                        zip(("<=8", "9-16", "17-24", ">24"), bins))
              + f"  max {max(histogram, default=0)}" + (f" ({total} lists)" if name == BOSS_PLAN[0] else ""))
    for value, log, t, objects in sorted(worst, reverse=True)[:args.top]:
        print(f"  {value:2d} with gravestones in BG and 16 px weapons: {log} t={t}: {objects}")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=["check", "dump", "load"])
    parser.add_argument("--top", type=int, default=10, help="load: how many of the busiest frames to list")
    parser.add_argument("logs", nargs="+", type=Path)
    parser.add_argument("--rom", type=Path, default=Path("reference/makaimurg.zip"))
    parser.add_argument("--snap", type=Path, help="directory of the snapshots REVG_SPRITES_SNAP=1 saved")
    parser.add_argument("--snap-prefix", default="s0")
    parser.add_argument("--output", type=Path, default=Path("build/revg-sprites/out"))
    args = parser.parse_args()
    # PowerShell passes wildcards through unexpanded.
    args.logs = [Path(name) for log in args.logs
                 for name in (sorted(glob.glob(str(log))) if glob.has_magic(str(log)) else [str(log)])]
    rom = Rom(args.rom)
    return {"check": check, "dump": dump, "load": load}[args.command](args, rom)


if __name__ == "__main__":
    raise SystemExit(main())
