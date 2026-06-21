# synth_front

Front-end for a Teensy-based groovebox. Renders the Teensy's display output via OpenGL and pipes MIDI/audio through the computer.

## Build

Requires CMake 3.16+ and Ninja.

```sh
just build        # configure + compile
just run          # run the binary (optionally: just run -- --port /dev/...)
just run -- $ARGS # run with arguments
just clean        # remove build/
just clean-all    # remove build/ and deps/
```

Or without `just`:

```sh
cmake -B build -G Ninja && cmake --build build
```

## CLI flags

| Flag | Description |
|------|-------------|
| `--port <path>` | Serial port path (auto-detects Teensy if omitted) |
| `--midi-source <name>` | MIDI input device name/prefix |
| `--list-midi` | List available MIDI input sources and exit |

## Key mapping

| Key | Action |
|-----|--------|
| `Up/Down/Left/Right` | Navigate (keys 0x01-0x04) |
| `Enter` | Confirm (0x10) |
| `,` / `.` | Page prev/next (0x07-0x08) |
| `1`-`0` | Button bank (0x50-0x59) |
| `F1` | Button (0x60) |
| `Q`-`I` (top row) | Musical notes C4-B4 |
| `A`-`K` | Encoder rotation (+/- with Shift) |
| `F6` | MIDI Start |
| `F7` / `Escape` | MIDI Stop |
| `F8` / `Tab` | MIDI Continue |
| `-` (minus) | MIDI Start (alternative) |

## Architecture

```
src/main.cpp           — Entry point, event loop
src/audio/             — SDL3 audio passthrough
src/input/             — Keyboard → synth key/encoder/transport mapping
src/midi/              — Platform MIDI input (CoreMIDI / ALSA / WinMM)
src/protocol/          — Binary packet protocol + RGB565 framebuffer
src/render/            — OpenGL 3.3 Core renderer
src/serial/            — Cross-platform serial I/O + Teensy auto-detect
```

## Dependencies

All fetched at CMake configure time:
- **SDL3** 3.4.8 — window, OpenGL context, input, audio
- **Google Test** (dev builds) — unit tests
- **glew-cmake** (Windows only) — OpenGL extension loading

System libraries: OpenGL, CoreMIDI (macOS), ALSA (Linux), WinMM (Windows).

## Tests

```sh
cmake --build build --target synth_front_tests && ctest --test-dir build -V
```

Tests cover packet encode/decode round-trips, checksum validation, and framebuffer double-buffering.
