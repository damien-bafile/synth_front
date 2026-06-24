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

`synth_front` is a thin host-side companion to a Teensy groovebox. It opens a
high-speed serial connection to the device, forwards keyboard/MIDI/touch input,
and renders the device's RGB565 display via OpenGL.

```
src/main.cpp           — Entry point, event loop, input routing
src/audio/             — SDL3 audio passthrough (Teensy → host speakers)
src/input/             — Keyboard → synth key/encoder/transport mapping
src/midi/              — Platform MIDI input (CoreMIDI / ALSA / WinMM)
src/protocol/          — Binary packet protocol + RGB565 framebuffer
src/render/            — OpenGL 3.3 Core renderer
src/serial/            — Cross-platform serial I/O + Teensy auto-detect
src/ui/                — Nuklear immediate-mode GUI backend
```

### Data flow

1. The **serial thread** (`main.cpp`) reads raw bytes from the Teensy and
   parses packets (`src/protocol/packet.cpp`). Display tiles are assembled into
   a double-buffered RGB565 framebuffer (`src/protocol/framebuffer.cpp`).
2. The **main thread** polls SDL events, maps keyboard/touch input to packets,
   drains queued MIDI messages, and sends everything to the Teensy.
3. Each frame the main thread copies the latest completed framebuffer, converts
   it to RGB888, uploads it to an OpenGL texture, and draws a letterboxed quad.
4. An optional Nuklear overlay provides Restart / Close / Minimize controls.

## Protocol

The serial link uses a framed binary protocol shared with the Teensy firmware:

```
[SYNC 0xAA] [type] [length: 4 bytes BE] [payload...] [XOR checksum]
```

- `type` — `PacketType` value (key, encoder, touch, MIDI, frame tile, etc.)
- `length` — big-endian 32-bit payload length (may be zero)
- `checksum` — XOR over `type`, `length`, and `payload`

See `src/protocol/protocol.h` for supported packet types and helper functions.

## Dependencies

CMake fetches most dependencies automatically into `deps/`:

- **SDL3** 3.4.8 — window, OpenGL context, input, audio
- **Google Test** (dev builds) — unit tests
- **glew-cmake** (Windows only) — OpenGL extension loading

System libraries required at link time:
- OpenGL
- CoreMIDI + CoreFoundation (macOS)
- ALSA (Linux)
- WinMM + SetupAPI + OpenGL32 (Windows)

The first configure downloads and builds SDL3, which can take a few minutes.

## Tests

```sh
cmake --build build --target synth_front_tests && ctest --test-dir build -V
```

The test executable links only the protocol and serial modules, so it can run
without SDL3 or a display.

Coverage includes:
- Packet encode/decode round-trips
- Checksum validation and sync recovery
- Framebuffer double-buffering and tile writes
- Encoder and touch packet helpers
