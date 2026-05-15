#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>

#include "serial/serial_port.h"
#include "serial/tcp_socket.h"
#include "serial/usb_detect.h"
#include "protocol/protocol.h"
#include "protocol/framebuffer.h"
#include "render/renderer.h"
#include "input/input.h"
#include <ui.h>

static constexpr int FB_RGB565_SIZE = 320 * 480 * 2;
static constexpr int FB_RGB888_SIZE = 320 * 480 * 3;

static std::atomic<bool> g_running{true};
static int g_conn_fd = -1;
static bool g_is_tcp = false;
static std::mutex g_serial_mutex;

static bool g_keys_held[256];

static int g_render_fps = 0;
static Uint64 g_fps_last = 0;
static int g_fps_count = 0;

static void conn_close(int fd, bool is_tcp) {
  if (fd >= 0) {
    if (is_tcp) tcp_close(fd);
    else serial_close(fd);
  }
}

static void send_key(int fd, uint8_t keycode, bool down) {
  std::lock_guard<std::mutex> lock(g_serial_mutex);
  packet_send(fd, down ? PacketType::KEY_DOWN : PacketType::KEY_UP, &keycode, 1);
}

static const char* key_name_short(uint8_t kc) {
  switch (kc) {
    case 0x01: return "UP";
    case 0x02: return "DN";
    case 0x03: return "LT";
    case 0x04: return "RT";
    case 0x10: return "EN";
    case 0x11: return "SP";
    case 0x12: return "ES";
    case 0x13: return "TB";
    case 0x14: return "BS";
    case 0x15: return "DL";
    case 0x20: return "Q"; case 0x21: return "W"; case 0x22: return "E";
    case 0x23: return "R"; case 0x24: return "T"; case 0x25: return "Y";
    case 0x26: return "U"; case 0x27: return "I"; case 0x28: return "O";
    case 0x29: return "P";
    case 0x30: return "A"; case 0x31: return "S"; case 0x32: return "D";
    case 0x33: return "F"; case 0x34: return "G"; case 0x35: return "H";
    case 0x36: return "J"; case 0x37: return "K"; case 0x38: return "L";
    case 0x40: return "Z"; case 0x41: return "X"; case 0x42: return "C";
    case 0x43: return "V"; case 0x44: return "B"; case 0x45: return "N";
    case 0x46: return "M";
    case 0x50: return "1"; case 0x51: return "2"; case 0x52: return "3";
    case 0x53: return "4"; case 0x54: return "5"; case 0x55: return "6";
    case 0x56: return "7"; case 0x57: return "8";
    case 0x60: return "F1"; case 0x61: return "F2"; case 0x62: return "F3";
    case 0x63: return "F4"; case 0x64: return "F5";
    default: return nullptr;
  }
}

static void build_keys_string(char* buf, int bufsz) {
  buf[0] = '\0';
  for (int kc = 0; kc < 256; kc++) {
    if (!g_keys_held[kc]) continue;
    const char* name = key_name_short(kc);
    if (!name) continue;
    int len = (int)std::strlen(buf);
    int nlen = (int)std::strlen(name);
    if (len > 0) buf[len++] = '+';
    if (len + nlen >= bufsz) break;
    std::memcpy(buf + len, name, nlen);
    buf[len + nlen] = '\0';
  }
}

static void convert_rgb565_to_rgb888(const uint8_t* src, uint8_t* dst, int pixels) {
  const uint16_t* s = reinterpret_cast<const uint16_t*>(src);
  for (int i = 0; i < pixels; i++) {
    uint16_t p = s[i];
    dst[i * 3 + 0] = ((p >> 11) & 0x1F) << 3;
    dst[i * 3 + 1] = ((p >> 5)  & 0x3F) << 2;
    dst[i * 3 + 2] = ( p        & 0x1F) << 3;
  }
}

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

int main(int argc, char* argv[]) {
  bool use_tcp = false;
  std::string port;
  std::string host = "127.0.0.1";
  int tcp_port = 9877;

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      port = argv[++i];
    } else if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
      use_tcp = true;
      std::string addr = argv[++i];
      auto colon = addr.find(':');
      if (colon != std::string::npos) {
        host = addr.substr(0, colon);
        tcp_port = std::stoi(addr.substr(colon + 1));
      }
    }
  }

  if (use_tcp) {
    fprintf(stderr, "Connecting to %s:%d...\n", host.c_str(), tcp_port);
    g_conn_fd = tcp_connect(host.c_str(), tcp_port);
    if (g_conn_fd < 0) {
      fprintf(stderr, "Failed to connect.\n");
      return 1;
    }
    g_is_tcp = true;
  } else {
    if (port.empty()) {
      port = find_teensy_port();
    }
    if (port.empty()) {
      fprintf(stderr, "No Teensy found. Use --port <path> or --host <addr:port>.\n");
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
  }

  SDL_Init(SDL_INIT_VIDEO);

  SDL_Window* window = SDL_CreateWindow("synth front", 640, 960,
    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window) {
    fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
    conn_close(g_conn_fd, g_is_tcp);
    SDL_Quit();
    return 1;
  }

  Renderer renderer;
  if (!renderer_init(&renderer, window)) {
    conn_close(g_conn_fd, g_is_tcp);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  framebuffer_init(320, 480);

  std::thread serial_thread(serial_thread_func);

  bool running = true;
  SDL_Event event;
  uint8_t fb_rgb565[FB_RGB565_SIZE];
  int fb_w, fb_h;

  while (running) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_EVENT_QUIT:
          running = false;
          break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
          if (!event.key.repeat) {
            uint8_t kc = input_map_key(event.key.key);
            if (kc != 0) {
              g_keys_held[kc] = (event.type == SDL_EVENT_KEY_DOWN);
              send_key(g_conn_fd, kc, event.type == SDL_EVENT_KEY_DOWN);
            }
          }
          break;
        }
        default:
          break;
      }
    }

    if (framebuffer_get(fb_rgb565, &fb_w, &fb_h)) {
      ui_fb_t fb = { fb_rgb565, fb_w, fb_h };

      ui_fill_rect(&fb, 0, 0, fb_w, 14, UI_BG_DARK);

      char keys_buf[64];
      build_keys_string(keys_buf, (int)sizeof(keys_buf));
      if (keys_buf[0]) {
        ui_draw_text(&fb, 2, 2, keys_buf, UI_ACCENT_2, UI_BG_DARK);
      }

      char status[32];
      std::snprintf(status, sizeof(status), "%s  %d fps",
                    g_is_tcp ? "TCP" : "USB", g_render_fps);
      int sw = ui_text_width(status);
      ui_draw_text(&fb, fb_w - sw - 4, 2, status, UI_TEXT_DIM, UI_BG_DARK);

      g_fps_count++;
      Uint64 now = SDL_GetTicks();
      if (now - g_fps_last >= 1000) {
        g_render_fps = g_fps_count;
        g_fps_count = 0;
        g_fps_last = now;
      }

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

  conn_close(g_conn_fd, g_is_tcp);

  renderer_destroy(&renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
