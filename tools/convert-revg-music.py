#!/usr/bin/env python3
"""Convert the Revision G songs to VRC7 sequence data (src/arcade-music-data.inc).

The songs come from the Z80 sound ROM (gg2.bin in reference/makaimurg.zip), played by
the driver model in tools/mame/decode-revg-sound.py, which writes the same YM2203
registers as MAME tick by tick (docs/revision-g-sound-rom-map.md). The arcade songs
use the six FM channels only (the SSG plays the effects), and the VRC7 has six FM
channels too, so each arcade channel becomes one VRC7 channel:

- Key-ons, key-offs and pitches are read from the register writes, at 240 Hz, and
  rounded to 60 Hz frames. A key-on while the key is already on does not restart
  the envelope on the YM2203, so it becomes a legato pitch change.
- A looping song repeats from the first event its channels jump back to ($7F), so
  the loop is the arcade's own. A song sent after a jingle on a higher layer starts
  when the jingle ends, as the Z80 holds the lower layer until then.
- The arcade voices are four-operator OPN voices. The most used voice of a song
  becomes the VRC7 custom patch (carrier and its strongest modulator, with the
  OPN rates halved to the OPLL's 4-bit rates); the others take the closest built-in
  VRC7 patch, transposed by octaves when its carrier multiplier differs. A jingle
  layered over a song has its own custom patch, loaded when it starts and again when
  the song under it takes over.
- The arcade songs have no drums, so neither do these.

The tables go in the driver's bank 8; each song's streams go in one of the data banks,
which the driver maps at $8000 while it plays.

Sequence commands, one stream per FM channel:
  $00 dd       key off, then wait dd frames
  $01-$5F dd   key on note n (MIDI n - 1), then wait dd frames
  $60 dd       wait dd frames
  $61 nn dd    change the pitch to note nn without a new attack, then wait dd frames
  $62 ii       set the instrument and volume ($30+ch) and read the next command
  $63 pp       load custom patch pp into $00-$07 and read the next command
  $FE          end of this channel
  $FF lo hi    jump
"""

import argparse
import hashlib
import importlib.util
import math
from pathlib import Path
from zipfile import ZipFile

GG2_SHA1 = "7ef9ec5c2072e21c787a6bbf700033f50c759c1d"
TICKS_PER_FRAME = 4
NOTE_BASE = -1                 # note byte n is MIDI n - 1 (the songs use MIDI 0-86)
NOTE_MAX = 0x5F
VRC7_HZ = 3579545.0 / 72.0
OPN_FM_HZ = 1500000.0 / 72.0 / 2.0   # 1.5 MHz, prescaler 1/3: f = fnum * this / 2^(20 - block)
MAX_TICKS = 240 * 600
DATA_BANKS = (9, 5)            # $8000 banks for the song streams (src/vrc7a-512k.cfg)

# (name, sound commands as (tick, command), looping). The order is the song id - 1
# (src/sound-driver.inc).
# The opening demo sends $3C, $3D and $3A 64 and 197 frames apart (measured with
# tools/mame/log-revg-sound.lua and measure-revg-stage1-end.lua: frames 630, 694, 827).
SONGS = [
    ("title", [(0, 0x3C), (64 * 4, 0x3D), (197 * 4, 0x3A)], False),   # opening demo
    ("stage", [(0, 0x30), (1, 0x2B)], True),      # sections 0-1, after the start jingle
    ("boss", [(0, 0x2D)], True),                  # sections 0-1 boss pages
    ("clear", [(0, 0x3B)], False),                # after the key is taken
    ("gameover", [(0, 0x2F)], False),
    ("stage3", [(0, 0x30), (1, 0x33)], True),     # sections 2-3
    ("stage5", [(0, 0x30), (1, 0x29)], True),     # sections 4-5
]

# The VRC7 built-in patches (the 2019 die dump, as in Mesen's MesenCore.dll).
VRC7_PATCHES = [
    None,
    bytes.fromhex("03210506e8814227"), bytes.fromhex("1341140dd8f62312"),
    bytes.fromhex("11110808fab22012"), bytes.fromhex("31610c07a8646127"),
    bytes.fromhex("32211e06e1760128"), bytes.fromhex("02010600a3e2f4f4"),
    bytes.fromhex("21611d0782811107"), bytes.fromhex("23212217a2720117"),
    bytes.fromhex("3511250040737201"), bytes.fromhex("b5010f0fa8a55102"),
    bytes.fromhex("17c12407f8f82212"), bytes.fromhex("7123110665741816"),
    bytes.fromhex("0102d305c9950302"), bytes.fromhex("61630c0094c033f6"),
    bytes.fromhex("21720d00c1d55606"),
]
OPLL_MUL = [0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 12, 12, 15, 15]
OPN_MUL = [0.5] + list(range(1, 16))

# OPN operator order in the registers ($30, $34, $38, $3C) is S1, S3, S2, S4.
S1, S3, S2, S4 = 0, 1, 2, 3
# The operators that modulate S4 directly, for each OPN algorithm.
S4_MODULATORS = {0: [S3], 1: [S3], 2: [S1, S3], 3: [S2, S3], 4: [S3], 5: [S1], 6: [], 7: []}
# What feeds each of those modulators (for an estimate of the extra brightness).
UPSTREAM = {0: {S3: [S2]}, 1: {S3: [S1, S2]}, 2: {S3: [S2]}, 3: {S2: [S1]}, 4: {S3: []}, 5: {}}


def load_decoder():
    path = Path(__file__).resolve().parent / "mame" / "decode-revg-sound.py"
    spec = importlib.util.spec_from_file_location("decode_revg_sound", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


# --- voices ---------------------------------------------------------------------

class OpnVoice:
    def __init__(self, data):
        self.ops = []
        for i in range(4):
            dt_mul, tl, ks_ar, dr, sr, sl_rr = (data[r * 4 + i] for r in range(6))
            self.ops.append({
                "mul": dt_mul & 15, "tl": tl & 0x7F, "ks": ks_ar >> 6, "ar": ks_ar & 0x1F,
                "dr": dr & 0x1F, "sr": sr & 0x1F, "sl": sl_rr >> 4, "rr": sl_rr & 15,
            })
        self.fb = data[24] >> 3 & 7
        self.alg = data[24] & 7


def nearest_opll_mul(value):
    return min(range(16), key=lambda i: (abs(math.log2(OPLL_MUL[i] / value)), i))


def opll_operator(op, sustained):
    """(byte $00/$01, AR/DR, SL/RR) of one OPLL operator from an OPN operator."""
    ar = min(15, round(op["ar"] / 2))
    dr = min(15, round(op["dr"] / 2))
    rr = op["rr"]
    if not sustained:
        # A percussive OPLL envelope decays with RR after the decay; the OPN decays with
        # its sustain rate SR (twice the OPLL step) and releases with RR.
        rr = max(rr, min(15, round(op["sr"] / 2)))
    flags = (0x20 if sustained else 0) | (0x10 if op["ks"] >= 2 else 0) | nearest_opll_mul(OPN_MUL[op["mul"]])
    return flags, ar << 4 | dr, op["sl"] << 4 | rr


def convert_voice(voice):
    """An OPLL custom patch (8 bytes) and the VRC7 volume for an OPN voice."""
    car = voice.ops[S4]
    mods = S4_MODULATORS[voice.alg]
    sustained = car["sr"] == 0
    if mods:
        mod_index = min(mods, key=lambda i: voice.ops[i]["tl"])
        mod = voice.ops[mod_index]
        tl = mod["tl"]
        # A second modulator within 6 dB adds about 3 dB of modulation.
        others = [voice.ops[i]["tl"] for i in mods if i != mod_index]
        if others and min(others) - tl <= 8:
            tl -= 4
        if mod_index == S1:
            # The OPLL's feedback 7 turns to noise on held notes: 6 at most.
            fb = min(6, voice.fb)
        else:
            # Operators further up the chain make the modulator richer: stand in feedback.
            up = [voice.ops[i]["tl"] for i in UPSTREAM.get(voice.alg, {}).get(mod_index, [])]
            level = min(up, default=127)
            fb = 4 if level <= 30 else 2 if level <= 45 else 0
            # A modulator that is itself strongly modulated puts far more harmonics into
            # the carrier than its TL alone says; without this the melody voice 6 (alg 0,
            # S2 at TL 41 driven by S3 at TL 23) came out a near sine wave, buried under
            # the bass.
            if level <= 45:
                tl -= (46 - level) // 2
            # The arcade lead (voice 6 and 29: S2 x5 into S3 x0.5 into S4 x2) is a buzzing,
            # gritty tone at D5 and up; a two-operator copy only gets that with the
            # modulator well up and fed back on itself (heard as the lead going missing).
            if level <= 45 and voice.alg == 0:
                tl -= 6
                fb = 6
        tl = max(0, min(63, tl))
    else:
        mod, tl, fb = dict(car, tl=63), 63, 0
    mf, mad, msr = opll_operator(mod, mod["sr"] == 0)
    cf, cad, csr = opll_operator(car, sustained)
    patch = bytes([mf, cf, tl, fb, mad, cad, msr, csr])
    carriers = [car["tl"]] + [voice.ops[i]["tl"] for i in {4: [S2], 5: [S2, S3], 6: [S2, S3], 7: [S1, S2, S3]}.get(voice.alg, [])]
    volume = min(15, round(min(carriers) * 0.75 / 3))
    return patch, volume


def patch_fields(patch):
    m, c = patch[0], patch[1]
    return {
        "mod_mul": OPLL_MUL[m & 15], "car_mul": OPLL_MUL[c & 15],
        "mod_eg": m >> 5 & 1, "car_eg": c >> 5 & 1,
        "tl": patch[2] & 63, "fb": patch[3] & 7,
        "mod_ar": patch[4] >> 4, "mod_dr": patch[4] & 15,
        "car_ar": patch[5] >> 4, "car_dr": patch[5] & 15,
        "mod_sl": patch[6] >> 4, "mod_rr": patch[6] & 15,
        "car_sl": patch[7] >> 4, "car_rr": patch[7] & 15,
    }


def nearest_builtin(patch):
    """(built-in patch number, transpose in semitones) closest to an OPLL patch."""
    want = patch_fields(patch)
    best = None
    for number in range(1, 16):
        have = patch_fields(VRC7_PATCHES[number])
        cost = 6 * abs(math.log2((want["mod_mul"] / want["car_mul"]) / (have["mod_mul"] / have["car_mul"])))
        cost += 4 * (want["car_eg"] != have["car_eg"])
        cost += abs(want["tl"] - have["tl"]) / 8 + abs(want["fb"] - have["fb"]) / 2
        for key in ("car_ar", "car_dr", "car_sl", "car_rr"):
            cost += abs(want[key] - have[key]) / 4
        for key in ("mod_ar", "mod_dr", "mod_sl"):
            cost += abs(want[key] - have[key]) / 8
        octave = round(math.log2(want["car_mul"] / have["car_mul"]))
        cost += 0.5 * abs(math.log2(want["car_mul"] / have["car_mul"]) - octave) * 12
        if best is None or cost < best[0]:
            best = (cost, number, 12 * octave)
    return best[1], best[2]


# --- playing the songs -------------------------------------------------------------

def opn_midi(block, fnum):
    if fnum == 0:
        return None
    freq = fnum * OPN_FM_HZ / (1 << (20 - block))
    return round(69 + 12 * math.log2(freq / 440))


def play(decoder, rom, commands, looping):
    """Per channel: events [(tick, kind, midi, voice, layer)] and its loop (start, end tick)."""

    class Recorder(decoder.Driver):
        def __init__(self, data):
            super().__init__(data)
            self.t = 0
            self.jumps = []
            self.first_fetch = {}

        def s16(self, ix, high, low, value):
            if (high, low) == (0x0C, 0x0D) and value != self.g16(ix, 0x0C, 0x0D) + 1:
                self.jumps.append((self.t, ix, value))
            super().s16(ix, high, low, value)

        def fetch(self, ix):
            self.first_fetch.setdefault((ix, self.g16(ix, 0x0C, 0x0D)), self.t)
            return super().fetch(ix)

    targets = decoder.command_targets(rom)
    driver = Recorder(rom)
    keyed = [False] * 6
    pitch_hi, pitch_lo = [0] * 6, [0] * 6
    events = [[] for _ in range(6)]
    latch = 0
    schedule = dict(commands)
    last_slot = targets[commands[-1][1]][1]
    loop = None
    end = None
    for t in range(MAX_TICKS):
        driver.t = t
        latch = schedule.get(t, latch)
        driver.writes = []
        driver.tick(latch)
        actions = [[] for _ in range(6)]
        for chip, register, value in driver.writes:
            if register == 0x28:
                c = value & 3
                if c < 3:
                    actions[chip * 3 + c].append("on" if value >> 4 else "off")
            elif 0xA4 <= register <= 0xA6:
                pitch_hi[chip * 3 + register - 0xA4] = value
            elif 0xA0 <= register <= 0xA2:
                pitch_lo[chip * 3 + register - 0xA0] = value
        layer = next((ix for ix in decoder.SONG_LAYERS if driver.g(ix, 0)), None)
        for ch in range(6):
            kind = None
            for action in actions[ch]:
                if action == "off":
                    if keyed[ch]:
                        kind = "off"
                    keyed[ch] = False
                elif keyed[ch]:
                    kind = kind if kind == "attack" else "legato"
                else:
                    kind = "attack"
                    keyed[ch] = True
            if kind is None:
                continue
            midi = opn_midi(pitch_hi[ch] >> 3 & 7, (pitch_hi[ch] & 7) << 8 | pitch_lo[ch])
            voice = driver.g((layer or last_slot) + ch * 0x20, 0x11)
            if kind == "legato" and events[ch] and events[ch][-1][2] == midi:
                continue
            events[ch].append((t, kind, midi, voice, layer or last_slot))
        if looping:
            # Each channel loops from the first event it jumps back to; stop when all have.
            loops = []
            for ch in range(6):
                ix = last_slot + ch * 0x20
                jumps = [(jt, target) for jt, jix, target in driver.jumps if jix == ix]
                if jumps:
                    loops.append((driver.first_fetch[(ix, jumps[0][1])], jumps[0][0]))
                elif driver.g(ix, 0) == 0 and t > 1:
                    loops.append(None)       # this channel ended
                else:
                    break
            else:
                loop = loops
                break
        if not looping and layer is None and t > commands[-1][0]:
            end = t
            break
    else:
        raise SystemExit(f"commands {commands}: no loop or end in {MAX_TICKS} ticks")
    if looping:
        spans = {stop - start for start, stop in filter(None, loop)}
        if len(spans) != 1:
            raise SystemExit(f"commands {commands}: the channels loop over different spans {spans}")
        if spans.pop() % TICKS_PER_FRAME:
            raise SystemExit(f"commands {commands}: the loop is not a whole number of frames")
        return events, loop
    return events, [(None, end + 2 * TICKS_PER_FRAME)] * 6


# --- encoding ----------------------------------------------------------------------

def frame_of(tick, origin):
    return (tick - origin + TICKS_PER_FRAME // 2) // TICKS_PER_FRAME


def encode_channel(events, instruments, start, stop, origin, marks):
    """Bytes of one channel's events in [start, stop), frames counted from origin.

    instruments maps (layer, voice) to (patch number, volume, transpose); marks maps a
    tick to the custom patch to load there ($63)."""
    timeline = []
    for tick, kind, midi, voice, layer in events:
        if start <= tick < stop:
            timeline.append((frame_of(tick, origin), kind, midi, (layer, voice)))
    first = frame_of(start, origin)
    last = frame_of(stop, origin)
    loads = {frame_of(tick, origin): index for tick, index in marks.items() if start <= tick < stop}
    # Two events in one frame: the later one wins, but a note that would vanish under
    # its own key-off moves the key-off a frame later.
    merged = []
    for frame, kind, midi, key in timeline:
        if merged and merged[-1][0] >= frame:
            previous = merged[-1]
            if kind == "off" and previous[1] in ("attack", "legato"):
                frame = previous[0] + 1
            else:
                merged.pop()
                frame = max(frame, merged[-1][0] + 1) if merged else frame
        merged.append((frame, kind, midi, key))
    if merged and merged[-1][0] >= last:
        raise SystemExit("an event lands on the loop end")
    data = []

    def load_at(frame):
        if frame in loads:
            data.extend([0x63, loads.pop(frame)])

    def emit(frame, length, command, argument=None):
        """command at frame and the wait until frame + length, split at 255 frames and
        where a patch load falls inside it."""
        load_at(frame)
        points = [frame] + sorted(f for f in loads if frame < f < frame + length) + [frame + length]
        for i in range(len(points) - 1):
            if i:
                load_at(points[i])
            span = points[i + 1] - points[i]
            head = i == 0
            while True:
                step = min(span, 255)
                if head:
                    data.append(command)
                    if argument is not None:
                        data.append(argument)
                    head = False
                else:
                    data.append(0x60)
                data.append(max(step, 1))
                span -= step
                if span <= 0:
                    break

    if not merged or merged[0][0] > first:
        emit(first, (merged[0][0] if merged else last) - first, 0x60)
    current = None
    for i, (frame, kind, midi, key) in enumerate(merged):
        length = (merged[i + 1][0] if i + 1 < len(merged) else last) - frame
        if kind == "off":
            emit(frame, length, 0x00)
            continue
        number, volume, transpose = instruments[key]
        register = number << 4 | volume
        if register != current:
            load_at(frame)
            data.extend([0x62, register])
            current = register
        note = midi + transpose - NOTE_BASE
        if not 1 <= note <= NOTE_MAX:
            raise SystemExit(f"note {midi} (+{transpose}) out of range")
        if kind == "attack":
            emit(frame, length, note)
        else:
            emit(frame, length, 0x61, note)
    return data


def vrc7_pitch(midi):
    freq = 440.0 * 2 ** ((midi - 69) / 12)
    best = None
    for block in range(8):
        fnum = round(freq * (1 << (19 - block)) / VRC7_HZ)
        if 1 <= fnum <= 511:
            error = abs(fnum * VRC7_HZ / (1 << (19 - block)) - freq)
            if best is None or error < best[0]:
                best = (error, block, fnum)
    return best[1], best[2]


def format_bytes(values, indent="    "):
    return [indent + ".byte " + ", ".join(f"${v:02x}" for v in values[i:i + 16])
            for i in range(0, len(values), 16)]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("output", type=Path, nargs="?", default=Path("src/arcade-music-data.inc"))
    parser.add_argument("--rom", type=Path, default=Path("reference/makaimurg.zip"))
    args = parser.parse_args()

    decoder = load_decoder()
    with ZipFile(args.rom) as rom_zip:
        rom = rom_zip.read("gg2.bin")
    if hashlib.sha1(rom).hexdigest() != GG2_SHA1:
        raise SystemExit("gg2.bin does not match MAME's makaimurg manifest")

    lines = [
        "; Generated by tools/convert-revg-music.py from the Revision G sound ROM (gg2.bin).",
        "; Songs: " + ", ".join(f"{i + 1} {name} ({' then '.join(f'${c:02x}' for _, c in cmds)})"
                                for i, (name, cmds, _) in enumerate(SONGS)),
        "",
        '.segment "BANK8"',
        "",
    ]
    lo, hi = [0], [0]
    for note in range(1, NOTE_MAX + 1):
        block, fnum = vrc7_pitch(note + NOTE_BASE)
        lo.append(fnum & 0xFF)
        hi.append(0x10 | block << 1 | fnum >> 8)
    lines += ["arc_fm_fnum_lo:"] + format_bytes(lo) + ["arc_fm_fnum_hi:"] + format_bytes(hi) + [""]

    patches = []          # custom patches, 8 bytes each
    songs = []            # (name, first patch, lines, size)
    for index, (name, commands, looping) in enumerate(SONGS):
        events, loops = play(decoder, rom, commands, looping)
        origin = min((e[0] for ch in events for e in ch), default=0)
        stop = max(s for _, s in filter(None, loops))
        # Each layer (a jingle, the song under it) gets its own custom patch: its most
        # used voice, by sounding ticks.
        use, layer_start = {}, {}
        for ch in events:
            for i, (tick, kind, midi, voice, layer) in enumerate(ch):
                layer_start[layer] = min(layer_start.get(layer, tick), tick)
                if kind != "off":
                    following = ch[i + 1][0] if i + 1 < len(ch) else stop
                    use[(layer, voice)] = use.get((layer, voice), 0) + following - tick
        # The sounding pitch of each voice (the carrier multiplier included), for the
        # balance below.
        pitches = {}
        for ch in events:
            for tick, kind, midi, voice, layer in ch:
                if kind != "off" and midi is not None:
                    pitches.setdefault((layer, voice), []).append(midi)
        instruments, notes, marks, first_patch = {}, [], {}, None
        for layer in sorted(layer_start, key=layer_start.get):
            voices = sorted((v for lay, v in use if lay == layer), key=lambda v: -use[(layer, v)])
            custom = None
            for voice in voices:
                opn = OpnVoice(rom[decoder.VOICES + 32 * voice:decoder.VOICES + 32 * voice + 25])
                patch, volume = convert_voice(opn)
                # Balance: the VRC7's two-operator voices make the low parts boom and the
                # high parts thin, so the melody lost to the bass. Bass voices (sounding
                # mostly below C3) go 6 dB down, the others 3 dB up.
                heard = sorted(pitches.get((layer, voice), [60]))
                sounding = heard[len(heard) // 2] + 12 * math.log2(OPN_MUL[opn.ops[S4]["mul"]])
                volume = min(15, volume + 2) if sounding < 48 else max(0, volume - 1)
                # Headroom: with several channels at volume 0 the VRC7 output clipped
                # (heard as crackle), so every voice goes 6 dB down from there.
                volume = min(15, volume + 2)
                if custom is None:
                    custom = patch
                    if patch not in patches:
                        patches.append(patch)
                    patch_index = patches.index(patch)
                    if first_patch is None:
                        first_patch = patch_index
                    else:
                        marks[layer_start[layer]] = patch_index
                    instruments[(layer, voice)] = (0, volume, 0)
                    notes.append(f"${layer:04x} voice {voice}: custom {patch_index} vol {volume} pitch {sounding:.0f}")
                else:
                    number, transpose = nearest_builtin(patch)
                    instruments[(layer, voice)] = (number, volume, transpose)
                    notes.append(f"${layer:04x} voice {voice}: built-in {number} {transpose:+d} vol {volume} pitch {sounding:.0f}")
        body = [f"; {index + 1} {name}: " + "; ".join(notes)]
        if looping:
            starts = [frame_of(lp[0], origin) for lp in loops if lp]
            span = next(s - b for b, s in filter(None, loops)) // TICKS_PER_FRAME
            body.append(f";   loop {span} frames, from frame {', '.join(map(str, starts))}")
        else:
            body.append(f";   {frame_of(stop, origin)} frames")
        size = 0
        for ch in range(6):
            label = f"arc_{name}_ch{ch}"
            channel_marks = marks if ch == 0 else {}      # the patch loads go in channel 0
            if looping and loops[ch]:
                start, stop = loops[ch]
                intro = encode_channel(events[ch], instruments, origin, start, origin, channel_marks)
                loop = encode_channel(events[ch], instruments, start, stop, origin, channel_marks)
                # The loop starts with the channel's instrument, whatever the intro left.
                first_key = next(((e[4], e[3]) for e in events[ch] if start <= e[0] < stop and e[1] != "off"), None)
                if first_key is not None and 0x62 not in loop[:4]:
                    number, volume, _ = instruments[first_key]
                    loop = [0x62, number << 4 | volume] + loop
                body.append(f"{label}:")
                body += format_bytes(intro)
                body.append(f"{label}_loop:")
                body += format_bytes(loop)
                body.append(f"    .byte $ff, <{label}_loop, >{label}_loop")
                size += len(intro) + len(loop) + 3
            else:
                if loops[ch]:
                    end = loops[ch][1]
                elif events[ch]:
                    end = max(e[0] for e in events[ch]) + TICKS_PER_FRAME
                else:
                    end = origin + TICKS_PER_FRAME
                data = encode_channel(events[ch], instruments, origin, end, origin, channel_marks) + [0xFE]
                body.append(f"{label}:")
                body += format_bytes(data)
                size += len(data)
        songs.append((name, first_patch, body, size))

    # Fill the data banks in song order.
    placement, used = {}, {bank: 1 for bank in DATA_BANKS}
    for name, _, _, size in songs:
        bank = next((b for b in DATA_BANKS if used[b] + size <= 0x2000), None)
        if bank is None:
            raise SystemExit(f"{name} ({size} bytes) does not fit the data banks")
        placement[name] = bank
        used[bank] += size

    lines += ["; The VRC7 custom patches ($00-$07)."]
    lines += ["arc_patches:"] + [f"    .byte {', '.join(f'${b:02x}' for b in p)}" for p in patches] + [""]
    lines += ["; By song id - 1: the custom patch loaded at the start, the data bank, the streams."]
    lines += ["arc_song_patch:", "    .byte " + ", ".join(str(s[1]) for s in songs)]
    lines += ["arc_song_bank:", "    .byte " + ", ".join(str(placement[s[0]]) for s in songs)]
    lines += ["arc_trk_lo:"] + [f"    .byte {', '.join(f'<arc_{s[0]}_ch{c}' for c in range(6))}" for s in songs]
    lines += ["arc_trk_hi:"] + [f"    .byte {', '.join(f'>arc_{s[0]}_ch{c}' for c in range(6))}" for s in songs]
    for bank in DATA_BANKS:
        lines += ["", f'.segment "BANK{bank}"', f"    .byte bank_tag_base + {bank}"]
        for name, _, body, size in songs:
            if placement[name] == bank:
                lines += [""] + body
    args.output.write_text("\n".join(lines) + "\n", newline="\n")
    print(f"{args.output}: " + ", ".join(f"{s[0]} {s[3]} B in bank {placement[s[0]]}" for s in songs)
          + f"; {len(patches)} custom patches")


if __name__ == "__main__":
    main()
