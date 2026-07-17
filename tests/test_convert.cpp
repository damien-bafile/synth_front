#include <gtest/gtest.h>
#include <cstring>
#include "protocol/convert.h"
#include "protocol/framebuffer.h"
#include "render/renderer.h"

TEST(ConvertTest, Rgb565ToRgb888Black) {
  uint16_t src_pixel = 0x0000;
  uint8_t src[2];
  std::memcpy(src, &src_pixel, 2);
  uint8_t dst[3] = {0xFF, 0xFF, 0xFF};
  convert_rgb565_to_rgb888(src, dst, 1);
  EXPECT_EQ(dst[0], 0);
  EXPECT_EQ(dst[1], 0);
  EXPECT_EQ(dst[2], 0);
}

TEST(ConvertTest, Rgb565ToRgb888White) {
  uint16_t src_pixel = 0xFFFF;
  uint8_t src[2];
  std::memcpy(src, &src_pixel, 2);
  uint8_t dst[3] = {0};
  convert_rgb565_to_rgb888(src, dst, 1);
  EXPECT_EQ(dst[0], 0xF8);
  EXPECT_EQ(dst[1], 0xFC);
  EXPECT_EQ(dst[2], 0xF8);
}

TEST(ConvertTest, Rgb565ToRgb888Red) {
  uint16_t src_pixel = 0xF800;
  uint8_t src[2];
  std::memcpy(src, &src_pixel, 2);
  uint8_t dst[3] = {0};
  convert_rgb565_to_rgb888(src, dst, 1);
  EXPECT_EQ(dst[0], 0xF8);
  EXPECT_EQ(dst[1], 0);
  EXPECT_EQ(dst[2], 0);
}

TEST(ConvertTest, Rgb565ToRgb888Green) {
  uint16_t src_pixel = 0x07E0;
  uint8_t src[2];
  std::memcpy(src, &src_pixel, 2);
  uint8_t dst[3] = {0};
  convert_rgb565_to_rgb888(src, dst, 1);
  EXPECT_EQ(dst[0], 0);
  EXPECT_EQ(dst[1], 0xFC);
  EXPECT_EQ(dst[2], 0);
}

TEST(ConvertTest, Rgb565ToRgb888Blue) {
  uint16_t src_pixel = 0x001F;
  uint8_t src[2];
  std::memcpy(src, &src_pixel, 2);
  uint8_t dst[3] = {0};
  convert_rgb565_to_rgb888(src, dst, 1);
  EXPECT_EQ(dst[0], 0);
  EXPECT_EQ(dst[1], 0);
  EXPECT_EQ(dst[2], 0xF8);
}

TEST(ConvertTest, Rgb565ToRgb888MultiplePixels) {
  uint16_t src_pixels[3] = {0xF800, 0x07E0, 0x001F};
  uint8_t src[6];
  std::memcpy(src, src_pixels, 6);
  uint8_t dst[9] = {0};
  convert_rgb565_to_rgb888(src, dst, 3);
  EXPECT_EQ(dst[0], 0xF8); EXPECT_EQ(dst[1], 0);   EXPECT_EQ(dst[2], 0);
  EXPECT_EQ(dst[3], 0);   EXPECT_EQ(dst[4], 0xFC); EXPECT_EQ(dst[5], 0);
  EXPECT_EQ(dst[6], 0);   EXPECT_EQ(dst[7], 0);   EXPECT_EQ(dst[8], 0xF8);
}

TEST(ConvertTest, WindowToFbCenter) {
  Renderer r{};
  r.vp_x = 0;
  r.vp_y = 0;
  r.vp_w = 640;
  r.vp_h = 960;
  uint16_t tx, ty;
  window_to_fb(320.0f, 480.0f, r, tx, ty);
  EXPECT_EQ(tx, FB_WIDTH / 2);
  EXPECT_EQ(ty, FB_HEIGHT / 2);
}

TEST(ConvertTest, WindowToFbTopLeft) {
  Renderer r{};
  r.vp_x = 0;
  r.vp_y = 0;
  r.vp_w = 640;
  r.vp_h = 960;
  uint16_t tx, ty;
  window_to_fb(0.0f, 0.0f, r, tx, ty);
  EXPECT_EQ(tx, 0);
  EXPECT_EQ(ty, 0);
}

TEST(ConvertTest, WindowToFbBottomRight) {
  Renderer r{};
  r.vp_x = 0;
  r.vp_y = 0;
  r.vp_w = 640;
  r.vp_h = 960;
  uint16_t tx, ty;
  window_to_fb(640.0f, 960.0f, r, tx, ty);
  EXPECT_EQ(tx, FB_WIDTH - 1);
  EXPECT_EQ(ty, FB_HEIGHT - 1);
}

TEST(ConvertTest, WindowToFbClampNegative) {
  Renderer r{};
  r.vp_x = 0;
  r.vp_y = 0;
  r.vp_w = 640;
  r.vp_h = 960;
  uint16_t tx, ty;
  window_to_fb(-50.0f, -30.0f, r, tx, ty);
  EXPECT_EQ(tx, 0);
  EXPECT_EQ(ty, 0);
}

TEST(ConvertTest, WindowToFbClampOvershoot) {
  Renderer r{};
  r.vp_x = 0;
  r.vp_y = 0;
  r.vp_w = 640;
  r.vp_h = 960;
  uint16_t tx, ty;
  window_to_fb(1000.0f, 2000.0f, r, tx, ty);
  EXPECT_EQ(tx, FB_WIDTH - 1);
  EXPECT_EQ(ty, FB_HEIGHT - 1);
}

TEST(ConvertTest, WindowToFbLetterboxOffset) {
  Renderer r{};
  r.vp_x = 60;
  r.vp_y = 0;
  r.vp_w = 520;
  r.vp_h = 960;
  uint16_t tx, ty;
  window_to_fb(60.0f, 0.0f, r, tx, ty);
  EXPECT_EQ(tx, 0);
  EXPECT_EQ(ty, 0);
}

TEST(ConvertTest, WindowToFbZeroViewport) {
  Renderer r{};
  r.vp_w = 0;
  r.vp_h = 0;
  uint16_t tx = 99, ty = 99;
  window_to_fb(100.0f, 200.0f, r, tx, ty);
  EXPECT_EQ(tx, 0);
  EXPECT_EQ(ty, 0);
}
