#!/usr/bin/env python3
"""Convert a JSON tile grid and placements to ca65 stage-bank data."""

import argparse
import json
import re
from pathlib import Path


TILES = {
    "sky": 0,
    "gravel": 1,
    "ledge": 2,
    "rock": 3,
    "oneway": 4,
    "ladder": 5,
    "hazard": 6,
}
ITEMS = {"armor": 1, "dagger": 2, "torch": 3, "lance": 4, "shield": 6}
PLATFORMS = {"obstacle": 1, "horizontal": 2, "vertical": 3}
PAGE_ROWS = 16
SOLID_TILE = TILES["rock"]


def int_in_range(value, minimum, maximum, field):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{field} must be an integer in {minimum}..{maximum}")
    return value


def encode_map(rows):
    if not isinstance(rows, list):
        raise ValueError("tiles must be an array of rows")
    height = int_in_range(len(rows), 1, 2 * PAGE_ROWS, "height")
    if not isinstance(rows[0], list) or len(rows[0]) not in (16, 32, 48):
        raise ValueError("width must be 16, 32 or 48 metatiles")
    width = len(rows[0])
    encoded_rows = []
    for y, row in enumerate(rows):
        if not isinstance(row, list) or len(row) != width:
            raise ValueError(f"tiles[{y}] must have {width} columns")
        encoded_row = []
        for x, tile in enumerate(row):
            if not isinstance(tile, str) or tile not in TILES:
                raise ValueError(f"tiles[{y}][{x}] is not a supported metatile: {tile!r}")
            encoded_row.append(TILES[tile])
        encoded_rows.append(encoded_row)

    columns = []
    for page_start in range(0, height, PAGE_ROWS):
        page = encoded_rows[page_start : page_start + PAGE_ROWS]
        for x in range(width):
            column = [row[x] for row in page]
            columns.append(column + [SOLID_TILE] * (PAGE_ROWS - len(column)))
    return width, height, columns


def encode_placements(entries, field, types, capacity, width, height):
    if not isinstance(entries, list) or len(entries) > capacity:
        raise ValueError(f"{field} must be an array of at most {capacity} entries")
    allowed_keys = {"type", "x", "y"}
    if field == "platforms":
        allowed_keys.add("span")
    records = []
    for index, entry in enumerate(entries):
        path = f"{field}[{index}]"
        check_fields(entry, allowed_keys, path)
        kind = entry.get("type")
        if not isinstance(kind, str) or kind not in types:
            raise ValueError(f"{path}.type is not supported: {kind!r}")
        x = int_in_range(entry.get("x"), 0, width * 16 - 1, f"{path}.x")
        y = int_in_range(entry.get("y"), 0, height * 16 - 1, f"{path}.y")
        record = [types[kind], x & 0xFF, x >> 8, y & 0xFF, y >> 8]
        if field == "platforms":
            maximum_span = {
                "obstacle": 0,
                "horizontal": min(255, width * 16 - 1 - x),
                "vertical": min(255, y),
            }[kind]
            minimum_span = 0 if kind == "obstacle" else 1
            record.append(int_in_range(entry.get("span", 0), minimum_span, maximum_span, f"{path}.span"))
        records.append(record)
    return records


def byte_line(values):
    return "    .byte " + ", ".join(f"${value:02x}" for value in values)


def check_fields(entry, allowed_keys, field):
    if not isinstance(entry, dict):
        raise ValueError(f"{field} must be an object")
    unknown_keys = sorted(entry.keys() - allowed_keys)
    if unknown_keys:
        raise ValueError(f"{field} has unexpected fields: {', '.join(unknown_keys)}")


def enemy_position(entry, field, width, height, extra_keys=()):
    check_fields(entry, {"x", "y", *extra_keys}, field)
    # X=0 is reserved for absent fixed enemies and turrets.
    x = int_in_range(entry.get("x"), 1, width * 16 - 1, f"{field}.x")
    y = int_in_range(entry.get("y"), 0, height * 16 - 1, f"{field}.y")
    return x, y


def encode_enemies(stage, width, height):
    enemies = stage.get("enemies", {})
    check_fields(enemies, {"fixed", "pot", "scheduler"}, "enemies")
    fixed = (0, 0)
    pot = (0, 0)
    pot_drop = 0
    if "fixed" in enemies:
        fixed = enemy_position(enemies["fixed"], "enemies.fixed", width, height)
    if "pot" in enemies:
        entry = enemies["pot"]
        pot = enemy_position(entry, "enemies.pot", width, height, ("drop",))
        drop = entry.get("drop")
        if not isinstance(drop, str) or drop not in ITEMS:
            raise ValueError(f"enemies.pot.drop is not supported: {drop!r}")
        pot_drop = ITEMS[drop]

    page = 0xFF
    turret = (0, 0)
    zombie = (0, 0)
    if "scheduler" in enemies:
        scheduler = enemies["scheduler"]
        field = "enemies.scheduler"
        check_fields(scheduler, {"axis", "page", "zombie", "turret"}, field)
        axis = scheduler.get("axis")
        if axis == "horizontal":
            max_page = (width * 16 - 256) // 256
        elif axis == "vertical":
            max_page = (height * 16 - 1) // 256
        else:
            raise ValueError(f"{field}.axis must be horizontal or vertical")
        page = int_in_range(scheduler.get("page"), 0, max_page, f"{field}.page")
        zombie = enemy_position(scheduler.get("zombie"), f"{field}.zombie", width, height)
        if "turret" in scheduler:
            turret = enemy_position(scheduler["turret"], f"{field}.turret", width, height)

    return {
        "enemy1_x": fixed[0], "enemy1_y": fixed[1],
        "pot_x": pot[0], "pot_y": pot[1], "pot_drop": pot_drop,
        "sched_page": page,
        "turret_x": turret[0], "turret_y": turret[1],
        "zombie_x": zombie[0], "zombie_y": zombie[1],
    }


def convert(stage, prefix):
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", prefix):
        raise ValueError("prefix must be a ca65 identifier")
    if not isinstance(stage, dict):
        raise ValueError("stage must be a JSON object")
    allowed_keys = {"tiles", "palette_pairs", "items", "platforms", "enemies"}
    unknown_keys = sorted(stage.keys() - allowed_keys)
    if unknown_keys:
        raise ValueError(f"unexpected fields: {', '.join(unknown_keys)}")

    width, height, columns = encode_map(stage.get("tiles"))
    pairs = stage.get("palette_pairs", [0] * (width // 2))
    if not isinstance(pairs, list) or len(pairs) != width // 2:
        raise ValueError(f"palette_pairs must have {width // 2} entries")
    palettes = [
        int_in_range(value, 0, 3, f"palette_pairs[{index}]")
        for index, value in enumerate(pairs)
        for _ in range(2)
    ]
    # Item slot 3 is reserved for the boss's key.
    items = encode_placements(stage.get("items", []), "items", ITEMS, 3, width, height)
    platforms = encode_placements(stage.get("platforms", []), "platforms", PLATFORMS, 4, width, height)
    enemies = encode_enemies(stage, width, height)

    lines = [
        "; Generated by tools/convert-stage-map.py.",
        f"{prefix}_width = {width}",
        f"{prefix}_height = {height}",
        f"{prefix}_item_count = {len(items)}",
        f"{prefix}_plat_count = {len(platforms)}",
        *(f"{prefix}_{name} = ${value:04x}" for name, value in enemies.items()),
        "",
        f"{prefix}_map:",
        f'.assert {prefix}_map = $8100, lderror, "Stage map must start at $8100"',
        *(byte_line(column) for column in columns),
        "",
        f"{prefix}_palettes:",
        *(byte_line(palettes[offset : offset + 16]) for offset in range(0, width, 16)),
        "",
        f"{prefix}_items:",
        *(byte_line(item) for item in items),
        "",
        f"{prefix}_plats:",
        *(byte_line(platform) for platform in platforms),
    ]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="stage JSON")
    parser.add_argument("output", type=Path, help="output ca65 include")
    parser.add_argument("--prefix", required=True, help="prefix for generated ca65 symbols")
    args = parser.parse_args()
    try:
        if args.input.resolve() == args.output.resolve():
            raise ValueError("input and output must be different files")
        stage = json.loads(args.input.read_text(encoding="utf-8-sig"))
        asm = convert(stage, args.prefix)
        args.output.write_text(asm, encoding="utf-8", newline="\n")
    except (OSError, ValueError) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")


if __name__ == "__main__":
    main()
