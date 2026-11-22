"""Dump the Revision G course maps, page table and object placements from the 6809 ROMs.

Reads MAME's makaimurg set and writes JSON (and, with --render, PNG previews) to
build/revg-stages/. The table layouts are described in docs/revision-g-stage-data.md.
"""

import argparse
import hashlib
import json
from pathlib import Path
from zipfile import BadZipFile, ZipFile

ROM_SHA1 = {
    "mj04g.bin": "07f7cf788810a1425016e016ce3579adb3253ac7",
    "mj03g.bin": "23a0a17d0abc4b084ffeba90266ef455361771cc",
    "mj05g.bin": "7694a981a6196d77fd2279fc34042b4cfb40c054",
}
BG_ROMS = ["gg11.bin", "gg10.bin", "gg9.bin", "gg8.bin", "gg7.bin", "gg6.bin"]

GRID_COLUMNS = 32          # bank2:$4000, one screen ID per 256x256 px page
GRID_ROWS = 11             # rows $4000-$415F; row 0 is the bottom of the world
PAGE_COUNT = GRID_COLUMNS * GRID_ROWS
SECTIONS = 7
SCHEDULER_TYPES = 16       # enemy scheduler slots $0690 + 32 * type
DIFFICULTY_KEYS = (0x00, 0x10, 0x20, 0x30)  # $9A & $30

# Names from MAME snapshots of each type (tools/mame/capture-revg-types.py) and, for the bosses,
# the section boss tables; see docs/revision-g-stage-data.md for the evidence and open cases.
ENEMY_NAMES = {
    0x00: "red-eyed flier from trees and walls (Woody Pig?)",
    0x01: "crow / bat",
    0x02: "small blue imp (Petite Devil?)",
    0x03: "Flying Knight",
    0x04: "purple winged flier from windows",
    0x05: "Red Arremer",
    0x06: "Satan",
    0x07: "magician? (not in any table, spawned by $EBCB)",
    0x08: "Unicorn",
    0x09: "floor skull",
    0x0A: "Big Man",
    0x0B: "zombie",
    0x0C: "Dragon",
    0x0D: "Astaroth",
    0x0E: "fire column on the bridge",
    0x0F: "unused",
    0x10: "unused",
    0x11: "man-eating plant",
    0x12: "grey stone pillar",
}
SUPPORT_NAMES = {
    0x00: "touch trigger (variant 0 hidden armour, 1 hidden 10000 points, 2 coin 200, 3 coin 500)",
    0x01: "tombstone / rock",
    0x02: "moving platform on a path",
    0x03: "path platform (variant 0) / falling platform (variant 1)",
    0x04: "moving platform",
    0x05: "ball platform that reacts to the player",
}


class Rom:
    def __init__(self, images):
        low = images["mj04g.bin"]
        banked = images["mj05g.bin"]
        self.fixed = low[0x2000:] + images["mj03g.bin"]
        self.banks = [banked[i * 0x2000:(i + 1) * 0x2000] for i in range(4)] + [low[:0x2000]]

    def byte(self, region, address):
        if region == "fixed":
            return self.fixed[address - 0x6000]
        return self.banks[region][address - 0x4000]

    def bytes(self, region, address, count):
        return bytes(self.byte(region, address + i) for i in range(count))

    def word(self, region, address):
        return self.byte(region, address) << 8 | self.byte(region, address + 1)


def s16(value):
    return value - 0x10000 if value & 0x8000 else value


def s8(value):
    return value - 0x100 if value & 0x80 else value


def hexa(value, width=4):
    return f"${value:0{width}X}"


def screen_ids(rom):
    return [[rom.byte(2, 0x4000 + row * GRID_COLUMNS + col) for col in range(GRID_COLUMNS)]
            for row in range(GRID_ROWS)]


def block_table(rom, screen):
    # $6309: bank2 $4200 + 2 * screen -> bank1 record table of 8-byte 32x32 blocks.
    return rom.word(2, 0x4200 + 2 * screen)


def screen_tiles(rom, screen):
    """Return a 16x16 list [row_from_bottom][column] of (code, attr) for one page.

    $6328-$6373: bank0 $4000 + 64 * screen holds 8 columns of 8 block numbers, each column
    listed bottom to top. A block's 8 bytes are tile numbers for lower-left, upper-left,
    lower-right, upper-right, followed by the four attributes in the same order.
    """
    table = block_table(rom, screen)
    tiles = [[None] * 16 for _ in range(16)]
    for col in range(8):
        for row in range(8):
            block = rom.byte(0, 0x4000 + 64 * screen + 8 * col + row)
            record = rom.bytes(1, table + 8 * block, 8)
            for index, (dx, dy) in enumerate(((0, 0), (0, 1), (1, 0), (1, 1))):
                tiles[2 * row + dy][2 * col + dx] = (record[index], record[4 + index])
    return tiles


def collision_tables(rom):
    # bank4 bases $448E (ground), $458E (right wall), $468E (left wall), indexed by signed tile.
    result = {}
    for name, base in (("ground", 0x448E), ("right_wall", 0x458E), ("left_wall", 0x468E)):
        result[name] = [rom.byte(4, base + s8(tile)) for tile in range(256)]
    # $9174: a tile with attribute bits $C0 clear and number $40-$7D is a ladder. $918F-$91A9
    # move X to the ladder's centre line: the tile's left edge when (number - $40) is odd
    # (right half), its right edge when even (left half).
    result["ladder"] = [0x40 <= tile <= 0x7D for tile in range(256)]
    return result


def page_entries(rom):
    """bank2 $4300 + 4 * page, read by $8DE7 when a page is entered.

    Byte 0 is loaded into A and overwritten before use (and is 0 for every page).
    Byte 1 goes to $DE, the palette set the IRQ copies in ($6656); 0 keeps the current one.
    Byte 2 is the stage song: not sent when the page number decreases (left $FF, down $E0),
    and only if it is above the current song $BD. With bit 7 set it is sent only by the
    direction-0 calls made at section start.
    Byte 3 is the enemy group for $0670.
    """
    entries = []
    for page in range(PAGE_COUNT):
        unused, palette, music, group = rom.bytes(2, 0x4300 + 4 * page, 4)
        entries.append({
            "page": page,
            "unused": unused,
            "palette": palette,
            "music": music & 0x7F,
            "music_start_only": bool(music & 0x80),
            "group": group,
        })
    return entries


def enemy_groups(rom):
    # $E8D4: bank2 $4FE0 + 2 * group -> count, then count x 2 bytes split into nibbles.
    first = rom.word(2, 0x4FE0)
    groups = []
    for index in range((first - 0x4FE0) // 2):
        pointer = rom.word(2, 0x4FE0 + 2 * index)
        count = rom.byte(2, pointer)
        entries = []
        for item in range(count):
            a, b = rom.bytes(2, pointer + 1 + 2 * item, 2)
            entries.append({
                "type": a >> 4,
                "name": ENEMY_NAMES.get(a >> 4),
                "pattern": (a & 0x0F) << 4 | (b & 0x0F),
                "flags": b >> 4,
            })
        groups.append({"group": index, "address": hexa(pointer), "entries": entries})
    return groups


def enemy_patterns(rom, groups):
    # $E9DD: bank2 $5188 + 2 * type -> pattern pointers; pattern = count, then records.
    tables = [rom.word(2, 0x5188 + 2 * kind) for kind in range(SCHEDULER_TYPES)]
    data_start = min(rom.word(2, tables[0] + 2 * i) for i in range((tables[1] - tables[0]) // 2))
    used = {}
    for group in groups:
        for entry in group["entries"]:
            used.setdefault(entry["type"], set()).add(entry["pattern"])
    patterns = []
    for kind, table in enumerate(tables):
        end = tables[kind + 1] if kind + 1 < SCHEDULER_TYPES else data_start
        available = (end - table) // 2
        for index in range(available):
            pointer = rom.word(2, table + 2 * index)
            count = rom.byte(2, pointer)
            records = []
            if kind not in (0x07, 0x0E, 0x0F):  # $EBCB uses the scheduler position; $EA7D spawns nothing
                for item in range(count):
                    x, y = rom.word(2, pointer + 1 + 6 * item), rom.word(2, pointer + 3 + 6 * item)
                    variant, wait = rom.bytes(2, pointer + 5 + 6 * item, 2)
                    records.append({"x": s16(x), "y": s16(y), "variant": variant, "wait": wait})
            patterns.append({
                "type": kind, "name": ENEMY_NAMES.get(kind), "pattern": index, "address": hexa(pointer), "count": count,
                "referenced": index in used.get(kind, ()), "records": records,
            })
    return tables, patterns


def placement_list(rom, pointer, names):
    count = rom.byte(2, pointer)
    records = []
    for item in range(count):
        kind, variant = rom.bytes(2, pointer + 1 + 6 * item, 2)
        x, y = rom.word(2, pointer + 3 + 6 * item), rom.word(2, pointer + 5 + 6 * item)
        records.append({"type": kind, "name": names.get(kind), "variant": variant, "x": x, "y": y})
    return records


def section_settings(rom):
    sections = []
    for section in range(SECTIONS):
        def position(table):
            page, x, y = rom.word("fixed", table + 4 * section), rom.byte("fixed", table + 4 * section + 2), \
                rom.byte("fixed", table + 4 * section + 3)
            index = page - 0x4000
            return {"page": index, "x": (index & 0x1F) * 256 + x, "y": (index >> 5) * 256 + y}

        x_min, x_max, y_max, y_min = rom.bytes("fixed", 0x6CAC + 4 * section, 4)
        enemy_list = rom.word(2, 0x4B00 + 4 * section)
        support_list = rom.word(2, 0x4B02 + 4 * section)
        limits_pointer = rom.word(4, 0x4240 + 2 * section)
        limits = {}
        for key in DIFFICULTY_KEYS:
            limits[hexa(key, 2)] = list(rom.bytes(4, limits_pointer + key, 16))
        sections.append({
            "section": section,
            "start": position(0x6CEE),
            "checkpoint": position(0x6D0A),
            "checkpoint_camera_x": rom.word("fixed", 0xECE4 + 2 * section),
            "camera": {"x_min": x_min * 256, "x_max": x_max * 256, "y_min": y_min * 256, "y_max": y_max * 256},
            "pages": {"columns": [x_min, x_max], "rows": [y_min, y_max]},
            "boss_count": rom.byte("fixed", 0x6ED1 + section),
            "timer_start": rom.word("fixed", 0x6EB5 + 2 * section),
            "timer_checkpoint": rom.word("fixed", 0x6EC3 + 2 * section),
            "one_shot_group_base": rom.byte("fixed", 0xEA84 + section),
            "type_limits_address": hexa(limits_pointer),
            "type_limits": limits,
            "enemy_list_address": hexa(enemy_list),
            "enemy_list": placement_list(rom, enemy_list, ENEMY_NAMES),
            "support_list_address": hexa(support_list),
            "support_list": placement_list(rom, support_list, SUPPORT_NAMES),
        })
    return sections


def section_map(rom, ids, section):
    """Return 16 px tile rows (top row first) covering the section's camera pages."""
    c0, c1 = section["pages"]["columns"]
    r0, r1 = section["pages"]["rows"]
    pages = {}
    rows = []
    for page_row in range(r1, r0 - 1, -1):
        for tile_row in range(15, -1, -1):
            row = []
            for page_col in range(c0, c1 + 1):
                screen = ids[page_row][page_col]
                if screen not in pages:
                    pages[screen] = screen_tiles(rom, screen)
                for tile_col in range(16):
                    code, attr = pages[screen][tile_row][tile_col]
                    row.append([code | (attr & 0xC0) << 2, attr & 0x3F])
            rows.append(row)
    return {
        "origin": {"x": c0 * 256, "y": r0 * 256},
        "width": (c1 - c0 + 1) * 16,
        "height": (r1 - r0 + 1) * 16,
        "screens": [[ids[r][c] for c in range(c0, c1 + 1)] for r in range(r1, r0 - 1, -1)],
        "tiles": rows,
    }


def palette_sets(rom, count):
    # $6656: palette n (1-based) is 128 bytes at bank3 $4000 + 128 * (n - 1):
    # 64 RRRRGGGG bytes for $3800-$383F, then 64 BBBBxxxx bytes for $3900-$393F.
    result = {}
    for number in range(1, count + 1):
        data = rom.bytes(3, 0x4000 + 128 * (number - 1), 128)
        colors = []
        for i in range(64):
            r, g, b = data[i] >> 4, data[i] & 15, data[64 + i] >> 4
            colors.append((r * 17, g * 17, b * 17))
        result[number] = colors
    return result


def render(path, tilemap, page_palettes, collision, gfx, overlay):
    """Draw a section; page_palettes maps (page column, page row from the top) to 8 palettes."""
    from PIL import Image

    width, height = tilemap["width"] * 16, tilemap["height"] * 16
    image = Image.new("RGB", (width, height))
    pixels = image.load()
    cache = {}
    for ty, row in enumerate(tilemap["tiles"]):
        for tx, (code, attr) in enumerate(row):
            if code not in cache:
                cache[code] = gfx(code)
            tile = cache[code]
            colors = page_palettes[(tx >> 4, ty >> 4)][attr & 7]
            for y in range(16):
                sy = 15 - y if attr & 0x20 else y
                for x in range(16):
                    sx = 15 - x if attr & 0x10 else x
                    pixels[tx * 16 + x, ty * 16 + y] = colors[tile[sy][sx]]
            if overlay and code < 256:
                height_value = collision["ground"][code]
                if height_value:
                    # Ground height counts up from the tile's bottom edge.
                    line = ty * 16 + 15 - min(height_value, 15)
                    for x in range(16):
                        pixels[tx * 16 + x, line] = (255, 0, 255)
    image.save(path)


def tile_decoder(images):
    tiles_rom = b"".join(images[name] for name in BG_ROMS)

    def decode(code):
        base = (code & 0x3FF) * 32
        planes = [tiles_rom[0x10000 + base:0x10000 + base + 32], tiles_rom[0x8000 + base:0x8000 + base + 32],
                  tiles_rom[base:base + 32]]
        px = [[0] * 16 for _ in range(16)]
        for y in range(16):
            for half in range(2):
                b0, b1, b2 = (plane[half * 16 + y] for plane in planes)
                for x in range(8):
                    bit = 7 - x
                    px[y][half * 8 + x] = ((b0 >> bit) & 1) << 2 | ((b1 >> bit) & 1) << 1 | ((b2 >> bit) & 1)
        return px

    return decode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, default=Path("reference/makaimurg.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/revg-stages"))
    parser.add_argument("--render", action="store_true", help="also write PNG previews of each section")
    args = parser.parse_args()

    names = list(ROM_SHA1) + (BG_ROMS if args.render else [])
    try:
        with ZipFile(args.rom) as rom_zip:
            images = {name: rom_zip.read(name) for name in names}
    except (BadZipFile, KeyError, OSError) as error:
        parser.error(str(error))
    for name, expected in ROM_SHA1.items():
        if hashlib.sha1(images[name]).hexdigest() != expected:
            parser.error(f"{name} does not match MAME's makaimurg manifest")

    rom = Rom(images)
    ids = screen_ids(rom)
    pages = page_entries(rom)
    groups = enemy_groups(rom)
    type_tables, patterns = enemy_patterns(rom, groups)
    sections = section_settings(rom)
    collision = collision_tables(rom)

    used_screens = sorted({screen for row in ids for screen in row})
    screens = {}
    for screen in used_screens:
        tiles = screen_tiles(rom, screen)
        screens[hexa(screen, 2)] = {
            "block_table": hexa(block_table(rom, screen)),
            "tiles": [[[code | (attr & 0xC0) << 2, attr & 0x3F] for code, attr in tiles[row]]
                      for row in range(15, -1, -1)],
        }

    args.output.mkdir(parents=True, exist_ok=True)

    def write(name, value):
        path = args.output / name
        path.write_text(json.dumps(value, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
        print(path)

    write("world.json", {
        "grid": {"columns": GRID_COLUMNS, "rows": GRID_ROWS, "row_order": "bottom_to_top",
                 "screen_ids": ids},
        "pages": pages,
        "collision": collision,
        "screens": screens,
    })
    write("enemies.json", {
        "enemy_types": {hexa(k, 2): v for k, v in ENEMY_NAMES.items()},
        "support_types": {hexa(k, 2): v for k, v in SUPPORT_NAMES.items()},
        "type_tables": [hexa(t) for t in type_tables],
        "groups": groups,
        "patterns": patterns,
    })
    write("sections.json", sections)

    palette_count = max(p["palette"] for p in pages)
    palettes = palette_sets(rom, palette_count) if args.render else None
    gfx = tile_decoder(images) if args.render else None
    for section in sections:
        tilemap = section_map(rom, ids, section)
        write(f"section{section['section']}-map.json", tilemap)
        if args.render:
            # Each page uses its own palette entry; a zero entry keeps the section start's set.
            start = section["start"]["page"]
            default = pages[start]["palette"] or 1
            c0, c1 = section["pages"]["columns"]
            r0, r1 = section["pages"]["rows"]
            page_palettes = {}
            for row in range(r0, r1 + 1):
                for col in range(c0, c1 + 1):
                    number = pages[row * GRID_COLUMNS + col]["palette"] or default
                    page_palettes[(col - c0, r1 - row)] = [palettes[number][8 * i:8 * i + 8] for i in range(8)]
            for overlay in (False, True):
                suffix = "-ground" if overlay else ""
                path = args.output / f"section{section['section']}{suffix}.png"
                render(path, tilemap, page_palettes, collision, gfx, overlay)
                print(path)


if __name__ == "__main__":
    main()
