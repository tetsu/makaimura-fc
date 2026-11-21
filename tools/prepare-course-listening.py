#!/usr/bin/env python3
"""Make A/B listening clips from the two Mesen course recordings at their original levels."""

import argparse
from pathlib import Path
import wave


RECORDINGS = Path(__file__).resolve().parents[1] / "build/audio/course-512k"
NTSC_FPS = 60.0988


def read_events(path):
    events = []
    for line in path.read_text(encoding="utf-8").splitlines():
        fields = line.split(maxsplit=1)
        if len(fields) == 2 and fields[0].isdigit():
            events.append((int(fields[0]), fields[1]))
    return events


def make_pairs(course):
    normal = RECORDINGS / f"{course}.wav"
    muted = RECORDINGS / f"{course}-muted-shot.wav"
    events = read_events(normal.with_suffix(".log"))
    if events != read_events(muted.with_suffix(".log")):
        raise ValueError("normal and muted recordings have different events")

    first_shot = next((frame for frame, event in events if event == "shot"), None)
    retry_frame = next((frame for frame, event in events if event == "checkpoint retry"), None)
    if first_shot is None or retry_frame is None:
        raise ValueError("expected first shot and checkpoint retry events")
    retry_shot = next((frame for frame, event in events if event == "shot" and frame > retry_frame), None)
    if retry_shot is None:
        raise ValueError("expected a shot after checkpoint retry")

    with wave.open(str(normal), "rb") as a, wave.open(str(muted), "rb") as b:
        rate = a.getframerate()
        channels = a.getnchannels()
        width = a.getsampwidth()
        muted_format = (b.getframerate(), b.getnchannels(), b.getsampwidth())
        if width != 2 or (rate, channels, width) != muted_format:
            raise ValueError("expected matching 16-bit PCM recordings")

        before = round(0.65 * rate)
        after = round(0.35 * rate)
        clip_frames = before + after
        frame_bytes = channels * width
        gap = bytes(round(0.60 * rate) * frame_bytes)
        for label, shot in (("first-shot", first_shot), ("retry-shot", retry_shot)):
            start = round(shot * rate / NTSC_FPS) - before
            if start < 0 or start + clip_frames > min(a.getnframes(), b.getnframes()):
                raise ValueError(f"{label} falls outside the recordings")
            a.setpos(start)
            b.setpos(start)
            normal_pcm = a.readframes(clip_frames)
            muted_pcm = b.readframes(clip_frames)
            if any(len(pcm) != clip_frames * frame_bytes for pcm in (normal_pcm, muted_pcm)):
                raise ValueError(f"{label} contains truncated audio data")

            path = RECORDINGS / f"{course}-{label}-ab.wav"
            with wave.open(str(path), "wb") as pair:
                pair.setparams((channels, width, rate, 0, "NONE", "not compressed"))
                pair.writeframes(muted_pcm + gap + normal_pcm)
            print(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    for course in ("original", "alternate"):
        try:
            make_pairs(course)
        except (OSError, EOFError, ValueError, wave.Error) as error:
            parser.exit(1, f"{course}: {error}\n")


if __name__ == "__main__":
    main()
