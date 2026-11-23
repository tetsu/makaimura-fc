#!/usr/bin/env python3
"""Convert the Revision G section 0 background art to streamed FC tiles.

Reads the arcade BG tiles, the section map and the palette set from MAME's makaimurg
set (reference/makaimurg.zip) and writes a ca65 include for banks 19 and up
(docs/stage-design.md, "背景の絵"):

- Four BG palettes. Each arcade 16 px tile takes the palette that draws it best,
  and its colours become the nearest of that palette's three colours or black.
  The HUD uses palette 0, whose colour 1 is the lightest of the four palettes.
- The 8x8 tiles of the 13 rows the FC shows (map rows 2-14), with the arcade flips
  drawn in, kept once each.
- A fixed CHR slot for each 8x8 tile. A tile is resident while its 16 px column is
  within the camera's 17 columns plus a margin on each side, and two tiles that
  are ever resident together get different slots. The game uploads a tile to its
  slot when it enters that range (lists per camera column, one for each direction),
  so the 219 free slots ($25-$FF) are enough for the whole section.
- The visual map (13 metatiles per column), the four slots of each metatile and
  the attribute bytes of each 32 px column.

With --preview, also writes a PNG of the converted section.
"""

import argparse
import hashlib
import importlib.util
import math
from pathlib import Path
from zipfile import ZipFile

BLANK = -1                 # metatile corner drawn with glyph $00
FIRST_SLOT = 0x25          # glyphs and digits use $00-$24
SLOT_COUNT = 256 - FIRST_SLOT
FIRST_ROW, LAST_ROW = 2, 14  # map rows under the HUD (docs/revision-g-stage-data.md)
VIEW_COLUMNS = 17          # 16 px columns visible at once (with fine scroll)
MARGIN = int(__import__("os").environ.get("REVG_MARGIN", 2))                 # columns loaded ahead of the view on each side
FIRST_BANK = 19
BANK_SIZE = 0x2000
BANK_BASE = 0xC000         # the data banks are read through the $C000 window

# The 2C02 palette (as in most emulators' defaults). Entries that repeat black or
# white, and $0D, are left out.
NES_RGB = {
    0x00: (84, 84, 84), 0x01: (0, 30, 116), 0x02: (8, 16, 144), 0x03: (48, 0, 136),
    0x04: (68, 0, 100), 0x05: (92, 0, 48), 0x06: (84, 4, 0), 0x07: (60, 24, 0),
    0x08: (32, 42, 0), 0x09: (8, 58, 0), 0x0A: (0, 64, 0), 0x0B: (0, 60, 0),
    0x0C: (0, 50, 60), 0x0F: (0, 0, 0),
    0x10: (152, 150, 152), 0x11: (8, 76, 196), 0x12: (48, 50, 236), 0x13: (92, 30, 228),
    0x14: (136, 20, 176), 0x15: (160, 20, 100), 0x16: (152, 34, 32), 0x17: (120, 60, 0),
    0x18: (84, 90, 0), 0x19: (40, 114, 0), 0x1A: (8, 124, 0), 0x1B: (0, 118, 40),
    0x1C: (0, 102, 120),
    0x20: (236, 238, 236), 0x21: (76, 154, 236), 0x22: (120, 124, 236), 0x23: (176, 98, 236),
    0x24: (228, 84, 236), 0x25: (236, 88, 180), 0x26: (236, 106, 100), 0x27: (212, 136, 32),
    0x28: (160, 170, 0), 0x29: (116, 196, 0), 0x2A: (76, 208, 32), 0x2B: (56, 204, 108),
    0x2C: (56, 180, 204), 0x2D: (60, 60, 60),
    0x31: (168, 204, 236), 0x32: (188, 188, 236), 0x33: (212, 178, 236), 0x34: (236, 174, 236),
    0x35: (236, 174, 212), 0x36: (236, 180, 176), 0x37: (228, 196, 144), 0x38: (204, 210, 120),
    0x39: (180, 222, 120), 0x3A: (168, 226, 144), 0x3B: (152, 226, 180), 0x3C: (160, 214, 228),
    0x3D: (160, 162, 160),
}
BLACK = 0x0F
CONTRAST_WEIGHT = float(__import__("os").environ.get("REVG_CONTRAST", 2))
WATER_WEIGHT = 8
GROUND_PALETTE = 6          # the arcade palette of the grass and earth
HUD_COLOUR = 0x10          # light grey HUD text, also the lightest stone colour
# Palette 0 is shared by both halves and the HUD: the HUD colour and a darker grey, so
# the stone (mountains, tombstones, the gate) keeps two shades everywhere.
FIXED_COLOURS = (HUD_COLOUR, 0x00)


# The tombstones are support objects (type $01, shape 3: 2x2 sprites) in the arcade, not
# BG tiles. They never move, so they are drawn into the BG art here. Centre X, arcade Y
# and animation frame, as tools/mame/log-revg-sprites.lua logs them (REVG_SPRITES_LOAD=1)
# for section 0; the same places as revg_tomb_* in src/revg-section0-map.inc.
TOMBSTONES = [
    (48, 62, 0x585B), (256, 62, 0x5860), (416, 62, 0x585B), (528, 62, 0x5865),
    (752, 62, 0x585B), (768, 142, 0x5860), (864, 142, 0x5865), (960, 142, 0x5865),
    (960, 62, 0x585B), (1104, 62, 0x585B), (1264, 62, 0x5860), (1520, 62, 0x5865),
]
SPRITE_PALETTE_OFFSET = 0x80   # mj04g.bin: the sprite colours $40-$7F, RRRRGGGG then BBBBxxxx
TOMBSTONE_WEIGHT = 8


def tombstone_overlays(rom_path):
    """(map column, map row) -> 16 rows of 16 RGB or None: the tombstone sprite pixels.

    A shape 3 object's sprites start 16 px left of its X, and the bottom pair's top row
    is at map Y 256 - arcade Y: $F79D writes sprite Y 256 - Y, and both the sprites and
    the 224-line screen start 16 lines down (map row 1)."""
    path = Path(__file__).resolve().parent / "mame" / "extract-revg-sprites.py"
    spec = importlib.util.spec_from_file_location("extract_revg_sprites", path)
    sp = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(sp)
    rom = sp.Rom(rom_path)
    with ZipFile(rom_path) as rom_zip:
        data = rom_zip.read("mj04g.bin")[SPRITE_PALETTE_OFFSET:SPRITE_PALETTE_OFFSET + 128]
    palette = [((data[i] >> 4) * 17, (data[i] & 15) * 17, (data[64 + i] >> 4) * 17) for i in range(64)]
    canvas = {}
    for x, y, pointer in TOMBSTONES:
        frame = rom.frame(pointer, 3)
        for code, left, top in sp.sprites_of(frame, 3):
            pens = rom.tile(code)
            for sy in range(16):
                for sx in range(16):
                    pen = pens[15 - sy if frame["flip_y"] else sy][15 - sx if frame["flip_x"] else sx]
                    if pen != 15:
                        canvas[(x + left + sx, 256 - y + top + sy)] = palette[frame["colour"] * 16 + pen]
    cells = {}
    for (px, py), rgb in canvas.items():
        cell = cells.setdefault((px // 16, py // 16), [[None] * 16 for _ in range(16)])
        cell[py % 16][px % 16] = rgb
    return {k: tuple(tuple(row) for row in v) for k, v in cells.items()}


def load_extractor():
    path = Path(__file__).resolve().parent / "mame" / "extract-revg-stages.py"
    spec = importlib.util.spec_from_file_location("extract_revg_stages", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def lab(rgb):
    def linear(c):
        c /= 255
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (linear(c) for c in rgb)
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883

    def f(t):
        return t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116
    fx, fy, fz = f(x), f(y), f(z)
    return 116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)


def distance(a, b):
    return math.dist(a, b)


class Tile16:
    """One arcade 16 px tile as drawn (flips applied) with its colours."""

    def __init__(self, pixels):
        self.pixels = pixels                      # 16 rows of 16 RGB tuples
        self.hist = {}
        for row in pixels:
            for rgb in row:
                self.hist[rgb] = self.hist.get(rgb, 0) + 1


def fit_palettes(tiles, weights, allowed, candidates, rgb_lab, hud_colour, restarts=40, seed=1):
    """Choose six palettes of three NES colours; return them and each tile's choice.

    Palettes 0 and 1 are shared; 2 and 3 are the graveyard's, 4 and 5 the forest's
    (loaded into palettes 2 and 3 past the swap). allowed[i] lists the palettes
    tile i may use.

    The cost is the summed Lab distance of every drawn pixel to the nearest of its
    palette's colours or black, plus the contrast lost when two arcade colours of a
    tile become the same colour (their distance, for the smaller pixel count). Without
    that term the close shades of the tree trunks and mountains all became one colour.
    Palette 0 keeps hud_colour (the HUD text). A local search changes one colour at a
    time, from several random starts.
    """
    import numpy as np
    import random

    all_colours = candidates + [BLACK]
    nes_lab = {c: lab(NES_RGB[c]) for c in all_colours}
    colours = sorted({rgb for t in tiles for rgb in t.hist})
    column = {c: i for i, c in enumerate(all_colours)}
    d = np.array([[distance(rgb_lab[rgb], nes_lab[c]) for c in all_colours] for rgb in colours])
    hist = np.array([[t.hist.get(rgb, 0) for rgb in colours] for t in tiles], dtype=float)
    weight = np.array(weights, dtype=float)
    black = d[:, column[BLACK]]
    pair = np.array([[distance(rgb_lab[a], rgb_lab[b]) for b in colours] for a in colours])
    upper = np.triu(np.ones_like(pair), 1)
    lost = CONTRAST_WEIGHT * np.minimum(hist[:, :, None], hist[:, None, :]) * (pair * upper)[None]  # tiles x K x K

    def palette_cost(p):
        """Cost of drawing every tile with palette p (one value per tile)."""
        options = np.stack([black] + [d[:, column[c]] for c in p])            # 4 x K
        mapped = options.argmin(axis=0)
        same = (mapped[:, None] == mapped[None, :]).astype(float)
        return hist @ options.min(axis=0) + (lost * same[None]).sum(axis=(1, 2))

    def costs(palettes):
        return np.stack([palette_cost(p) for p in palettes], axis=1)        # tiles x palettes

    mask = np.full((len(tiles), 6), np.inf)
    for i, options in enumerate(allowed):
        mask[i, list(options)] = 0

    def total_from(columns):
        return ((columns + mask).min(axis=1) * weight).sum()

    rng = random.Random(seed)
    heavy = sorted(range(len(tiles)), key=lambda i: -hist[i].sum())[:40]
    best, best_cost = None, None
    for _ in range(restarts):
        palettes = [list(hud_colour) + rng.sample(candidates, 3 - len(hud_colour))]             + [rng.sample(candidates, 3) for _ in range(5)]
        # Start the other palettes from the colours of a few heavy tiles.
        for p in range(1, 6):
            tile = tiles[rng.choice(heavy)]
            wish = sorted(candidates, key=lambda c: sum(d[colours.index(rgb), column[c]] * n
                                                        for rgb, n in tile.hist.items()))
            palettes[p] = wish[:3]
        table = costs(palettes)
        cost = total_from(table)
        improved = True
        while improved:
            improved = False
            for p in range(6):
                for slot in range(3):
                    if p == 0 and slot < len(hud_colour):
                        continue
                    for c in candidates:
                        if c in palettes[p]:
                            continue
                        old = palettes[p][slot]
                        palettes[p][slot] = c
                        trial = table.copy()
                        trial[:, p] = palette_cost(palettes[p])
                        new = total_from(trial)
                        if new < cost - 1e-6:
                            cost, improved, table = new, True, trial
                        else:
                            palettes[p][slot] = old
        if best_cost is None or cost < best_cost:
            best, best_cost = [list(p) for p in palettes], cost
    choice = list((costs(best) + mask).argmin(axis=1))
    dist = {(rgb, c): d[i, column[c]] for i, rgb in enumerate(colours) for c in all_colours}
    return best, choice, dist


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--rom", type=Path, default=Path("reference/makaimurg.zip"))
    parser.add_argument("--section", type=int, default=0)
    parser.add_argument("--preview", type=Path)
    parser.add_argument("--swap", type=int, default=97,
                        help="camera column (16 px) where the forest palettes replace the graveyard's")
    args = parser.parse_args()

    ex = load_extractor()
    with ZipFile(args.rom) as rom_zip:
        images = {name: rom_zip.read(name) for name in list(ex.ROM_SHA1) + ex.BG_ROMS}
    for name, expected in ex.ROM_SHA1.items():
        if hashlib.sha1(images[name]).hexdigest() != expected:
            raise SystemExit(f"{name} does not match MAME's makaimurg manifest")
    rom = ex.Rom(images)
    gfx = ex.tile_decoder(images)
    pages = ex.page_entries(rom)
    section = ex.section_settings(rom)[args.section]
    ids = ex.screen_ids(rom)
    tilemap = ex.section_map(rom, ids, section)
    c0 = section["pages"]["columns"][0]
    r0 = section["pages"]["rows"][0]
    numbers = {pages[r0 * ex.GRID_COLUMNS + c]["palette"] for c in range(c0, section["pages"]["columns"][1] + 1)}
    numbers.discard(0)
    if len(numbers) != 1:
        raise SystemExit(f"section {args.section} changes palettes between pages: {sorted(numbers)}")
    arcade_palette = ex.palette_sets(rom, max(numbers))[numbers.pop()]

    width = tilemap["width"]
    rows = list(range(FIRST_ROW, LAST_ROW + 1))

    # The forest palettes replace the graveyard's when the camera reaches column swap.
    # Every column visible on either side of that step uses only the shared palettes.
    swap = args.swap

    def zone(col):
        if col < swap - 1:
            return 0
        return 1 if col > swap + VIEW_COLUMNS - 1 else 2

    overlays = tombstone_overlays(args.rom)

    # Arcade 16 px tiles as drawn, with the tombstones drawn over them.
    sources = {}          # arcade tile code -> 16x16 colour numbers
    drawn_tiles = {}      # (code, flips and palette, zone, tombstone part) -> Tile16
    cells = [[None] * len(rows) for _ in range(width)]
    for r_index, row in enumerate(rows):
        for col, (code, attr) in enumerate(tilemap["tiles"][row]):
            over = overlays.get((col, row))
            key = (code & 0x3FF, attr & 0x37, zone(col), over)
            if key not in drawn_tiles:
                if key[0] not in sources:
                    sources[key[0]] = gfx(key[0])
                src = sources[key[0]]
                colours = arcade_palette[8 * (attr & 7):8 * (attr & 7) + 8]
                pixels = []
                for y in range(16):
                    sy = 15 - y if attr & 0x20 else y
                    line = [colours[src[sy][15 - x if attr & 0x10 else x]] for x in range(16)]
                    if over:
                        line = [o if o is not None else c for o, c in zip(over[y], line)]
                    pixels.append(tuple(line))
                drawn_tiles[key] = Tile16(pixels)
            cells[col][r_index] = key
    unique = list(drawn_tiles)
    tiles = [drawn_tiles[k] for k in unique]
    weights = [0] * len(unique)
    index_of = {k: i for i, k in enumerate(unique)}
    for col in cells:
        for key in col:
            weights[index_of[key]] += 1

    # Water covers few tiles but must stay blue, so its tiles count more.
    for i, tile in enumerate(tiles):
        if any(b > r + 40 and b > g for (r, g, b) in tile.hist):
            weights[i] *= WATER_WEIGHT
        if unique[i][3]:
            weights[i] *= TOMBSTONE_WEIGHT
    rgb_lab = {rgb: lab(rgb) for t in tiles for rgb in t.hist}
    candidates = sorted(c for c in NES_RGB if c != BLACK)
    zone_palettes = ((0, 1, 2, 3), (0, 1, 4, 5), (0, 1))
    # The ground (arcade palette 6) runs through both halves and the swap, so it always
    # uses palette 1 and looks the same everywhere.
    # A tombstone over the grass keeps its greys: it may take any palette of its zone.
    allowed = [(1,) if key[1] & 7 == GROUND_PALETTE and not key[3] else zone_palettes[key[2]]
               for key in unique]
    palettes, choice, dist = fit_palettes(tiles, weights, allowed, candidates, rgb_lab, FIXED_COLOURS)

    # Palette 0 starts with the HUD colour (colour 1 of the text); the others go dark to light.
    def lightness(c):
        return lab(NES_RGB[c])[0]
    palettes[0] = list(FIXED_COLOURS) + sorted(palettes[0][len(FIXED_COLOURS):], key=lightness)
    for p in palettes[1:]:
        p.sort(key=lightness)
    # The attribute bits: the forest's palettes 4 and 5 are loaded as 2 and 3.
    attribute_palette = (0, 1, 2, 3, 2, 3)

    # Draw each 16 px tile with its palette: 2-bit indices.
    def indices(tile, palette):
        out = []
        options = [BLACK] + palette
        for row in tile.pixels:
            out.append([min(range(4), key=lambda i: dist[(rgb, options[i])]) for rgb in row])
        return out

    eight = {}            # 8x8 index pattern -> tile id
    patterns = []
    metatiles = {}        # (tl, tr, bl, br, palette) -> metatile id
    metatile_list = []
    tile_meta = [None] * len(unique)
    for i, tile in enumerate(tiles):
        drawn = indices(tile, palettes[choice[i]])
        quads = []
        for qy in (0, 8):
            for qx in (0, 8):
                pattern = tuple(tuple(drawn[qy + y][qx:qx + 8]) for y in range(8))
                if pattern not in eight:
                    eight[pattern] = len(patterns)
                    patterns.append(pattern)
                quads.append(eight[pattern])
        meta = (*quads, choice[i])
        if meta not in metatiles:
            metatiles[meta] = len(metatile_list)
            metatile_list.append(meta)
        tile_meta[i] = metatiles[meta]
    if len(metatile_list) > 256:
        raise SystemExit(f"{len(metatile_list)} metatiles do not fit a byte")
    vmap = [[tile_meta[index_of[key]] for key in col] for col in cells]
    exact_tiles = len(patterns)

    # Which 8x8 tiles each camera column needs.
    camera_columns = width - 16 + 1
    usage = [0] * len(patterns)
    for col in vmap:
        for m in col:
            for t in metatile_list[m][:4]:
                usage[t] += 1

    # The blank tile is glyph $00, always loaded: it takes no slot and is never uploaded.
    blank_pattern = tuple(tuple([0] * 8) for _ in range(8))
    blank = eight.get(blank_pattern, -1)

    def camera_windows(rep):
        column_tiles = [set() for _ in range(width)]
        for col in range(width):
            for m in vmap[col]:
                column_tiles[col].update(rep[t] for t in metatile_list[m][:4])
            column_tiles[col].discard(blank)
        windows = []
        for p in range(camera_columns):
            result = set()
            for col in range(max(0, p - MARGIN), min(width, p + VIEW_COLUMNS + MARGIN)):
                result |= column_tiles[col]
            windows.append(result)
        return windows

    # Too many tiles share some camera windows, so merge the closest pair in the fullest
    # window (the less used tile takes the other's pattern) until each fits the budget.
    import numpy as np
    pixels = np.array(patterns, dtype=np.int8).reshape(len(patterns), 64)
    rep = list(range(len(patterns)))
    merged = 0

    def merge_to(budget):
        nonlocal merged
        while True:
            windows = camera_windows(rep)
            worst = max(windows, key=len)
            if len(worst) <= budget:
                return windows
            ids = sorted(worst)
            sub = pixels[ids]
            diff = (sub[:, None, :] != sub[None, :, :]).sum(axis=2)
            np.fill_diagonal(diff, 99)
            i, j = np.unravel_index(np.argmin(diff), diff.shape)
            keep, drop = (ids[i], ids[j]) if usage[ids[i]] >= usage[ids[j]] else (ids[j], ids[i])
            for t in range(len(rep)):
                if rep[t] == drop:
                    rep[t] = keep
            usage[keep] += usage[drop]
            merged += 1

    def assign_slots(windows):
        conflicts = {}
        first_seen = {}
        for p, window in enumerate(windows):
            for t in window:
                conflicts.setdefault(t, set()).update(window)
                first_seen.setdefault(t, p)
        slots = {}
        for t in sorted(conflicts, key=lambda t: first_seen[t]):
            used = {slots[o] for o in conflicts[t] if o in slots}
            slot = next(s for s in range(SLOT_COUNT + 256) if s not in used)
            if slot >= SLOT_COUNT:
                return None
            slots[t] = slot
        return slots

    budget = SLOT_COUNT
    while True:
        windows = merge_to(budget)
        slots = assign_slots(windows)
        if slots is not None:
            break
        budget -= 2
    largest = max(len(w) for w in windows)

    # Keep only the tiles still used, numbered in order of first use.
    kept = sorted(slots, key=lambda t: (min(p for p, w in enumerate(windows) if t in w), t))
    renumber = {t: i for i, t in enumerate(kept)}
    patterns = [patterns[t] for t in kept]
    slot_of = [slots[t] for t in kept]
    renumber[blank] = BLANK
    metatile_list = [(*(renumber[rep[t]] for t in meta[:4]), meta[4]) for meta in metatile_list]
    windows = [{renumber[t] for t in w} for w in windows]
    slots_used = max(slot_of) + 1

    right = [sorted(windows[0])] + [sorted(windows[p] - windows[p - 1]) for p in range(1, camera_columns)]
    left = [[]] + [sorted(windows[p - 1] - windows[p]) for p in range(1, camera_columns)]

    # Attribute bytes: one per 32 px column pair and attribute row 1-7 (rows 2-15).
    def palette_at(col, row):
        if col >= width or not FIRST_ROW <= row <= LAST_ROW:
            return 0
        return attribute_palette[metatile_list[vmap[col][row - FIRST_ROW]][4]]
    attributes = []
    for pair in range(0, width, 2):
        for attr_row in range(1, 8):
            top, bottom = 2 * attr_row, 2 * attr_row + 1
            attributes.append(palette_at(pair, top) | palette_at(pair + 1, top) << 2
                              | palette_at(pair, bottom) << 4 | palette_at(pair + 1, bottom) << 6)

    # Lay the data out in banks of the $C000 window.
    def chr_bytes(pattern):
        low = [sum(((pattern[y][x] & 1) << (7 - x)) for x in range(8)) for y in range(8)]
        high = [sum(((pattern[y][x] >> 1) << (7 - x)) for x in range(8)) for y in range(8)]
        return low + high

    lines = [
        f"; Generated by tools/convert-revg-bg.py from Revision G section {args.section}.",
        f"; {exact_tiles} 8x8 tiles, {merged} merged into similar ones, {len(patterns)} left in {slots_used} CHR slots",
        f"; (largest camera window {largest}), {len(metatile_list)} metatiles.",
        f"revg_bg_first_slot = ${FIRST_SLOT:02X}",
        f"revg_bg_view_rows = {len(rows)}",
        f"revg_bg_camera_columns = {camera_columns}",
        f"revg_bg_margin = {MARGIN}",
        f"revg_bg_swap_column = {swap}",
        "; Palettes (in the map bank): 0-3 up to the swap; 2 and 3 become the forest's from it.",
        "; " + " ".join("/".join(f"{c:02X}" for c in p) for p in palettes),
    ]
    banks = []

    def bank(comment):
        banks.append((comment, []))
        return banks[-1][1]

    def table(items, label, values):
        items.append((label, list(values)))

    # Bank 19: the visual map as one table per row (indexed by column), the slots of
    # each metatile's four tiles, and the attribute bytes per row (indexed by 32 px pair).
    items = bank("visual map, metatiles and attributes")
    for r in range(len(rows)):
        table(items, f"revg_bg_map_row{r}", (vmap[col][r] for col in range(width)))
    for q, name in enumerate(("tl", "tr", "bl", "br")):
        table(items, f"revg_bg_mt_{name}", (0 if meta[q] == BLANK else FIRST_SLOT + slot_of[meta[q]]
                                            for meta in metatile_list))
    for r in range(7):
        table(items, f"revg_bg_attr_row{r}", attributes[r::7])
    table(items, "revg_bg_palette", (c for p in palettes[:4] for c in [BLACK] + p))
    table(items, "revg_bg_palette_forest", (c for p in palettes[4:] for c in [BLACK] + p))
    # Bank 20: the tiles to upload when the camera enters each column, for each direction.
    items = bank("upload lists")
    for name, lists in (("right", right), ("left", left)):
        flat, starts = [], []
        for entries in lists:
            starts.append(len(flat))
            flat.extend(entries)
        starts.append(len(flat))
        table(items, f"revg_bg_{name}_start_lo", (s & 0xFF for s in starts))
        table(items, f"revg_bg_{name}_start_hi", (s >> 8 for s in starts))
        table(items, f"revg_bg_{name}_id_lo", (t & 0xFF for t in flat))
        table(items, f"revg_bg_{name}_id_hi", (t >> 8 for t in flat))
    # Banks 21 and up: 256 tiles each from $C010, then their slots from $D010.
    pattern_banks = (len(patterns) + 255) // 256
    for b in range(pattern_banks):
        ids = range(256 * b, min(len(patterns), 256 * b + 256))
        items = bank(f"tiles {ids[0]}-{ids[-1]}")
        table(items, None, [0xFF] * 15)
        table(items, "revg_bg_patterns" if b == 0 else None, (v for t in ids for v in chr_bytes(patterns[t])))
        table(items, None, [0xFF] * (0x1000 - 16 * len(ids)))
        table(items, "revg_bg_slots" if b == 0 else None, (FIRST_SLOT + slot_of[t] for t in ids))

    out = []
    for index, (comment, items) in enumerate(banks):
        number = FIRST_BANK + index
        out += [f"; Bank {number}: {comment}.", ".org $C000", f"    .byte bank_tag_base + {number}"]
        used = 1
        for label, values in items:
            used += len(values)
            if used > BANK_SIZE:
                raise SystemExit(f"bank {number} ({comment}) is over 8 KiB")
            if label:
                out.append(f"{label}:")
            for start in range(0, len(values), 32):
                out.append("    .byte " + ", ".join(f"${v:02X}" for v in values[start:start + 32]))
        out.append(f"    .res ${BANK_BASE + BANK_SIZE:04X} - *, $FF")
    lines += [
        f"revg_bg_map_bank = {FIRST_BANK}",
        f"revg_bg_list_bank = {FIRST_BANK + 1}",
        f"revg_bg_pattern_bank = {FIRST_BANK + 2}",
        f"revg_bg_bank_count = {len(banks)}",
        "; The data banks, placed by vrc7a-test.s in the padding from bank 19.",
        ".macro revg_bg_banks",
        *out,
        ".endmacro",
    ]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    sizes = " + ".join(str(sum(len(v) for _, v in items)) for _, items in banks[:2])
    print(f"{args.output}: {len(unique)} arcade tiles, {len(metatile_list)} metatiles, {exact_tiles} 8x8 tiles, "
          f"{merged} merged to {len(patterns)}, "
          f"{slots_used}/{SLOT_COUNT} slots (largest window {largest}), map and list banks {sizes} B, palettes "
          + " ".join("/".join(f"{c:02X}" for c in p) for p in palettes))

    if args.preview:
        from PIL import Image
        image = Image.new("RGB", (width * 16, len(rows) * 16))
        px = image.load()
        for col in range(width):
            for r, m in enumerate(vmap[col]):
                meta = metatile_list[m]
                options = [NES_RGB[BLACK]] + [NES_RGB[c] for c in palettes[meta[4]]]
                for q in range(4):
                    pattern = blank_pattern if meta[q] == BLANK else patterns[meta[q]]
                    ox, oy = col * 16 + (q & 1) * 8, r * 16 + (q >> 1) * 8
                    for y in range(8):
                        for x in range(8):
                            px[ox + x, oy + y] = options[pattern[y][x]]
        image.save(args.preview)
        print(args.preview)


if __name__ == "__main__":
    main()
