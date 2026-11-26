#!/usr/bin/env python3
"""Convert Revision G Z80 SSG sound effects to NES APU frame streams (src/revg-se-data.inc).

Simulates each Z80 SE command using tools/mame/decode-revg-sound.py (240 Hz), samples the
YM2203 SSG tone/noise/mixer/volume registers at 60 Hz, and emits compact APU pulse/noise
streams for src/sound-driver.inc (BANK8).
"""

import argparse
import importlib.util
from pathlib import Path
from zipfile import ZipFile

TICKS_PER_FRAME = 4
# YM2203 master clock = 1.5 MHz, SSG prescaler 1/2 -> 750 kHz / 16 = 46,875 Hz base.
# NES NTSC APU pulse base = 1,789,773 / 16 = 111,860.8125 Hz.
SSG_TO_NES_PERIOD = (1789773.0 / 16.0) / (1500000.0 / 32.0)
MAX_FRAMES = 48  # Cap long SEs so BANK8 stays compact and crisp


def load_decoder():
    path = Path(__file__).resolve().parent / "mame" / "decode-revg-sound.py"
    spec = importlib.util.spec_from_file_location("decode_revg_sound", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_gg2(zip_path: Path) -> bytes:
    with ZipFile(zip_path) as zf:
        return zf.read("gg2.bin")


def simulate_se(decoder, rom: bytes, cmd: int, max_frames: int = MAX_FRAMES):
    """Run one Z80 SE command and return per-frame SSG state snapshots."""
    drv = decoder.Driver(rom)
    # Track both YM2203 chips' SSG registers (0..10).
    ssg = [[0] * 11, [0] * 11]
    ssg[0][7] = 0x3F
    ssg[1][7] = 0x3F
    frames = []
    started = False
    silent_streak = 0

    for frame in range(max_frames):
        for sub in range(TICKS_PER_FRAME):
            latch = cmd if (frame == 0 and sub == 0) else 0xFF
            drv.writes.clear()
            drv.tick(latch)
            for chip, reg, val in drv.writes:
                if 0 <= reg <= 10 and chip in (0, 1):
                    ssg[chip][reg] = val

        # Extract active tone and noise components across both YM2203 chips.
        tones = []
        noises = []
        for chip in (0, 1):
            r = ssg[chip]
            mixer = r[7]
            noise_per = r[6] & 0x1F
            for ch in range(3):
                vol = r[8 + ch] & 0x0F
                if vol == 0:
                    continue
                tone_en = (mixer & (1 << ch)) == 0
                noise_en = (mixer & (8 << ch)) == 0
                if tone_en:
                    per = r[ch * 2] | ((r[ch * 2 + 1] & 0x0F) << 8)
                    if per > 0:
                        tones.append((vol, per))
                if noise_en:
                    noises.append((vol, noise_per))

        if tones or noises:
            started = True
            silent_streak = 0
        elif started:
            silent_streak += 1
            if silent_streak >= 3:
                break

        best_tone = max(tones, key=lambda t: t[0]) if tones else None
        best_noise = max(noises, key=lambda n: n[0]) if noises else None
        frames.append((best_tone, best_noise))

    # Trim trailing silence
    while frames and frames[-1] == (None, None):
        frames.pop()
    return frames


def to_nes_pulse_period(ssg_period: int) -> int:
    p = int(round(ssg_period * SSG_TO_NES_PERIOD)) - 1
    return max(8, min(0x7FF, p))


def to_nes_noise_index(ssg_noise: int) -> int:
    # SSG noise period 0..31 -> NES noise index 0..15
    return max(0, min(15, int(round(ssg_noise * 15.0 / 31.0))))


def encode_pulse_stream(frames, duty_bits: int = 0x80, vol_boost: int = 1):
    out = []
    last_per = 0x100
    for tone, _ in frames:
        if tone is None:
            out.extend([duty_bits | 0x30, last_per & 0xFF, (last_per >> 8) & 0x07])
        else:
            vol, ssg_per = tone
            vol = min(15, max(1, vol + vol_boost))
            per = to_nes_pulse_period(ssg_per)
            last_per = per
            out.extend([duty_bits | 0x30 | vol, per & 0xFF, (per >> 8) & 0x07])
    out.append(0xFF)
    return out


def encode_noise_stream(frames, vol_boost: int = 1):
    out = []
    last_idx = 4
    for _, noise in frames:
        if noise is None:
            out.extend([0x30, last_idx])
        else:
            vol, ssg_n = noise
            vol = min(15, max(1, vol + vol_boost))
            idx = to_nes_noise_index(ssg_n)
            last_idx = idx
            out.extend([0x30 | vol, idx])
    out.append(0xFF)
    return out


# (label_base, z80_cmd, ch_a, ch_b, max_frames, duty_bits)
# Channel codes: 0 = Pulse 1, 1 = Pulse 2, 2 = Noise, 0xFF = none
SE_SPECS = [
    ("snd_se_shot",    0x06, 1, 0xFF, 16, 0x40),  # Weapon throw (Pulse 2)
    ("snd_se_damage",  0x01, 2, 0,    18, 0x80),  # Arthur damaged (Noise + Pulse 1)
    ("snd_se_key",     0x10, 0, 0xFF, 28, 0x80),  # Armor / Key pickup (Pulse 1)
    ("snd_se_pickup",  0x10, 0, 0xFF, 20, 0x80),  # Item pickup (Pulse 1)
    ("snd_se_hit",     0x0D, 2, 1,    12, 0x80),  # Enemy hit (Noise + Pulse 2)
    ("snd_se_kill",    0x1B, 2, 0xFF, 16, 0x80),  # Enemy kill (Noise)
    ("snd_se_clank",   0x0D, 1, 2,    10, 0x40),  # Shield / tombstone clank (Pulse 2 + Noise)
    ("snd_se_death",   0x0F, 0, 2,    28, 0x80),  # Boss defeat / death (Pulse 1 + Noise)
    ("snd_se_efire",   0x23, 2, 0xFF, 14, 0x80),  # Enemy shot (Noise)
    ("snd_se_rise",    0x17, 2, 0xFF, 16, 0x80),  # Zombie rise (Noise)
    ("snd_se_stomp",   0x0C, 2, 1,    14, 0x80),  # Boss stomp (Noise + Pulse 2)
    ("snd_se_jump",    0x04, 1, 0xFF, 12, 0x40),  # Arthur jump (Pulse 2)
    ("snd_se_land",    0x05, 2, 0xFF,  8, 0x80),  # Arthur landing (Noise)
]


def fmt_bytes(data):
    lines = []
    for i in range(0, len(data), 12):
        chunk = ", ".join(f"${b:02x}" for b in data[i:i + 12])
        lines.append(f"    .byte {chunk}")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="Output .inc file (e.g. src/revg-se-data.inc)")
    parser.add_argument("--rom", type=Path, default=Path("reference/makaimurg.zip"))
    args = parser.parse_args()

    decoder = load_decoder()
    rom = load_gg2(args.rom)

    lines = [
        "; Generated by tools/convert-revg-se.py from Revision G Z80 sound ROM (gg2.bin).",
        "; APU Pulse/Noise frame streams for src/sound-driver.inc (BANK8).",
        "",
    ]

    total_bytes = 0
    for label, cmd, ch_a, ch_b, max_f, duty in SE_SPECS:
        frames = simulate_se(decoder, rom, cmd, max_f)
        if not frames:
            raise SystemExit(f"SE {label} (${cmd:02x}) produced 0 frames!")
        # Emit stream A
        s_a = encode_noise_stream(frames) if ch_a == 2 else encode_pulse_stream(frames, duty)
        lines.append(f"; {label} (Z80 cmd ${cmd:02x}, {len(frames)} frames)")
        lines.append(f"{label}_0:")
        lines.append(fmt_bytes(s_a))
        total_bytes += len(s_a)
        if ch_b != 0xFF:
            s_b = encode_noise_stream(frames) if ch_b == 2 else encode_pulse_stream(frames, duty)
            lines.append(f"{label}_1:")
            lines.append(fmt_bytes(s_b))
            total_bytes += len(s_b)
        lines.append("")

    args.output.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {args.output}: {len(SE_SPECS)} SEs, {total_bytes} bytes")


if __name__ == "__main__":
    main()
