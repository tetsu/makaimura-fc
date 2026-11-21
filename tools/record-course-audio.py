#!/usr/bin/env python3
"""Record one 512 KiB course from power-on using Mesen 2.1.1 for Windows."""

import argparse
import ctypes as c
import os
from pathlib import Path
import time
import wave


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


class ControllerConfig(c.Structure):
    _fields_ = [
        ("mappings", (c.c_uint16 * 123) * 4),
        ("turbo_speed", c.c_uint32),
        ("type", c.c_uint32),
    ]


class NesConfig(c.Structure):
    # Mesen 2.1.1 SettingTypes.h. Keep the unused settings around the
    # controller and volume fields in their original ABI order.
    _fields_ = [
        ("controllers", ControllerConfig * 12),
        ("light_detection_radius", c.c_uint32),
        ("auto_configure_input", c.c_bool),
        ("region", c.c_uint32),
        ("disk_and_database_flags", c.c_bool * 5),
        ("vs_output", c.c_uint32 * 2),
        ("rendering_flags", c.c_bool * 8),
        ("overscan", c.c_uint32 * 8),
        ("console_type", c.c_uint32),
        ("accuracy_flags", c.c_bool * 18),
        ("ram_power_on_state", c.c_uint32),
        ("extra_scanlines", c.c_uint32 * 2),
        ("apu_and_debug_flags", c.c_bool * 6),
        ("input_scanline", c.c_int32),
        ("full_color_palette", c.c_bool),
        ("palette", c.c_uint32 * 512),
        ("volumes", c.c_uint32 * 11),
        ("epsm_volume", c.c_uint32),
        ("panning", c.c_uint32 * 11),
        ("stereo_filter_settings", c.c_uint32 * 5),
    ]


def path_arg(path):
    return str(path).encode("utf-8")


def record(core_path, rom, route, output, wav_path):
    if os.name != "nt" or c.sizeof(c.c_void_p) != 8:
        raise ValueError("the Mesen recorder requires Windows and 64-bit Python")
    with os.add_dll_directory(str(core_path.parent)):
        core = c.CDLL(str(core_path))

    def bind(name, result, *args):
        function = getattr(core, name)
        function.restype = result
        function.argtypes = args
        return function

    if bind("GetMesenVersion", c.c_uint32)() != 0x020101:
        raise ValueError("this recorder expects MesenCore 2.1.1")

    bind("InitDll", None)()
    try:
        initialize = bind("InitializeEmu", None, c.c_char_p, c.c_void_p, c.c_void_p, *[c.c_bool] * 4)
        initialize(path_arg(output), None, None, True, True, True, True)
        config = bind("GetNesConfig", NesConfig)()
        config.controllers[0].type = 6  # NesController
        config.region = 1  # NTSC
        config.ram_power_on_state = 1  # AllZeros for matching runs
        config.volumes[:] = [100] * len(config.volumes)
        bind("SetNesConfig", None, NesConfig)(config)
        bind("Pause", None)()
        set_flag = bind("SetEmulationFlag", None, c.c_uint32, c.c_bool)
        set_flag(0x10, True)  # ConsoleMode
        set_flag(0x04, True)  # MaximumSpeed
        if not bind("LoadRom", c.c_bool, c.c_char_p, c.c_char_p)(path_arg(rom), None):
            raise ValueError(f"Mesen could not load {rom}")

        load_script = bind("LoadScript", c.c_int32, c.c_char_p, c.c_char_p, c.c_char_p, c.c_int32)
        script_id = load_script(b"course-audio", path_arg(output), route, -1)
        if script_id < 0:
            raise ValueError("Mesen could not load the recording route")

        bind("WaveRecord", None, c.c_char_p)(path_arg(wav_path))
        if not bind("WaveIsRecording", c.c_bool)():
            raise ValueError(f"Mesen could not start recording {wav_path}")

        bind("Resume", None)()
        is_running = bind("IsRunning", c.c_bool)
        get_log = bind("GetScriptLog", None, c.c_int32, c.c_void_p, c.c_uint32)
        buffer = c.create_string_buffer(16384)
        last_log = b""
        deadline = time.monotonic() + 60
        while is_running() and time.monotonic() < deadline:
            # Stopping the ROM discards the Lua log in the core.
            get_log(script_id, buffer, len(buffer))
            last_log = buffer.value or last_log
            time.sleep(0.10)
        bind("WaveStop", None)()

        log_path = wav_path.with_suffix(".log")
        log_path.write_bytes(last_log)
        if is_running():
            raise ValueError("recording route timed out")
        if bind("GetStopCode", c.c_int32)() or not all(
            marker in last_log for marker in (b"shot", b"checkpoint retry", b"clear", b"menu")
        ):
            raise ValueError(f"recording route did not complete; see {log_path}")
    finally:
        bind("Release", None)()

    with wave.open(str(wav_path), "rb") as wav:
        if not any(wav.readframes(wav.getnframes())):
            raise ValueError("Mesen recorded no audio")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("course", choices=("original", "alternate"))
    parser.add_argument("--mute-shot", action="store_true", help="record a control with pulse 2 at zero volume")
    parser.add_argument("--core", type=Path, default=BUILD / "mesen/MesenCore.dll")
    args = parser.parse_args()

    output = BUILD / "audio" / "course-512k"
    try:
        output.mkdir(parents=True, exist_ok=True)
        rom = BUILD / "vrc7a-test-512k.nes"
        route = (ROOT / "tools/mesen/course-audio.lua").read_bytes()
        alternate = "true" if args.course == "alternate" else "false"
        mute = "true" if args.mute_shot else "false"
        route = f"local alternateCourse, muteShot = {alternate}, {mute}\n".encode() + route
        suffix = "-muted-shot" if args.mute_shot else ""
        wav_path = output / f"{args.course}{suffix}.wav"
        record(args.core.resolve(), rom, route, output, wav_path)
    except (OSError, EOFError, ValueError, wave.Error) as error:
        parser.exit(1, f"{error}\n")
    print(wav_path)


if __name__ == "__main__":
    main()
