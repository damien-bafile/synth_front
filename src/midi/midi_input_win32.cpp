/// @file midi_input_win32.cpp
/// @brief WinMM MIDI input backend for Windows.
///
/// Opens a MIDI input device with a callback function, pumps the MM message
/// loop on a dedicated thread, and forwards 3-byte MIDI messages to the user
/// callback.

#include "midi_input.h"
#include <windows.h>
#include <mmeapi.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
#include <future>

struct Win32MidiBackend {
  HMIDIIN handle = nullptr;        ///< Open WinMM MIDI input handle.
  UINT dev_id = 0;                 ///< Selected device index.
  std::thread thread;              ///< Thread running the MM message pump.
  DWORD thread_id = 0;             ///< Thread ID used to post WM_QUIT.
  std::atomic<bool> running{false};///< Set false to stop the message pump.
  MidiCallback* cb_ptr = nullptr;  ///< Pointer to heap-allocated callback.
  std::promise<MMRESULT> open_promise;///< Signals open success/failure to caller.
};

// WinMM MIDI input callback. Extracts the 3-byte MIDI message packed into
// param1 and forwards it to the user callback.
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

// WinMM requires a message pump on the thread that opened the device. This
// thread opens midiIn, starts it, then pumps messages until told to quit.
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

  // Pump messages so the callback can fire on this thread.
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

  // Pick the first device, or the first device whose name contains source_name.
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

  // Store the callback on the heap so its address remains stable for WinMM.
  auto* cb_ptr = new MidiCallback(std::move(cb));
  m->cb = cb_ptr;
  b->cb_ptr = cb_ptr;
  b->dev_id = dev_id;

  auto open_future = b->open_promise.get_future();

  // Start the message-pump thread and wait for it to finish opening the device.
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

  // Signal the message loop to exit and wait for the thread to shut down.
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
