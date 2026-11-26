#!/usr/bin/env python3
"""Convert the Revision G Stage 1 sprites (Arthur, weapons, items, enemies) to FC data.

Reads the OBJ ROM, the animation frames and the sprite colours from MAME's makaimurg
set (reference/makaimurg.zip) and writes src/revg-sprite-data.inc
(docs/revision-g-sprites.md explains the arcade side):

- Every frame that tools/mame/log-revg-sprites.lua logged for the objects of section 0
  (tools/fixtures/revg-stage1-sprites.json keeps the list: frame address and shape) is
  read from ROM as the arcade draws it: the 16x16 codes in the shape's columns, the
  flips, the duration and the next frame. A next frame that was never logged ends the
  animation there (the last frame is held).
- Each 16x16 sprite becomes its two 8x16 halves; empty halves are dropped. The pieces
  keep the arcade's place relative to the object point, so the game draws a frame at
  the arcade X and Y and it lands where the arcade puts it.
- Four sprite palettes of three NES colours, fitted to all the pieces (Arthur counts
  most); each piece takes the palette that draws it best.
- The 8x16 tiles are kept once, flips included: the game streams them into CHR-RAM
  slots when a frame needs them (src/revg-objects.inc).

The game refers to a frame by the arcade address: rf_XXXX is the frame id of the arcade
frame at $XXXX. A frame id is the record's address in a metadata bank ($8000 window),
bits 13-15 the bank (from revg_spr_meta_bank).

  python tools/convert-revg-sprites.py                    # write src/revg-sprite-data.inc
  python tools/convert-revg-sprites.py --from-log build/revg-sprites/out/sprites.json
                                                          # refresh the fixture first
  python tools/convert-revg-sprites.py --preview sheet.png
"""

import argparse
import importlib.util
import json
import math
import random
from pathlib import Path
from zipfile import ZipFile

FIXTURE = Path(__file__).resolve().parent / "fixtures" / "revg-stage1-sprites.json"
FIRST_BANK = 24                      # after the background banks (19-23)
BANK_SIZE = 0x2000
TILES_PER_BANK = 256                 # 32 bytes each
SPRITE_PALETTE_OFFSET = 0x80         # mj04g.bin: RRRRGGGG x 64, then BBBBxxxx x 64

# (pool, type, shapes or None for all, weight in the palette fit). Section 0 only.
GROUPS = [
    ("player", "$00", None, 6.0),
    ("weapon", "$00", None, 1.5), ("weapon", "$01", None, 2.0), ("weapon", "$02", None, 1.5),
    ("weapon", "$03", None, 1.0), ("weapon", "$04", None, 1.5), ("weapon", "$05", None, 1.5),
    ("extra", "$00", None, 1.0), ("extra", "$01", None, 1.0),
    ("enemyshot", "$00", None, 1.0), ("enemyshot", "$01", None, 1.0), ("enemyshot", "$04", None, 1.0),
    ("enemyshot", "$06", None, 1.0), ("enemyshot", "$07", None, 1.0),
    ("enemy", "$00", [0, 2, 3], 1.0), ("enemy", "$01", [0, 3], 1.5), ("enemy", "$03", None, 1.0),
    ("enemy", "$05", None, 1.5), ("enemy", "$07", None, 1.0), ("enemy", "$08", None, 1.5),
    ("enemy", "$0B", None, 2.5), ("enemy", "$11", None, 3.0),
    ("support", "$00", None, 0.5), ("support", "$02", None, 8.0),
]


# The four sprite palettes. Fitting them freely gave Arthur two grey palettes and left
# out either the blue (zombies, crows, the flying knight) or the green and brown (the
# plants and the raft, the arcade's grass island) or the red (the Red Arremer, fire), so
# they are set by hand from the fits; each piece still takes the one that draws it best.
# The first was grey ($00, $3D), which left Arthur all grey. With the beard brown and
# the skin ($17, $37), his face and the naked Arthur take it, and the armour, whose
# lavender greys are closest to the light blues, takes the blue one. A None entry is
# fitted freely (that gave $17 and $10: a brown face and body).
PINNED = [[0x0F, 0x17, 0x37], [0x0F, 0x16, 0x27], [0x0F, 0x22, 0x31], [0x09, 0x17, 0x1A]]

# (first frame, shape): the armour, lance, axe and dagger items.
EXTRA_ANIMATIONS = [(0x5B0C, 1), (0x5B20, 2), (0x5B32, 0), (0x5B4E, 0)]
# (frame, shape): single frames of the opening demo (tools/fixtures/revg-opening.txt).
# The demo sets each one itself ($7BA6), so their next fields are not followed. Arthur
# sitting and jumping, the princess, the armour on the ground and the demon.
STATIC_FRAMES = [(0x798E, 3), (0x971E, 3), (0x7AD5, 1), (0x7AD8, 1), (0x7ADB, 1), (0x7B26, 1),
                 (0xA4AB, 4), (0x4E59, 4), (0x4E64, 4), (0x4E6F, 4), (0x4E7A, 4), (0x4E98, 4),
                 (0x4EB6, 4)]
# Arthur's cheer after taking the key ($7374, set by the key sequence for 60 frames).
STATIC_FRAMES += [(0x7374, 3)]
# The walks Arthur uses in the opening, naked and armoured, as animations.
EXTRA_ANIMATIONS += [(0x95F2, 3), (0x9596, 3)]
# Stage 2 (Section 1) enemies: Petit Devil ($02), Purple Flyer ($04 walk & fly),
# Big Man ($0A walk), and Big Man's projectile (enemyshot $03).
EXTRA_ANIMATIONS += [(0xA893, 0), (0xB10B, 0), (0xB19E, 2), (0x50E4, 3), (0xFD02, 0)]
STATIC_FRAMES += [(0x5170, 3), (0x5184, 3), (0x51B0, 3)]


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


TOOLS = Path(__file__).resolve().parent
sp = load_module("extract_revg_sprites", TOOLS / "mame" / "extract-revg-sprites.py")
bg = load_module("convert_revg_bg", TOOLS / "convert-revg-bg.py")


def refresh_fixture(log_path):
    data = json.loads(Path(log_path).read_text())
    frames, groups = {}, []
    for pool, typ, shapes, _ in GROUPS:
        starts = []
        for g in data:
            if g["pool"] != pool or g["type"] != typ or (shapes and g["shape"] not in shapes):
                continue
            for anim in g["animations"]:
                logged = [f for f in anim["frames"] if f["logged"]]
                if not logged:
                    continue
                starts.append(anim["start"])
                for f in logged:
                    frames.setdefault(f["ptr"], g["shape"])
        groups.append({"pool": pool, "type": typ, "starts": sorted(set(starts))})
    FIXTURE.write_text(json.dumps({"groups": groups, "frames": dict(sorted(frames.items()))}, indent=1) + "\n")
    print(f"{FIXTURE}: {len(frames)} frames")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("output", type=Path, nargs="?", default=Path("src/revg-sprite-data.inc"))
    parser.add_argument("--rom", type=Path, default=Path("reference/makaimurg.zip"))
    parser.add_argument("--from-log", type=Path)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    if args.from_log:
        refresh_fixture(args.from_log)

    fixture = json.loads(FIXTURE.read_text())
    shape_of = {int(p[1:], 16): s for p, s in fixture["frames"].items()}
    rom = sp.Rom(args.rom)
    # Frames the logs never showed: the item kinds no enemy dropped while logging
    # (bank4:$5AD1 lists each kind's first frame and shape). Their animations are followed
    # in ROM until they come back to a frame.
    for start, shape in EXTRA_ANIMATIONS:
        ptr = start
        while ptr not in shape_of or ptr == start:
            shape_of[ptr] = shape
            f = rom.frame(ptr, shape)
            ptr = f["next"]
            if ptr == start:
                break
    for ptr, shape in STATIC_FRAMES:
        shape_of.setdefault(ptr, shape)
    group_of = {}
    for (pool, typ, _, weight), g in zip(GROUPS, fixture["groups"]):
        for start in g["starts"]:
            group_of.setdefault(int(start[1:], 16), weight)
    with ZipFile(args.rom) as rom_zip:
        data = rom_zip.read("mj04g.bin")[SPRITE_PALETTE_OFFSET:SPRITE_PALETTE_OFFSET + 128]
    arcade_rgb = [((data[i] >> 4) * 17, (data[i] & 15) * 17, (data[64 + i] >> 4) * 17) for i in range(64)]

    # Each group's weight goes to the frames reachable from its animations.
    weight_of = {}
    for start, weight in group_of.items():
        ptr, seen = start, set()
        while ptr in shape_of and ptr not in seen:
            seen.add(ptr)
            weight_of[ptr] = max(weight_of.get(ptr, 0), weight)
            ptr = rom.frame(ptr, shape_of[ptr])["next"]

    # --- frames and their pieces (8x16, relative to the object point, Y down) ---------
    frames = {}
    for ptr, shape in sorted(shape_of.items()):
        frame = rom.frame(ptr, shape)
        pieces = []
        for code, left, top in sp.sprites_of(frame, shape):
            pens = rom.tile(code)
            drawn = [[pens[15 - y if frame["flip_y"] else y][15 - x if frame["flip_x"] else x] for x in range(16)]
                     for y in range(16)]
            for half in (0, 1):
                cols = [row[half * 8:half * 8 + 8] for row in drawn]
                if all(p == 15 for row in cols for p in row):
                    continue
                rgb = tuple(tuple(None if p == 15 else arcade_rgb[frame["colour"] * 16 + p] for p in row)
                            for row in cols)
                pieces.append({"dx": left + half * 8, "dy": top, "rgb": rgb})
        nxt = frame["next"] if frame["next"] in shape_of else ptr
        frames[ptr] = {"duration": frame["duration"], "next": nxt, "pieces": pieces,
                       "weight": weight_of.get(ptr, 1.0)}

    # --- palette fit -------------------------------------------------------------------
    import numpy as np
    unique = {}
    for f in frames.values():
        for p in f["pieces"]:
            unique[p["rgb"]] = unique.get(p["rgb"], 0) + f["weight"]
    pieces = list(unique)
    weights = np.array([unique[p] for p in pieces])
    colours = sorted({c for p in pieces for row in p for c in row if c is not None})
    rgb_lab = {c: bg.lab(c) for c in colours}
    candidates = sorted(c for c in bg.NES_RGB if c not in (0x2D,))
    nes_lab = {c: bg.lab(bg.NES_RGB[c]) for c in candidates}
    d = np.array([[math.dist(rgb_lab[a], nes_lab[c]) for c in candidates] for a in colours])
    col_index = {c: i for i, c in enumerate(colours)}
    cand_index = {c: i for i, c in enumerate(candidates)}
    hist = np.zeros((len(pieces), len(colours)))
    for i, p in enumerate(pieces):
        for row in p:
            for c in row:
                if c is not None:
                    hist[i, col_index[c]] += 1
    pair = np.array([[math.dist(rgb_lab[a], rgb_lab[b]) for b in colours] for a in colours])
    upper = np.triu(np.ones_like(pair), 1)
    lost = 1.5 * np.minimum(hist[:, :, None], hist[:, None, :]) * (pair * upper)[None]

    def palette_cost(p):
        options = np.stack([d[:, cand_index[c]] for c in p])
        mapped = options.argmin(axis=0)
        same = (mapped[:, None] == mapped[None, :]).astype(float)
        return hist @ options.min(axis=0) + (lost * same[None]).sum(axis=(1, 2))

    def total(table):
        return (table.min(axis=1) * weights).sum()

    rng = random.Random(1)
    heavy = sorted(range(len(pieces)), key=lambda i: -weights[i] * hist[i].sum())[:60]
    best, best_cost = None, None
    for _ in range(12):
        palettes = []
        for p in PINNED:
            if p is None:
                i = rng.choice(heavy)
                wish = sorted(candidates, key=lambda c: hist[i] @ d[:, cand_index[c]])
                p = wish[:3]
            palettes.append(list(p))
        table = np.stack([palette_cost(p) for p in palettes], axis=1)
        cost = total(table)
        improved = True
        while improved:
            improved = False
            for pi in [i for i, p in enumerate(PINNED) if p is None]:
                for slot in range(3):
                    for c in candidates:
                        if c in palettes[pi]:
                            continue
                        old = palettes[pi][slot]
                        palettes[pi][slot] = c
                        trial = table.copy()
                        trial[:, pi] = palette_cost(palettes[pi])
                        new = total(trial)
                        if new < cost - 1e-6:
                            cost, improved, table = new, True, trial
                        else:
                            palettes[pi][slot] = old
        if best_cost is None or cost < best_cost:
            best, best_cost = [list(p) for p in palettes], cost
    # Order each palette dark to light (only for readability).
    best = [sorted(p, key=lambda c: nes_lab[c][0]) for p in best]
    table = np.stack([palette_cost(p) for p in best], axis=1)
    choice = {pieces[i]: int(table[i].argmin()) for i in range(len(pieces))}

    def indices(rgb, palette):
        out = []
        for row in rgb:
            line = []
            for c in row:
                if c is None:
                    line.append(0)
                else:
                    line.append(1 + min(range(3), key=lambda k: d[col_index[c], cand_index[palette[k]]]))
            out.append(tuple(line))
        return tuple(out)

    # --- tiles, flips folded -----------------------------------------------------------
    # A tile id is bank * 256 + index; index 0 of each bank holds the bank tag, so ids
    # with a zero low byte are skipped.
    tiles, tile_id = {}, {}

    def variants(pattern):
        h = tuple(tuple(reversed(r)) for r in pattern)
        v = tuple(reversed(pattern))
        hv = tuple(reversed(h))
        return [(pattern, 0), (h, 0x40), (v, 0x80), (hv, 0xC0)]

    def place(pattern):
        for form, flip in variants(pattern):
            if form in tile_id:
                return tile_id[form], flip
        n = len(tiles)
        new = n + n // 255 + 1
        tile_id[pattern] = new
        tiles[new] = pattern
        return new, 0

    order = sorted(frames, key=lambda p: -frames[p]["weight"])     # Arthur's tiles first
    for ptr in order:
        for p in frames[ptr]["pieces"]:
            pal = choice[p["rgb"]]
            p["palette"] = pal
            p["tile"], p["flip"] = place(indices(p["rgb"], best[pal]))
    last_id = max(tiles)
    if last_id >= 1024:
        raise SystemExit(f"tile id {last_id}: the tile id has 10 bits")

    # --- records -----------------------------------------------------------------------
    tile_banks = last_id // 256 + 1
    meta_first = FIRST_BANK + tile_banks
    layout = {}
    bank, offset = 0, 1                       # byte 0 of each bank is the bank tag
    for ptr in sorted(frames):
        size = 4 + 4 * len(frames[ptr]["pieces"])
        if offset + size > BANK_SIZE:
            bank, offset = bank + 1, 1
        layout[ptr] = (bank, offset)
        offset += size
    meta_banks = bank + 1
    if meta_banks > 8:
        raise SystemExit("frame ids have 3 bank bits")

    def frame_id(ptr):
        b, o = layout[ptr]
        return b << 13 | o

    total_pieces = sum(len(f["pieces"]) for f in frames.values())
    lines = [
        "; Generated by tools/convert-revg-sprites.py from Revision G (makaimurg) section 0.",
        f"; {len(frames)} frames, {total_pieces} pieces, {len(tiles)} 8x16 tiles in {tile_banks} banks "
        f"from {FIRST_BANK}, frame records in {meta_banks} banks.",
        f"revg_spr_tile_bank = {FIRST_BANK}",
        f"revg_spr_meta_bank = {meta_first}",
        f"revg_spr_bank_count = {tile_banks + meta_banks}",
        f"revg_spr_tile_ids = {last_id + 1}",
        "",
        ".macro revg_spr_palette_bytes",
        "    .byte " + ", ".join(f"$0F, ${p[0]:02X}, ${p[1]:02X}, ${p[2]:02X}" for p in best),
        ".endmacro",
        "",
        "; Frame ids: bits 0-12 the record's offset in its bank ($8000 window), bits 13-15",
        "; the bank from revg_spr_meta_bank. A record: duration, next frame id (lo, hi),",
        "; piece count, then per piece: top (Y down from the object point), left, tile id",
        "; low byte, OAM attribute (bits 0-1 palette, 6-7 flips) with the tile id bits 8-9",
        "; in bits 2-3.",
    ]
    for ptr in sorted(frames):
        lines.append(f"rf_{ptr:04X} = ${frame_id(ptr):04X}")
    lines += ["", ".macro revg_spr_banks"]

    def tile_bytes(pattern):
        out = []
        for half in (pattern[:8], pattern[8:]):
            out += [sum(1 << (7 - x) for x in range(8) if row[x] & 1) for row in half]
            out += [sum(1 << (7 - x) for x in range(8) if row[x] & 2) for row in half]
        return out

    for b in range(tile_banks):
        lines += [f"; Bank {FIRST_BANK + b}: tiles ${b * 256:03X}-${b * 256 + 255:03X}.",
                  ".org $8000", f"    .byte bank_tag_base + {FIRST_BANK + b}", "    .res 31, $ff"]
        for i in range(1, 256):
            t = tiles.get(b * 256 + i)
            if t is None:
                break
            lines.append("    .byte " + ", ".join(f"${v:02X}" for v in tile_bytes(t)))
        lines += ["    .res $a000 - *, $ff", ".reloc"]
    current = None
    for ptr in sorted(frames):
        b, o = layout[ptr]
        if b != current:
            if current is not None:
                lines += ["    .res $a000 - *, $ff", ".reloc"]
            lines += [f"; Bank {meta_first + b}: frame records.", ".org $8000",
                      f"    .byte bank_tag_base + {meta_first + b}"]
            current = b
        f = frames[ptr]
        nid = frame_id(f["next"])
        body = [f["duration"], nid & 0xFF, nid >> 8, len(f["pieces"])]
        for p in f["pieces"]:
            body += [p["dy"] & 0xFF, p["dx"] & 0xFF, p["tile"] & 0xFF,
                     p["flip"] | p["palette"] | (p["tile"] >> 8) << 2]
        lines.append(f"; ${ptr:04X} -> ${f['next']:04X}")
        lines.append("    .byte " + ", ".join(f"${v:02X}" for v in body))
    lines += ["    .res $a000 - *, $ff", ".reloc", ".endmacro"]
    args.output.write_text("\n".join(lines) + "\n", newline="\n")
    print(f"{args.output}: {len(frames)} frames, {total_pieces} pieces, {len(tiles)} tiles "
          f"({tile_banks} banks), records in {meta_banks} banks; palettes "
          + " ".join("/".join(f"{c:02X}" for c in p) for p in best))

    if args.preview:
        from PIL import Image, ImageDraw
        nes = bg.NES_RGB
        cols, cell = 16, 72
        ptrs = sorted(frames)
        img = Image.new("RGB", (cols * cell, ((len(ptrs) + cols - 1) // cols) * cell), (60, 60, 80))
        dr = ImageDraw.Draw(img)
        for n, ptr in enumerate(ptrs):
            ox, oy = (n % cols) * cell + 36, (n // cols) * cell + 40
            for p in frames[ptr]["pieces"]:
                pat = tiles[p["tile"]]
                if p["flip"] & 0x40:
                    pat = tuple(tuple(reversed(r)) for r in pat)
                if p["flip"] & 0x80:
                    pat = tuple(reversed(pat))
                for y in range(16):
                    for x in range(8):
                        v = pat[y][x]
                        if v:
                            img.putpixel((ox + p["dx"] + x, oy + p["dy"] + y), nes[best[p["palette"]][v - 1]])
            dr.text(((n % cols) * cell + 2, (n // cols) * cell + 60), f"{ptr:04X}", fill=(255, 255, 0))
        img.save(args.preview)


if __name__ == "__main__":
    main()
