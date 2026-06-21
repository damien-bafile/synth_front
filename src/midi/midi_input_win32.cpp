#include "midi_input.h"
#include <windows.h>
#include <mmeapi.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
#include <future>

struct Win32MidiBackend {
  HMIDIIN handle = nullptr;
  UINT dev_id = 0;
  std::thread thread;
  DWORD thread_id = 0;
  std::atomic<bool> running{false};
  MidiCallback* cb_ptr = nullptr;
  std::promise<MMRESULT> open_promise;
};

static void CALLBACK midi_in_callback(HMIDIIN, UINT msg, DWORD_PTR instance, DWORD_PTR param1,
                                      DWORD_PTR) {
  if (msg != MIM_DATA)
    return;

  MidiCallback* cb = reinterpret_cast<MidiCallback*>(instance);
  uint8_t status = static_cast<uint8_t>(param1 & 0xFF);
  uint8_t data1 = static_cast<uint8_t>((param1 >> 8) & 0xFF);
  uint8_t data2 = static_cast<uint8_t>((param1 >> 16) & 0xFF);

  (*cb)(status, data1, data2);
}

static void midi_thread_func(Win32MidiBackend* b) {
  b->thread_id = GetCurrentThreadId();

  MMRESULT res = midiInOpen(&b->handle, b->dev_id, reinterpret_cast<DWORD_PTR>(midi_in_callback),
                            reinterpret_cast<DWORD_PTR>(b->cb_ptr), CALLBACK_FUNCTION);
  if (res != MMSYSERR_NOERROR) {
    b->open_promise.set_value(res);
    return;
  }

  res = midiInStart(b->handle);
  if (res != MMSYSERR_NOERROR) {
    b->open_promise.set_value(res);
    midiInClose(b->handle);
    b->handle = nullptr;
    return;
  }

  b->open_promise.set_value(MMSYSERR_NOERROR);

  MSG msg;
  while (b->running.load(std::memory_order_acquire)) {
    if (GetMessage(&msg, nullptr, 0, 0) > 0) {
      DispatchMessage(&msg);
    }
  }

  midiInStop(b->handle);
  midiInReset(b->handle);
  midiInClose(b->handle);
  b->handle = nullptr;
}

bool midi_input_open(MidiInput* m, const char* source_name, MidiCallback cb) {
  auto* b = new Win32MidiBackend;
  m->opaque = b;
  m->cb = nullptr;
  m->running = false;

  UINT num_devs = midiInGetNumDevs();
  UINT dev_id = 0;
  bool found = false;

  if (source_name) {
    for (UINT i = 0; i < num_devs; i++) {
      MIDIINCAPSA caps;
      if (midiInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
        if (strstr(caps.szPname, source_name)) {
          dev_id = i;
          found = true;
          break;
        }
      }
    }
    if (!found) {
      fprintf(stderr, "MIDI: no matching source \"%s\", running without MIDI input.\n",
              source_name);
      delete b;
      m->opaque = nullptr;
      m->running = true;
      return true;
    }
  } else if (num_devs == 0) {
    fprintf(stderr, "MIDI: no MIDI input devices found, running without MIDI input.\n");
    delete b;
    m->opaque = nullptr;
    m->running = true;
    return true;
  }

  auto* cb_ptr = new MidiCallback(std::move(cb));
  m->cb = cb_ptr;
  b->cb_ptr = cb_ptr;
  b->dev_id = dev_id;

  auto open_future = b->open_promise.get_future();

  b->running.store(true, std::memory_order_release);
  b->thread = std::thread(midi_thread_func, b);

  MMRESULT result = open_future.get();

  if (result != MMSYSERR_NOERROR) {
    b->running.store(false, std::memory_order_release);
    PostThreadMessage(b->thread_id, WM_QUIT, 0, 0);
    b->thread.join();
    fprintf(stderr, "MIDI: failed to open device (err %u)\n", result);
    delete cb_ptr;
    m->cb = nullptr;
    delete b;
    m->opaque = nullptr;
    return false;
  }

  m->running = true;
  return true;
}

void midi_input_close(MidiInput* m) {
  auto* b = static_cast<Win32MidiBackend*>(m->opaque);
  if (!b)
    return;

  b->running.store(false, std::memory_order_release);

  if (b->thread.joinable()) {
    PostThreadMessage(b->thread_id, WM_QUIT, 0, 0);
    b->thread.join();
  }

  if (m->cb) {
    delete m->cb;
    m->cb = nullptr;
  }
  delete b;
  m->opaque = nullptr;
  m->running = false;
}

std::vector<std::string> midi_input_list_sources() {
  std::vector<std::string> names;
  UINT n = midiInGetNumDevs();
  for (UINT i = 0; i < n; i++) {
    MIDIINCAPSA caps;
    if (midiInGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
      names.push_back(caps.szPname);
    }
  }
  return names;
}
