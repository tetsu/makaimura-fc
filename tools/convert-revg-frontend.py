"""Convert the arcade title screen and the opening demo for the game build.

Inputs (tools/mame/capture-revg-frontend.lua makes them; see docs/revision-g-sprites.md,
"タイトルとオープニング"):
  tools/fixtures/revg-title.png    the title after a coin (256x224), the text layer only
  tools/fixtures/revg-opening.txt  the opening's four objects, one line per frame

Output: src/revg-frontend-data.inc with
  * the title as an FC background: up to 255 tiles, the name table, the attributes and
    four palettes. The arcade's 224 lines go 8 lines down on the FC's 240.
  * the opening as a list of records: frames, background fade (0 full - 3 black), then
    Arthur, the princess, the demon and the armour (frame id from revg-sprite-data.inc,
    or 0 when hidden; arcade X and Y). A record with 0 frames ends the list.
Both go in one bank (macro revg_frontend_banks), after the sprite banks.

  python tools/convert-revg-frontend.py
"""

import argparse
import importlib.util
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent
OPENING_OBJECTS = ["0510", "15c2", "0890", "0540"]


def load_bg():
    spec = importlib.util.spec_from_file_location("convert_revg_bg", ROOT / "convert-revg-bg.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def distance(a, b):
    # Weighted RGB, close enough to tell the few title colours apart.
    return 2 * (a[0] - b[0]) ** 2 + 4 * (a[1] - b[1]) ** 2 + 3 * (a[2] - b[2]) ** 2


def convert_title(path, nes_rgb):
    image = Image.open(path).convert("RGB")
    assert image.size == (256, 224), image.size
    colours = {k: v for k, v in nes_rgb.items() if k != 0x0F}
    nearest = {}

    def nes(rgb):
        if rgb == (0, 0, 0):
            return 0x0F
        if rgb not in nearest:
            nearest[rgb] = min(colours, key=lambda k: distance(colours[k], rgb))
        return nearest[rgb]

    # FC screen: 256x240, the picture 8 lines down.
    screen = [[0x0F] * 256 for _ in range(240)]
    for y in range(224):
        for x in range(256):
            screen[y + 8][x] = nes(image.getpixel((x, y)))

    # Colour counts of each 16x16 attribute block.
    blocks = {}
    for by in range(15):
        for bx in range(16):
            count = {}
            for y in range(by * 16, by * 16 + 16):
                for x in range(bx * 16, bx * 16 + 16):
                    c = screen[y][x]
                    if c != 0x0F:
                        count[c] = count.get(c, 0) + 1
            blocks[(bx, by)] = count

    def block_error(count, palette):
        return sum(n * min(distance(nes_rgb[c], nes_rgb[p]) for p in palette) for c, n in count.items()
                   if c not in palette)

    # Four palettes of three: start from the heaviest blocks' top colours, then assign
    # blocks and refit each palette to its blocks' most used colours.
    def top3(count):
        return tuple(sorted(sorted(count, key=lambda c: -count[c])[:3]))

    weighted = {}
    for count in blocks.values():
        if count:
            key = top3(count)
            weighted[key] = weighted.get(key, 0) + sum(count.values())
    palettes = [list(k) for k, _ in sorted(weighted.items(), key=lambda kv: -kv[1])[:4]]
    while len(palettes) < 4:
        palettes.append([0x0F])
    for _ in range(24):
        assign = {b: min(range(4), key=lambda i: block_error(count, palettes[i])) for b, count in blocks.items()}
        refit = []
        for i in range(4):
            total = {}
            for b, count in blocks.items():
                if assign[b] == i:
                    for c, n in count.items():
                        total[c] = total.get(c, 0) + n
            refit.append(sorted(total, key=lambda c: -total[c])[:3])
        # A palette no block chose takes the colours of the block drawn worst.
        for i in range(4):
            if not refit[i]:
                worst = max(blocks, key=lambda b: block_error(blocks[b], refit[assign[b]] or [0x0F]))
                refit[i] = top3(blocks[worst]) and list(top3(blocks[worst]))
        refit = [r or [0x0F] for r in refit]
        if refit == palettes:
            break
        palettes = refit
    assign = {b: min(range(4), key=lambda i: block_error(count, palettes[i])) for b, count in blocks.items()}
    palettes = [(p + [0x0F] * 3)[:3] for p in palettes]

    # Tiles.
    tiles, names = [bytes(16)], []
    index_of = {bytes(16): 0}
    for ty in range(30):
        row = []
        for tx in range(32):
            palette = palettes[assign[(tx // 2, ty // 2)]]
            planes = [0] * 16
            for y in range(8):
                for x in range(8):
                    c = screen[ty * 8 + y][tx * 8 + x]
                    if c == 0x0F:
                        v = 0
                    else:
                        v = 1 + min(range(3), key=lambda i: distance(nes_rgb[c], nes_rgb[palette[i]]))
                    planes[y] |= (v & 1) << (7 - x)
                    planes[y + 8] |= (v >> 1) << (7 - x)
            data = bytes(planes)
            if data not in index_of:
                index_of[data] = len(tiles)
                tiles.append(data)
            row.append(index_of[data])
        names.append(row)
    if len(tiles) > 256:
        raise SystemExit(f"The title needs {len(tiles)} tiles")
    attributes = []
    for ay in range(8):
        for ax in range(8):
            value = 0
            for quarter, (dx, dy) in enumerate(((0, 0), (1, 0), (0, 1), (1, 1))):
                bx, by = ax * 2 + dx, ay * 2 + dy
                if by < 15:
                    value |= assign[(bx, by)] << (2 * quarter)
            attributes.append(value)
    palette_bytes = []
    for p in palettes:
        palette_bytes += [0x0F] + p
    return tiles, names, attributes, palette_bytes


def convert_opening(path):
    records, previous, fades = [], None, []
    for line in path.read_text().splitlines():
        parts = line.split(",")
        if parts[0] != "op":
            continue
        total = int(parts[2])
        if total not in fades:
            fades.append(total)
        objects = {}
        for item in parts[3:]:
            address, visible, shape, frame, x, y = item.split(":")
            objects[address] = None if visible == "0" else (int(frame, 16), int(x), int(y))
        state = (fades.index(total), tuple(objects[a] for a in OPENING_OBJECTS))
        if records and state == previous and records[-1][0] < 255:
            records[-1][0] += 1
        else:
            records.append([1, state])
        previous = state
    assert len(fades) == 4, fades
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("output", type=Path, nargs="?", default=Path("src/revg-frontend-data.inc"))
    parser.add_argument("--title", type=Path, default=ROOT / "fixtures" / "revg-title.png")
    parser.add_argument("--opening", type=Path, default=ROOT / "fixtures" / "revg-opening.txt")
    args = parser.parse_args()
    bg = load_bg()
    tiles, names, attributes, palette = convert_title(args.title, bg.NES_RGB)
    records = convert_opening(args.opening)

    out = ["; Generated by tools/convert-revg-frontend.py from tools/fixtures/revg-title.png and",
           "; tools/fixtures/revg-opening.txt. Do not edit.",
           f"; Title: {len(tiles)} tiles. Opening: {sum(r[0] for r in records)} frames in {len(records)} records.",
           "revg_frontend_bank_count = 1",
           f"revg_title_tile_count = {len(tiles)}",
           "",
           ".macro revg_frontend_banks",
           ".org $8000",
           "    .byte bank_tag_base + 19 + revg_bg_bank_count + revg_spr_bank_count",
           "revg_title_palette:",
           "    .byte " + ", ".join(f"${b:02X}" for b in palette),
           "revg_title_attributes:"]
    for i in range(0, 64, 16):
        out.append("    .byte " + ", ".join(f"${b:02X}" for b in attributes[i:i + 16]))
    out.append("revg_title_names:")
    for row in names:
        out.append("    .byte " + ", ".join(f"${b:02X}" for b in row))
    out.append("revg_title_tiles:")
    for t in tiles:
        out.append("    .byte " + ", ".join(f"${b:02X}" for b in t))
    out.append("; frames, fade, then Arthur, the princess, the demon, the armour: frame id, X, Y")
    out.append("revg_opening_track:")
    for count, (fade, objects) in records:
        fields = [f"{count}", f"{fade}"]
        for o in objects:
            if o is None:
                fields += ["<0", ">0", "0", "0"]
            else:
                frame, x, y = o
                assert x < 256 and y < 256, o
                fields += [f"<rf_{frame:04X}", f">rf_{frame:04X}", f"{x}", f"{y}"]
        out.append("    .byte " + ", ".join(fields))
    out.append("    .byte 0")
    out.append("    .res $a000 - *, $ff")
    out.append(".reloc")
    out.append(".endmacro")
    args.output.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")
    print(f"{args.output}: title {len(tiles)} tiles, palettes "
          + " ".join("/".join(f"{c:02X}" for c in palette[i * 4 + 1:i * 4 + 4]) for i in range(4))
          + f"; opening {len(records)} records")


if __name__ == "__main__":
    main()
