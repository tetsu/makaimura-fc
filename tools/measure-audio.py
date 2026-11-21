#!/usr/bin/env python3
"""Report per-channel levels from 16-bit PCM WAV recordings."""

import argparse
from array import array
from math import log10, sqrt
from pathlib import Path
import sys
import wave


PCM_SCALE = 32768


def dbfs(amplitude):
    if amplitude == 0:
        return "-inf"
    return f"{20 * log10(amplitude / PCM_SCALE):.2f}"


def measure(path):
    with wave.open(str(path), "rb") as wav:
        if wav.getsampwidth() != 2:
            raise ValueError("expected a 16-bit PCM WAV")
        frame_count = wav.getnframes()
        channels = wav.getnchannels()
        rate = wav.getframerate()
        raw = wav.readframes(frame_count)

    if not frame_count:
        raise ValueError("recording contains no samples")
    if len(raw) != frame_count * channels * 2:
        raise ValueError("audio data is truncated")

    samples = array("h", raw)
    if sys.byteorder == "big":
        samples.byteswap()

    print(f"{path} ({frame_count / rate:.3f} s, {rate} Hz)")
    for channel in range(channels):
        signal = samples[channel::channels]
        peak = max(abs(min(signal)), max(signal))
        sample_sum = sum(signal)
        sample_squares = sum(value * value for value in signal)
        # Remove DC offset before calculating RMS so it doesn't count as sound.
        rms = sqrt(frame_count * sample_squares - sample_sum**2) / frame_count
        rail_hits = signal.count(-PCM_SCALE) + signal.count(PCM_SCALE - 1)
        print(f"  Ch {channel + 1}: RMS {dbfs(rms)} dBFS, peak {dbfs(peak)} dBFS, "
              f"full-scale samples {rail_hits}/{frame_count}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("recordings", metavar="WAV", nargs="+", type=Path)
    args = parser.parse_args()
    for path in args.recordings:
        try:
            measure(path)
        except (OSError, EOFError, ValueError, wave.Error) as error:
            parser.exit(1, f"{path}: {error}\n")


if __name__ == "__main__":
    main()
