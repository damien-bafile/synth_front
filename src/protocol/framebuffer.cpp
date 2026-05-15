#include "framebuffer.h"
#include <cstring>
#include <atomic>

static constexpr int MAX_FB_PIXELS = 320 * 480;
static constexpr int FB_RGB565_SIZE = MAX_FB_PIXELS * 2;

static uint8_t g_fb[2][FB_RGB565_SIZE];
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
  int row_bytes = tw * 2;
  const uint8_t* src = rgb565_data;
  for (int y = 0; y < th; y++) {
    int dst_offs = ((ty + y) * g_width + tx) * 2;
    std::memcpy(g_fb[g_cur] + dst_offs, src, row_bytes);
    src += row_bytes;
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
  std::memcpy(out, g_fb[idx], g_width * g_height * 2);
  *out_width = g_width;
  *out_height = g_height;
  return true;
}
