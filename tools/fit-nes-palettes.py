#!/usr/bin/env python3
"""Fit NES palettes to authored PNG art and export local 2-bit indices."""

import argparse
import json
from collections import Counter
from pathlib import Path

from PIL import Image


# Use one canonical black and skip the alternate black and unsafe $0D codes.
COLOR_CODES = tuple(code for code in range(64) if code % 16 < 13 or code == 0x0F)
PALETTE_COUNT = 4
REGION_SIZES = {"bg": (16, 16), "obj": (8, 16)}


def color_distance(left, right):
    return sum(weight * (a - b) ** 2 for a, b, weight in zip(left, right, (2, 4, 1)))


def region_pixels(pixels, box):
    left, top, right, bottom = box
    for y in range(top, bottom):
        for x in range(left, right):
            yield pixels[x, y]


def read_regions(image, mode):
    width, height = REGION_SIZES[mode]
    if not image.width or not image.height or image.width % width or image.height % height:
        raise ValueError(f"{mode} image dimensions must be multiples of {width}x{height}")

    regions = []
    pixels = image.load()
    for y in range(0, image.height, height):
        for x in range(0, image.width, width):
            box = (x, y, x + width, y + height)
            histogram = Counter()
            for red, green, blue, alpha in region_pixels(pixels, box):
                if alpha == 255:
                    histogram[red, green, blue] += 1
                elif mode == "bg" or alpha != 0:
                    raise ValueError(f"invalid alpha in {mode} region at ({x}, {y})")
            regions.append(tuple(sorted(histogram.items())))
    return regions


def region_candidate(histogram, distances, backdrop, candidates):
    # If the independently nearest colors fit, this region needs no compromise.
    nearest_codes = {
        min(distances[rgb], key=distances[rgb].__getitem__)
        for rgb, _ in histogram
    } - {backdrop}
    if len(nearest_codes) <= 3:
        chosen = sorted(nearest_codes) or [candidates[0]]
        return tuple(chosen + [chosen[0]] * (3 - len(chosen)))

    # Build a regional candidate by greedily reducing its weighted pixel error.
    errors = {
        rgb: distances[rgb][backdrop] if backdrop is not None else float("inf")
        for rgb, _ in histogram
    }
    chosen = []
    available = list(candidates)
    for _ in range(3):
        code = min(
            available,
            key=lambda candidate: sum(
                count * min(errors[rgb], distances[rgb][candidate])
                for rgb, count in histogram
            ),
        )
        chosen.append(code)
        available.remove(code)
        for rgb, _ in histogram:
            errors[rgb] = min(errors[rgb], distances[rgb][code])
    return tuple(sorted(chosen))


def fit_palettes(regions, rgb_table, backdrop):
    frequencies = Counter(region for region in regions if region)
    distances = {
        rgb: {code: color_distance(rgb, rgb_table[code]) for code in COLOR_CODES}
        for rgb in {rgb for region in frequencies for rgb, _ in region}
    }
    color_codes = tuple(code for code in COLOR_CODES if code != backdrop)
    candidates = sorted({
        region_candidate(histogram, distances, backdrop, color_codes)
        for histogram in frequencies
    })

    # Select at most four regional candidates to minimize total image error.
    # Track each distinct histogram once, weighted by its number of regions.
    def palette_error(palette, histogram):
        codes = (backdrop, *palette) if backdrop is not None else palette
        return sum(
            count * min(distances[rgb][code] for code in codes)
            for rgb, count in histogram
        )

    errors = {
        palette: [palette_error(palette, histogram) for histogram in frequencies]
        for palette in candidates
    }
    weights = list(frequencies.values())
    best_errors = [float("inf")] * len(weights)
    selected = []
    while candidates and len(selected) < PALETTE_COUNT:
        palette = min(
            candidates,
            key=lambda candidate: sum(
                weight * min(old, new)
                for weight, old, new in zip(weights, best_errors, errors[candidate])
            ),
        )
        combined = [min(old, new) for old, new in zip(best_errors, errors[palette])]
        if combined == best_errors:
            break
        selected.append(palette)
        candidates.remove(palette)
        best_errors = combined

    if not selected:
        selected.append((0x0F, 0x0F, 0x0F))
    assignments = {
        histogram: min(range(len(selected)), key=lambda index: errors[selected[index]][offset])
        for offset, histogram in enumerate(frequencies)
    }
    selected.extend([selected[0]] * (PALETTE_COUNT - len(selected)))
    return selected, [assignments.get(region, 0) for region in regions]


def convert(image, rgb_table, mode, backdrop):
    regions = read_regions(image, mode)
    palettes, assignments = fit_palettes(regions, rgb_table, backdrop)
    palettes = [[0x0F if backdrop is None else backdrop, *palette] for palette in palettes]
    width, height = REGION_SIZES[mode]
    columns = image.width // width
    indexed = Image.new("P", image.size)
    indexed.putpalette([
        channel
        for palette in palettes
        for code in palette
        for channel in rgb_table[code]
    ])

    pixels = image.load()
    start_slot = 0 if mode == "bg" else 1
    for index, (histogram, palette_id) in enumerate(zip(regions, assignments)):
        palette = palettes[palette_id]
        slots = {
            rgb: min(
                range(start_slot, 4),
                key=lambda slot: color_distance(rgb, rgb_table[palette[slot]]),
            )
            for rgb, _ in histogram
        }
        x = index % columns * width
        y = index // columns * height
        box = (x, y, x + width, y + height)
        region = Image.new("P", (width, height))
        region.putdata([
            4 * palette_id + slots[red, green, blue] if alpha else 0
            for red, green, blue, alpha in region_pixels(pixels, box)
        ])
        indexed.paste(region, box)

    return indexed, {
        "mode": mode,
        "region_size": [width, height],
        "palettes": palettes,
        "palette_ids": [
            assignments[offset : offset + columns]
            for offset in range(0, len(assignments), columns)
        ],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="authored source PNG")
    parser.add_argument("indexed_png", type=Path, help="indexed output PNG")
    parser.add_argument("palette_json", type=Path, help="output palettes and region assignments")
    parser.add_argument("--mode", choices=REGION_SIZES, required=True)
    parser.add_argument("--rgb-table", type=Path, required=True, help="64 RGB triplets (192 bytes)")
    parser.add_argument("--backdrop", type=lambda value: int(value, 0), help="BG color zero as a NES color code")
    args = parser.parse_args()

    if args.mode == "bg" and args.backdrop not in COLOR_CODES:
        parser.error("bg mode requires --backdrop with a supported NES color code")
    if args.mode == "obj" and args.backdrop is not None:
        parser.error("--backdrop is only valid in bg mode")
    paths = (args.image, args.rgb_table, args.indexed_png, args.palette_json)
    if len({path.resolve() for path in paths}) != len(paths):
        parser.error("input and output paths must be distinct")

    try:
        rgb_bytes = args.rgb_table.read_bytes()
        if len(rgb_bytes) != 192:
            raise ValueError("RGB table must contain 64 RGB triplets (192 bytes)")
        rgb_table = [tuple(rgb_bytes[offset : offset + 3]) for offset in range(0, 192, 3)]
        with Image.open(args.image, formats=["PNG"]) as source:
            indexed, metadata = convert(source.convert("RGBA"), rgb_table, args.mode, args.backdrop)
        options = {"transparency": 0} if args.mode == "obj" else {}
        indexed.save(args.indexed_png, format="PNG", **options)
        args.palette_json.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
