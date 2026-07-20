# Teensy Restart CLI Flags Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add `--list-ports`, `--soft-reset`, and `--hard-reset` CLI flags to synth_front.

**Architecture:** Three immediate-exit flags parsed in main.cpp's existing ad-hoc loop. `--soft-reset` sends `PacketType::RESET` over the serial protocol. `--hard-reset` toggles DTR via a new `serial_set_dtr()` function in the cross-platform serial port layer. All follow the existing `--list-midi` / `--list-audio` pattern.

**Tech Stack:** C++20/C, CMake/Ninja, serial_port (termios/Win32)

**Global Constraints**

- `serial_set_dtr()` must compile on macOS, Linux, and Windows (MSYS2/MinGW).
- CLI flags must follow the existing ad-hoc `strcmp` pattern in main.cpp (no argparse).
- `--soft-reset` and `--hard-reset` are mutually exclusive.
- `--soft-reset` / `--hard-reset` respect `--port` if given, else auto-detect via `find_teensy_port()`.

---

### Task 1: Add `serial_set_dtr()` to serial_port

**Files:**
- Modify: `src/serial/serial_port.h` — add declaration
- Modify: `src/serial/serial_port.c` — add implementations (Win32 + Unix)

- [ ] **Step 1: Add declaration to serial_port.h**

Insert after `serial_write`:

```c
/// Set or clear the DTR (Data Terminal Ready) modem control line.
/// @param fd    Open serial port descriptor.
/// @param level 1 = assert DTR, 0 = clear DTR.
/// @return 0 on success, -1 on error.
int serial_set_dtr(int fd, int level);
```

- [ ] **Step 2: Add Win32 implementation to serial_port.c**

In the `#ifdef _WIN32` block, before the `#else`:

```c
int serial_set_dtr(int fd, int level) {
  HANDLE h = INT_TO_HANDLE(fd);
  if (!EscapeCommFunction(h, level ? SETDTR : CLRDTR))
    return -1;
  return 0;
}
```

- [ ] **Step 3: Add Unix implementation to serial_port.c**

In the `#else` block (after `serial_write`):

```c
int serial_set_dtr(int fd, int level) {
  int flags;
  if (ioctl(fd, TIOCMGET, &flags) < 0)
    return -1;
  if (level)
    flags |= TIOCM_DTR;
  else
    flags &= ~TIOCM_DTR;
  if (ioctl(fd, TIOCMSET, &flags) < 0)
    return -1;
  return 0;
}
```

### Task 2: Add CLI flags to main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add flag variables and parse `--list-ports`**

Add to the flag parsing loop:

```cpp
    } else if (std::strcmp(argv[i], "--list-ports") == 0) {
      auto ports = find_serial_ports();
      for (const auto& p : ports)
        fprintf(stdout, "%s\n", p.c_str());
      return 0;
    } else if (std::strcmp(argv[i], "--soft-reset") == 0) {
      do_soft_reset = true;
    } else if (std::strcmp(argv[i], "--hard-reset") == 0) {
      do_hard_reset = true;
    }
```

And add the local variables before the loop:

```cpp
  bool do_soft_reset = false;
  bool do_hard_reset = false;
```

- [ ] **Step 2: Add mutual exclusion check and reset dispatch**

After the flag parsing loop, before the `if (port.empty())` block:

```cpp
  if (do_soft_reset && do_hard_reset) {
    fprintf(stderr, "error: --soft-reset and --hard-reset are mutually exclusive\n");
    return 1;
  }

  if (do_soft_reset || do_hard_reset) {
    if (port.empty())
      port = find_teensy_port();
    if (port.empty()) {
      fprintf(stderr, "error: no Teensy found\n");
      return 1;
    }
    int fd = serial_open(port.c_str(), 2000000);
    if (fd < 0) {
      fprintf(stderr, "error: failed to open %s\n", port.c_str());
      return 1;
    }
    if (do_soft_reset) {
      packet_send(fd, PacketType::RESET, nullptr, 0);
    } else {
      serial_set_dtr(fd, 1);
      SDL_Delay(100);
      serial_set_dtr(fd, 0);
    }
    serial_close(fd);
    return 0;
  }
```

### Task 3: Build and verify

- [ ] **Step 1: Build**

Run: `cmake --build build`

- [ ] **Step 2: Run tests**

Run: `ctest --test-dir build -V`

### Task 4: Commit

- [ ] **Step 1: Commit**

```bash
git add -A
git commit -m "feat: add --list-ports, --soft-reset, --hard-reset CLI flags"
```
