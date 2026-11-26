#!/usr/bin/env python3
"""Convert authentic Revision G arcade Makai Map captures (left & right halves) into
NES 2bpp CHR + 12-row nametable + attributes + 4 palettes (src/revg-map-data.inc).

Inputs:
  tools/fixtures/revg-map-left.png   (256x224 MAME capture, Stages 1-4)
  tools/fixtures/revg-map-right.png  (256x224 MAME capture, Stages 4-7 Castle)

Output:
  src/revg-map-data.inc  (macro revg_map_bank for Bank 11 / PAD_11 in REVG_STAGE1)

Only the lower 12 tile rows (arcade y=128..223 -> FC rows 16..27) contain the Makai
panorama art, leaving tiles $00..$3F free for the menu font and marker so text renders
cleanly in the black upper half of the screen.
"""

import argparse
import importlib.util
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent
TILE_BASE = 0x40     # Menu font + extra glyphs occupy $00..$3F
MAX_TILES = 256 - TILE_BASE
ROW_START = 16       # Arcade y=128..223 -> 12 tile rows (rows 16..27 on FC)
ROW_COUNT = 12


def load_bg():
    spec = importlib.util.spec_from_file_location("convert_revg_bg", ROOT / "convert-revg-bg.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def distance(a, b):
    return 2 * (a[0] - b[0]) ** 2 + 4 * (a[1] - b[1]) ** 2 + 3 * (a[2] - b[2]) ** 2


def convert_half(path: Path, nes_rgb, erase_rects):
    img = Image.open(path).convert("RGB")
    assert img.size == (256, 224), img.size
    px = img.load()
    # Erase the baked-in Arthur pin sprite from the MAME capture so FC draws it dynamically.
    for x0, y0, x1, y1 in erase_rects:
        for y in range(y0, y1):
            for x in range(x0, x1):
                px[x, y] = (0, 0, 0)

    colours = {k: v for k, v in nes_rgb.items() if k != 0x0F}
    nearest = {}

    def nes(rgb):
        if rgb == (0, 0, 0):
            return 0x0F
        if rgb not in nearest:
            nearest[rgb] = min(colours, key=lambda k: distance(colours[k], rgb))
        return nearest[rgb]

    # Crop y = 128..223 (96 px = 12 tile rows = 6 attribute rows)
    h = ROW_COUNT * 8
    screen = [[nes(px[x, 128 + y]) for x in range(256)] for y in range(h)]

    # Palette 0 is reserved for white/gold text in the upper half ($0f, $30, $28, $16),
    # and palettes 1..3 are fitted to the 16x16 blocks of the Makai panorama!
    blocks = {}
    for by in range(h // 16):
        for bx in range(16):
            cnt = {}
            for y in range(by * 16, by * 16 + 16):
                for x in range(bx * 16, bx * 16 + 16):
                    c = screen[y][x]
                    if c != 0x0F:
                        cnt[c] = cnt.get(c, 0) + 1
            blocks[(bx, by)] = cnt

    def block_err(cnt, pal):
        return sum(n * min(distance(nes_rgb[c], nes_rgb[p]) for p in pal) for c, n in cnt.items() if c not in pal)

    def top3(cnt):
        return tuple(sorted(sorted(cnt, key=lambda c: -cnt[c])[:3]))

    weighted = {}
    for cnt in blocks.values():
        if cnt:
            k = top3(cnt)
            weighted[k] = weighted.get(k, 0) + sum(cnt.values())

    palettes = [[0x30, 0x28, 0x16]]  # Pal 0: HUD/text (white, gold, red)
    for k, _ in sorted(weighted.items(), key=lambda kv: -kv[1]):
        if len(palettes) == 4:
            break
        pal = list(k)
        while len(pal) < 3:
            pal.append(0x0F)
        palettes.append(pal)
    while len(palettes) < 4:
        palettes.append([0x30, 0x10, 0x00])

    # Refine palettes 1..3
    for _ in range(20):
        assign = {b: min(range(1, 4), key=lambda i: block_err(cnt, palettes[i])) for b, cnt in blocks.items()}
        for i in range(1, 4):
            tot = {}
            for b, cnt in blocks.items():
                if assign[b] == i:
                    for c, n in cnt.items():
                        tot[c] = tot.get(c, 0) + n
            if tot:
                pal = sorted(tot, key=lambda c: -tot[c])[:3]
                while len(pal) < 3:
                    pal.append(0x0F)
                palettes[i] = pal

    assign = {b: min(range(1, 4), key=lambda i: block_err(cnt, palettes[i])) for b, cnt in blocks.items()}

    # Build deduplicated 2bpp CHR tiles (tile 0 = blank, panorama tiles start at TILE_BASE)
    tile_map = {}
    chr_tiles = []
    nametable = []
    for ty in range(ROW_COUNT):
        for tx in range(32):
            pal_idx = assign[(tx // 2, ty // 2)]
            pal = [0x0F] + palettes[pal_idx]
            p0, p1 = [], []
            empty = True
            for r in range(8):
                b0 = b1 = 0
                for c in range(8):
                    col = screen[ty * 8 + r][tx * 8 + c]
                    if col == 0x0F:
                        idx = 0
                    else:
                        idx = min(range(1, 4), key=lambda i: distance(nes_rgb[col], nes_rgb[pal[i]]))
                        empty = False
                    b0 = (b0 << 1) | (idx & 1)
                    b1 = (b1 << 1) | ((idx >> 1) & 1)
                p0.append(b0)
                p1.append(b1)
            if empty:
                nametable.append(0)
            else:
                key = bytes(p0 + p1)
                if key not in tile_map:
                    tile_map[key] = TILE_BASE + len(chr_tiles)
                    chr_tiles.append(key)
                nametable.append(tile_map[key])

    # If > MAX_TILES, merge least-used tiles with their closest Hamming match
    if len(chr_tiles) > MAX_TILES:
        from collections import Counter
        freq = Counter(t for t in nametable if t >= TILE_BASE)
        while len(chr_tiles) > MAX_TILES:
            # Drop rarest tile and remap to closest remaining tile
            rarest_idx = min(range(len(chr_tiles)), key=lambda i: freq[TILE_BASE + i])
            rarest_bytes = chr_tiles[rarest_idx]
            best_j = min(
                (j for j in range(len(chr_tiles)) if j != rarest_idx),
                key=lambda j: sum((a ^ b).bit_count() for a, b in zip(rarest_bytes, chr_tiles[j])),
            )
            old_id = TILE_BASE + rarest_idx
            new_id = TILE_BASE + best_j
            last_id = TILE_BASE + len(chr_tiles) - 1
            # Swap last into rarest_idx if needed
            if rarest_idx != len(chr_tiles) - 1:
                chr_tiles[rarest_idx] = chr_tiles[-1]
                if new_id == last_id:
                    new_id = old_id
                nametable = [
                    new_id if t == old_id else (old_id if t == last_id else t)
                    for t in nametable
                ]
                freq[old_id] = freq[last_id]
            else:
                nametable = [new_id if t == old_id else t for t in nametable]
            chr_tiles.pop()
            freq[new_id] += 1
            del freq[last_id]

    # Build 24 attribute bytes covering rows 16..27 ($23E0..$23F7 = 3 attr rows x 8 bytes)
    attrs = []
    for ay in range(3):
        for ax in range(8):
            tl = assign[(ax * 2, ay * 2)]
            tr = assign[(ax * 2 + 1, ay * 2)]
            bl = assign[(ax * 2, ay * 2 + 1)]
            br = assign[(ax * 2 + 1, ay * 2 + 1)]
            attrs.append(tl | (tr << 2) | (bl << 4) | (br << 6))

    pal_bytes = []
    for pal in palettes:
        pal_bytes.extend([0x0F] + pal)

    return chr_tiles, nametable, attrs, pal_bytes


def fmt_bytes(data):
    out = []
    for i in range(0, len(data), 16):
        out.append("    .byte " + ", ".join(f"${b:02x}" for b in data[i:i + 16]))
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, default=Path("src/revg-map-data.inc"), nargs="?")
    args = parser.parse_args()

    bg = load_bg()
    nes_rgb = bg.NES_RGB

    # Erase baked-in Arthur pin sprites from MAME captures:
    # revg-map-left.png: pin is at x=26..42, y=168..200
    # revg-map-right.png: pin is at x=138..154, y=168..200
    left = convert_half(ROOT / "fixtures" / "revg-map-left.png", nes_rgb, [(26, 168, 42, 200)])
    right = convert_half(ROOT / "fixtures" / "revg-map-right.png", nes_rgb, [(138, 168, 154, 198)])

    lines = [
        "; Generated by tools/convert-revg-map.py from MAME captures of makaimurg ($6400).",
        "; Authentic arcade Makai Stage Progression Map (left: Stages 1-4, right: Stages 4-7).",
        "; Placed in Bank 11 (PAD_11, $8000 window) for REVG_STAGE1 builds.",
        "",
        "revg_map_bank_id = 11",
        f"revg_map_tile_base = ${TILE_BASE:02x}",
        f"revg_map_left_tile_count = {len(left[0])}",
        f"revg_map_right_tile_count = {len(right[0])}",
        "",
        ".macro revg_map_bank",
        "    .byte bank_tag_base + revg_map_bank_id",
        "revg_map_left_palette:",
    ]
    lines += fmt_bytes(left[3])
    lines += ["revg_map_left_attrs:"] + fmt_bytes(left[2])
    lines += ["revg_map_left_names:"] + fmt_bytes(left[1])
    lines += ["revg_map_left_tiles:"]
    for t in left[0]:
        lines += fmt_bytes(t)
    lines += ["revg_map_right_palette:"] + fmt_bytes(right[3])
    lines += ["revg_map_right_attrs:"] + fmt_bytes(right[2])
    lines += ["revg_map_right_names:"] + fmt_bytes(right[1])
    lines += ["revg_map_right_tiles:"]
    for t in right[0]:
        lines += fmt_bytes(t)
    total = 1 + 16 + 24 + 384 + len(left[0]) * 16 + 16 + 24 + 384 + len(right[0]) * 16
    assert total <= 8192, f"Map bank overflow: {total} > 8192"
    lines += [f"    .res $2000 - {total}, $ff", ".endmacro", ""]

    args.output.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {args.output}: left={len(left[0])} tiles, right={len(right[0])} tiles, total={total}/8192 B")


if __name__ == "__main__":
    main()
