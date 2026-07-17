/// @file framebuffer.cpp
/// @brief Double-buffered RGB565 framebuffer implementation.
///
/// Two static buffers are ping-ponged between the serial producer thread
/// (writing tiles and finishing frames) and the main consumer thread
/// (reading completed frames). g_done uses an atomic exchange so the consumer
/// safely claims the latest frame exactly once.

#include "framebuffer.h"
#include <cstring>
#include <atomic>

static uint8_t g_fb[2][FB_RGB565_SIZE];
static std::atomic<int> g_cur{0};
static std::atomic<int> g_width{0};
static std::atomic<int> g_height{0};
static std::atomic<int> g_done{-1};

// Reset framebuffer state for new dimensions; rejects sizes above the static limit.
bool framebuffer_init(int width, int height) {
  int pixels = width * height;
  if (pixels > FB_MAX_PIXELS)
    return false;
  if (width != g_width.load(std::memory_order_relaxed) || height != g_height.load(std::memory_order_relaxed)) {
    g_width.store(width, std::memory_order_relaxed);
    g_height.store(height, std::memory_order_relaxed);
    g_cur.store(0, std::memory_order_relaxed);
    g_done.store(-1, std::memory_order_release);
  }
  return true;
}

// Copy a tile's worth of RGB565 pixels into the active framebuffer.
void framebuffer_write_tile(int tx, int ty, int tw, int th, const uint8_t* rgb565_data) {
  int row_bytes = tw * 2;
  int stride = g_width.load(std::memory_order_relaxed);
  int buf_idx = g_cur.load(std::memory_order_relaxed);
  const uint8_t* src = rgb565_data;
  for (int y = 0; y < th; y++) {
    int dst_offs = ((ty + y) * stride + tx) * 2;
    std::memcpy(g_fb[buf_idx] + dst_offs, src, row_bytes);
    src += row_bytes;
  }
}

// Swap buffers: the current frame becomes available for reading, and writing continues on the
// other.
void framebuffer_finish_frame() {
  int done = g_cur.load(std::memory_order_relaxed);
  g_cur.store(done ^ 1, std::memory_order_relaxed);
  g_done.store(done, std::memory_order_release);
}

// Clear both buffers to black and publish a frame.
void framebuffer_clear() {
  std::memset(g_fb[0], 0, FB_RGB565_SIZE);
  std::memset(g_fb[1], 0, FB_RGB565_SIZE);
  g_cur.store(0, std::memory_order_relaxed);
  g_done.store(0, std::memory_order_release);
}

// Copy the latest finished frame into out; returns false if no new frame is ready.
bool framebuffer_get(uint8_t* out, int* out_width, int* out_height) {
  int idx = g_done.exchange(-1, std::memory_order_acquire);
  if (idx < 0)
    return false;
  int w = g_width.load(std::memory_order_relaxed);
  int h = g_height.load(std::memory_order_relaxed);
  std::memcpy(out, g_fb[idx], w * h * 2);
  *out_width = w;
  *out_height = h;
  return true;
}
