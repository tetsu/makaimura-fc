"""Run capture-revg-types.lua from every page of every Revision G section and build a contact
sheet of the enemy and support types it saw, so their numbers can be matched to names.

Writes build/revg-types/types.json (first sighting of each type per section) and
build/revg-types/types.png (one crop per pool/type/section around the object's position).
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import subprocess

from PIL import Image, ImageDraw

MAME_ARGS = ["makaimurg", "-noreadconfig", "-video", "none", "-sound", "none", "-nothrottle",
             "-skip_gameinfo", "-autoboot_delay", "0"]
CROP = 64


def run(mame, rom_path, work, section, page, frames, walk):
    tag = f"{section}-{'start' if page is None else f'{page:x}'}-{int(walk)}"
    env = dict(os.environ, REVG_TYPES_SECTION=str(section), REVG_TYPES_FRAMES=str(frames),
               REVG_TYPES_WALK="1" if walk else "0")
    env.pop("REVG_TYPES_PAGE", None)
    if page is not None:
        env["REVG_TYPES_PAGE"] = f"{page:x}"
    snap = work / "snap" / tag
    snap.mkdir(parents=True, exist_ok=True)
    command = [str(mame)] + MAME_ARGS + [
        "-rompath", str(rom_path), "-autoboot_script", str(Path(__file__).with_name("capture-revg-types.lua")),
        "-cfg_directory", str(work / "cfg" / tag), "-nvram_directory", str(work / "nvram" / tag),
        "-snapshot_directory", str(snap), "-seconds_to_run", str(60 + frames // 60)]
    output = subprocess.run(command, env=env, capture_output=True, text=True, check=True).stdout
    seen = []
    for line in output.splitlines():
        if not line.startswith("seen,"):
            continue
        _, sec, pg, pool, kind, variant, sx, sy, frame, slot, name = line.split(",")
        seen.append({"section": int(sec), "page": None if pg == "-" else int(pg, 16), "pool": pool,
                     "type": int(kind, 16), "variant": int(variant, 16), "screen": [int(sx), int(sy)],
                     "frame": int(frame), "slot": f"${slot.upper()}", "file": str(snap / name)})
    return seen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rom-path", type=Path, default=Path("reference"))
    parser.add_argument("--data", type=Path, default=Path("build/revg-stages"))
    parser.add_argument("--work", type=Path, default=Path("build/revg-types"))
    parser.add_argument("--sections", default="0,1,2,3,4,5,6")
    parser.add_argument("--frames", type=int, default=1200)
    parser.add_argument("--jobs", type=int, default=8)
    args = parser.parse_args()
    sections = {s["section"]: s for s in json.loads((args.data / "sections.json").read_text(encoding="utf-8"))}

    jobs = []
    for number in [int(n) for n in args.sections.split(",")]:
        pages = sections[number]["pages"]
        jobs.append((number, None, True))
        jobs.append((number, None, False))
        for row in range(pages["rows"][0], pages["rows"][1] + 1):
            for col in range(pages["columns"][0], pages["columns"][1] + 1):
                jobs.append((number, row * 32 + col, False))
    with ThreadPoolExecutor(args.jobs) as pool:
        results = list(pool.map(lambda job: run(args.mame, args.rom_path, args.work, job[0], job[1],
                                                args.frames, job[2]), jobs))

    first = {}
    for seen in results:
        for item in seen:
            key = (item["pool"], item["type"], item["section"])
            if key not in first or item["frame"] < first[key]["frame"]:
                first[key] = item
    items = [first[key] for key in sorted(first)]
    (args.work / "types.json").write_text(json.dumps(items, indent=1), encoding="utf-8")

    # Each cell: the first sighting, then the same moment with only this object lifted 24 px
    # (pixels that changed are tinted), so neighbouring objects cannot be mistaken for it.
    columns, scale = 4, 2
    cell_w, cell_h = CROP * scale * 2 + 8, CROP * scale + 16
    sheet = Image.new("RGB", (columns * cell_w, ((len(items) + columns - 1) // columns) * cell_h))
    draw = ImageDraw.Draw(sheet)
    for index, item in enumerate(items):
        before = Image.open(item["file"]).convert("RGB")
        lifted = Path(item["file"].replace(".png", "-up.png"))
        after = Image.open(lifted).convert("RGB") if lifted.exists() else before.copy()
        tint = after.copy()
        for py in range(after.height):
            for px in range(after.width):
                if before.getpixel((px, py)) != after.getpixel((px, py)):
                    r, g, b = after.getpixel((px, py))
                    tint.putpixel((px, py), (255, g // 2, b // 2))
        sx, sy = item["screen"]
        sy += 8  # the slot position is near the feet; centre the crop on the body
        box = (sx - CROP // 2, sy - CROP // 2, sx + CROP // 2, sy + CROP // 2)
        x, y = (index % columns) * cell_w, (index // columns) * cell_h
        sheet.paste(before.crop(box).resize((CROP * scale, CROP * scale), Image.NEAREST), (x, y + 14))
        sheet.paste(tint.crop(box).resize((CROP * scale, CROP * scale), Image.NEAREST), (x + CROP * scale + 2, y + 14))
        draw.text((x + 2, y + 1), f"{item['pool']}{item['type']:02X} section {item['section']} variant {item['variant']:02X}",
                  fill=(255, 255, 0))
    sheet.save(args.work / "types.png")
    print(f"{len(items)} pool/type/section sightings from {len(jobs)} runs")


if __name__ == "__main__":
    main()
