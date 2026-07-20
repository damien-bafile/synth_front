# Teensy Restart CLI Flags

## Summary

Add three immediate-exit CLI flags to `synth_front`: `--list-ports`, `--soft-reset`, and
`--hard-reset`. These follow the existing `--list-midi` / `--list-audio` pattern — the
program performs a single action and exits without entering the main event loop.

## Flags

| Flag | Arg? | Behavior |
|------|------|----------|
| `--list-ports` | None | Enumerate all detected serial ports via `find_serial_ports()` and print each on its own line; exit 0. No serial connection is opened. |
| `--soft-reset` | None | Open serial (auto-detect or use `--port` if also given), send `PacketType::RESET` (0x0C), close, exit 0. |
| `--hard-reset` | None | Open serial (auto-detect or use `--port` if also given), toggle DTR to trigger Teensy bootloader reset, close, exit 0. |

`--soft-reset` and `--hard-reset` are mutually exclusive with each other and with normal
GUI launch. `--list-ports` is an immediate-exit flag like `--list-midi` — it always
exits and can be combined with other immediate-exit flags (only the first takes effect).
If `--soft-reset` or `--hard-reset` is given without `--port`, auto-detection is used
(via `find_teensy_port()`). If no Teensy is found, print an error and exit 1.

## Components

### 1. Serial port: `serial_set_dtr()`

New function in `serial_port.h` / `serial_port.c`:

```c
/// Set or clear the DTR line.
/// @param fd    serial port fd from serial_open()
/// @param level 1 = assert DTR, 0 = clear DTR
/// @return 0 on success, -1 on error
int serial_set_dtr(int fd, int level);
```

**Platform implementations:**

- **Unix (macOS/Linux):** `ioctl(fd, TIOCMSET, &flags)` toggling `TIOCM_DTR`.
- **Windows:** `EscapeCommFunction(h, SETDTR)` / `EscapeCommFunction(h, CLRDTR)` where `h` is
  `(HANDLE)(intptr_t)fd`.

**Reset sequence for `--hard-reset`:** assert DTR for 100ms, then clear DTR. This produces a
negative-going pulse on the DTR line that the Teensy's auto-reset circuit (DTR → 0.1µF cap →
MCU RESET pin) converts into a brief RESET assertion.

### 2. CLI parsing: `main.cpp`

Add three new `else if` branches to the existing flag loop:

```cpp
} else if (std::strcmp(argv[i], "--list-ports") == 0) {
    for (const auto& p : find_serial_ports())
        fmt::print("{}\n", p);
    return 0;
} else if (std::strcmp(argv[i], "--soft-reset") == 0) {
    do_soft_reset = true;
} else if (std::strcmp(argv[i], "--hard-reset") == 0) {
    do_hard_reset = true;
}
```

After the loop, add a validation section:

```cpp
if (do_soft_reset && do_hard_reset) {
    fmt::print(stderr, "error: --soft-reset and --hard-reset are mutually exclusive\n");
    return 1;
}
```

And a reset dispatch section (placed before the main init, after port selection):

```cpp
if (do_soft_reset) {
    if (port.empty() && (port = find_teensy_port()).empty()) {
        fmt::print(stderr, "error: no teensy found\n");
        return 1;
    }
    int fd = serial_open(port.c_str(), 2000000);
    if (fd < 0) { perror("serial_open"); return 1; }
    packet_send(fd, PacketType::RESET, nullptr, 0);
    serial_close(fd);
    return 0;
}
if (do_hard_reset) {
    if (port.empty() && (port = find_teensy_port()).empty()) {
        fmt::print(stderr, "error: no teensy found\n");
        return 1;
    }
    int fd = serial_open(port.c_str(), 2000000);
    if (fd < 0) { perror("serial_open"); return 1; }
    serial_set_dtr(fd, 1);
    SDL_Delay(100);
    serial_set_dtr(fd, 0);
    serial_close(fd);
    return 0;
}
```

### 3. Build: `CMakeLists.txt`

No changes needed. No new source files.

## Error handling

| Scenario | Behavior |
|----------|----------|
| Both `--soft-reset` and `--hard-reset` given | Print error to stderr, exit 1 |
| No Teensy found (auto-detect) | Print "no teensy found" to stderr, exit 1 |
| Serial open fails | `perror("serial_open")`, exit 1 |
| `serial_set_dtr()` fails | Print error to stderr, exit 1 |

## Testing

The reset flags can be tested manually:
- `./build/synth_front --list-ports` — should print port names
- `./build/synth_front --soft-reset` — should restart the Teensy's software
- `./build/synth_front --hard-reset` — should trigger a full Teensy reboot
- `./build/synth_front --soft-reset --hard-reset` — should error
- `./build/synth_front --list-ports --soft-reset` — should error (mutually exclusive)

The `serial_set_dtr()` function is covered by the existing test suite via `serial_port.c`
linkage (protocol tests link it already).
