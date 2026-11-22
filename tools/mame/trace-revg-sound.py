"""Trace reachable Z80 code in makaimurg's sound ROM and list code and data apart.

Reads build/revg-disassembly/sound.bin (written by disassemble-revg.py), follows
control flow from the reset and IRQ entries, and writes sound-traced.asm with
labels on branch targets and every untraced byte as data. With --trace, checks
a MAME debugger trace of the Z80 against the traced instruction addresses.
"""

import argparse
from pathlib import Path
import re
import subprocess

ENTRIES = {
    0x0000: "reset",
    0x0038: "irq",
    0x0738: "ret_0738",     # pushed as a return address at $074d
    # No reference anywhere in the ROM; decoded so the bytes read as code.
    0x01BB: "unused_01bb",
    0x02C3: "unused_02c3",
    0x059A: "unused_059a",
}
# `rst $30` pops its return address and jumps through the word table that
# follows the opcode. Entry counts come from the mask applied just before it.
JUMP_TABLES = {0x011F: 64, 0x05A3: 8, 0x0759: 8, 0x07D6: 8}
# Call-like restarts: $18 adds A to HL, $20 reads (HL+A), $28 reads a word.
RETURNING_RST = {0x18, 0x20, 0x28}
LINE = re.compile(r"^([0-9a-f]{4}): ((?:[0-9a-f]{2} )+) *(.*)$")
TARGET = re.compile(r"^(jp|jr|call|djnz)\s+(?:(\w+),)?\$([0-9a-f]{4})$")
WINDOW = 0x100


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--unidasm", required=True, type=Path)
    parser.add_argument("--directory", type=Path, default=Path("build/revg-disassembly"))
    parser.add_argument("--trace", type=Path, help="MAME `trace` output for audiocpu")
    args = parser.parse_args()

    binary = args.directory / "sound.bin"
    rom = binary.read_bytes()
    decoded = {}

    def decode(address):
        """Decode from `address`; alignment follows the traced flow, not the file."""
        if address not in decoded:
            output = subprocess.run(
                [str(args.unidasm.resolve()), str(binary), "-arch", "z80",
                 "-basepc", f"0x{address:x}", "-skip", f"0x{address:x}",
                 "-count", f"0x{min(WINDOW, len(rom) - address):x}", "-lower"],
                capture_output=True, text=True, check=True,
            ).stdout
            for line in output.splitlines():
                match = LINE.match(line)
                if not match:
                    continue
                at = int(match[1], 16)
                size = len(match[2].split())
                # The last instruction of a window may be cut short; redo it later.
                if at + size >= address + WINDOW and at != address:
                    break
                decoded.setdefault(at, (size, " ".join(match[3].split())))
        return decoded[address]

    code = {}                  # address -> (size, text)
    labels = dict(ENTRIES)     # address -> name
    tables = {}                # table address -> entry count
    pending = list(ENTRIES)

    def target(address, kind):
        labels.setdefault(address, f"{kind}_{address:04x}")
        if address < len(rom):
            pending.append(address)

    while pending:
        pc = pending.pop()
        while pc < len(rom) and pc not in code:
            size, text = decode(pc)
            code[pc] = (size, text)
            branch = TARGET.match(text)
            if branch:
                kind = "sub" if branch[1] == "call" else "loc"
                target(int(branch[3], 16), kind)
                if branch[1] in ("jp", "jr") and branch[2] is None:
                    break
            elif text.startswith("rst "):
                vector = int(text.split("$")[1], 16)
                target(vector, "rst")
                if pc in JUMP_TABLES:
                    tables[pc + 1] = JUMP_TABLES[pc]
                    for index in range(JUMP_TABLES[pc]):
                        offset = pc + 1 + index * 2
                        target(rom[offset] | rom[offset + 1] << 8, "loc")
                    break
                if vector not in RETURNING_RST:
                    raise SystemExit(f"${pc:04x}: unhandled {text}")
            elif text in ("ret", "reti", "retn") or text.startswith("jp ("):
                break
            pc += size

    owner = {}
    for address, (size, _) in code.items():
        for offset in range(address, address + size):
            if owner.setdefault(offset, address) != address:
                raise SystemExit(f"${offset:04x}: overlapping instructions")
    for address, count in tables.items():
        for offset in range(address, address + count * 2):
            owner[offset] = "table"

    end = len(rom)
    while rom[end - 1] == 0xFF:
        end -= 1

    def name(address):
        return labels.get(address, f"${address:04x}")

    lines, ranges = [], []
    pc = 0
    while pc < len(rom):
        kind = "code" if pc in code else "table" if owner.get(pc) == "table" else \
            "fill" if pc >= end else "data"
        if not ranges or ranges[-1][0] != kind:
            ranges.append([kind, pc, pc])
            lines.append(f"\n; ---- {kind} from ${pc:04x}")
        if pc in labels and kind != "table":
            lines.append(f"{labels[pc]}:")
        if kind == "code":
            size, text = code[pc]
            text = TARGET.sub(lambda m: f"{m[1]:4} " + (f"{m[2]}," if m[2] else "")
                              + name(int(m[3], 16)), text)
            lines.append(f"  {pc:04x}: {rom[pc:pc + size].hex(' '):12} {text}")
        elif kind == "table":
            size = 2
            lines.append(f"  {pc:04x}: dw {name(rom[pc] | rom[pc + 1] << 8)}")
        elif kind == "fill":
            size = len(rom) - pc
            lines.append(f"  {pc:04x}: ds ${size:04x},$ff")
        else:
            size = 1
            while (size < 16 and pc + size < end and pc + size not in owner
                   and pc + size not in labels):
                size += 1
            lines.append(f"  {pc:04x}: db " + ",".join(f"${b:02x}" for b in rom[pc:pc + size]))
        pc += size
        ranges[-1][2] = pc - 1

    listing = args.directory / "sound-traced.asm"
    listing.write_text("\n".join(lines).lstrip() + "\n", encoding="ascii", newline="\n")

    totals = {}
    for kind, first, last in ranges:
        totals[kind] = totals.get(kind, 0) + last - first + 1
        print(f"{kind:5} ${first:04x}-${last:04x} {last - first + 1:6}")
    print(" ".join(f"{kind}={size}" for kind, size in totals.items()), listing)

    if args.trace:
        executed = set()
        with args.trace.open(errors="replace") as trace:
            for line in trace:
                match = re.match(r"([0-9A-F]{4}): ", line)
                if match:
                    executed.add(int(match[1], 16))
        outside = sorted(executed - code.keys())
        idle = sorted(code.keys() - executed)
        print(f"trace: {len(executed)} of {len(code)} traced instructions executed")
        print("never executed: " + " ".join(f"${a:04x}" for a in idle))
        if outside:
            raise SystemExit("executed outside traced code: "
                             + " ".join(f"${a:04x}" for a in outside))


if __name__ == "__main__":
    main()
