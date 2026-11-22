#!/usr/bin/env python3
"""Generate geometric areas and courses for the Mesen integration tests."""

import argparse
import json
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw


TILES = ("sky", "gravel", "ledge", "rock", "oneway", "ladder", "hazard")
PACKER = Path(__file__).with_name("pack-nes-chr.py")


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def pack_sheet(output, mode, image, region_width, colors, first_tile, *options):
    # The artwork uses one palette. Other palette slots repeat it.
    rgb = [(0, 0, 0), (50, 88, 105), (87, 181, 171), (245, 240, 206)]
    image.putpalette([channel for color in rgb * 4 for channel in color])
    png = output / f"{mode}.png"
    image.save(png, **({"transparency": 0} if mode == "obj" else {}))
    metadata = output / f"{mode}-palette.json"
    write_json(metadata, {
        "mode": mode,
        "region_size": [region_width, 16],
        "palettes": [colors] * 4,
        "palette_ids": [[0] * (image.width // region_width)],
    })
    include = output / f"{mode}.inc"
    subprocess.run([
        sys.executable, str(PACKER), str(png), str(metadata), str(include),
        "--prefix", mode, "--first-tile", first_tile, *options,
    ], check=True)
    return include.name


def make_background(output, colors, first_tile):
    image = Image.new("P", (16 * len(TILES), 16))
    draw = ImageDraw.Draw(image)
    for index in range(1, len(TILES)):
        x = 16 * index
        draw.rectangle((x, 0, x + 15, 15), fill=1)
        draw.line((x, index, x + 15, index), fill=2, width=2)
        draw.rectangle((x + 3, 10, x + 5, 12), fill=3)
    return pack_sheet(output, "bg", image, 16, colors, first_tile)


def make_player(output, colors, first_tile):
    image = Image.new("P", (32, 16))
    draw = ImageDraw.Draw(image)
    poses = {}
    for index, name in enumerate(("idle", "walk_a", "walk_b", "jump")):
        x = 8 * index
        draw.rectangle((x, 2, x + 7, 11), fill=1)
        draw.rectangle((x + 4, 4, x + 6, 6), fill=3)
        draw.rectangle((x + index, 12, x + index + 2, 15), fill=2)
        poses[name] = [{"region": [index, 0], "dx": -4, "dy": -16}]
    manifest = output / "poses.json"
    write_json(manifest, poses)
    include = pack_sheet(output, "obj", image, 8, colors, first_tile, "--poses", str(manifest))
    return include, {name: name for name in poses}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    output = parser.parse_args().output
    output.mkdir(parents=True, exist_ok=True)

    bg = make_background(output, [0x0F, 0x01, 0x1C, 0x30], "0x40")
    obj, poses = make_player(output, [0x0F, 0x16, 0x28, 0x30], "0x10")
    steps = output / "steps"
    steps.mkdir(exist_ok=True)
    # Give the second area its own tile ranges and palette colors so the
    # test can detect a load from either of the first area's banks.
    steps_bg = make_background(steps, [0x0F, 0x07, 0x18, 0x37], "0x60")
    steps_obj, steps_poses = make_player(steps, [0x0F, 0x12, 0x2A, 0x3C], "0x20")
    write_json(output / "areas.json", {
        "trial": {"bg": bg, "obj": obj},
        "steps": {"bg": f"steps/{steps_bg}", "obj": f"steps/{steps_obj}"},
    })
    # A flat test route isolates encounter timing. Both encounters straddle
    # world-coordinate page boundaries during a shot.
    tiles = [["sky"] * 48 for _ in range(11)] + [["ledge"] * 48] + [["rock"] * 48 for _ in range(3)]
    for column in range(3, 48, 6):
        tiles[5][column] = "gravel"
    stage = {
        "area": "trial",
        "tiles": tiles,
        "regions": {name: [index, 0] for index, name in enumerate(TILES)},
        "start_x": 16,
        "checkpoint_x": 464,
        "goal_x": 760,
        "player_poses": poses,
        "enemies": {"fixed": {"x": 258, "y": 176}, "ranged": {"x": 524, "y": 176}},
    }
    write_json(output / "stage.json", stage)

    # A short final route reuses the first area's graphics with its own
    # starting point, marker, encounter and goal.
    finish_tiles = [row.copy() for row in tiles]
    finish_tiles[5][1] = "gravel"
    write_json(output / "finish.json", {
        **stage, "tiles": finish_tiles, "start_x": 32, "checkpoint_x": 96, "goal_x": 176,
        "enemies": {"fixed": {"x": 112, "y": 176}},
    })

    hazard_tiles = [row.copy() for row in tiles]
    hazard_tiles[10][8] = "hazard"  # X=128..143, immediately above the floor.
    write_json(output / "hazard.json", {
        **stage, "tiles": hazard_tiles, "start_x": 32, "checkpoint_x": 96, "goal_x": 176,
        "enemies": {},
    })

    oneway_tiles = [row.copy() for row in tiles]
    oneway_tiles[9][3:5] = ["oneway", "oneway"]  # X=48..79, Y=144.
    oneway_tiles[10][4] = "oneway"  # A lower platform in the same column as the jump.
    write_json(output / "oneway.json", {
        **stage, "tiles": oneway_tiles, "start_x": 32, "checkpoint_x": 96, "goal_x": 176,
        "enemies": {},
    })

    ladder_tiles = [row.copy() for row in tiles]
    for row in range(7, 11):
        ladder_tiles[row][4] = "ladder"
    ladder_tiles[7][5:7] = ["oneway", "oneway"]  # Landing at the ladder top, Y=112.
    write_json(output / "ladder.json", {
        **stage, "tiles": ladder_tiles, "start_x": 32, "checkpoint_x": 120, "goal_x": 176,
        "enemies": {},
    })

    marker_tiles = [row.copy() for row in ladder_tiles]
    marker_tiles[7][5:12] = ["oneway"] * 7
    marker_tiles[10][1] = "hazard"  # X=16..31, below the ladder landing.
    write_json(output / "markers.json", {
        **stage, "tiles": marker_tiles, "start_x": 32, "checkpoint_x": 96, "checkpoint_y": 112,
        "goal_x": 176, "goal_y": 112, "enemies": {},
    })

    # The raised floor spans X=96..319 at Y=144. The enemy on the
    # lower floor forces a retry after crossing the elevated checkpoint.
    for column in range(6, 20):
        tiles[9][column] = "ledge"
        tiles[10][column] = "rock"
        tiles[11][column] = "rock"
    # Move a gravel marker to X=512, where its two tile columns are
    # streamed into the nametable ring just after the camera wraps at X=256.
    tiles[5][32], tiles[5][33] = tiles[5][33], tiles[5][32]
    stage.update(area="steps", player_poses=steps_poses, checkpoint_x=288,
                 enemies={"fixed": {"x": 380, "y": 176}})
    write_json(output / "steps.json", stage)


if __name__ == "__main__":
    main()
