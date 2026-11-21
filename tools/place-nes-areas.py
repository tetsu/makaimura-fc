#!/usr/bin/env python3
"""Assign prepared BG and OBJ includes to free VRC7a PRG banks."""

import argparse
import json
import re
from importlib import import_module
from pathlib import Path


DIRECTORY_BANK = 11
FIRST_BANK = 19
LAST_BANK = 50
VIEWER_BANK = 51
TRIAL_BANK = 52
MAX_TRIALS = 2
FIXED_BANK = 63
BANK_SIZE = 0x2000
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
TRIAL_POSES = ("idle", "walk_a", "walk_b", "jump")
TRIAL_POSE_TABLE_OFFSET = 0x22


def read_areas(manifest_path):
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    max_areas = (LAST_BANK - FIRST_BANK + 1) // 2
    if not isinstance(manifest, dict) or not 1 <= len(manifest) <= max_areas:
        raise ValueError(f"manifest must contain 1..{max_areas} named areas")

    areas = {}
    for name, tables in manifest.items():
        if not IDENTIFIER.fullmatch(name) or not isinstance(tables, dict) or tables.keys() != {"bg", "obj"}:
            raise ValueError(f"area {name!r} must have an identifier name and bg and obj paths")
        paths = []
        for mode in ("bg", "obj"):
            source = tables[mode]
            if not isinstance(source, str) or not source:
                raise ValueError(f"area {name!r} {mode} must be an include path")
            path = (manifest_path.parent / source).resolve()
            if not path.is_file():
                raise ValueError(f"area {name!r} {mode} include not found: {path}")
            paths.append(path)
        areas[name] = paths
    return areas


def make_trial(path, areas, bank):
    stage_map = import_module("convert-stage-map")
    trial = json.loads(path.read_text(encoding="utf-8-sig"))
    fields = {"area", "tiles", "regions", "start_x", "checkpoint_x", "goal_x"}
    if not isinstance(trial, dict) or not fields <= trial.keys() <= fields | {"player_poses", "enemies"}:
        raise ValueError(f"trial must define {', '.join(sorted(fields))} and may define player_poses or enemies")
    name = trial["area"]
    if not isinstance(name, str) or name not in areas:
        raise ValueError("trial.area must name an area from the manifest")
    stage = {"tiles": trial["tiles"]}
    width, height, columns = stage_map.encode_map(stage["tiles"])
    if (width, height) != (48, 15):
        raise ValueError("trial must have 48 columns and 15 rows")
    solid = {stage_map.TILES["ledge"], stage_map.TILES["rock"]}
    floors = [next((row * 16 for row, tile in enumerate(column) if tile in solid), 240)
              for column in columns]
    if any(not 48 <= floor <= 224 for floor in floors):
        raise ValueError("each trial column must have a solid floor at Y=48..224")
    bounded = stage_map.int_in_range
    start = bounded(trial["start_x"], 16, 239, "trial.start_x")
    checkpoint = bounded(trial["checkpoint_x"], start + 16, width * 16 - 32, "trial.checkpoint_x")
    goal = bounded(trial["goal_x"], checkpoint + 16, width * 16 - 8, "trial.goal_x")
    camera_max = width // 16 - 1
    camera_x = checkpoint // 256
    checkpoint_feet = floors[checkpoint // 16]
    camera_y = max(0, checkpoint_feet - 160)
    regions = trial["regions"]
    if not isinstance(regions, dict) or regions.keys() != stage_map.TILES.keys():
        raise ValueError(f"trial.regions must define {', '.join(stage_map.TILES)}")
    bg = f"area_{name}_bg"
    lines = [
        f'.segment "BANK{bank}"',
        f".scope trial_bank_{bank}",
        "trial_bank_start:",
        f"    .byte bank_tag_base + {bank}",
        f"    stage_header {width}, {height}, {camera_max}, 1, {start}, {checkpoint}, "
        f"{checkpoint_feet}, {camera_x}, {camera_y}, {goal}, {floors[goal // 16]}, trial_map, trial_palettes",
        "trial_area_index:",
        f"    .byte {list(areas).index(name)}",
        "trial_region_offsets:",
    ]
    for tile in stage_map.TILES:
        region = regions[tile]
        if not isinstance(region, list) or len(region) != 2:
            raise ValueError(f"trial.regions.{tile} must be [column, row]")
        col, row = [bounded(value, 0, 65535, f"trial.regions.{tile}") for value in region]
        lines.extend([
            f'    .assert {col} < {bg}::bg_region_columns .and {row} < {bg}::bg_region_rows, '
            f'error, "Trial BG region for {tile} is out of bounds"',
            f"    .word 5 * ({row} * {bg}::bg_region_columns + {col})",
        ])
    lines.extend([
        '    .assert trial_area_index = $8013, lderror, "Unexpected trial header size"',
        f'    .assert {bg}::bg_chr_vram >= CHR_BACKGROUND_BYTES, error, "Trial BG overlaps HUD or marker tiles"',
        f'    .assert * - trial_bank_start = ${TRIAL_POSE_TABLE_OFFSET:02x}, error, '
        '"Unexpected trial pose table offset"',
    ])
    lines.append("trial_player_pose_addresses:")
    if "player_poses" in trial:
        poses = trial["player_poses"]
        if not isinstance(poses, dict) or poses.keys() != set(TRIAL_POSES):
            raise ValueError(f"trial.player_poses must define {', '.join(TRIAL_POSES)}")
        obj = f"area_{name}_obj"
        for state in TRIAL_POSES:
            pose = poses[state]
            if not isinstance(pose, str) or not IDENTIFIER.fullmatch(pose):
                raise ValueError(f"trial.player_poses.{state} must be a packed OBJ pose name")
            lines.append(f"    .addr {obj}::obj_pose_{pose}")
        lines.append(f'    .assert {obj}::obj_chr_vram >= chr_sprite_address + CHR_SPRITE_BYTES, '
                     'error, "Trial OBJ overlaps gameplay sprite patterns"')
    else:
        lines.append("    .res 8, 0         ; No trial player poses")
    lines.extend([
        "trial_has_enemies:",
        f"    .byte {int('enemies' in trial)}",
        "trial_enemy_positions:",
    ])
    if "enemies" in trial:
        enemies = trial["enemies"]
        stage_map.check_fields(enemies, {"fixed", "ranged"}, "trial.enemies")
        for kind, min_x in (("fixed", 8), ("ranged", 12)):
            x, y = 0, 0
            if kind in enemies:
                field = f"trial.enemies.{kind}"
                x, y = stage_map.enemy_position(enemies[kind], field, width, height)
                bounded(x, min_x, width * 16 - 8, f"{field}.x")
                if y != floors[x // 16]:
                    raise ValueError(f"{field}.y must equal the floor at x={x} (Y={floors[x // 16]})")
            lines.append(f"    .word {x}")
            lines.append(f"    .byte {y}")
    else:
        lines.append("    .res 6, 0")
    lines.extend([
        '    .res $100 - (* - trial_bank_start), $ff',
        stage_map.convert(stage, "trial").rstrip(),
        ".endscope",
    ])
    return lines


def check_trial_rom(path, trial_count):
    # The offsets follow the common 18-byte stage header and one area index.
    rom = path.read_bytes()

    def bank_offset(bank):
        return 16 + bank * BANK_SIZE

    def word(offset):
        return int.from_bytes(rom[offset : offset + 2], "little")

    for bank in range(TRIAL_BANK, TRIAL_BANK + trial_count):
        try:
            check_trial_bank(rom, bank, bank_offset, word)
        except (ValueError, IndexError) as error:
            raise ValueError(f"trial {bank - TRIAL_BANK + 1}: {error}") from error


def check_trial_bank(rom, bank, bank_offset, word):
    trial = bank_offset(bank) + 0x13
    bg_descriptor = bank_offset(DIRECTORY_BANK) + 2 + rom[trial] * 36
    bg_bank = bank_offset(rom[bg_descriptor])
    regions = bg_bank + word(bg_descriptor + 13) - 0x8000
    palette_ids = {rom[regions + word(trial + 1 + 2 * tile) + 4] for tile in range(7)}
    if len(palette_ids) != 1:
        raise ValueError("trial BG regions must all use the same palette")
    palette_id = palette_ids.pop()
    if palette_id > 3:
        raise ValueError("trial BG palette must be in 0..3")
    palette = bg_bank + word(bg_descriptor + 7) - 0x8000 + 4 * palette_id
    if rom[palette] != 0x0F:
        raise ValueError("trial BG needs backdrop $0F shared with the HUD and sprites")

    pose_table = bank_offset(bank) + TRIAL_POSE_TABLE_OFFSET
    if word(pose_table):
        obj_descriptor = bg_descriptor + 18
        obj_bank = bank_offset(rom[obj_descriptor])
        check_trial_player(rom, word, pose_table, obj_bank)


def check_trial_player(rom, word, pose_table, obj_bank):
    palettes = set()
    for i, state in enumerate(TRIAL_POSES):
        offset = obj_bank + word(pose_table + 2 * i) - 0x8000
        count = rom[offset]
        if not 1 <= count <= 2:
            raise ValueError(f"trial player pose {state} must have one or two visible parts")
        for part in range(count):
            start = offset + 1 + 4 * part
            dx, dy = (int.from_bytes(rom[p : p + 1], "little", signed=True)
                      for p in (start, start + 1))
            if not (-16 <= dx <= 8 and -32 <= dy <= -1):
                raise ValueError(f"trial player pose {state} needs dx=-16..8 and dy=-32..-1")
            palettes.add(rom[start + 3] & 3)
    if len(palettes) != 1:
        raise ValueError("trial player poses must all use the same OBJ palette")


def make_include(areas, trials):
    banks = []
    lines = ["; Generated by place-nes-areas.py."]
    for name, paths in areas.items():
        for mode, path in zip(("bg", "obj"), paths):
            bank = FIRST_BANK + len(banks)
            scope = f"area_{name}_{mode}"
            banks.append((bank, scope, mode))
            lines.extend([
                f'.segment "BANK{bank}"',
                f"    .byte bank_tag_base + {bank}",
                f".scope {scope}",
                path.read_text(encoding="utf-8").rstrip("\n"),
                ".endscope",
            ])

    tail = FIRST_BANK + len(banks)
    if tail < VIEWER_BANK:
        lines.extend([
            '.segment "AREA_PAD"',
            f".repeat {VIEWER_BANK - tail}, index",
            f"    .byte bank_tag_base + {tail} + index",
            "    .res $1fff, $ff",
            ".endrepeat",
        ])
    tail_start = VIEWER_BANK + 1 + len(trials)
    lines.extend([
        '.segment "AREA_TAIL"',
        f".repeat {FIXED_BANK - tail_start}, index",
        f"    .byte bank_tag_base + {tail_start} + index",
        "    .res $1fff, $ff",
        ".endrepeat",
        f'.segment "BANK{DIRECTORY_BANK}"',
        f"    .byte bank_tag_base + {DIRECTORY_BANK}",
        f"nes_area_directory_bank = {DIRECTORY_BANK}",
        f"nes_area_viewer_bank = {VIEWER_BANK}",
        ".export nes_area_directory_bank, nes_area_count, nes_area_table",
        "nes_area_count:",
        f"    .byte {len(areas)}",
        "nes_area_table:",
    ])
    for bank, scope, mode in banks:
        lines.append(f"    .byte {bank}")
        for field in ("chr_vram", "chr_size", "chr", "palette", "region_columns", "region_rows", "regions"):
            lines.append(f"    .word {scope}::{mode}_{field}")
        if mode == "obj":
            lines.extend([f"    .word {scope}::obj_poses", f"    .byte {scope}::obj_pose_count"])
        else:
            lines.extend(["    .word 0", "    .byte 0"])
    if trials:
        lines.extend([
            f"nes_area_trial_bank = {TRIAL_BANK}",
            f"nes_area_trial_count = {len(trials)}",
            ".macro emit_area_trials",
        ])
        for course in trials:
            lines.extend(course)
        # The runtime addresses must match in every switchable course bank.
        for label in ("trial_area_index", "trial_region_offsets", "trial_player_pose_addresses",
                      "trial_has_enemies", "trial_enemy_positions"):
            lines.append(f"{label} = trial_bank_{TRIAL_BANK}::{label}")
            for bank in range(TRIAL_BANK + 1, TRIAL_BANK + len(trials)):
                lines.append(f'    .assert {label} = trial_bank_{bank}::{label}, lderror, "Trial layout mismatch"')
        lines.append(".endmacro")
    return "\n".join([*lines, ""])


def make_config(base_config, bank_count, trial_count):
    tail = FIRST_BANK + bank_count
    memory = [
        f"    PRG{bank}: start = $8000, size = $2000, type = ro, fill = yes, fillval = $FF;"
        for bank in range(FIRST_BANK, tail)
    ]
    segments = [f"    BANK{bank}: load = PRG{bank}, type = ro;" for bank in range(FIRST_BANK, tail)]
    if tail < VIEWER_BANK:
        memory.append(f"    AREA_PAD: start = $0000, size = ${(VIEWER_BANK - tail) * BANK_SIZE:x},"
                      " type = ro, fill = yes, fillval = $FF;")
        segments.append("    AREA_PAD: load = AREA_PAD, type = ro;")
    memory.append(f"    PRG{VIEWER_BANK}: start = $A000, size = $2000, type = ro, fill = yes, fillval = $FF;")
    segments.append(f"    BANK{VIEWER_BANK}: load = PRG{VIEWER_BANK}, type = ro;")
    for bank in range(TRIAL_BANK, TRIAL_BANK + trial_count):
        memory.append(f"    PRG{bank}: start = $8000, size = $2000, type = ro, fill = yes, fillval = $FF;")
        segments.append(f"    BANK{bank}: load = PRG{bank}, type = ro;")
    tail_banks = FIXED_BANK - VIEWER_BANK - 1 - trial_count
    memory.append(f"    AREA_TAIL: start = $0000, size = ${tail_banks * BANK_SIZE:x},"
                  " type = ro, fill = yes, fillval = $FF;")
    segments.append("    AREA_TAIL: load = AREA_TAIL, type = ro;")
    config = base_config.splitlines()
    for pad, replacement_memory, replacement_segments in (
        ("PAD_19_62", memory, segments),
        ("PAD_11", [f"    PRG{DIRECTORY_BANK}: start = $8000, size = $2000, type = ro, fill = yes, fillval = $FF;"],
         [f"    BANK{DIRECTORY_BANK}: load = PRG{DIRECTORY_BANK}, type = ro;"]),
    ):
        pad_lines = [index for index, line in enumerate(config) if line.lstrip().startswith(f"{pad}:")]
        if len(pad_lines) != 2:
            raise ValueError(f"base linker config must define {pad} memory and segment")
        memory_index, segment_index = pad_lines
        config[segment_index:segment_index + 1] = replacement_segments
        config[memory_index:memory_index + 1] = replacement_memory
    return "\n".join([*config, ""])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", nargs="?", type=Path, help="JSON area names with bg and obj include paths")
    parser.add_argument("base_config", nargs="?", type=Path, help="512 KiB linker configuration")
    parser.add_argument("output_dir", nargs="?", type=Path, help="directory for nes-areas.inc and nes-areas.cfg")
    parser.add_argument("--trial", type=Path, action="append", default=[], help="JSON trial course (up to two)")
    parser.add_argument("--check-trial-rom", type=Path, help="validate selected BG and OBJ art in a linked trial ROM")
    parser.add_argument("--trial-count", type=int, choices=range(1, MAX_TRIALS + 1), default=1,
                        help="number of courses to validate in the linked trial ROM")
    args = parser.parse_args()
    try:
        if args.check_trial_rom:
            check_trial_rom(args.check_trial_rom, args.trial_count)
            return
        if not all((args.manifest, args.base_config, args.output_dir)):
            parser.error("manifest, base_config and output_dir are required when placing areas")
        areas = read_areas(args.manifest)
        output_inc = args.output_dir / "nes-areas.inc"
        output_cfg = args.output_dir / "nes-areas.cfg"
        inputs = {args.manifest.resolve(), args.base_config.resolve()}
        if len(args.trial) > MAX_TRIALS:
            raise ValueError(f"at most {MAX_TRIALS} trial courses are supported")
        inputs.update(path.resolve() for path in args.trial)
        inputs.update(path for paths in areas.values() for path in paths)
        if any(output.resolve() in inputs for output in (output_inc, output_cfg)):
            raise ValueError("output files must be different from the inputs")
        trials = [make_trial(path, areas, bank) for bank, path in enumerate(args.trial, TRIAL_BANK)]
        include = make_include(areas, trials)
        config = make_config(args.base_config.read_text(encoding="utf-8"), 2 * len(areas), len(trials))
        args.output_dir.mkdir(parents=True, exist_ok=True)
        output_inc.write_text(include, encoding="utf-8", newline="\n")
        output_cfg.write_text(config, encoding="utf-8", newline="\n")
    except (OSError, ValueError, IndexError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
