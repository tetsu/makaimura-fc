"""Decode makaimurg's Z80 song and effect data, and check the decoding against MAME.

Reads build/revg-disassembly/sound.bin (written by disassemble-revg.py).

  python tools/mame/decode-revg-sound.py
      writes build/revg-disassembly/sound-sequences.txt: every command's song
      channels or effect frames as readable events.
  python tools/mame/decode-revg-sound.py --capture --mame <dir>
      runs MAME once per command with capture-revg-sound-writes.lua and saves
      the YM2203 writes to build/revg-sound-writes/<command>.txt.
  python tools/mame/decode-revg-sound.py --check
      runs a model of the driver (a line-by-line port of the Z80 code) for each
      captured command and compares its register writes with MAME's, tick by tick.

The model follows the Z80 routines named in the comments; RAM is the real
$c000-$c7ff layout, so state that the code leaves behind behaves the same way.
"""

import argparse
import os
from pathlib import Path
import subprocess

COMMANDS = 0x0120          # 64-word dispatch table
STOP = 0x01A0
SONG_START = 0x02DE
EFFECT_START = 0x0A48
SONG_INIT = {0x02B1: 0xC100, 0x02BA: 0xC200, 0x02C3: 0xC300, 0x02CC: 0xC400}
PITCH_TABLE = 0x01EF       # [block|fnum high, fnum low] words, 12 per octave
LENGTHS = 0x0577           # note lengths by top three bits: 0,0,2,4,8,16,32,64
DOTTED = 0x057F            # the same, times 1.5
VOICES = 0x0BD5            # 32 bytes per voice
SILENT_SSG = 0x0BC9
SONG_LAYERS = (0xC400, 0xC300, 0xC200, 0xC100)       # highest priority first
SSG_SLOTS = ((0xC580, 0xC560, 0xC540, 0xC520, 0xC500),
             (0xC700, 0xC780, 0xC660, 0xC640, 0xC620))
TICKS_PER_FRAME = 4        # the Z80 IRQ runs at 240 Hz
NOTE_NAMES = "C C# D D# E F F# G G# A A# B".split()


def word(rom, address):
    return rom[address] | rom[address + 1] << 8


def command_targets(rom):
    """Return {command: (kind, ram slot, data address, init routine)}."""
    result = {}
    for command in range(64):
        target = word(rom, COMMANDS + command * 2)
        if target == STOP:
            result[command] = ("stop", None, None, None)
        elif rom[target] == 0xDD and rom[target + 1] == 0x21:      # ld ix,slot / ld hl,data
            assert rom[target + 4] == 0x21 and word(rom, target + 8) == EFFECT_START
            result[command] = ("effect", word(rom, target + 2), word(rom, target + 5), None)
        elif rom[target] == 0xCD:                                    # call init / ld hl,header
            init = word(rom, target + 1)
            assert rom[target + 3] == 0x21 and word(rom, target + 7) == SONG_START
            result[command] = ("song", SONG_INIT[init], word(rom, target + 4), init)
        else:
            raise SystemExit(f"command ${command:02x}: unknown stub at ${target:04x}")
    return result


class Driver:
    """The Z80 sound driver, tick by tick. `writes` collects (chip, register, value)."""

    def __init__(self, rom):
        self.rom = rom
        self.ram = bytearray(0x800)
        self.writes = []

    # memory -------------------------------------------------------------
    def rd(self, address):
        address &= 0xFFFF
        if address < 0x8000:
            return self.rom[address]
        if 0xC000 <= address < 0xC800:
            return self.ram[address - 0xC000]
        raise SystemExit(f"read from ${address:04x}")

    def g(self, ix, offset):
        return self.ram[ix + offset - 0xC000]

    def s(self, ix, offset, value):
        self.ram[ix + offset - 0xC000] = value & 0xFF

    def g16(self, ix, high, low):
        return self.g(ix, high) << 8 | self.g(ix, low)

    def s16(self, ix, high, low, value):
        self.s(ix, high, value >> 8)
        self.s(ix, low, value)

    def ym(self, chip, register, value):
        self.writes.append((chip, register & 0xFF, value & 0xFF))

    # $00d4: one IRQ ----------------------------------------------------------
    def tick(self, latch):
        self.ram[0] = (self.ram[0] + 1) & 0xFF
        self.command(latch)
        self.update_songs()
        self.update_effects()
        self.write_ssg()

    # $010e
    def command(self, value):
        if value == self.ram[1]:
            return
        self.ram[1] = value
        if value == 0xFF:
            return
        kind, slot, data, init = command_targets(self.rom)[value & 0x3F]
        if kind == "stop":                       # $01a0
            for base in (0xC500, 0xC620):
                for index in range(5):
                    self.s(base + index * 0x20, 0, 0)
        elif kind == "effect":                   # $0a48
            ix = slot
            self.s16(ix, 0x15, 0x16, data)
            self.s16(ix, 0x17, 0x18, data)
            self.s(ix, 0, self.g(ix, 0) - 1)
            self.s16(ix, 0x13, 0x14, 0)
            self.s(ix, 0x19, 0)
            self.s(ix, 0x08, 0xFF)
        else:
            for index in range(6):               # $02d3
                self.s(slot + index * 0x20, 0, 0)
            self.start_song(slot, data)

    # $02de
    def start_song(self, ix, hl):
        tempo = self.rd(hl)
        hl += 1
        for channel in range(6):
            self.s(ix, 0, self.g(ix, 0) - 1)
            self.s(ix, 0x09, channel)
            self.s(ix, 0x0D, self.rd(hl))
            self.s(ix, 0x0C, self.rd(hl + 1))
            self.s(ix, 0x11, self.rd(hl + 2))
            self.s(ix, 0x08, self.rd(hl + 3))
            hl += 4
            self.s(ix, 0x17, tempo)
            for offset in (0x03, 0x04, 0x1E, 0x1F):
                self.s(ix, offset, 0)
            ix += 0x20

    # $031a: only the highest layer whose first channel is active plays
    def update_songs(self):
        for ix in SONG_LAYERS:
            if self.g(ix, 0):
                break
        else:
            return
        for channel in range(6):
            iy = 0xC010 + channel                # $0365: reload a voice that changed
            if self.ram[iy - 0xC000] != self.g(ix, 0x11):
                self.s(ix, 0x09, channel)
                self.load_voice(ix)
                self.ram[iy - 0xC000] = self.g(ix, 0x11)
            if self.g(ix, 0):
                if self.g(ix, 0x1F) & 1 and self.ram[0] & 1:   # $03d9: $30 triplet
                    self.step(ix)
                self.step(ix)
            ix += 0x20

    # $037a
    def load_voice(self, ix):
        channel = self.g(ix, 0x09)
        chip, c = (0, channel) if channel < 3 else (1, channel - 3)
        hl = (self.g(ix, 0x11) * 32 + VOICES) & 0xFFFF
        register = 0x30 + c
        for _ in range(24):
            self.ym(chip, register, self.rd(hl))
            register += 4
            hl += 1
        self.ym(chip, register + 0x20, self.rd(hl))
        hl += 1
        mask = self.rd(hl)
        a = channel
        if a < 3:
            a = ((a & 3) + mask) & 0xFF
            self.s(ix, 0x05, a)
        self.s(ix, 0x06, (((a - 3) & 3) + mask) & 0xFF)

    # $03ea
    def step(self, ix):
        count = self.g16(ix, 0x03, 0x04)
        if count:
            count -= 1
            if count:
                self.s16(ix, 0x03, 0x04, count)
                if self.g(ix, 0x1E):
                    return
                gate = self.g16(ix, 0x0A, 0x0B)
                if not gate:
                    return
                gate -= 1
                self.s16(ix, 0x0A, 0x0B, gate)
                if gate:
                    return
                if self.g(ix, 0x09) < 3:         # $0417: key off
                    self.ym(0, 0x28, self.g(ix, 0x05) & 3)
                else:
                    self.ym(1, 0x28, self.g(ix, 0x06) & 3)
                return
        self.s(ix, 0x1F, 0)                      # $0444
        while True:                              # $0448
            event = self.fetch(ix)
            low = event & 0x1F
            if low == 0:                         # rest
                self.rest(ix)
                self.length(ix, LENGTHS)
                return
            if low == 0x1F:                      # $059b
                kind = event >> 5
                if kind == 0:                    # $1f nn: pitch table base
                    base = (PITCH_TABLE + ((self.fetch(ix) * 2) & 0xFF)) & 0xFFFF
                    self.s16(ix, 0x13, 0x14, base)
                elif kind == 1:                  # $3f nn: tempo
                    self.s(ix, 0x17, self.fetch(ix))
                elif kind == 2:                  # $5f nn: stored, never read
                    self.s(ix, 0x15, self.fetch(ix))
                elif kind == 3:                  # $7f lo hi: jump
                    low_byte = self.fetch(ix)
                    self.s16(ix, 0x0C, 0x0D, self.fetch(ix) << 8 | low_byte)
                elif kind == 4:                  # $9f: repeat start
                    self.s(ix, 0x0E, self.g(ix, 0x0C))
                    self.s(ix, 0x0F, self.g(ix, 0x0D))
                    self.s(ix, 0x10, 0)
                elif kind == 5:                  # $bf nn: repeat end
                    times = self.fetch(ix)
                    self.s(ix, 0x10, self.g(ix, 0x10) + 1)
                    if times >= self.g(ix, 0x10):
                        self.s(ix, 0x0C, self.g(ix, 0x0E))
                        self.s(ix, 0x0D, self.g(ix, 0x0F))
                elif kind == 6:                  # $df: dotted note or rest
                    if self.fetch(ix) & 0x1F == 0:
                        self.rest(ix)
                        self.length(ix, DOTTED)
                        return
                    self.key_on(ix)
                    self.s(ix, 0, self.g(ix, 0) | 0x40)
                    self.length(ix, DOTTED)
                    return
                else:                            # $ff: channel ends
                    self.s(ix, 0, 0)
                    return
                continue
            if event & 0xE0 == 0:                # $01-$1e nn: voice and gate
                self.s(ix, 0x11, event)
                self.s(ix, 0x08, self.fetch(ix))
                self.load_voice(ix)
                continue
            if event < 0x40:                     # $21-$3e
                if event == 0x30:
                    self.s(ix, 0x1F, 1)          # next note at double speed
                else:
                    self.s(ix, 0x1E, (event + 1) & 0x1F)   # no key off for n notes
                continue
            if self.g(ix, 0x1E):                 # note
                self.s(ix, 0x1E, self.g(ix, 0x1E) - 1)
            self.key_on(ix)
            self.length(ix, LENGTHS)
            return

    # $060e
    def fetch(self, ix):
        pointer = self.g16(ix, 0x0C, 0x0D)
        value = self.rd(pointer)
        self.s(ix, 0x16, value)
        self.s16(ix, 0x0C, 0x0D, pointer + 1)
        return value

    # $04b1
    def rest(self, ix):
        self.s(ix, 0x01, 0)
        self.s(ix, 0x02, 0)
        self.s(ix, 0x07, 2)

    # $04bd
    def key_on(self, ix):
        pointer = self.g16(ix, 0x13, 0x14) + (self.g(ix, 0x16) & 0x1F) * 2
        self.s(ix, 0x01, self.rd(pointer))
        self.s(ix, 0x02, self.rd(pointer + 1))
        self.s(ix, 0, self.g(ix, 0) | 0x80)
        self.s(ix, 0x07, 1)
        channel = self.g(ix, 0x09)
        if channel < 3:
            self.ym(0, 0x28, self.g(ix, 0x05))
            self.ym(0, 0xA4 + channel, self.g(ix, 0x01))
            self.ym(0, 0xA0 + channel, self.g(ix, 0x02))
        else:
            self.ym(1, 0x28, self.g(ix, 0x06))
            self.ym(1, 0xA1 + channel, self.g(ix, 0x01))
            self.ym(1, 0x9D + channel, self.g(ix, 0x02))

    # $052f
    def length(self, ix, table):
        units = self.rd(table + (self.g(ix, 0x16) >> 5))
        count = (self.g(ix, 0x17) * units) & 0xFFFF
        self.s16(ix, 0x03, 0x04, count)
        ratio = self.g(ix, 0x08) & 0x0F or 1
        self.s16(ix, 0x0A, 0x0B, (count * (ratio + 1) >> 4) & 0xFFFF)

    # $0620: advance the highest active effect on each chip's SSG
    def update_effects(self):
        for slots in SSG_SLOTS:
            for ix in slots:
                if self.g(ix, 0):
                    self.effect_tick(ix)
                    break

    # $0697
    def effect_tick(self, ix):
        wait = self.g16(ix, 0x13, 0x14)
        if wait:
            self.s16(ix, 0x13, 0x14, wait - 1)
            self.sweep(ix)
            return
        if self.effect_event(ix, self.effect_fetch(ix)):          # $074d
            self.s16(ix, 0x13, 0x14, self.g16(ix, 0x13, 0x14) - 1)   # $0738

    # $06ad
    def sweep(self, ix):
        for step, low in ((0x0C, 0x01), (0x0D, 0x03), (0x0E, 0x05)):
            delta = self.g(ix, step)
            if delta:
                value = self.g(ix, low) | self.g(ix, low + 1) << 8
                value = (value + delta - (0x100 if delta & 0x80 else 0)) & 0xFFFF
                self.s(ix, low, value)
                self.s(ix, low + 1, value >> 8)
        if self.ram[0] & 3:
            return
        for step, target in ((0x0F, 0x09), (0x10, 0x0A), (0x11, 0x0B), (0x12, 0x07)):
            if self.g(ix, step):
                self.s(ix, target, self.g(ix, target) + self.g(ix, step))

    # $090d: reads one byte and peeks at the next
    def effect_fetch(self, ix):
        pointer = self.g16(ix, 0x15, 0x16)
        self.s(ix, 0x1A, self.rd(pointer))
        self.s(ix, 0x1B, self.rd(pointer + 1))
        self.s16(ix, 0x15, 0x16, pointer + 1)
        return self.g(ix, 0x1A)

    # $0754; returns False when the effect ended
    def effect_event(self, ix, value):
        while True:
            kind = value >> 2 & 7
            if kind == 0:                        # $076a: wait, then settings
                self.s(ix, 0x13, self.g(ix, 0x1A))
                self.effect_fetch(ix)
                self.s(ix, 0x14, self.g(ix, 0x1A))
                while self.g(ix, 0x1B) & 0xE0:   # $07c1
                    redo = self.effect_setting(ix)
                    if redo is not None:
                        value = redo
                        break
                else:
                    return True
                continue
            if kind == 1:                        # $077c: repeat start
                self.s(ix, 0x17, self.g(ix, 0x15))
                self.s(ix, 0x18, self.g(ix, 0x16))
                self.s(ix, 0x19, 0)
            elif kind == 2:                      # $078f: repeat end
                times = self.effect_fetch(ix)
                self.s(ix, 0x19, self.g(ix, 0x19) + 1)
                if times != self.g(ix, 0x19):
                    self.s(ix, 0x15, self.g(ix, 0x17))
                    self.s(ix, 0x16, self.g(ix, 0x18))
            elif kind == 3:                      # $07ab: jump
                low = self.effect_fetch(ix)
                high = self.effect_fetch(ix)
                self.s16(ix, 0x15, 0x16, high << 8 | low)
            else:                                # $07bb: end
                self.s(ix, 0, 0)
                return False
            value = self.effect_fetch(ix)

    # $07cd; returns an event byte to dispatch again when the setting is $00-$1f
    def effect_setting(self, ix):
        value = self.effect_fetch(ix)
        kind = value >> 5
        if kind == 0:
            return self.g(ix, 0x1A)
        if kind <= 3:                            # tone period A/B/C and its sweep
            bit = kind - 1
            period = (value & 0x0F) << 8 | self.effect_fetch(ix)
            low = 0x01 + bit * 2
            if period == 0:
                self.s(ix, low, 0)
                self.s(ix, low + 1, 0)
                self.s(ix, 0x08, self.g(ix, 0x08) | 1 << bit)
            elif period != 0x0FFF:
                self.s(ix, 0x08, self.g(ix, 0x08) & ~(1 << bit))
                period = period << 4 & 0xFFFF
                self.s(ix, low, period)
                self.s(ix, low + 1, period >> 8)
            self.s(ix, 0x0C + bit, self.effect_fetch(ix))
        elif kind <= 6:                          # volume A/B/C and its sweep
            index = kind - 4
            if value & 0x1F != 0x1F:
                self.s(ix, 0x09 + index, (value & 0x1F) << 4)
            self.s(ix, 0x0F + index, self.effect_fetch(ix))
        else:                                    # noise period, noise enables, sweep
            if value & 0x1F != 0x1F:
                self.s(ix, 0x07, (value & 0x1F) << 3)
            enables = self.effect_fetch(ix) & 0x38
            self.s(ix, 0x08, (self.g(ix, 0x08) & 0xC7) + enables)
            self.s(ix, 0x12, self.effect_fetch(ix))
        return None

    # $0a68
    def write_ssg(self):
        for chip, slots in enumerate(SSG_SLOTS):
            for ix in slots:
                if self.g(ix, 0):
                    values = []
                    for low in (0x01, 0x03, 0x05):
                        value = (self.g(ix, low) | self.g(ix, low + 1) << 8) >> 4
                        values += [value & 0xFF, value >> 8]
                    values += [self.g(ix, 0x07) >> 3, self.g(ix, 0x08)]
                    values += [self.g(ix, offset) >> 4 for offset in (0x09, 0x0A, 0x0B)]
                    break
            else:
                values = list(self.rom[SILENT_SSG + 1:SILENT_SSG + 12])
            for register, value in enumerate(values):
                self.ym(chip, register, value)


# ---- readable listing -------------------------------------------------------

def pitch_name(rom, base, index):
    """Name a note from the pitch-table entry it reads.

    With the 1.5 MHz clock and the 1/3 prescaler the driver selects ($2d, $2e),
    f = fnum * 1.5 MHz / (72 * 2^(21 - block)); entry n of the table is then
    MIDI note n (entry 1, block 0 fnum $367, is 8.65 Hz = C#-1).
    """
    entry = (base + index * 2 - PITCH_TABLE) // 2
    if not 0 <= entry < (0x02B1 - PITCH_TABLE) // 2:
        return f"?{index:02x}"
    if entry == 0:
        return "silent"
    return f"{NOTE_NAMES[entry % 12]}{entry // 12 - 1}"


def song_channel(rom, pointer, tempo):
    """List one song channel's events until it ends or jumps back."""
    lines, seen, base = [], set(), None
    while pointer not in seen:
        seen.add(pointer)
        at = pointer
        event = rom[pointer]
        pointer += 1
        low = event & 0x1F
        if low == 0x1F:
            kind = event >> 5
            if kind == 0:
                base = PITCH_TABLE + ((rom[pointer] * 2) & 0xFF)
                text = f"octave base {rom[pointer]:02x}"
                pointer += 1
            elif kind == 1:
                tempo = rom[pointer]
                text = f"tempo {tempo}"
                pointer += 1
            elif kind == 2:
                text = f"param {rom[pointer]:02x} (unused)"
                pointer += 1
            elif kind == 3:
                target = word(rom, pointer)
                lines.append(f"  {at:04x}: jump ${target:04x}")
                if target in seen:
                    break
                pointer = target
                continue
            elif kind == 4:
                text = "repeat start"
            elif kind == 5:
                text = f"repeat end, play {rom[pointer] + 1} times"
                pointer += 1
            elif kind == 6:
                following = rom[pointer]
                pointer += 1
                text = "dotted " + note_text(rom, base, following, tempo, DOTTED)
            else:
                lines.append(f"  {at:04x}: end")
                break
        elif low == 0:
            text = note_text(rom, base, event, tempo, LENGTHS)
        elif event & 0xE0 == 0:
            text = f"voice {event:02x} gate {rom[pointer] & 0x0F}"
            pointer += 1
        elif event < 0x40:
            text = "triplet: next note counts down 1.5 per tick" if event == 0x30 else \
                f"legato for {((event + 1) & 0x1F)} notes"
        else:
            text = note_text(rom, base, event, tempo, LENGTHS)
        lines.append(f"  {at:04x}: {text}")
    return lines


def note_text(rom, base, event, tempo, table):
    units = rom[table + (event >> 5)]
    ticks = tempo * units
    if event & 0x1F == 0:
        return f"rest {units} ({ticks} ticks)"
    name = pitch_name(rom, base, event & 0x1F) if base is not None else f"#{event & 0x1F}"
    return f"{name} {units} ({ticks} ticks)"


def effect_frames(rom, pointer):
    """List one effect's frames until it ends or jumps back."""
    lines, seen = [], set()
    while pointer not in seen:
        seen.add(pointer)
        at = pointer
        value = rom[pointer]
        kind = value >> 2 & 7
        if kind == 0:
            wait = value << 8 | rom[pointer + 1]
            pointer += 2
            settings = []
            while rom[pointer] & 0xE0:
                setting = rom[pointer]
                group = setting >> 5
                if group <= 3:
                    period = (setting & 0x0F) << 8 | rom[pointer + 1]
                    channel = "ABC"[group - 1]
                    state = "off" if period == 0 else "keep" if period == 0x0FFF \
                        else f"{period:03x}"
                    settings.append(f"tone{channel} {state} sweep {signed(rom[pointer + 2])}")
                    pointer += 3
                elif group <= 6:
                    channel = "ABC"[group - 4]
                    level = setting & 0x1F
                    state = "keep" if level == 0x1F else str(level)
                    settings.append(f"vol{channel} {state} sweep {signed(rom[pointer + 1])}")
                    pointer += 2
                else:
                    level = setting & 0x1F
                    state = "keep" if level == 0x1F else str(level)
                    noise = "".join(c for bit, c in ((8, "A"), (16, "B"), (32, "C"))
                                    if not rom[pointer + 1] & bit) or "none"
                    settings.append(f"noise {state} on {noise} sweep {signed(rom[pointer + 2])}")
                    pointer += 3
            lines.append(f"  {at:04x}: wait {wait} " + ", ".join(settings))
        elif kind == 1:
            lines.append(f"  {at:04x}: repeat start")
            pointer += 1
        elif kind == 2:
            lines.append(f"  {at:04x}: repeat end, play {rom[pointer + 1]} times")
            pointer += 2
        elif kind == 3:
            target = word(rom, pointer + 1)
            lines.append(f"  {at:04x}: jump ${target:04x}")
            if target in seen:
                break
            pointer = target
        else:
            lines.append(f"  {at:04x}: end")
            break
    return lines


def signed(value):
    return value - 0x100 if value & 0x80 else value


def listing(rom):
    lines = []
    for command, (kind, slot, data, _) in command_targets(rom).items():
        if kind == "stop":
            lines.append(f"command ${command:02x}: stop effects")
        elif kind == "effect":
            chip = 1 if slot >= 0xC600 else 0
            lines.append(f"command ${command:02x}: effect at ${data:04x}, "
                         f"slot ${slot:04x} (YM{chip + 1} SSG)")
            lines += effect_frames(rom, data)
        else:
            tempo = rom[data]
            lines.append(f"command ${command:02x}: song at ${data:04x}, layer ${slot:04x}, "
                         f"tempo {tempo}")
            for channel in range(6):
                entry = data + 1 + channel * 4
                pointer = word(rom, entry)
                lines.append(f" FM{channel + 1} at ${pointer:04x} voice {rom[entry + 2]:02x} "
                             f"gate {rom[entry + 3] & 0x0F}")
                lines += song_channel(rom, pointer, tempo)
        lines.append("")
    return lines


# ---- MAME comparison --------------------------------------------------------

def read_capture(path):
    ticks, current = [], None
    for line in path.read_text().splitlines():
        if line == "T":
            current = []
            ticks.append(current)
        elif line.startswith("W "):
            _, chip, register, value = line.split()
            current.append((int(chip), int(register, 16), int(value, 16)))
    return ticks[:-1]          # MAME exits part way through the last tick


def check(rom, directory):
    failures = 0
    for command in range(64):
        path = directory / f"{command:02X}.txt"
        if not path.exists():
            continue
        expected = read_capture(path)
        driver = Driver(rom)
        for tick, writes in enumerate(expected):
            driver.writes = []
            driver.tick(command)
            if driver.writes != writes:
                failures += 1
                print(f"${command:02x}: tick {tick} differs")
                print("  mame :", " ".join(f"{c}:{r:02x}={v:02x}" for c, r, v in writes))
                print("  model:", " ".join(f"{c}:{r:02x}={v:02x}" for c, r, v in driver.writes))
                break
        else:
            total = sum(len(w) for w in expected)
            print(f"${command:02x}: {len(expected)} ticks, {total} writes match")
    if failures:
        raise SystemExit(f"{failures} commands differ")


def capture(mame, directory, frames):
    directory.mkdir(parents=True, exist_ok=True)
    for command in range(64):
        environment = dict(os.environ, REVG_SOUND_COMMAND=f"{command:02X}",
                           REVG_SOUND_FRAMES=str(frames),
                           REVG_SOUND_OUTPUT=str(directory / f"{command:02X}.txt"))
        subprocess.run(
            [str(mame / "mame.exe"), "makaimurg", "-rompath", "reference", "-noreadconfig",
             "-video", "none", "-sound", "none", "-nothrottle", "-skip_gameinfo",
             "-autoboot_delay", "0",
             "-autoboot_script", "tools/mame/capture-revg-sound-writes.lua",
             "-cfg_directory", str(directory / "cfg"),
             "-nvram_directory", str(directory / "nvram"),
             "-seconds_to_run", str(frames // 60 + 30)],
            env=environment, check=True, capture_output=True)
        print(f"captured ${command:02x}")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--directory", type=Path, default=Path("build/revg-disassembly"))
    parser.add_argument("--writes", type=Path, default=Path("build/revg-sound-writes"))
    parser.add_argument("--capture", action="store_true")
    parser.add_argument("--mame", type=Path)
    parser.add_argument("--frames", type=int, default=600)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    rom = (args.directory / "sound.bin").read_bytes()
    if args.capture:
        if not args.mame:
            parser.error("--capture needs --mame")
        capture(args.mame, args.writes, args.frames)
    if args.check:
        check(rom, args.writes)
    if not (args.capture or args.check):
        output = args.directory / "sound-sequences.txt"
        output.write_text("\n".join(listing(rom)) + "\n", encoding="ascii", newline="\n")
        print(output)


if __name__ == "__main__":
    main()
