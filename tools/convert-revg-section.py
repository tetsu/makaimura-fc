#!/usr/bin/env python3
"""Convert an extracted Revision G section map to the engine's stage map (layout and collision).

Reads build/revg-stages/sectionN-map.json, world.json and sections.json from
tools/mame/extract-revg-stages.py and writes a ca65 include with a column-major
map of 16 rows per column and one background palette per column. The art is a
placeholder (grass, dirt, tombstones); docs/stage-design.md explains the rules.

FC rows follow docs/revision-g-stage-data.md: FC y = camera Y + 255 - Y, so with
camera Y 0 the FC row is the arcade row counted from the top. The player's feet
stand at arcade Y = tile bottom + ground height + 18 (start and checkpoint Y 64 on
the height-14 floor), which is FC feet 16 * row - 2 - height.

A floor tile of height h ($448E) holds the feet from that surface down to the tile's
in-tile height 0, which is FC cell row - 1 from offset 14 - h to 14 ($8B0A). The cell
says where the surface is: mt_floor for height 14 (offset 0), mt_floor_top + offset - 1
for the lower ones (the forest bumps and the 1 px step before the lake).
"""

import argparse
import json
from pathlib import Path

ROWS = 15               # FC rows 0..14; row 15 of each map column is filler
COLUMN_STRIDE = 16
FEET_OFFSET = 18        # arcade feet Y above the tile bottom, minus the ground height
TOMBSTONE = 1           # support type for tombstones and rocks

# Engine metatile IDs (src/terrain-data.inc).
MT_SKY = 0
MT_LADDER = 5
MT_FLOOR = 15           # one-way floor drawn as grass
MT_DIRT = 16            # no collision, drawn as dirt
MT_TOMB = 17            # drawn as a tombstone; the box is in the tombstone table
MT_FLOOR_TOP = 18       # one-way floors whose surface is 1..14 px into the cell
FULL_HEIGHT = 14        # the height that puts the surface at the top of the FC cell


def floor_cell(height):
    """Metatile of a floor of this arcade height in the FC row above its tile."""
    if not 0 < height <= 16:
        raise ValueError(f"floor height {height} does not fit one FC cell")
    top = max(0, FULL_HEIGHT - height)
    return MT_FLOOR if top == 0 else MT_FLOOR_TOP + top - 1


def convert(tiles, collision, supports, rows=ROWS, x_min=0):
    width = len(tiles[0])
    cells = [[MT_SKY] * rows for _ in range(width)]
    floors = {}
    for row, line in enumerate(tiles):
        if row == 0 or row - 1 >= rows:
            continue
        for col, (code, _attr) in enumerate(line):
            if code >= 256:          # attribute bits 7-6 set: ignored by every check
                continue
            height = collision["ground"][code]
            if height:
                # The surface is in the FC row above the tile (feet 16 * row - 2 - height).
                floors.setdefault(col, {})[row - 1] = floor_cell(height)
            if collision["ladder"][code]:
                # A ladder tile sits one row lower than the floor its top serves.
                cells[col][row - 1] = MT_LADDER
    pits = []
    for col in range(width):
        col_floors = floors.get(col)
        if not col_floors:
            pits.append(col)
            continue
        for row, cell in col_floors.items():
            if cells[col][row] != MT_LADDER:
                cells[col][row] = cell
        for row in range(max(col_floors) + 1, rows):
            cells[col][row] = MT_DIRT
    tombs = []
    for support in supports:
        if support["type"] != TOMBSTONE:
            continue
        lx = support["x"] - x_min
        col = lx // 16
        feet = len(tiles) * 16 - (support["y"] + 2)
        row = feet // 16 - 1
        if cells[col][row + 1] != MT_FLOOR:
            raise ValueError(f"tombstone at x={support['x']} has no floor below")
        cells[col][row] = MT_TOMB
        tombs.append((lx, feet))
    return cells, pits, tombs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--stages", type=Path, default=Path("build/revg-stages"))
    parser.add_argument("--section", type=int, default=0)
    parser.add_argument("--prefix", default="revg")
    args = parser.parse_args()

    section_map = json.loads((args.stages / f"section{args.section}-map.json").read_text(encoding="utf-8"))
    world = json.loads((args.stages / "world.json").read_text(encoding="utf-8"))
    section = json.loads((args.stages / "sections.json").read_text(encoding="utf-8"))[args.section]
    map_height = section_map["height"]
    if map_height not in (16, 32):
        raise SystemExit(f"unsupported section map height: {map_height}")
    vpages = map_height // 16
    rows = ROWS * vpages  # 15 for 1-page, 30 for 2-page

    cells, pits, tombs = convert(
        section_map["tiles"], world["collision"], section["support_list"],
        rows=rows, x_min=section["camera"]["x_min"],
    )
    width = len(cells)
    if width > 255:
        raise SystemExit("the engine addresses at most 255 map columns")
    p = args.prefix
    lines = [
        f"; Generated by tools/convert-revg-section.py from Revision G section {args.section}.",
        f"; Pits at columns {', '.join(map(str, pits))}. Tombstones (x, feet): {tombs}.",
        f"{p}_width = {width}",
        f"{p}_height = {rows}",
        f"{p}_map:",
        f".assert <{p}_map = 0, error, \"{p}_map must be 256-byte aligned\"",
    ]
    if vpages == 1:
        for col in cells:
            lines.append("    .byte " + ", ".join(f"{cell:2d}" for cell in col + [MT_DIRT] * (COLUMN_STRIDE - ROWS)))
    else:
        # Engine 2-vertical-page layout (read_map_cell): upper page (rows 0..15) first,
        # then lower page (rows 16..29 + 2 filler rows) at offset width * 16.
        for col in cells:
            lines.append("    .byte " + ", ".join(f"{cell:2d}" for cell in col[:16]))
        for col in cells:
            lower = col[16:rows] + [MT_DIRT] * (16 - (rows - 16))
            lines.append("    .byte " + ", ".join(f"{cell:2d}" for cell in lower))
    lines.append(f"{p}_palettes:")
    for start in range(0, width, 32):
        lines.append("    .byte " + ", ".join("0" for _ in range(start, min(start + 32, width))))
    # The tombstones are arcade support objects ($9E14): boxes around their centre X and
    # the feet of the floor they stand on. The map cell only draws them.
    lines.append("; Tombstones: centre X and the feet of the floor below (arcade Y = 254 - feet).")
    lines.append(f"{p}_tomb_count = {len(tombs)}")
    if tombs:
        lines += [
            f"{p}_tomb_x_lo: .byte " + ", ".join(f"<{x}" for x, _ in tombs),
            f"{p}_tomb_x_hi: .byte " + ", ".join(f">{x}" for x, _ in tombs),
            f"{p}_tomb_feet: .byte " + ", ".join(str(feet) for _, feet in tombs),
        ]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{args.output}: {width} columns, {len(pits)} pit columns, {len(tombs)} tombstones")


if __name__ == "__main__":
    main()
