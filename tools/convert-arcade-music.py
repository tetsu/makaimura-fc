#!/usr/bin/env python3
"""
Convert captured makaimurg YM2203x2 register logs (build/ym_<song>.log)
into faithful VRC7a FM 6-voice + FC APU percussion sequence data (src/arcade-music-data.inc).
"""
import math, os

FM_CLOCK = 1500000.0 / 72.0  # Exact makaimurg YM2203 FM clock calibration
VRC7_BASE_HZ = 3579545.0 / 72.0

def midi_to_vrc7(midi):
    freq = 440.0 * (2.0 ** ((midi - 69) / 12.0))
    best_blk, best_fn, best_err = 0, 256, 1e9
    for blk in range(8):
        fn = int(round(freq * (1 << (19 - blk)) / VRC7_BASE_HZ))
        if 1 <= fn <= 511:
            actual = fn * VRC7_BASE_HZ / (1 << (19 - blk))
            err = abs(actual - freq)
            if err < best_err:
                best_err = err
                best_blk = blk
                best_fn = fn
    return best_blk, best_fn

def ym_fm_to_midi(blk, fnum):
    if fnum == 0:
        return 0
    freq = fnum * FM_CLOCK / (1 << (20 - blk))
    if freq < 20.0:
        return 0
    return int(round(69.0 + 12.0 * math.log2(freq / 440.0)))

def parse_ym_log(path):
    fm_hi = [0]*6
    fm_lo = [0]*6
    fm_key = [False]*6
    fm_note = [0]*6
    fm_evs = [[] for _ in range(6)]
    drum_evs = []
    ssg_mix = [0x3f, 0x3f]
    ssg_prev_vol = [0]*6
    ssg_noise_per = [0, 0]
    max_fr = 0

    for line in open(path):
        fr, chip, reg, val = line.split()
        fr, chip, reg, val = int(fr), int(chip), int(reg, 16), int(val, 16)
        if fr > max_fr:
            max_fr = fr
        if reg == 0x28:
            ch = val & 3
            if ch < 3:
                gch = chip * 3 + ch
                slots = (val >> 4) & 0x0f
                if slots:
                    blk = (fm_hi[gch] >> 3) & 7
                    fn = ((fm_hi[gch] & 7) << 8) | fm_lo[gch]
                    midi = ym_fm_to_midi(blk, fn)
                    if midi > 0:
                        fm_evs[gch].append((fr, 'on', midi))
                        fm_key[gch] = True
                        fm_note[gch] = midi
                else:
                    if fm_key[gch]:
                        fm_evs[gch].append((fr, 'off', 0))
                        fm_key[gch] = False
                        fm_note[gch] = 0
        elif 0xa4 <= reg <= 0xa6:
            fm_hi[chip * 3 + (reg - 0xa4)] = val
        elif 0xa0 <= reg <= 0xa2:
            gch = chip * 3 + (reg - 0xa0)
            fm_lo[gch] = val
            if fm_key[gch]:
                blk = (fm_hi[gch] >> 3) & 7
                fn = ((fm_hi[gch] & 7) << 8) | fm_lo[gch]
                midi = ym_fm_to_midi(blk, fn)
                if midi != fm_note[gch] and midi > 0:
                    fm_evs[gch].append((fr, 'on', midi))
                    fm_note[gch] = midi
        elif reg == 0x06:
            ssg_noise_per[chip] = val & 0x1f
        elif reg == 0x07:
            ssg_mix[chip] = val
        elif 0x08 <= reg <= 0x0a:
            ch = reg - 0x08
            gch = chip * 3 + ch
            vol = val & 0x0f
            if vol >= 10 and ssg_prev_vol[gch] < 8:
                noise_on = ((ssg_mix[chip] >> (ch + 3)) & 1) == 0
                if chip == 0 and noise_on:
                    drum_evs.append((fr, 2 if ssg_noise_per[chip] >= 8 else 3))
                elif chip == 1 and ch == 0:
                    drum_evs.append((fr, 1))
            ssg_prev_vol[gch] = vol
    return fm_evs, drum_evs, max_fr

def detect_loop(fm_evs, default_len):
    for ch in [0, 4, 1, 2]:
        ons = [(fr, midi) for (fr, kind, midi) in fm_evs[ch] if kind == 'on']
        if len(ons) < 30:
            continue
        sig = [(ons[i][1], ons[i+1][0] - ons[i][0]) for i in range(len(ons) - 1)]
        for start_i in range(0, min(60, len(sig) - 20)):
            pat = sig[start_i : start_i + 10]
            for j in range(start_i + 16, len(sig) - 10):
                ok = True
                for k in range(10):
                    if sig[j + k][0] != pat[k][0] or abs(sig[j + k][1] - pat[k][1]) > 1:
                        ok = False
                        break
                if ok:
                    ls, le = ons[start_i][0], ons[j][0]
                    if le - ls >= 120:
                        return ls, le
    return 0, default_len

# Bytecode per track:
#   $00       : key-off / rest, followed by 1-byte dur (1..250)
#   $01..$54  : key-on MIDI (23 + byte) = 24..107, followed by 1-byte dur (1..250)
#   $55       : hold current note (no re-keyon), followed by 1-byte dur (1..250)
#   $FE       : end of non-looping song
#   $FF, lo, hi : jump to 16-bit loop address in BANK9
def encode_fm_track(evs, loop_start, loop_end, is_looping):
    by_fr = {}
    for fr, kind, midi in evs:
        if fr < loop_end:
            by_fr[fr] = (kind, midi)
    frames = sorted(by_fr.keys())
    intro, loop = [], []

    def emit(target, cmd, dur):
        while dur > 250:
            target.append(cmd)
            target.append(250)
            dur -= 250
            if 1 <= cmd <= 0x54:
                cmd = 0x55
        if dur > 0:
            target.append(cmd)
            target.append(dur)

    def enc_seg(s, e, target):
        sf = [f for f in frames if s <= f < e]
        if not sf:
            emit(target, 0, e - s)
            return
        if sf[0] > s:
            emit(target, 0, sf[0] - s)
        for i, fr in enumerate(sf):
            nxt = sf[i+1] if i + 1 < len(sf) else e
            kind, midi = by_fr[fr]
            if kind == 'on' and 24 <= midi <= 107:
                cmd = midi - 23
            else:
                cmd = 0
            emit(target, cmd, nxt - fr)

    if is_looping:
        enc_seg(0, loop_start, intro)
        enc_seg(loop_start, loop_end, loop)
    else:
        enc_seg(0, loop_end, intro)
    return intro, loop

def encode_drum_track(drum_evs, loop_start, loop_end, is_looping):
    by_fr = {}
    for fr, kind in drum_evs:
        if fr < loop_end:
            if fr not in by_fr or kind == 2 or (kind == 1 and by_fr[fr] == 3):
                by_fr[fr] = kind
    frames = sorted(by_fr.keys())
    intro, loop = [], []

    def emit(target, cmd, dur):
        while dur > 250:
            target.append(cmd)
            target.append(250)
            dur -= 250
            cmd = 0
        if dur > 0:
            target.append(cmd)
            target.append(dur)

    def enc_seg(s, e, target):
        sf = [f for f in frames if s <= f < e]
        if not sf:
            emit(target, 0, e - s)
            return
        if sf[0] > s:
            emit(target, 0, sf[0] - s)
        for i, fr in enumerate(sf):
            nxt = sf[i+1] if i + 1 < len(sf) else e
            emit(target, by_fr[fr], nxt - fr)

    if is_looping:
        enc_seg(0, loop_start, intro)
        enc_seg(loop_start, loop_end, loop)
    else:
        enc_seg(0, loop_end, intro)
    return intro, loop

def format_bytes(arr, indent="    "):
    lines = []
    for i in range(0, len(arr), 16):
        chunk = arr[i:i+16]
        lines.append(indent + ".byte " + ", ".join(f"${b:02x}" for b in chunk))
    return "\n".join(lines)

# Song IDs (1..7):
#   1 = title/start (Start Demo jingle, looping? Actually Stage 1 intro or Start jingle)
#   2 = stage1 (Stage 1 & 2: Graveyard & Forest)
#   3 = boss   (Boss theme)
#   4 = clear  (Stage Clear jingle)
#   5 = gameover (Game Over jingle)
#   6 = stage3 (Stage 3 & 4: Cavern & Bridge)
#   7 = stage5 (Stage 5 & 6: Castle)
SONGS = [
    ("start",    1, False, 319, 0, [0x01, 0x02, 0x33, 0x44, 0x34, 0x84]),
    ("stage1",   2, True,  2800, 0, [0x01, 0x02, 0x33, 0x43, 0xa2, 0x84]),
    ("boss",     3, True,  1200, 1, [0x01, 0x02, 0x42, 0x43, 0xa1, 0x83]),
    ("clear",    4, False, 359, 0, [0x01, 0x02, 0x32, 0x33, 0xa2, 0x84]),
    ("gameover", 5, False, 319, 0, [0x02, 0x33, 0x43, 0x44, 0xa2, 0x84]),
    ("stage3",   6, True,  1400, 0, [0x01, 0x02, 0x43, 0x33, 0xa2, 0x84]),
    ("stage5",   7, True,  1800, 1, [0x01, 0x02, 0x42, 0x33, 0xa2, 0x84]),
]

out = []
out.append("; Auto-generated by tools/convert-arcade-music.py from makaimurg YM2203x2 logs.")
out.append("; Placed in BANK9 ($C000-$DFFF) alongside the Bank 8 sound driver.")
out.append("")
out.append(".segment \"BANK9\"")
out.append("    .byte bank_tag_base + 9")
out.append("")

# Pitch table for MIDI 24 (C1) .. 107 (B7) -> index 1..84 (index 0 = rest)
lo_tbl = [0]
hi_tbl = [0]
for midi in range(24, 108):
    blk, fn = midi_to_vrc7(midi)
    lo_tbl.append(fn & 0xff)
    hi_tbl.append(0x10 | (blk << 1) | ((fn >> 8) & 1))

out.append("arc_fm_fnum_lo:")
out.append(format_bytes(lo_tbl))
out.append("arc_fm_fnum_hi:")
out.append(format_bytes(hi_tbl))
out.append("")

# Song metadata tables (indexed by song_id 0..7)
out.append("arc_song_patch_id:")
out.append("    .byte 0, " + ", ".join(str(s[4]) for s in SONGS))
out.append("")

out.append("arc_song_inst_base:")
out.append("    .byte 0, " + ", ".join(str(i * 6) for i in range(len(SONGS))))
out.append("arc_inst_presets:")
for s in SONGS:
    out.append(f"    ; {s[0]}")
    out.append("    .byte " + ", ".join(f"${b:02x}" for b in s[5]))
out.append("")

# Track pointer tables (7 tracks per song: FM ch0..5 + Drum ch6)
out.append("arc_song_trk_offset:")
out.append("    .byte 0, " + ", ".join(str(i * 7) for i in range(len(SONGS))))
out.append("arc_trk_lo:")
for s in SONGS:
    name = s[0]
    out.append("    .byte " + ", ".join(f"<arc_{name}_ch{c}" for c in range(7)))
out.append("arc_trk_hi:")
for s in SONGS:
    name = s[0]
    out.append("    .byte " + ", ".join(f">arc_{name}_ch{c}" for c in range(7)))
out.append("")

total_data = 0
for name, sid, looping, def_len, patch, insts in SONGS:
    fm_evs, drum_evs, max_fr = parse_ym_log(f"build/ym_{name}.log")
    if looping:
        ls, le = detect_loop(fm_evs, def_len)
    else:
        last_on = max((e[0] for ch in range(6) for e in fm_evs[ch]), default=def_len)
        ls, le = 0, min(max_fr, last_on + 24)
    out.append(f"; === Song {sid}: {name} (loop={looping}, [{ls}..{le}]) ===")
    for ch in range(6):
        bi, bl = encode_fm_track(fm_evs[ch], ls, le, looping)
        out.append(f"arc_{name}_ch{ch}:")
        if bi:
            out.append(format_bytes(bi))
        if looping:
            out.append(f"arc_{name}_ch{ch}_loop:")
            if bl:
                out.append(format_bytes(bl))
            out.append(f"    .byte $ff, <arc_{name}_ch{ch}_loop, >arc_{name}_ch{ch}_loop")
        else:
            out.append("    .byte $fe")
        total_data += len(bi) + len(bl) + (3 if looping else 1)
    di, dl = encode_drum_track(drum_evs, ls, le, looping)
    out.append(f"arc_{name}_ch6:")
    if di:
        out.append(format_bytes(di))
    if looping:
        out.append(f"arc_{name}_ch6_loop:")
        if dl:
            out.append(format_bytes(dl))
        out.append(f"    .byte $ff, <arc_{name}_ch6_loop, >arc_{name}_ch6_loop")
    else:
        out.append("    .byte $fe")
    total_data += len(di) + len(dl) + (3 if looping else 1)
    out.append("")

os.makedirs("src", exist_ok=True)
with open("src/arcade-music-data.inc", "w", newline="\n") as f:
    f.write("\n".join(out) + "\n")
print(f"Generated src/arcade-music-data.inc ({total_data} B track data + tables, fits in 8 KiB BANK9!)")
