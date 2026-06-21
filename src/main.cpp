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

static constexpr int FB_RGB565_SIZE = 320 * 480 * 2;
static constexpr int FB_RGB888_SIZE = 320 * 480 * 3;

static std::atomic<bool> g_running{true};
static int g_conn_fd = -1;
static std::mutex g_serial_mutex;

struct MidiEvent {
  PacketType type;
  uint8_t channel;
  uint8_t data1;
  uint8_t data2;
};

static constexpr int MIDI_QUEUE_SIZE = 256;
static MidiEvent g_midi_queue[MIDI_QUEUE_SIZE];
static std::atomic<int> g_midi_head{0};
static int g_midi_tail = 0;

// Close a serial or TCP connection by its file descriptor.
static void conn_close(int fd) {
  if (fd >= 0) serial_close(fd);
}

// Send a key-down or key-up event to the connected device.
static void send_key(int fd, uint8_t keycode, bool down) {
  std::lock_guard<std::mutex> lock(g_serial_mutex);
  packet_send(fd, down ? PacketType::KEY_DOWN : PacketType::KEY_UP, &keycode, 1);
}

// Send a MIDI packet to the connected device.
static void send_midi(int fd, PacketType type, uint8_t channel, uint8_t data1, uint8_t data2) {
  uint8_t payload[3] = {channel, data1, data2};
  std::lock_guard<std::mutex> lock(g_serial_mutex);
  packet_send(fd, type, payload, 3);
}

// Send a transport packet (START/CONTINUE/STOP) to the connected device.
static void send_transport(int fd, PacketType type) {
  std::lock_guard<std::mutex> lock(g_serial_mutex);
  packet_send_transport(fd, type);
}

// Send a MIDI note on/off to the connected device (channel 0).
static void send_midi_note(int fd, uint8_t note, bool on) {
  send_midi(fd, on ? PacketType::MIDI_NOTE_ON : PacketType::MIDI_NOTE_OFF, 0, note, on ? 127 : 0);
}

// Send an encoder delta to the connected device.
static void send_encoder(int fd, uint8_t index, int16_t delta) {
  std::lock_guard<std::mutex> lock(g_serial_mutex);
  packet_send_encoder(fd, index, delta);
}

// Send a touch event to the connected device.
static void send_touch(int fd, uint16_t x, uint16_t y, bool pressed) {
  std::lock_guard<std::mutex> lock(g_serial_mutex);
  packet_send_touch(fd, x, y, pressed ? 1 : 0);
}

// Convert an RGB565 pixel buffer to RGB888 for OpenGL texture upload.
static void convert_rgb565_to_rgb888(const uint8_t* src, uint8_t* dst, int pixels) {
  const uint16_t* s = reinterpret_cast<const uint16_t*>(src);
  for (int i = 0; i < pixels; i++) {
    uint16_t p = s[i];
    dst[i * 3 + 0] = ((p >> 11) & 0x1F) << 3;
    dst[i * 3 + 1] = ((p >> 5)  & 0x3F) << 2;
    dst[i * 3 + 2] = ( p        & 0x1F) << 3;
  }
}

// Background thread: read packets from serial/TCP, parse frame tiles and debug messages.
static void serial_thread_func() {
  static uint8_t buf[65536];
  int buf_len = 0;

  while (g_running.load(std::memory_order_acquire)) {
    int n = serial_read(g_conn_fd, buf + buf_len, (int)sizeof(buf) - buf_len);
    if (n <= 0) {
      SDL_Delay(1);
      continue;
    }
    buf_len += n;

    int consumed = 0;
    while (consumed < buf_len) {
      Packet pkt;
      int parsed = packet_parse(buf + consumed, buf_len - consumed, &pkt);
      if (parsed == 0) break;
      if (parsed < 0) {
        consumed++;
        continue;
      }

      switch (pkt.type) {
        case PacketType::FRAME_TILE: {
          if (pkt.payload.size() >= 14) {
            uint16_t total_w = (pkt.payload[0] << 8) | pkt.payload[1];
            uint16_t total_h = (pkt.payload[2] << 8) | pkt.payload[3];
            // uint8_t fmt = pkt.payload[4];
            uint16_t tx = (pkt.payload[5] << 8) | pkt.payload[6];
            uint16_t ty = (pkt.payload[7] << 8) | pkt.payload[8];
            uint16_t tw = (pkt.payload[9] << 8)  | pkt.payload[10];
            uint16_t th = (pkt.payload[11] << 8) | pkt.payload[12];
            const uint8_t* pixels = pkt.payload.data() + 13;
            framebuffer_init(total_w, total_h);
            framebuffer_write_tile(tx, ty, tw, th, pixels);
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
    fprintf(stderr, "No Teensy found. Use --port <path>.\n");
    fprintf(stderr, "Available serial ports:\n");
    for (const auto& p : find_serial_ports()) {
      fprintf(stderr, "  %s\n", p.c_str());
    }
    return 1;
  }
  fprintf(stderr, "Connecting to Teensy on %s...\n", port.c_str());
  g_conn_fd = serial_open(port.c_str(), 2000000);
  if (g_conn_fd < 0) {
    fprintf(stderr, "Failed to open serial port.\n");
    return 1;
  }

  MidiInput midi_in{};
  midi_input_open(&midi_in,
      midi_source.empty() ? nullptr : midi_source.c_str(),
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
        int slot = g_midi_head.load(std::memory_order_relaxed);
        g_midi_queue[slot % MIDI_QUEUE_SIZE] = {type, channel, data1, data2};
        g_midi_head.store(slot + 1, std::memory_order_release);
      });

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

  SDL_Window* window = SDL_CreateWindow("synth front", 640, 960,
    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window) {
    fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
    midi_input_close(&midi_in);
    conn_close(g_conn_fd);
    SDL_Quit();
    return 1;
  }

  Renderer renderer;
  if (!renderer_init(&renderer, window)) {
    midi_input_close(&midi_in);
    conn_close(g_conn_fd);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  framebuffer_init(320, 480);

  audio_init();

  std::thread serial_thread(serial_thread_func);

  bool running = true;
  SDL_Event event;
  uint8_t fb_rgb565[FB_RGB565_SIZE];
  int fb_w, fb_h;
  bool mouse_down = false;

  while (running) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_EVENT_QUIT:
          running = false;
          break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
          if (event.type == SDL_EVENT_MOUSE_MOTION && !mouse_down) break;
          if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            mouse_down = true;
          } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            mouse_down = false;
          }
          float mx = (event.type == SDL_EVENT_MOUSE_MOTION)
                         ? event.motion.x : event.button.x;
          float my = (event.type == SDL_EVENT_MOUSE_MOTION)
                         ? event.motion.y : event.button.y;
          int ww_logical, wh_logical;
          SDL_GetWindowSize(window, &ww_logical, &wh_logical);
          uint16_t tx = (uint16_t)(mx / ww_logical * 320.0f);
          uint16_t ty = (uint16_t)(my / wh_logical * 480.0f);
          if (tx >= 320) tx = 319;
          if (ty >= 480) ty = 479;
          send_touch(g_conn_fd, tx, ty, mouse_down);
          break;
        }
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
          bool down = (event.type == SDL_EVENT_KEY_DOWN);
          auto mod = SDL_GetModState();
          auto result = input_map_key(event.key.key, (mod & SDL_KMOD_SHIFT) != 0);

          bool is_encoder = (result.action == InputAction::ENCODER);
          if (event.key.repeat && !is_encoder) break;
          if (!down && (is_encoder || result.action == InputAction::TRANSPORT)) break;

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
              if (result.encoder_delta != 0) {
                send_encoder(g_conn_fd, result.value, result.encoder_delta);
              }
              break;
          }
          break;
        }
        default:
          break;
      }
    }

    {
      int head = g_midi_head.load(std::memory_order_acquire);
      while (g_midi_tail != head) {
        MidiEvent& e = g_midi_queue[g_midi_tail % MIDI_QUEUE_SIZE];
        send_midi(g_conn_fd, e.type, e.channel, e.data1, e.data2);
        g_midi_tail++;
      }
    }

    if (framebuffer_get(fb_rgb565, &fb_w, &fb_h)) {
      static uint8_t fb_rgb888[FB_RGB888_SIZE];
      convert_rgb565_to_rgb888(fb_rgb565, fb_rgb888, fb_w * fb_h);
      renderer_upload_frame(&renderer, fb_rgb888, fb_w, fb_h);
    }

    int ww, wh;
    SDL_GetWindowSizeInPixels(window, &ww, &wh);
    renderer_draw(&renderer, ww, wh);
    SDL_GL_SwapWindow(window);
  }

  g_running.store(false, std::memory_order_release);
  serial_thread.join();

  midi_input_close(&midi_in);
  conn_close(g_conn_fd);

  audio_shutdown();

  renderer_destroy(&renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
