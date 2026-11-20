"""Produce local, bank-separated 6809 listings for MAME's makaimurg set."""

import argparse
import hashlib
from pathlib import Path
import subprocess
from zipfile import BadZipFile, ZipFile


ROM_SHA1 = {
    "mj04g.bin": "07f7cf788810a1425016e016ce3579adb3253ac7",
    "mj03g.bin": "23a0a17d0abc4b084ffeba90266ef455361771cc",
    "mj05g.bin": "7694a981a6196d77fd2279fc34042b4cfb40c054",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--unidasm", required=True, type=Path)
    parser.add_argument("--rom", type=Path, default=Path("reference/makaimurg.zip"))
    parser.add_argument("--output", type=Path, default=Path("build/revg-disassembly"))
    args = parser.parse_args()

    try:
        with ZipFile(args.rom) as rom:
            images = {name: rom.read(name) for name in ROM_SHA1}
    except (BadZipFile, KeyError, OSError) as error:
        parser.error(str(error))

    for name, expected in ROM_SHA1.items():
        if hashlib.sha1(images[name]).hexdigest() != expected:
            parser.error(f"{name} does not match MAME's makaimurg manifest")

    low_fixed_and_bank4 = images["mj04g.bin"]
    banked = images["mj05g.bin"]
    regions = {
        "fixed": (0x6000, low_fixed_and_bank4[0x2000:] + images["mj03g.bin"]),
        "bank4": (0x4000, low_fixed_and_bank4[:0x2000]),
    }
    for bank in range(4):
        start = bank * 0x2000
        regions[f"bank{bank}"] = (0x4000, banked[start:start + 0x2000])

    args.output.mkdir(parents=True, exist_ok=True)
    for name, (address, data) in regions.items():
        binary = args.output / f"{name}.bin"
        binary.write_bytes(data)
        listing = args.output / f"{name}.asm"
        with listing.open("wb") as output:
            subprocess.run(
                [str(args.unidasm.resolve()), str(binary), "-arch", "m6809",
                 "-basepc", f"0x{address:x}", "-lower"],
                stdout=output,
                check=True,
            )
        print(f"{name:5} ${address:04x}-${address + len(data) - 1:04x} {listing}")


if __name__ == "__main__":
    main()
