#include "framebuffer.h"
#include <cstring>
#include <atomic>

static constexpr int MAX_FB_PIXELS = 320 * 480;
static constexpr int FB_RGB888_SIZE = MAX_FB_PIXELS * 3;

static uint8_t g_fb[2][FB_RGB888_SIZE];
static int g_cur = 0;
static int g_width = 0;
static int g_height = 0;
static std::atomic<int> g_done{-1};

bool framebuffer_init(int width, int height) {
  int pixels = width * height;
  if (pixels > MAX_FB_PIXELS) return false;
  if (width != g_width || height != g_height) {
    g_width = width;
    g_height = height;
    g_cur = 0;
    g_done.store(-1, std::memory_order_release);
  }
  return true;
}

void framebuffer_write_tile(int tx, int ty, int tw, int th, const uint8_t* rgb565_data) {
  const uint16_t* src = reinterpret_cast<const uint16_t*>(rgb565_data);
  for (int y = 0; y < th; y++) {
    int src_offs = y * tw;
    int dst_offs = ((ty + y) * g_width + tx) * 3;
    for (int x = 0; x < tw; x++) {
      uint16_t p = src[src_offs + x];
      g_fb[g_cur][dst_offs + x * 3 + 0] = ((p >> 11) & 0x1F) << 3;
      g_fb[g_cur][dst_offs + x * 3 + 1] = ((p >> 5)  & 0x3F) << 2;
      g_fb[g_cur][dst_offs + x * 3 + 2] = ( p        & 0x1F) << 3;
    }
  }
}

void framebuffer_finish_frame() {
  int done = g_cur;
  g_cur ^= 1;
  g_done.store(done, std::memory_order_release);
}

bool framebuffer_get(uint8_t* out, int* out_width, int* out_height) {
  int idx = g_done.exchange(-1, std::memory_order_acquire);
  if (idx < 0) return false;
  int bytes = g_width * g_height * 3;
  std::memcpy(out, g_fb[idx], bytes);
  *out_width = g_width;
  *out_height = g_height;
  return true;
}
