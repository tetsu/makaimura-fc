#!/usr/bin/env python3
"""Pack palette-fitted PNG art into ca65 data for a NES pattern table."""

import argparse
import json
import re
from collections import Counter
from pathlib import Path

from PIL import Image


TILE_WIDTH = 8
TABLE_TILES = 256
TILE_BYTES = 16
REGION_SIZES = {"bg": (16, 16), "obj": (8, 16)}
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def validate_grid(grid, rows, columns, maximum, label):
    if not isinstance(grid, list) or len(grid) != rows:
        raise ValueError(f"{label} must have {rows} rows")
    for index, row in enumerate(grid):
        if not isinstance(row, list) or len(row) != columns:
            raise ValueError(f"{label}[{index}] must have {columns} entries")
        if any(type(value) is not int or not 0 <= value <= maximum for value in row):
            raise ValueError(f"{label}[{index}] entries must be integers in 0..{maximum}")


def validate_image(image, metadata):
    if not isinstance(metadata, dict) or metadata.get("mode") not in ("bg", "obj"):
        raise ValueError("palette metadata must have mode bg or obj")
    mode = metadata["mode"]
    width, height = REGION_SIZES[mode]
    if metadata.get("region_size") != [width, height]:
        raise ValueError(f"{mode} metadata must specify {width}x{height} regions")
    if image.mode != "P" or image.width % width or image.height % height:
        raise ValueError(f"{mode} input must be an indexed PNG sized in {width}x{height} regions")
    transparency = image.info.get("transparency")
    if (mode == "bg" and transparency is not None) or (mode == "obj" and transparency != 0):
        raise ValueError("BG must be opaque and OBJ must use PNG index 0 for transparency")
    palettes = metadata.get("palettes")
    validate_grid(palettes, 4, 4, 63, "palettes")
    if mode == "bg" and any(palette[0] != palettes[0][0] for palette in palettes):
        raise ValueError("all BG palettes must share color zero")
    validate_grid(metadata.get("palette_ids"), image.height // height, image.width // width, 3, "palette_ids")


def read_region(pixels, x, y, mode, palette_id):
    width, height = REGION_SIZES[mode]
    base_index = 4 * palette_id
    first_opaque = base_index + (mode == "obj")
    slots = []
    for row in range(y, y + height):
        for column in range(x, x + width):
            index = pixels[column, row]
            if mode == "obj" and index == 0:
                slots.append(0)
            elif first_opaque <= index < base_index + 4:
                slots.append(index & 3)
            else:
                raise ValueError(f"pixel ({column}, {row}) does not match palette {palette_id}")
    return slots


def encode_tile(slots, stride, x, y):
    """Encode one 8x8 tile with bit 7 at the left edge of each row."""
    low = bytearray()
    high = bytearray()
    for row in range(y, y + TILE_WIDTH):
        low_bits = high_bits = 0
        for slot in slots[row * stride + x : row * stride + x + TILE_WIDTH]:
            low_bits = (low_bits << 1) | (slot & 1)
            high_bits = (high_bits << 1) | (slot >> 1)
        low.append(low_bits)
        high.append(high_bits)
    return bytes(low + high)


def pack(image, metadata, first_tile):
    validate_image(image, metadata)
    mode = metadata["mode"]
    if not 0 <= first_tile < TABLE_TILES or (mode == "obj" and first_tile % 2):
        raise ValueError("first tile must be in 0..255 and even for OBJ")
    width, height = REGION_SIZES[mode]
    pixels = image.load()
    chr_data = bytearray()
    references = []
    tile_ids = {}

    def intern(pattern):
        if pattern not in tile_ids:
            tile = first_tile + len(chr_data) // TILE_BYTES
            if tile + len(pattern) // TILE_BYTES > TABLE_TILES:
                raise ValueError(f"{mode.upper()} patterns exceed the 4 KiB table from tile {first_tile}")
            tile_ids[pattern] = tile
            chr_data.extend(pattern)
        return tile_ids[pattern]

    for row, palette_row in enumerate(metadata["palette_ids"]):
        for column, palette_id in enumerate(palette_row):
            slots = read_region(pixels, column * width, row * height, mode, palette_id)
            if mode == "bg":
                tiles = [
                    intern(encode_tile(slots, width, x, y))
                    for y in (0, 8)
                    for x in (0, 8)
                ]
                references.append((*tiles, palette_id))
            elif any(slots):
                pair = encode_tile(slots, width, 0, 0) + encode_tile(slots, width, 0, 8)
                references.append((intern(pair) | 1, palette_id))
            else:
                references.append((0, 0xFF))

    return chr_data, references


def byte_line(values):
    return "    .byte " + ", ".join(f"${value:02x}" for value in values)


def pack_poses(manifest, palette_ids, references):
    if not isinstance(manifest, dict) or not 1 <= len(manifest) <= 255:
        raise ValueError("pose manifest must be an object containing 1..255 named poses")
    rows, columns = len(palette_ids), len(palette_ids[0])
    poses = {}
    for name, parts in manifest.items():
        if not IDENTIFIER.fullmatch(name) or not isinstance(parts, list):
            raise ValueError(f"pose {name!r} must have an identifier name and an array of parts")
        records = []
        scanlines = Counter()
        for index, part in enumerate(parts):
            label = f"pose {name!r} part {index}"
            if not isinstance(part, dict) or part.keys() - {"region", "dx", "dy", "flip_x", "flip_y"}:
                raise ValueError(f"{label} must specify region, dx, dy and optional flip_x, flip_y")
            region = part.get("region")
            if (not isinstance(region, list) or len(region) != 2
                    or any(type(value) is not int for value in region)
                    or not (0 <= region[0] < columns and 0 <= region[1] < rows)):
                raise ValueError(f"{label} region must be [column, row] inside the OBJ sheet")
            dx, dy = part.get("dx"), part.get("dy")
            if any(type(value) is not int or not -128 <= value <= 127 for value in (dx, dy)):
                raise ValueError(f"{label} dx and dy must be integers in -128..127")
            flip_x, flip_y = part.get("flip_x", False), part.get("flip_y", False)
            if type(flip_x) is not bool or type(flip_y) is not bool:
                raise ValueError(f"{label} flip_x and flip_y must be booleans")

            column, row = region
            tile, palette = references[row * columns + column]
            if palette != 0xFF:
                attributes = palette | (flip_x << 6) | (flip_y << 7)
                records.append((dx & 0xFF, dy & 0xFF, tile, attributes))
                scanlines.update(range(dy, dy + REGION_SIZES["obj"][1]))

        if len(records) > 64:
            raise ValueError(f"pose {name!r} exceeds 64 OAM entries")
        for scanline, count in scanlines.items():
            if count > 8:
                raise ValueError(f"pose {name!r} exceeds 8 sprites on relative scanline {scanline}")
        poses[name] = records
    return poses


def pose_assembly(prefix, poses):
    yield f"{prefix}_pose_count = {len(poses)}"
    yield f"{prefix}_poses:"
    for name in poses:
        yield f"    .addr {prefix}_pose_{name}"
    for name, records in poses.items():
        yield f"{prefix}_pose_{name}:"
        yield byte_line([len(records)])
        yield from map(byte_line, records)
    yield ""


def to_assembly(prefix, metadata, first_tile, chr_data, references):
    mode = metadata["mode"]
    ppu_base = 0x1000 if mode == "obj" else 0
    palette_ids = metadata["palette_ids"]
    return "\n".join([
        "; Generated by pack-nes-chr.py.",
        f"{prefix}_chr_vram = ${ppu_base + first_tile * TILE_BYTES:04x}",
        f"{prefix}_chr_size = ${len(chr_data):04x}",
        f"{prefix}_region_columns = {len(palette_ids[0])}",
        f"{prefix}_region_rows = {len(palette_ids)}",
        "",
        f"{prefix}_chr:",
        *(byte_line(chr_data[index : index + 16]) for index in range(0, len(chr_data), 16)),
        f"{prefix}_palette:",
        byte_line([color for palette in metadata["palettes"] for color in palette]),
        f"{prefix}_regions:",
        *(byte_line(row) for row in references),
        "",
    ])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="indexed PNG from fit-nes-palettes.py")
    parser.add_argument("palette_json", type=Path, help="matching palette metadata")
    parser.add_argument("output", type=Path, help="output ca65 include")
    parser.add_argument("--prefix", required=True, help="prefix for generated ca65 symbols")
    parser.add_argument("--first-tile", type=lambda value: int(value, 0), default=0,
                        help="first available tile number within the pattern table")
    parser.add_argument("--poses", type=Path, help="OBJ pose manifest in JSON")
    args = parser.parse_args()
    if not IDENTIFIER.fullmatch(args.prefix):
        parser.error("--prefix must be a ca65 identifier")
    inputs = [args.image, args.palette_json]
    if args.poses is not None:
        inputs.append(args.poses)
    if any(args.output.resolve() == path.resolve() for path in inputs):
        parser.error("output must be different from the inputs")

    try:
        metadata = json.loads(args.palette_json.read_text(encoding="utf-8-sig"))
        with Image.open(args.image, formats=["PNG"]) as image:
            chr_data, references = pack(image, metadata, args.first_tile)
        asm = to_assembly(args.prefix, metadata, args.first_tile, chr_data, references)
        if args.poses is not None:
            if metadata["mode"] != "obj":
                raise ValueError("--poses requires OBJ input")
            manifest = json.loads(args.poses.read_text(encoding="utf-8-sig"))
            poses = pack_poses(manifest, metadata["palette_ids"], references)
            asm += "\n" + "\n".join(pose_assembly(args.prefix, poses))
        args.output.write_text(asm, encoding="utf-8", newline="\n")
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
