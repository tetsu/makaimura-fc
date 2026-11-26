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
from zipfile import ZipFile
from PIL import Image

ROOT = Path(__file__).resolve().parent
TILE_BASE = 0x40     # Menu font + extra glyphs occupy $00..$3F
MAX_TILES = 256 - TILE_BASE
ROW_START = 16       # Arcade y=128..223 -> 12 tile rows (rows 16..27 on FC)
ROW_COUNT = 12
ARCADE_SCROLL_MAX = 143  # Revision G $6440: cmpd #$008f (399 px continuous panorama)


def load_bg():
    spec = importlib.util.spec_from_file_location("convert_revg_bg", ROOT / "convert-revg-bg.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def load_sprites():
    spec = importlib.util.spec_from_file_location("extract_revg_sprites", ROOT / "mame" / "extract-revg-sprites.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def extract_arthur_pin_chr(rom_path: Path, nes_rgb):
    """Extract arcade Makai Map Arthur marker sprite ($25B, attr $84 = H-flip) as two 8x16 NES sprite tiles."""
    sp = load_sprites()
    rom = sp.Rom(rom_path)
    with ZipFile(rom_path) as z:
        pal_data = z.read("mj04g.bin")[0x80:0x100]
    arcade_rgb = [((pal_data[i] >> 4) * 17, (pal_data[i] & 15) * 17, (pal_data[64 + i] >> 4) * 17) for i in range(64)]

    # Arcade $6403-$6409: tile $5B, attr $84 -> code $25B, palette 0, flip_x=True, flip_y=False.
    pens = rom.tile(0x25B)
    # 3-color NES sprite palette: 1=red tip ($16), 2=face tan ($27), 3=silver/lavender armor ($21).
    # Map by hue so dark lavender armor shadows stay silver/lavender instead of bleeding into red.
    pin_pal = [0x0F, 0x16, 0x27, 0x21]

    def nearest_idx(rgb):
        r, g, b = rgb
        if r > 180 and g < 80 and b < 80:
            return 1  # red pointer tip
        if r > 120 and g > 80 and r - b > 40:
            return 2  # Arthur's tan visor/face
        return 3      # silver/lavender helmet & pin body

    grid = []
    for y in range(16):
        row = []
        for x in range(16):
            p = pens[y][15 - x]  # flip_x=True
            if p == 15:
                row.append(0)
            else:
                row.append(nearest_idx(arcade_rgb[p]))
        grid.append(row)

    # Encode left 8x16 tile ($1000) and right 8x16 tile ($1020) in NES 8x16 sprite CHR order.
    chr_bytes = []
    for half in (0, 1):
        for band in (0, 8):
            p0, p1 = [], []
            for y in range(band, band + 8):
                b0 = b1 = 0
                for x in range(half * 8, half * 8 + 8):
                    v = grid[y][x]
                    b0 = (b0 << 1) | (v & 1)
                    b1 = (b1 << 1) | ((v >> 1) & 1)
                p0.append(b0)
                p1.append(b1)
            chr_bytes.extend(p0 + p1)
    return chr_bytes, pin_pal


def distance(a, b):
    return 2 * (a[0] - b[0]) ** 2 + 4 * (a[1] - b[1]) ** 2 + 3 * (a[2] - b[2]) ** 2


def convert_panorama(left_path: Path, right_path: Path, nes_rgb, left_erase, right_erase):
    img_l = Image.open(left_path).convert("RGB")
    img_r = Image.open(right_path).convert("RGB")
    assert img_l.size == (256, 224) and img_r.size == (256, 224)
    px_l, px_r = img_l.load(), img_r.load()
    for x0, y0, x1, y1 in left_erase:
        for y in range(y0, y1):
            for x in range(x0, x1):
                px_l[x, y] = (0, 0, 0)
    for x0, y0, x1, y1 in right_erase:
        for y in range(y0, y1):
            for x in range(x0, x1):
                px_r[x, y] = (0, 0, 0)

    colours = {k: v for k, v in nes_rgb.items() if k != 0x0F}
    nearest = {}

    def nes(rgb):
        if rgb == (0, 0, 0):
            return 0x0F
        if rgb not in nearest:
            nearest[rgb] = min(colours, key=lambda k: distance(colours[k], rgb))
        return nearest[rgb]

    # Stitch the exact 399x96 continuous arcade panorama (left[0..255] + right[113..255],
    # matching Revision G $6440 scroll range 0..143 with 0 overlapping pixels), padded to 512px
    # ($2000 left + $2400 right) with black beyond column 398.
    h = ROW_COUNT * 8
    overlap = 256 - ARCADE_SCROLL_MAX  # 113 px shared between left (scx=0) and right (scx=143)
    screen = []
    for y in range(h):
        row = [nes(px_l[x, 128 + y]) for x in range(256)]
        row += [nes(px_r[x, 128 + y]) for x in range(overlap, 256)]
        row += [0x0F] * (512 - len(row))
        screen.append(row)

    blocks = {}
    for by in range(h // 16):
        for bx in range(32):
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

    tile_map = {}
    chr_tiles = []
    nt_all = []
    for ty in range(ROW_COUNT):
        for tx in range(64):
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
                nt_all.append(0)
            else:
                key = bytes(p0 + p1)
                if key not in tile_map:
                    tile_map[key] = TILE_BASE + len(chr_tiles)
                    chr_tiles.append(key)
                nt_all.append(tile_map[key])

    if len(chr_tiles) > MAX_TILES:
        from collections import Counter
        freq = Counter(t for t in nt_all if t >= TILE_BASE)
        while len(chr_tiles) > MAX_TILES:
            rarest_idx = min(range(len(chr_tiles)), key=lambda i: freq[TILE_BASE + i])
            rarest_bytes = chr_tiles[rarest_idx]
            best_j = min(
                (j for j in range(len(chr_tiles)) if j != rarest_idx),
                key=lambda j: sum((a ^ b).bit_count() for a, b in zip(rarest_bytes, chr_tiles[j])),
            )
            old_id = TILE_BASE + rarest_idx
            new_id = TILE_BASE + best_j
            last_id = TILE_BASE + len(chr_tiles) - 1
            if rarest_idx != len(chr_tiles) - 1:
                chr_tiles[rarest_idx] = chr_tiles[-1]
                if new_id == last_id:
                    new_id = old_id
                nt_all = [
                    new_id if t == old_id else (old_id if t == last_id else t)
                    for t in nt_all
                ]
                freq[old_id] = freq[last_id]
            else:
                nt_all = [new_id if t == old_id else t for t in nt_all]
            chr_tiles.pop()
            freq[new_id] += 1
            del freq[last_id]

    nt_left, nt_right = [], []
    for ty in range(ROW_COUNT):
        row = nt_all[ty * 64:(ty + 1) * 64]
        nt_left.extend(row[:32])
        nt_right.extend(row[32:])

    attrs_left, attrs_right = [], []
    for ay in range(3):
        for ax in range(8):
            tl = assign[(ax * 2, ay * 2)]
            tr = assign[(ax * 2 + 1, ay * 2)]
            bl = assign[(ax * 2, ay * 2 + 1)]
            br = assign[(ax * 2 + 1, ay * 2 + 1)]
            attrs_left.append(tl | (tr << 2) | (bl << 4) | (br << 6))
        for ax in range(8, 16):
            tl = assign[(ax * 2, ay * 2)]
            tr = assign[(ax * 2 + 1, ay * 2)]
            bl = assign[(ax * 2, ay * 2 + 1)]
            br = assign[(ax * 2 + 1, ay * 2 + 1)]
            attrs_right.append(tl | (tr << 2) | (bl << 4) | (br << 6))

    pal_bytes = []
    for pal in palettes:
        pal_bytes.extend([0x0F] + pal)

    return chr_tiles, nt_left, attrs_left, nt_right, attrs_right, pal_bytes


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

    tiles, nt_l, attr_l, nt_r, attr_r, pal = convert_panorama(
        ROOT / "fixtures" / "revg-map-left.png",
        ROOT / "fixtures" / "revg-map-right.png",
        nes_rgb,
        [],
        [],
    )
    pin_chr, pin_pal = extract_arthur_pin_chr(ROOT.parent / "reference" / "makaimurg.zip", nes_rgb)

    lines = [
        "; Generated by tools/convert-revg-map.py from MAME captures of makaimurg ($6400).",
        "; Continuous 399px arcade Makai Stage Progression Map ($2000 left + $2400 right, scroll 0..143)",
        "; plus the authentic 16x16 Arthur marker pin sprite ($25B, attr $84).",
        "; Placed in Bank 11 (PAD_11, $8000 window) for REVG_STAGE1 builds.",
        "",
        "revg_map_bank_id = 11",
        f"revg_map_tile_base = ${TILE_BASE:02x}",
        f"revg_map_tile_count = {len(tiles)}",
        "",
        ".macro revg_map_bank",
        "    .byte bank_tag_base + revg_map_bank_id",
        "revg_map_palette:",
    ]
    lines += fmt_bytes(pal)
    lines += ["revg_map_pin_palette:"] + fmt_bytes(pin_pal)
    lines += ["revg_map_pin_chr:"] + fmt_bytes(pin_chr)
    lines += ["revg_map_left_attrs:"] + fmt_bytes(attr_l)
    lines += ["revg_map_left_names:"] + fmt_bytes(nt_l)
    lines += ["revg_map_right_attrs:"] + fmt_bytes(attr_r)
    lines += ["revg_map_right_names:"] + fmt_bytes(nt_r)
    lines += ["revg_map_tiles:"]
    for t in tiles:
        lines += fmt_bytes(t)
    total = 1 + 16 + 4 + 64 + 24 + 384 + 24 + 384 + len(tiles) * 16
    assert total <= 8192, f"Map bank overflow: {total} > 8192"
    lines += [f"    .res $2000 - {total}, $ff", ".endmacro", ""]

    args.output.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {args.output}: shared={len(tiles)} tiles, total={total}/8192 B")


if __name__ == "__main__":
    main()
