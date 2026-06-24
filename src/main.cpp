/// @file main.cpp
/// @brief Application entry point and main event loop.
///
/// Responsibilities:
///   - Parse command-line options and open a serial/TCP connection to the Teensy.
///   - Start a background thread that reads packets from the connection.
///   - Set up SDL (video + audio), OpenGL, and Dear ImGui.
///   - Handle keyboard, mouse, touch, and MIDI input and forward them as packets.
///   - Render the Teensy display framebuffer each frame with an optional UI overlay.
///
/// Threading notes:
///   - The serial reader thread is the only thread that writes to the framebuffer.
///   - g_conn_fd is read/written by multiple threads and is protected with
///     g_serial_mutex for packet writes.
///   - g_midi_queue is a single-producer single-consumer ring buffer between the
///     MIDI callback thread and the main thread.

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>

#include "serial/serial_port.h"
#include "serial/usb_detect.h"
#include "protocol/protocol.h"
#include "protocol/framebuffer.h"
#include "render/renderer.h"
#include "input/input.h"
#include "midi/midi_input.h"
#include "audio/audio.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_opengl3.h"

static std::atomic<bool> g_running{true};
static std::atomic<int> g_conn_fd{-1};
static std::mutex g_serial_mutex;
static std::atomic<bool> g_show_ui{true};
static std::atomic<bool> g_connected{false};
static float g_ui_x = 0, g_ui_y = 0, g_ui_w = 640, g_ui_h = 50;

struct MidiEvent {
  PacketType type;
  uint8_t channel;
  uint8_t data1;
  uint8_t data2;
};

static constexpr int MIDI_QUEUE_SIZE = 256;
static constexpr int SERIAL_BUF_SIZE = 65536;
static constexpr int READ_TIMEOUT_MS = 2000;
static MidiEvent g_midi_queue[MIDI_QUEUE_SIZE];
static std::atomic<int> g_midi_head{0};
static std::atomic<int> g_midi_tail{0};

// Close a serial or TCP connection by its file descriptor.
static void conn_close(int fd) {
  if (fd >= 0)
    serial_close(fd);
}

// Send a key-down or key-up event to the connected device.
static void send_key(int fd, uint8_t keycode, bool down) {
  if (fd < 0) return;
  std::lock_guard<std::mutex> lock(g_serial_mutex);
  packet_send(fd, down ? PacketType::KEY_DOWN : PacketType::KEY_UP, &keycode, 1);
}

// Send a MIDI packet to the connected device.
static void send_midi(int fd, PacketType type, uint8_t channel, uint8_t data1, uint8_t data2) {
  if (fd < 0) return;
  uint8_t payload[3] = {channel, data1, data2};
  std::lock_guard<std::mutex> lock(g_serial_mutex);
  packet_send(fd, type, payload, 3);
}

// Send a transport packet (START/CONTINUE/STOP) to the connected device.
static void send_transport(int fd, PacketType type) {
  if (fd < 0) return;
  std::lock_guard<std::mutex> lock(g_serial_mutex);
  packet_send_transport(fd, type);
}

// Send a MIDI note on/off to the connected device (channel 0).
static void send_midi_note(int fd, uint8_t note, bool on) {
  send_midi(fd, on ? PacketType::MIDI_NOTE_ON : PacketType::MIDI_NOTE_OFF, 0, note, on ? 127 : 0);
}

// Send an encoder delta to the connected device.
static void send_encoder(int fd, uint8_t index, int16_t delta) {
  if (fd < 0) return;
  std::lock_guard<std::mutex> lock(g_serial_mutex);
  packet_send_encoder(fd, index, delta);
}

// Send a touch event to the connected device.
static void send_touch(int fd, uint16_t x, uint16_t y, bool pressed) {
  if (fd < 0) return;
  std::lock_guard<std::mutex> lock(g_serial_mutex);
  packet_send_touch(fd, x, y, pressed ? 1 : 0);
}

// Map window-logical coordinates to framebuffer coordinates, correcting for letterbox viewport.
static void window_to_fb(float mx, float my, const Renderer& r, uint16_t& tx, uint16_t& ty) {
  if (r.vp_w == 0 || r.vp_h == 0) {
    tx = ty = 0;
    return;
  }
  float rx = (mx - r.vp_x) / r.vp_w;
  float ry = (my - r.vp_y) / r.vp_h;
  if (rx < 0.0f)
    rx = 0.0f;
  if (rx > 1.0f)
    rx = 1.0f;
  if (ry < 0.0f)
    ry = 0.0f;
  if (ry > 1.0f)
    ry = 1.0f;
  tx = (uint16_t)(rx * FB_WIDTH);
  ty = (uint16_t)(ry * FB_HEIGHT);
  if (tx >= FB_WIDTH)
    tx = FB_WIDTH - 1;
  if (ty >= FB_HEIGHT)
    ty = FB_HEIGHT - 1;
}

// Convert an RGB565 pixel buffer to RGB888 for OpenGL texture upload.
static void convert_rgb565_to_rgb888(const uint8_t* src, uint8_t* dst, int pixels) {
  const uint16_t* s = reinterpret_cast<const uint16_t*>(src);
  for (int i = 0; i < pixels; i++) {
    uint16_t p = s[i];
    dst[i * 3 + 0] = ((p >> 11) & 0x1F) << 3;
    dst[i * 3 + 1] = ((p >> 5) & 0x3F) << 2;
    dst[i * 3 + 2] = (p & 0x1F) << 3;
  }
}

// Background thread: read packets from serial/TCP, parse frame tiles and debug messages.
static void serial_thread_func() {
  static uint8_t buf[SERIAL_BUF_SIZE];
  int buf_len = 0;
  uint32_t last_data_time = SDL_GetTicks();

  while (g_running.load(std::memory_order_acquire)) {
    int fd = g_conn_fd.load(std::memory_order_relaxed);
    if (fd < 0) {
      SDL_Delay(10);
      continue;
    }
    int n = serial_read(fd, buf + buf_len, static_cast<int>(sizeof(buf)) - buf_len);
    if (n < 0) {
      conn_close(fd);
      g_conn_fd.store(-1, std::memory_order_relaxed);
      g_connected.store(false, std::memory_order_release);
      framebuffer_clear();
      buf_len = 0;
      continue;
    }
    if (n == 0) {
      if (SDL_GetTicks() - last_data_time > READ_TIMEOUT_MS) {
        conn_close(fd);
        g_conn_fd.store(-1, std::memory_order_relaxed);
        g_connected.store(false, std::memory_order_release);
        framebuffer_clear();
        buf_len = 0;
        continue;
      }
      SDL_Delay(1);
      continue;
    }
    buf_len += n;
    last_data_time = SDL_GetTicks();

    // Parse as many complete packets as possible from the accumulated buffer.
    int consumed = 0;
    while (consumed < buf_len) {
      Packet pkt;
      int parsed = packet_parse(buf + consumed, buf_len - consumed, &pkt);
      if (parsed == 0)
        break;
      if (parsed < 0) {
        consumed++;
        continue;
      }

      switch (pkt.type) {
      case PacketType::FRAME_TILE: {
        // Tile layout: [total_w, total_h, reserved, tx, ty, tw, th, pixels...]
        if (pkt.payload.size() >= 14) {
          uint16_t total_w = (pkt.payload[0] << 8) | pkt.payload[1];
          uint16_t total_h = (pkt.payload[2] << 8) | pkt.payload[3];
          uint16_t tx = (pkt.payload[5] << 8) | pkt.payload[6];
          uint16_t ty = (pkt.payload[7] << 8) | pkt.payload[8];
          uint16_t tw = (pkt.payload[9] << 8) | pkt.payload[10];
          uint16_t th = (pkt.payload[11] << 8) | pkt.payload[12];
          const uint8_t* pixels = pkt.payload.data() + 13;
          framebuffer_init(total_w, total_h);
          framebuffer_write_tile(tx, ty, tw, th, pixels);
          // Last tile in the frame publishes the completed buffer.
          if (tx + tw >= total_w && ty + th >= total_h) {
            framebuffer_finish_frame();
          }
        }
        break;
      }
      case PacketType::FRAME:
      case PacketType::DEBUG: {
        if (!pkt.payload.empty()) {
          int len = pkt.payload[0];
          if (len <= (int)pkt.payload.size() - 1) {
            fwrite(pkt.payload.data() + 1, 1, len, stdout);
            fputc('\n', stdout);
            fflush(stdout);
          }
        }
        break;
      }
      case PacketType::READY:
        fprintf(stderr, "Teensy connected and ready.\n");
        g_connected.store(true, std::memory_order_release);
        break;
      default:
        break;
      }
      consumed += parsed;
    }

    if (consumed > 0 && consumed < buf_len) {
      std::memmove(buf, buf + consumed, buf_len - consumed);
    }
    buf_len -= consumed;
  }
}

// Poll for a Teensy serial port and re-open the connection.
static void try_reconnect() {
  if (g_conn_fd.load(std::memory_order_relaxed) >= 0)
    return;
  std::string port = find_teensy_port();
  if (port.empty())
    return;
  fprintf(stderr, "Reconnecting to Teensy on %s...\n", port.c_str());
  int fd = serial_open(port.c_str(), 2000000);
  if (fd < 0) {
    fprintf(stderr, "Failed to open %s\n", port.c_str());
    return;
  }
  g_conn_fd.store(fd, std::memory_order_relaxed);
}

// Entry point: open serial/TCP, set up SDL/OpenGL window, run render loop with key handling.
int main(int argc, char* argv[]) {
  std::string port;
  std::string midi_source;

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      port = argv[++i];
    } else if (std::strcmp(argv[i], "--list-midi") == 0) {
      auto sources = midi_input_list_sources();
      fprintf(stderr, "MIDI input sources:\n");
      for (const auto& s : sources) {
        fprintf(stderr, "  %s\n", s.c_str());
      }
      return 0;
    } else if (std::strcmp(argv[i], "--midi-source") == 0 && i + 1 < argc) {
      midi_source = argv[++i];
    }
  }

  if (port.empty()) {
    port = find_teensy_port();
  }
  if (port.empty()) {
    fprintf(stderr, "No Teensy found (use --port <path>). Will auto-retry.\n");
  } else {
    fprintf(stderr, "Connecting to Teensy on %s...\n", port.c_str());
    g_conn_fd = serial_open(port.c_str(), 2000000);
    if (g_conn_fd < 0)
      fprintf(stderr, "Failed to open serial port. Will auto-retry.\n");
  }

  MidiInput midi_in{};
  // Open MIDI input and route incoming messages to the serial packet queue.
  midi_input_open(&midi_in, midi_source.empty() ? nullptr : midi_source.c_str(),
                  [](uint8_t status, uint8_t data1, uint8_t data2) {
                    uint8_t channel = status & 0x0F;
                    uint8_t msg = status & 0xF0;
                    PacketType type;
                    if (msg == 0x90 && data2 > 0) {
                      type = PacketType::MIDI_NOTE_ON;
                    } else if (msg == 0x80 || (msg == 0x90 && data2 == 0)) {
                      type = PacketType::MIDI_NOTE_OFF;
                    } else if (msg == 0xB0) {
                      type = PacketType::MIDI_CC;
                    } else if (msg == 0xE0) {
                      type = PacketType::MIDI_PITCH_BEND;
                    } else if (status == 0xFA) {
                      send_transport(g_conn_fd, PacketType::MIDI_START);
                      return;
                    } else if (status == 0xFB) {
                      send_transport(g_conn_fd, PacketType::MIDI_CONTINUE);
                      return;
                    } else if (status == 0xFC) {
                      send_transport(g_conn_fd, PacketType::MIDI_STOP);
                      return;
                    } else {
                      return;
                    }
                    // Push channel messages into the lock-free ring buffer.
                    int slot = g_midi_head.load(std::memory_order_relaxed);
                    int tail = g_midi_tail.load(std::memory_order_acquire);
                    if (slot - tail < MIDI_QUEUE_SIZE) {
                      g_midi_queue[slot % MIDI_QUEUE_SIZE] = {type, channel, data1, data2};
                      g_midi_head.store(slot + 1, std::memory_order_release);
                    }
                  });

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

  SDL_Window* window =
      SDL_CreateWindow("synth front", 640, 960,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window) {
    fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
    midi_input_close(&midi_in);
    conn_close(g_conn_fd);
    SDL_Quit();
    return 1;
  }

  Renderer renderer{};
  if (!renderer_init(&renderer, window)) {
    midi_input_close(&midi_in);
    conn_close(g_conn_fd);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  framebuffer_init(320, 480);

  audio_init();

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplSDL3_InitForOpenGL(window, renderer.gl_ctx);
  ImGui_ImplOpenGL3_Init("#version 330 core");
  ImGui::StyleColorsDark();

  std::thread serial_thread(serial_thread_func);

  bool running = true;
  SDL_Event event;
  uint8_t fb_rgb565[FB_RGB565_SIZE];
  int fb_w, fb_h;
  bool mouse_down = false, finger_down = false;

  while (running) {
    if (!g_connected.load(std::memory_order_acquire))
      try_reconnect();

    int pw, ph;
    SDL_GetWindowSizeInPixels(window, &pw, &ph);

    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);

      switch (event.type) {
      case SDL_EVENT_QUIT:
        running = false;
        break;

      case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        mouse_down = true;
        uint16_t tx, ty;
        window_to_fb(event.button.x, event.button.y, renderer, tx, ty);
        if (!(g_show_ui
              && event.button.x >= g_ui_x && event.button.x < g_ui_x + g_ui_w
              && event.button.y >= g_ui_y && event.button.y < g_ui_y + g_ui_h))
          send_touch(g_conn_fd, tx, ty, true);
        break;
      }
      case SDL_EVENT_MOUSE_BUTTON_UP: {
        mouse_down = false;
        uint16_t tx, ty;
        window_to_fb(event.button.x, event.button.y, renderer, tx, ty);
        if (!(g_show_ui
              && event.button.x >= g_ui_x && event.button.x < g_ui_x + g_ui_w
              && event.button.y >= g_ui_y && event.button.y < g_ui_y + g_ui_h))
          send_touch(g_conn_fd, tx, ty, false);
        break;
      }
      case SDL_EVENT_MOUSE_MOTION: {
        float mx = event.motion.x;
        float my = event.motion.y;
        if (!mouse_down) break;
        if (g_show_ui && mx >= g_ui_x && mx < g_ui_x + g_ui_w && my >= g_ui_y && my < g_ui_y + g_ui_h)
          break;
        uint16_t tx, ty;
        window_to_fb(mx, my, renderer, tx, ty);
        send_touch(g_conn_fd, tx, ty, true);
        break;
      }

      case SDL_EVENT_FINGER_DOWN: {
        finger_down = true;
        { float mx = event.tfinger.x * pw;
          float my = event.tfinger.y * ph;
          uint16_t tx, ty;
          window_to_fb(mx, my, renderer, tx, ty);
          if (!(g_show_ui && mx >= g_ui_x && mx < g_ui_x + g_ui_w && my >= g_ui_y && my < g_ui_y + g_ui_h))
            send_touch(g_conn_fd, tx, ty, true);
        }
        break;
      }
      case SDL_EVENT_FINGER_UP: {
        finger_down = false;
        { float mx = event.tfinger.x * pw;
          float my = event.tfinger.y * ph;
          uint16_t tx, ty;
          window_to_fb(mx, my, renderer, tx, ty);
          if (!(g_show_ui && mx >= g_ui_x && mx < g_ui_x + g_ui_w && my >= g_ui_y && my < g_ui_y + g_ui_h))
            send_touch(g_conn_fd, tx, ty, false);
        }
        break;
      }
      case SDL_EVENT_FINGER_MOTION: {
        float mx = event.tfinger.x * pw;
        float my = event.tfinger.y * ph;
        if (!finger_down) break;
        if (g_show_ui && mx >= g_ui_x && mx < g_ui_x + g_ui_w && my >= g_ui_y && my < g_ui_y + g_ui_h)
          break;
        uint16_t tx, ty;
        window_to_fb(mx, my, renderer, tx, ty);
        send_touch(g_conn_fd, tx, ty, true);
        break;
      }

      case SDL_EVENT_KEY_DOWN:
      case SDL_EVENT_KEY_UP: {
        bool down = (event.type == SDL_EVENT_KEY_DOWN);
        auto mod = SDL_GetModState();
        if (down && event.key.key == SDLK_F1) {
          g_show_ui = !g_show_ui;
          break;
        }
        if ((mod & SDL_KMOD_CTRL) && (mod & SDL_KMOD_SHIFT) && event.key.key == SDLK_R) {
          if (down)
            packet_send(g_conn_fd, PacketType::RESET, nullptr, 0);
          break;
        }
        auto result = input_map_key(event.key.key, (mod & SDL_KMOD_SHIFT) != 0);

        bool is_encoder = (result.action == InputAction::ENCODER);
        if (event.key.repeat && !is_encoder)
          break;
        if (!down && (is_encoder || result.action == InputAction::TRANSPORT))
          break;

        switch (result.action) {
        case InputAction::KEY:
          send_key(g_conn_fd, result.value, down);
          break;
        case InputAction::TRANSPORT:
          send_transport(g_conn_fd, static_cast<PacketType>(result.value));
          break;
        case InputAction::NOTE:
          send_midi_note(g_conn_fd, result.value, down);
          break;
        case InputAction::ENCODER:
          if (result.encoder_delta != 0)
            send_encoder(g_conn_fd, result.value, result.encoder_delta);
          break;
        }
        break;
      }
      default:
        break;
      }
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Drain queued MIDI channel messages into the serial connection.
    {
      int head = g_midi_head.load(std::memory_order_acquire);
      while (g_midi_tail != head) {
        MidiEvent& e = g_midi_queue[g_midi_tail % MIDI_QUEUE_SIZE];
        send_midi(g_conn_fd, e.type, e.channel, e.data1, e.data2);
        g_midi_tail++;
      }
    }

    // Fetch the latest completed frame from the serial thread, convert it to
    // RGB888, and upload it to the GL texture.
    bool have_frame = g_connected.load(std::memory_order_acquire) && framebuffer_get(fb_rgb565, &fb_w, &fb_h);
    if (have_frame) {
      static uint8_t fb_rgb888[FB_RGB888_SIZE];
      convert_rgb565_to_rgb888(fb_rgb565, fb_rgb888, fb_w * fb_h);
      renderer_upload_frame(&renderer, fb_rgb888, fb_w, fb_h);
    }

    renderer_draw(&renderer, pw, ph);

    // Build the ImGui overlay window.
    if (g_show_ui) {
      ImGui::SetNextWindowPos(ImVec2((float)pw * 0.5f, 0.0f), ImGuiCond_Appearing, ImVec2(0.5f, 0.0f));
      ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar;
      bool connected = g_connected.load(std::memory_order_acquire);
      const char* title = connected ? "Menu" : "Disconnected";
      if (!connected)
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.5f, 0.08f, 0.08f, 1.0f));
      if (ImGui::Begin(title, nullptr, window_flags)) {
        if (ImGui::Button("Restart"))
          packet_send(g_conn_fd, PacketType::RESET, nullptr, 0);
        ImGui::SameLine();
        if (ImGui::Button("Close"))
          running = false;

        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 sz = ImGui::GetWindowSize();
        g_ui_x = pos.x; g_ui_y = pos.y;
        g_ui_w = sz.x; g_ui_h = sz.y;
      }
      if (!connected)
        ImGui::PopStyleColor();
      ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window);
  }

  g_running.store(false, std::memory_order_release);
  serial_thread.join();

  midi_input_close(&midi_in);
  conn_close(g_conn_fd);

  audio_shutdown();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  renderer_destroy(&renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
