"""Start each Revision G section page in MAME and compare its RAM with extract-revg-stages.py output.

Checks, for every run:
  * the background bytes written by the section's initial expansion ($62AB) against the
    screen-ID grid and block tables;
  * background RAM ($2800 tile numbers, $2C00 attributes) inside the camera window;
  * the palette work copy at $1632 against the bank3 palette set selected by the page table;
  * enemy-pool and support-pool slots against the section's fixed placement lists.
"""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

MAME_ARGS = ["makaimurg", "-noreadconfig", "-video", "none", "-sound", "none", "-nothrottle",
             "-skip_gameinfo", "-autoboot_delay", "0"]


def load(directory):
    world = json.loads((directory / "world.json").read_text(encoding="utf-8"))
    sections = json.loads((directory / "sections.json").read_text(encoding="utf-8"))
    return world, sections


def expected_tile(world, tx, ty):
    grid = world["grid"]["screen_ids"]
    page_col, page_row = tx >> 4, ty >> 4
    if not (0 <= page_row < len(grid) and 0 <= page_col < len(grid[0])):
        return None
    screen = world["screens"][f"${grid[page_row][page_col]:02X}"]
    code, attr = screen["tiles"][15 - (ty & 15)][tx & 15]
    return code & 0xFF, (code >> 8) << 6 | attr


def run_mame(mame, rom_path, section, page, walk, work):
    env = dict(os.environ, REVG_STAGE_SECTION=str(section), REVG_STAGE_WALK=str(walk),
               REVG_STAGE_CHECKPOINT="1" if page == "checkpoint" else "0")
    if page == "checkpoint":
        page = None
    if page is None:
        env.pop("REVG_STAGE_PAGE", None)
    else:
        env["REVG_STAGE_PAGE"] = f"{page:x}"
    command = [str(mame)] + MAME_ARGS + [
        "-rompath", str(rom_path), "-autoboot_script", str(Path(__file__).with_name("dump-revg-stage.lua")),
        "-cfg_directory", str(work / "cfg"), "-nvram_directory", str(work / "nvram"),
        "-seconds_to_run", str(60 + walk // 60)]
    output = subprocess.run(command, env=env, capture_output=True, text=True, check=True).stdout
    dumps, current, expansion, motion = [], None, None, None
    for line in output.splitlines():
        key, _, value = line.partition(",")
        if key == "expand_dump":
            current = expansion = {"page": int(value.split(",")[1], 16)}
        elif key == "stage_dump":
            fields = value.split(",")
            # frame counts from the first dump, 3 frames after the initial expansion stopped.
            current = {"section": int(fields[0]), "frame": int(fields[2]), "loaded_section": int(fields[3]),
                       "page": int(fields[4], 16), "camera_x": int(fields[5], 16),
                       "camera_y": int(fields[6], 16), "mode": fields[9], "player": fields[10]}
            dumps.append(current)
        elif key == "expanded":
            current[key] = [None if value[i:i + 2] == "--" else int(value[i:i + 2], 16)
                            for i in range(0, len(value), 2)]
        elif key in ("bg", "palette"):
            current[key] = bytes.fromhex(value)
        elif key == "motion":
            moved, cells, colors = value.split(",")
            motion = {"camera_moved": moved == "1", "cells": frozenset(int(c, 16) for c in cells.split()),
                      "colors": [int(c) for c in colors.split()]}
        elif key in ("enemies", "support"):
            slots = []
            for item in value.split():
                slot, kind, variant, x, y = item.split(":")
                slots.append((int(kind, 16), int(variant, 16), int(x, 16), int(y, 16)))
            current[key] = slots
    return expansion, dumps, motion


def check_expansion(world, expansion):
    """Compare the bytes $62AB's expansion wrote: page p in full, p-1 in full, then p+1
    columns 0-7, which reuse the ring columns of p-1's columns 0-7. Every written byte must
    belong to one of those cells, and every cell must hold its last expected value.

    Two cases cannot be predicted from the tables and are only counted:
      * $62DC-$62E0 index the block-table pointers with LDX B,X, a signed 8-bit offset, so for
        p+1 with a screen ID of $40 or more the pointer comes from $4100 + 2 * ID, which is
        zero; the blocks are then read from RAM. Column streaming ($80F1 and its siblings use
        a 16-bit index) rewrites those columns before they scroll into view.
      * at page 0, p-1 is read from $3FFF, outside the grid.
    """
    page, written = expansion["page"], expansion["expanded"]
    grid = world["grid"]["screen_ids"]
    expected, unpredictable = {}, set()
    for target, columns in ((page, range(16)), (page - 1, range(16)), (page + 1, range(8))):
        cells = [(col, row, 0x200 * (target & 1) + 32 * col + 16 * ((target >> 5) & 1) + row)
                 for col in columns for row in range(16)]
        if not 0 <= target < len(grid) * len(grid[0]):
            unpredictable.update(index for _, _, index in cells)
            continue
        screen = grid[target >> 5][target & 31]
        if target == page + 1 and screen >= 0x40:
            unpredictable.update(index for _, _, index in cells)
            continue
        tiles = world["screens"][f"${screen:02X}"]["tiles"]
        for col, row, index in cells:
            code, attr = tiles[row][col]
            unpredictable.discard(index)
            expected[index] = (target, col, row, code & 0xFF, (code >> 8) << 6 | attr)
    mismatches = []
    for index, (target, col, row, code, attr) in expected.items():
        if index in unpredictable:
            continue  # overwritten later by p+1 read from RAM
        actual = written[index], written[0x400 + index]
        if actual != (code, attr):
            mismatches.append((target, col, row, (code, attr), actual))
    stray = [0x2800 + i for i, value in enumerate(written)
             if value is not None and (i & 0x3FF) not in expected and (i & 0x3FF) not in unpredictable]
    if stray:
        mismatches.append(("unexpected writes", [f"${a:04X}" for a in stray[:4]], len(stray)))
    return len(expected) - len(unpredictable & set(expected)), len(unpredictable), mismatches


def check_background(world, dump, animated=frozenset()):
    """Compare the tiles MAME shows: screen x = X - camera X, screen y = camera Y + 239 - Y.

    The 224 visible lines cover world Y camera+16 .. camera+239. Ring cells listed in
    `animated` (they changed while the game stood still) are counted, not compared. After a
    start in the middle of a section, the two leftmost columns may still hold the column
    512 px ahead, because column streaming fills that ring slot first; those cells may match
    either column.
    """
    cam_x, cam_y = dump["camera_x"], dump["camera_y"]
    checked, mismatches, ahead, moving = 0, [], 0, 0
    for tx in range(cam_x >> 4, ((cam_x + 255) >> 4) + 1):
        for ty in range((cam_y + 16) >> 4, ((cam_y + 239) >> 4) + 1):
            expected = expected_tile(world, tx, ty)
            if expected is None:
                continue
            index = 32 * (tx & 31) + 16 * ((ty >> 4) & 1) + 15 - (ty & 15)
            if index in animated:
                moving += 1
                continue
            actual = dump["bg"][index], dump["bg"][0x400 + index]
            checked += 1
            if actual == expected:
                continue
            if tx - (cam_x >> 4) < 2 and actual == expected_tile(world, tx + 32, ty):
                ahead += 1
                continue
            mismatches.append((tx, ty, expected, actual))
    return checked, mismatches, ahead, moving


def expected_palette(world, rom_palettes, dump):
    # The page table palette applies on entry; a zero keeps the previous one.
    pages = world["pages"]
    page = dump["page"]
    number = pages[page]["palette"]
    return number, rom_palettes.get(number)


def check_slots(expected_records, slots):
    live = set(slots)
    found = [r for r in expected_records if (r["type"], r["variant"], r["x"], r["y"]) in live]
    return len(found), len(expected_records)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rom-path", type=Path, default=Path("reference"))
    parser.add_argument("--data", type=Path, default=Path("build/revg-stages"))
    parser.add_argument("--work", type=Path, default=Path("build/revg-stage-verify"))
    parser.add_argument("--sections", default="0,1,2,3,4,5,6")
    parser.add_argument("--walk", type=int, default=900, help="frames to hold right from each section start")
    args = parser.parse_args()

    world, sections = load(args.data)
    import importlib.util
    spec = importlib.util.spec_from_file_location("extract", Path(__file__).with_name("extract-revg-stages.py"))
    extract = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(extract)
    from zipfile import ZipFile
    with ZipFile(args.rom_path / "makaimurg.zip") as rom_zip:
        rom = extract.Rom({name: rom_zip.read(name) for name in extract.ROM_SHA1})
    raw_palettes = {n: rom.bytes(3, 0x4000 + 128 * (n - 1), 128) for n in range(1, 16)}

    args.work.mkdir(parents=True, exist_ok=True)
    failures = 0
    totals = {"runs": 0, "tiles": 0, "palettes": 0}
    for section in sections:
        number = section["section"]
        if str(number) not in args.sections.split(","):
            continue
        c0, c1 = section["pages"]["columns"]
        r0, r1 = section["pages"]["rows"]
        runs = [(None, args.walk), ("checkpoint", args.walk)] + [
            ((row << 5) | col, 0) for row in range(r0, r1 + 1) for col in range(c0, c1 + 1)]
        for page, walk in runs:
            expansion, dumps, motion = run_mame(args.mame, args.rom_path, number, page, walk, args.work)
            totals["runs"] += 1
            checked, skipped, mismatches = check_expansion(world, expansion)
            totals["expanded"] = totals.get("expanded", 0) + checked
            totals["unpredictable"] = totals.get("unpredictable", 0) + skipped
            expected_page = {None: section["start"]["page"], "checkpoint": section["checkpoint"]["page"]}.get(page, page)
            ok = not mismatches and expansion["page"] == expected_page
            failures += not ok
            print(f"{'ok' if ok else 'FAIL':4} section {number} expansion of page ${expansion['page']:03X}: "
                  f"{checked - len(mismatches)}/{checked} tiles, {skipped} cells read from outside the tables"
                  + ("" if ok else f" first {mismatches[:3]}"), flush=True)
            settled = dumps[0]
            # Cells and colours that changed while standing still for 120 frames. If the camera
            # moved in that time the cell list is not usable and nothing is excluded.
            animated = frozenset() if motion["camera_moved"] else motion["cells"]
            cycling = motion["colors"]
            # The game itself only starts at the start and checkpoint pages. Starting elsewhere
            # leaves the leftmost columns (and, in odd page rows, the rows below) to the
            # streaming code, so those views are reported but not judged.
            judged = page in (None, "checkpoint")
            name = {None: "start", "checkpoint": "checkpoint"}.get(page) or f"${page:03X}"
            for dump in dumps:
                label = f"section {number} page {name} frame {dump['frame']}"
                problems = []
                if dump["frame"] and (dump["mode"], dump["player"][:2]) != (settled["mode"], settled["player"][:2]):
                    print(f"skip {label}: play state changed {settled['mode']}/{settled['player']} -> "
                          f"{dump['mode']}/{dump['player']}", flush=True)
                    totals["skipped"] = totals.get("skipped", 0) + 1
                    continue
                if dump["loaded_section"] != number:
                    problems.append(f"loaded section {dump['loaded_section']}")
                checked, mismatches, ahead, moving = check_background(
                    world, dump, animated if dump is settled else frozenset())
                totals["tiles"] += checked
                totals["animated"] = totals.get("animated", 0) + moving
                if mismatches:
                    problems.append(f"{len(mismatches)}/{checked} tiles differ, first {mismatches[:3]}")
                palette_number, _ = expected_palette(world, raw_palettes, dump)
                palette_note = ""
                if palette_number and dump is settled:
                    rom_set, ram_set = raw_palettes[palette_number], dump["palette"]
                    differ = [i for i in range(64) if (rom_set[i], rom_set[64 + i] >> 4)
                              != (ram_set[i], ram_set[64 + i] >> 4) and i not in cycling]
                    if differ:
                        palette_note = f" (differs in colors {differ}, cycling {cycling})"
                        totals["palette_notes"] = totals.get("palette_notes", 0) + 1
                    else:
                        totals["palettes"] += 1
                        palette_note = f" (cycling colors {cycling})" if cycling else ""
                enemy_found, enemy_total = check_slots(section["enemy_list"], dump["enemies"])
                support_found, support_total = check_slots(section["support_list"], dump["support"])
                if not problems:
                    status = "ok"
                elif judged:
                    status = "FAIL"
                    failures += 1
                else:
                    status = "info"
                    totals["unjudged"] = totals.get("unjudged", 0) + 1
                print(f"{status:4} {label}: page ${dump['page']:03X} camera ({dump['camera_x']},{dump['camera_y']}) "
                      f"tiles {checked} (animated {moving}, 512 px ahead {ahead}) palette {palette_number}"
                      f"{palette_note} enemy-list {enemy_found}/{enemy_total} "
                      f"support-list {support_found}/{support_total}"
                      + ("" if not problems else " -- " + "; ".join(problems)), flush=True)
    print(f"runs {totals['runs']}, expanded tiles compared {totals.get('expanded', 0)} "
          f"(plus {totals.get('unpredictable', 0)} read from outside the tables), "
          f"visible tiles compared {totals['tiles']} (plus {totals.get('animated', 0)} animated), "
          f"palettes matched {totals['palettes']} (other {totals.get('palette_notes', 0)}), "
          f"skipped dumps {totals.get('skipped', 0)}, "
          f"views from other pages not matching {totals.get('unjudged', 0)}, failed {failures}")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
