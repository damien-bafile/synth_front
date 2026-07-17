/// @file convert.cpp
/// @brief RGB565→RGB888 conversion and coordinate mapping.

#include "convert.h"
#include "../render/renderer.h"
#include "framebuffer.h"

void convert_rgb565_to_rgb888(const uint8_t* src, uint8_t* dst, int pixels) {
  const uint16_t* s = reinterpret_cast<const uint16_t*>(src);
  for (int i = 0; i < pixels; i++) {
    uint16_t p = s[i];
    dst[i * 3 + 0] = ((p >> 11) & 0x1F) << 3;
    dst[i * 3 + 1] = ((p >> 5) & 0x3F) << 2;
    dst[i * 3 + 2] = (p & 0x1F) << 3;
  }
}

void window_to_fb(float mx, float my, const Renderer& r, uint16_t& tx, uint16_t& ty) {
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
  tx = static_cast<uint16_t>(rx * FB_WIDTH);
  ty = static_cast<uint16_t>(ry * FB_HEIGHT);
  if (tx >= FB_WIDTH)
    tx = FB_WIDTH - 1;
  if (ty >= FB_HEIGHT)
    ty = FB_HEIGHT - 1;
}
