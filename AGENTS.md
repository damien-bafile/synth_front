# AGENTS.md — synth_front

This file contains agent-focused guidance for working on `synth_front`, a
host-side front-end for a Teensy-based groovebox.

## Project overview

- **Language:** C++20 with some C modules.
- **Build system:** CMake 3.16+ with Ninja.
- **Primary deps:** SDL3 (windowing, OpenGL, audio, input), Dear ImGui (UI).
- **Platforms:** Linux, macOS, Windows (MSYS2/MinGW).

## Build commands

```sh
just build        # configure + compile (recommended)
just run          # run the binary
just run -- $ARGS # pass arguments through
just clean        # remove build/
just clean-all    # remove build/ and deps/

# Without just:
cmake -B build -G Ninja && cmake --build build
```

The first configure fetches SDL3 and Google Test into `deps/` and can take
several minutes.

## Tests

```sh
cmake --build build --target synth_front_tests && ctest --test-dir build -V
```

The test target links only protocol/serial code and does not need a display or
audio device.

## Coding conventions

### Comment style

- **Public headers** use Doxygen `///` comments for every function, struct,
  enum, and important constant. Document parameters, return values, and
  thread-safety / ownership notes.
- **Implementation files** start with a `@file` block explaining the module's
  responsibility, followed by brief inline `//` comments for non-obvious logic.
- **Third-party code** (vendored shaders) should generally
  not be reformatted or heavily commented.

### C++ guidance

This project aims to follow the C++ Core Guidelines where practical:

- Prefer `const`/`constexpr` by default.
- Use RAII; avoid naked `new`/`delete` and `malloc`/`free` in new code.
- Prefer `std::unique_ptr` for ownership; raw pointers are non-owning observers.
- Use `enum class` for scoped enumerations.
- Avoid non-const globals where possible; document lifetime and thread-safety
  when they are unavoidable.
- Pass cheap types by value, larger types by `const&`.

### Existing global state in `main.cpp`

`main.cpp` intentionally uses a few globals to share state between the main
thread, serial thread, and MIDI callback:

- `g_conn_fd` — connection file descriptor (serial writes guarded by `g_serial_mutex`).
- `g_running` — atomic flag used to stop the serial thread.
- `g_midi_queue` / `g_midi_head` / `g_midi_tail` — lock-free SPSC ring buffer.

If you refactor this, prefer encapsulating the globals rather than adding more.

## Module boundaries

| Module | Responsibility | Notes |
|--------|---------------|-------|
| `src/protocol/` | Binary packet protocol + RGB565 framebuffer | Linkable from tests without SDL3. |
| `src/serial/` | Serial/TCP I/O + Teensy auto-detect | Plain C interface in `serial_port.h`. |
| `src/render/` | OpenGL 3.3 Core textured quad renderer | Zero-initialize `Renderer` before `renderer_init()`. |
| `src/input/` | SDL keycode → synth action mapping | Returns `InputResult` for main.cpp to route. |
| `src/midi/` | Platform MIDI input | One of `_alsa`, `_coremidi`, `_win32` is compiled per platform. |
| `src/audio/` | SDL3 audio passthrough | Plain C interface. |
| `src/ui/` | (unused — ImGui configured in main.cpp) | |

## Things to avoid


- Do not change the wire-format packet type values in `src/protocol/protocol.h`
  without also updating the Teensy firmware.
- Do not assume all MIDI source files compile on every platform; only one is
  active per build.

## Useful debugging flags

```sh
# List available MIDI inputs
./build/synth_front --list-midi

# Connect to a specific serial port
./build/synth_front --port /dev/ttyACM0

# Verbose ALSA MIDI debugging (Linux)
./build/synth_front --midi-source "some device"
```
