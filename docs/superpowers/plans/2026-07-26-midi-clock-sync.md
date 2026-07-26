# MIDI Clock Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add bidirectional MIDI clock synchronization between synth_front (host) and the Teensy groovebox firmware.

**Architecture:** New `src/clock/` module with a `MidiClock` class managing a three-state machine (OFF/MASTER/SLAVE). Master mode uses a dedicated thread pacing MIDI_CLOCK packets at 24 PPQN with spin-wait refinement. Slave mode parses incoming MIDI_CLOCK packets in the serial thread and estimates BPM from tick intervals. A new `CLOCK_MODE (0x0D)` packet tells the Teensy which mode to use. Minor Teensy-side changes add the CLOCK_MODE handler and INTERNAL-mode clock emission.

**Tech Stack:** C++20, SDL3 (timer, thread, mutex), Dear ImGui, Google Test

## Global Constraints

- Packet type values must match between host (`synth_front/src/protocol/protocol.h`) and Teensy (`teensy_groovebox/src/comms/protocol.h`)
- Thread safety: clock thread writes serial via existing `g_serial_mutex`; serial thread writes tick count via `std::atomic`
- BPM range: 20–300, integer steps
- MIDI clock: 24 ticks per quarter note
- New code follows existing conventions: Doxygen on public headers, no naked new/delete, RAII, `enum class`

---

### Task 1: Add MIDI_CLOCK and CLOCK_MODE packet types to host protocol

**Files:**
- Modify: `src/protocol/protocol.h`

**Interfaces:**
- Consumes: existing `PacketType` enum
- Produces: `PacketType::MIDI_CLOCK = 0xF8`, `PacketType::CLOCK_MODE = 0x0D`

- [ ] **Step 1: Add two new enum values**

In `src/protocol/protocol.h`, add between `TOUCH` and `FRAME`:

```
  CLOCK_MODE = 0x0D, ///< Set Teensy MidiClock mode: payload [0=INTERNAL, 1=SLAVE].
```

And between `MIDI_PITCH_BEND` and `MIDI_START`:

```
  MIDI_CLOCK = 0xF8, ///< MIDI timing clock tick (24 per quarter note).
```

- [ ] **Step 2: Build to verify**

```bash
cmake --build build
```

Expected: compiles without warnings or errors.

---

### Task 2: Update CMakeLists.txt — add clock source to main target

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add clock source to SOURCES list**

In `CMakeLists.txt`, add to the `set(SOURCES ...)` block:

```
  "${CMAKE_SOURCE_DIR}/src/clock/midi_clock.cpp"
```

- [ ] **Step 2: Build to verify**

```bash
cmake --build build
```

Expected: compiles without errors.

---

### Task 3: Create MidiClock class (header)

**Files:**
- Create: `src/clock/midi_clock.h`

**Interfaces:**
- Consumes: `PacketType::MIDI_CLOCK`, `PacketType::CLOCK_MODE`, `PacketType::MIDI_START`, `PacketType::MIDI_STOP`, `packet_send()` from `protocol.h`
- Produces: `MidiClock` class with `init()`, `set_mode()`, `set_bpm()`, `start_transport()`, `stop_transport()`, `on_tick()`, `mode()`, `bpm()`, `estimated_bpm()`, `tick_count()`, `is_running()`

- [ ] **Step 1: Create the header file**

`src/clock/midi_clock.h`:

```cpp
#pragma once
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>

enum class ClockMode : uint8_t { OFF, MASTER, SLAVE };

class MidiClock {
public:
    MidiClock() = default;
    ~MidiClock();

    void init(std::atomic<int>* conn_fd, std::mutex* serial_mutex);

    void set_mode(ClockMode mode);
    ClockMode mode() const;

    void set_bpm(int bpm);
    int bpm() const;
    int estimated_bpm() const;

    void start_transport();
    void stop_transport();
    bool is_running() const;

    void on_tick();
    int tick_count() const;

private:
    void clock_thread_func();

    std::atomic<ClockMode> m_mode{ClockMode::OFF};
    std::atomic<int> m_bpm{120};
    std::atomic<bool> m_transport_running{false};
    std::atomic<int> m_tick_count{0};

    std::atomic<int> m_estimated_bpm{0};
    uint32_t m_last_tick_us{0};
    int m_tick_samples{0};
    uint32_t m_tick_intervals_us{0};

    std::thread m_clock_thread;

    static uint32_t tick_interval_us(int bpm);

    std::atomic<int>* m_conn_fd{nullptr};
    std::mutex* m_serial_mutex{nullptr};
};
```

- [ ] **Step 2: Build to verify** (will fail on missing .cpp — that's expected)

```bash
cmake --build build 2>&1 | head -5
```

Expected: linker error about missing `MidiClock` methods.

---

### Task 4: Implement MidiClock class

**Files:**
- Create: `src/clock/midi_clock.cpp`

**Interfaces:**
- Produces: `MidiClock` method implementations

- [ ] **Step 1: Implement the .cpp file**

`src/clock/midi_clock.cpp`:

```cpp
#include "clock/midi_clock.h"
#include "protocol/protocol.h"
#include <chrono>
#include <thread>

MidiClock::~MidiClock() {
    set_mode(ClockMode::OFF);
}

void MidiClock::init(std::atomic<int>* conn_fd, std::mutex* serial_mutex) {
    m_conn_fd = conn_fd;
    m_serial_mutex = serial_mutex;
}

void MidiClock::set_mode(ClockMode new_mode) {
    ClockMode old = m_mode.exchange(new_mode);
    if (old == ClockMode::MASTER && new_mode != ClockMode::MASTER) {
        stop_transport();
    }
    int fd = m_conn_fd ? m_conn_fd->load(std::memory_order_relaxed) : -1;
    if (fd >= 0 && m_serial_mutex) {
        uint8_t mode_byte = (new_mode == ClockMode::SLAVE) ? 1 : 0;
        std::lock_guard<std::mutex> lock(*m_serial_mutex);
        packet_send(fd, PacketType::CLOCK_MODE, &mode_byte, 1);
    }
}

ClockMode MidiClock::mode() const { return m_mode.load(); }

void MidiClock::set_bpm(int bpm) {
    if (bpm < 20) bpm = 20;
    if (bpm > 300) bpm = 300;
    m_bpm.store(bpm);
}

int MidiClock::bpm() const { return m_bpm.load(); }
int MidiClock::estimated_bpm() const { return m_estimated_bpm.load(); }

void MidiClock::start_transport() {
    m_tick_count.store(0);
    m_transport_running.store(true);

    int fd = m_conn_fd ? m_conn_fd->load(std::memory_order_relaxed) : -1;
    if (m_mode.load() == ClockMode::MASTER) {
        if (fd >= 0 && m_serial_mutex) {
            std::lock_guard<std::mutex> lock(*m_serial_mutex);
            packet_send_transport(fd, PacketType::MIDI_START);
        }
        if (m_clock_thread.joinable())
            m_clock_thread.join();
        m_clock_thread = std::thread(&MidiClock::clock_thread_func, this);
    } else if (m_mode.load() == ClockMode::SLAVE) {
        m_last_tick_us = 0;
        m_tick_samples = 0;
        m_tick_intervals_us = 0;
    }
}

void MidiClock::stop_transport() {
    m_transport_running.store(false);

    if (m_mode.load() == ClockMode::MASTER) {
        if (m_clock_thread.joinable())
            m_clock_thread.join();
        int fd = m_conn_fd ? m_conn_fd->load(std::memory_order_relaxed) : -1;
        if (fd >= 0 && m_serial_mutex) {
            std::lock_guard<std::mutex> lock(*m_serial_mutex);
            packet_send_transport(fd, PacketType::MIDI_STOP);
        }
    }
}

bool MidiClock::is_running() const { return m_transport_running.load(); }

void MidiClock::on_tick() {
    if (m_mode.load() != ClockMode::SLAVE)
        return;
    m_tick_count.fetch_add(1);

    auto now = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    if (m_last_tick_us > 0) {
        uint32_t interval = now - m_last_tick_us;
        if (interval < 200000) {
            m_tick_intervals_us += interval;
            m_tick_samples++;
            if (m_tick_samples >= 12) {
                uint32_t avg = m_tick_intervals_us / m_tick_samples;
                if (avg > 0) {
                    m_estimated_bpm.store(60000000UL / (avg * 24));
                }
                m_tick_samples = 0;
                m_tick_intervals_us = 0;
            }
        }
    }
    m_last_tick_us = now;
}

int MidiClock::tick_count() const { return m_tick_count.load(); }

void MidiClock::clock_thread_func() {
    while (m_transport_running.load() && m_mode.load() == ClockMode::MASTER) {
        int fd = m_conn_fd ? m_conn_fd->load(std::memory_order_relaxed) : -1;
        if (fd >= 0 && m_serial_mutex) {
            std::lock_guard<std::mutex> lock(*m_serial_mutex);
            packet_send(fd, PacketType::MIDI_CLOCK, nullptr, 0);
        }

        uint32_t interval_us = tick_interval_us(m_bpm.load());
        auto deadline = std::chrono::steady_clock::now()
                      + std::chrono::microseconds(interval_us);

        // Coarse sleep, leaving ~1ms for spin-wait
        if (interval_us > 1000) {
            std::this_thread::sleep_for(
                std::chrono::microseconds(interval_us - 1000));
        }

        // Spin-wait for tight jitter
        while (std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
    }
}

uint32_t MidiClock::tick_interval_us(int bpm) {
    return 60000000 / (static_cast<uint32_t>(bpm) * 24);
}
```

- [ ] **Step 2: Build**

```bash
cmake -B build -G Ninja && cmake --build build
```

Expected: compiles without errors.

---

### Task 5: Add MidiClock to test target and write tests

**Files:**
- Modify: `CMakeLists.txt` (test sources)
- Create: `tests/test_midi_clock.cpp`

**Interfaces:**
- Consumes: `MidiClock` class from `clock/midi_clock.h`

- [ ] **Step 1: Add clock source + test to CMakeLists.txt**

Append to the `set(TEST_SOURCES ...)` block:

```
  "${CMAKE_SOURCE_DIR}/tests/test_midi_clock.cpp"
```

Append to the `add_executable(...)` block:

```
  "${CMAKE_SOURCE_DIR}/src/clock/midi_clock.cpp"
```

- [ ] **Step 2: Create test file**

`tests/test_midi_clock.cpp`:

```cpp
#include <gtest/gtest.h>
#include "clock/midi_clock.h"
#include <thread>

static std::atomic<int> dummy_fd{-1};
static std::mutex dummy_mutex;

struct MidiClockTest : ::testing::Test {
    MidiClock clock;
    void SetUp() override {
        clock.init(&dummy_fd, &dummy_mutex);
        clock.set_mode(ClockMode::OFF);
    }
};

TEST_F(MidiClockTest, DefaultState) {
    EXPECT_EQ(clock.mode(), ClockMode::OFF);
    EXPECT_EQ(clock.bpm(), 120);
    EXPECT_FALSE(clock.is_running());
    EXPECT_EQ(clock.tick_count(), 0);
}

TEST_F(MidiClockTest, SetMode) {
    clock.set_mode(ClockMode::MASTER);
    EXPECT_EQ(clock.mode(), ClockMode::MASTER);
    clock.set_mode(ClockMode::SLAVE);
    EXPECT_EQ(clock.mode(), ClockMode::SLAVE);
    clock.set_mode(ClockMode::OFF);
    EXPECT_EQ(clock.mode(), ClockMode::OFF);
}

TEST_F(MidiClockTest, SetBpm) {
    clock.set_bpm(140);
    EXPECT_EQ(clock.bpm(), 140);
}

TEST_F(MidiClockTest, BpmClampLow) {
    clock.set_bpm(5);
    EXPECT_EQ(clock.bpm(), 20);
}

TEST_F(MidiClockTest, BpmClampHigh) {
    clock.set_bpm(500);
    EXPECT_EQ(clock.bpm(), 300);
}

TEST_F(MidiClockTest, TransportStartStop) {
    clock.set_mode(ClockMode::OFF);
    clock.start_transport();
    EXPECT_TRUE(clock.is_running());
    clock.stop_transport();
    EXPECT_FALSE(clock.is_running());
}

TEST_F(MidiClockTest, OnTickIncrementsCountInSlaveMode) {
    clock.set_mode(ClockMode::SLAVE);
    clock.on_tick();
    EXPECT_EQ(clock.tick_count(), 1);
    clock.on_tick();
    EXPECT_EQ(clock.tick_count(), 2);
}

TEST_F(MidiClockTest, OnTickDoesNothingInOffMode) {
    clock.set_mode(ClockMode::OFF);
    clock.on_tick();
    EXPECT_EQ(clock.tick_count(), 0);
}

TEST_F(MidiClockTest, TickIntervalCalculation) {
    // 120 BPM: interval = 60000000 / (120 * 24) = 20833 us
    EXPECT_EQ(MidiClock::tick_interval_us(120), 20833U);
    // 60 BPM: interval = 60000000 / (60 * 24) = 41666 us
    EXPECT_EQ(MidiClock::tick_interval_us(60), 41666U);
}
```

- [ ] **Step 3: Run tests**

```bash
cmake --build build --target synth_front_tests && ctest --test-dir build -V
```

Expected: all tests pass.

---

### Task 6: Add Clock tab to ImGui UI

**Files:**
- Modify: `src/ui/device_panel.h`
- Modify: `src/ui/device_panel.cpp`

**Interfaces:**
- Consumes: `MidiClock` class, `ClockMode` enum
- Produces: clock source combo, BPM slider, transport buttons, current BPM readout

- [ ] **Step 1: Update header**

`src/ui/device_panel.h` — add `RenderClockTab` declaration after `RenderDeviceTabs`:

```cpp
class MidiClock;
void RenderClockTab(MidiClock* clock);
```

- [ ] **Step 2: Implement clock tab**

`src/ui/device_panel.cpp` — add at the top:

```cpp
#include "clock/midi_clock.h"
```

Add at the end of the file:

```cpp
void RenderClockTab(MidiClock* clock) {
    if (!clock) return;

    const char* modes[] = {"OFF", "MASTER", "SLAVE"};
    ClockMode current = clock->mode();
    int current_idx = static_cast<int>(current);
    if (current_idx < 0 || current_idx > 2) current_idx = 0;

    ImGui::Text("Clock Source");
    ImGui::SameLine();
    if (ImGui::Combo("##clocksrc", &current_idx, modes, IM_ARRAYSIZE(modes))) {
        clock->set_mode(static_cast<ClockMode>(current_idx));
    }

    if (clock->mode() == ClockMode::MASTER) {
        int bpm = clock->bpm();
        ImGui::Text("BPM");
        ImGui::SameLine();
        if (ImGui::SliderInt("##bpm", &bpm, 20, 300)) {
            clock->set_bpm(bpm);
        }

        if (ImGui::Button(clock->is_running() ? "Stop" : "Play")) {
            if (clock->is_running())
                clock->stop_transport();
            else
                clock->start_transport();
        }
    }

    if (clock->mode() == ClockMode::SLAVE) {
        int ebpm = clock->estimated_bpm();
        ImGui::Text("Estimated BPM: %d", ebpm > 0 ? ebpm : 0);
    }
}
```

---

### Task 7: Wire MidiClock into main.cpp

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: `MidiClock` class, `RenderClockTab`, `PacketType::MIDI_CLOCK`
- Produces: clock initialization, serial thread handler, UI integration, shutdown

- [ ] **Step 1: Add include**

After `#include "ui/device_panel.h"`:

```cpp
#include "clock/midi_clock.h"
```

- [ ] **Step 2: Add global MidiClock instance**

After `static std::atomic<bool> g_audio_active{false};`:

```cpp
static MidiClock g_midi_clock;
```

- [ ] **Step 3: Initialize MidiClock**

In `main()`, after setting up serial but before the main loop (around line 314, after `g_conn_fd` assignment):

```cpp
g_midi_clock.init(&g_conn_fd, &g_serial_mutex);
```

- [ ] **Step 4: Handle MIDI_CLOCK in serial thread**

In `serial_thread_func`, add a case to the switch inside the parse loop (around line 194, before `default:`):

```cpp
      case PacketType::MIDI_CLOCK:
        g_midi_clock.on_tick();
        break;
```

- [ ] **Step 5: Add Clock tab to ImGui**

In the main loop's ImGui section, inside `BeginTabBar`, after `RenderDeviceTabs(...)`:

```cpp
          if (ImGui::BeginTabItem("Clock")) {
            RenderClockTab(&g_midi_clock);
            ImGui::EndTabItem();
          }
```

- [ ] **Step 6: Build and verify**

```bash
cmake -B build -G Ninja && cmake --build build
```

Expected: compiles without errors.

---

### Task 8: Teensy firmware — add PACKET_CLOCK_MODE handler

**Files:**
- Modify: `teensy_groovebox/src/comms/protocol.h`
- Modify: `teensy_groovebox/src/comms/packet_dispatcher.cpp`

- [ ] **Step 1: Add packet type constant**

In `teensy_groovebox/src/comms/protocol.h`, add to the enum after `PACKET_RESET = 0x0C`:

```
  PACKET_CLOCK_MODE    = 0x0D,
```

- [ ] **Step 2: Add handler in packet_dispatcher**

In `teensy_groovebox/src/comms/packet_dispatcher.cpp`, add a case before `PACKET_TOUCH` (around line 496-498):

```cpp
    case PACKET_CLOCK_MODE:
      if (plen >= 1) {
        g_midi_clock.set_mode(payload[0] == 1 ? MidiClock::SLAVE : MidiClock::INTERNAL);
      }
      break;
```

---

### Task 9: Teensy firmware — emit MIDI_CLOCK in INTERNAL mode

**Files:**
- Modify: `teensy_groovebox/src/app/app.cpp`

- [ ] **Step 1: Add MIDI_CLOCK emission**

In `teensy_groovebox/src/app/app.cpp`, inside the main loop where the sequencer is updated (around line 209, after `g_sequencer.tick(...)`), add:

```cpp
  // Emit MIDI_CLOCK when running on internal clock so the host can follow
  if (g_midi_clock.mode() == MidiClock::INTERNAL
      && g_app_state.transport == TRANSPORT_PLAYING) {
    static uint32_t last_clock_us = 0;
    uint32_t now = micros();
    uint32_t interval = 60000000 / (g_sequencer.bpm() * 24);
    if (now - last_clock_us >= interval) {
      protocol_send_packet(PACKET_MIDI_CLOCK, nullptr, 0);
      last_clock_us = now;
    }
  }
```

Add include for `midi_clock.h` if not already present (it is — `packet_dispatcher.cpp` includes it; verify `app.cpp` has it):

```cpp
#include "midi/midi_clock.h"
```

---

### Task 10: Build and integration test both sides

- [ ] **Step 1: Build host**

```bash
cmake -B build -G Ninja && cmake --build build
```

- [ ] **Step 2: Build Teensy firmware**

(Assuming the Teensy build system — PlatformIO or Arduino IDE — this step verifies compilation.)

- [ ] **Step 3: Run host tests**

```bash
cmake --build build --target synth_front_tests && ctest --test-dir build -V
```

Expected: all tests pass (including new MidiClock tests).

- [ ] **Step 4: Manual integration test — Master mode**

1. Build and flash Teensy firmware
2. Run `./build/synth_front`
3. Press F1 to open UI, go to Clock tab
4. Set Clock Source to MASTER
5. Set BPM to 120
6. Press Play
7. Verify: Teensy sequencer starts and follows host BPM (Teensy display shows transport playing)
8. Press Stop
9. Verify: Teensy sequencer stops

- [ ] **Step 5: Manual integration test — Slave mode**

1. Start transport on Teensy (via its UI or keyboard)
2. In host UI, set Clock Source to SLAVE
3. Verify: host displays estimated BPM matching Teensy's tempo
