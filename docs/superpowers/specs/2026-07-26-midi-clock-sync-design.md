# MIDI Clock Sync — Design Spec

## Overview

Add bidirectional MIDI clock synchronization between `synth_front` (host) and
the Teensy groovebox firmware. The host can act as a MASTER clock source or a
SLAVE following the Teensy's internal clock. BPM is adjustable from the host
UI. Minor Teensy firmware changes are required on both sides.

This is Phase 1 of a three-phase plan (MIDI clock → data management → host-side
editing). A future VST3 MIDI effect plugin extracting the clock module is
planned but out of scope for this spec.

## Protocol Additions

Two packet types added to both the host (`src/protocol/protocol.h`) and Teensy
firmware (`src/comms/protocol.h`):

| Packet | Value | Payload | Purpose |
|--------|-------|---------|---------|
| `MIDI_CLOCK` | `0xF8` | (empty) | One tick at 24 PPQN. Already defined in Teensy firmware; add to host. |
| `CLOCK_MODE` | `0x0D` | `[mode: 1 byte]` | Set Teensy `MidiClock` mode: `0` = INTERNAL, `1` = SLAVE. New on both sides. |

## Host MidiClock Module (`src/clock/`)

New module with two files:

- `src/clock/midi_clock.h` — Public API, `MidiClock` class
- `src/clock/midi_clock.cpp` — Implementation

### State Machine

```
enum ClockMode { OFF, MASTER, SLAVE };

OFF:     No clock sent or received. Teensy runs its internal clock.
MASTER:  Host generates 24-PPQN ticks. Sends CLOCK_MODE(SLAVE) to Teensy on entry.
SLAVE:   Host receives ticks from Teensy. Sends CLOCK_MODE(INTERNAL) on entry.
```

### Master Mode

- A dedicated `clock_thread_func()` wakes at the 24-PPQN interval:
  `interval_ms = 60000 / (bpm * 24)`
- Uses coarse `SDL_Delay` (1 ms granularity) followed by a microsecond-accurate
  spin-wait for tighter jitter control.
- Sends `MIDI_CLOCK` packets over `g_serial_mutex`-protected serial write path.
- On transport START: sends `MIDI_START`, resets tick counter, begins ticking.
- On transport STOP: sends `MIDI_STOP`, exits tick loop.
- On transport CONTINUE: sends `MIDI_CONTINUE`, resumes ticking.
- BPM range: 20–300, integer steps.

### Slave Mode

- Serial thread parses incoming `MIDI_CLOCK` (0xF8) packets.
- Updates an atomic tick count and estimates BPM from microsecond intervals
  (same approach as Teensy's `MidiClock::on_tick()`: average over 12 samples).
- Exposes `estimated_bpm()` for UI display.

### Thread Safety

- `ClockMode`, BPM, and transport state protected by a dedicated `std::mutex`.
- Serial thread writes tick count via `std::atomic<uint32_t>`.
- Clock thread synchronizes serial writes with `g_serial_mutex` (already exists).

## ImGui UI

A new **Clock** tab in the existing `ImGui` overlay tab bar with:

| Control | Behavior |
|---------|----------|
| **Clock Source** combo | OFF / MASTER / SLAVE |
| **BPM** slider | Enabled only in MASTER mode. Range 20–300, step 1. |
| **Current BPM** readout | In SLAVE mode, shows estimated BPM from incoming ticks. |
| **Play / Stop** buttons | Sends transport START/STOP packets. |

## main.cpp Integration

1. On startup: initialize `MidiClock` module (mode = OFF).
2. In the main loop: drain clock events (tick count updates in SLAVE mode).
3. On mode change (UI): send `CLOCK_MODE` packet to Teensy, start/stop clock
   thread for MASTER mode.
4. On transport button (UI): call `MidiClock::start()/stop()` which sends
   appropriate packets and manages the clock thread.
5. Serial thread: handle `PacketType::MIDI_CLOCK` (0xF8) to feed slave mode.

## Teensy Firmware Changes

In `src/comms/packet_dispatcher.cpp`:

1. **Handle `PACKET_CLOCK_MODE (0x0D)`**: Call
   `g_midi_clock.set_mode(payload[0] == 1 ? MidiClock::SLAVE : MidiClock::INTERNAL)`.
2. **Emit `PACKET_MIDI_CLOCK` from INTERNAL mode**: When the sequencer advances
   in INTERNAL mode, send a `MIDI_CLOCK` packet so the host can follow as SLAVE.

Also add `PACKET_CLOCK_MODE = 0x0D` to `src/comms/protocol.h`.

## Implementation Order

1. Host: add `MIDI_CLOCK` and `CLOCK_MODE` types to `protocol.h`
2. Host: create `src/clock/midi_clock.h/.cpp` (class, state machine, thread)
3. Host: add Clock tab to ImGui UI (`src/device_panel.cpp` or new file)
4. Host: wire into `main.cpp` (serial thread handler, UI callbacks, init/cleanup)
5. Teensy: add `PACKET_CLOCK_MODE` to `protocol.h` and dispatcher
6. Teensy: emit `MIDI_CLOCK` in INTERNAL mode from sequencer
7. Test: build both sides, verify MASTER drives Teensy, verify SLAVE follows

## Future

A VST3 MIDI effect plugin will extract the clock logic into a standalone
binary that a DAW can load. The DAW provides transport/BPM; the plugin
converts to `MIDI_CLOCK` serial packets. Not in scope for this iteration.
