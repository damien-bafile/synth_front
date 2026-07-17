#include <gtest/gtest.h>
#include <cstring>
#include "protocol/framebuffer.h"

TEST(FramebufferTest, InitAndGetFrame) {
  ASSERT_TRUE(framebuffer_init(320, 480));

  uint8_t tile_data[32 * 32 * 2] = {};
  framebuffer_write_tile(0, 0, 32, 32, tile_data);
  framebuffer_write_tile(0, 32, 32, 32, tile_data);
  framebuffer_finish_frame();

  uint8_t out[FB_RGB565_SIZE];
  int w, h;
  bool got = framebuffer_get(out, &w, &h);
  EXPECT_TRUE(got);
  EXPECT_EQ(w, 320);
  EXPECT_EQ(h, 480);
}

TEST(FramebufferTest, NoFrameAvailable) {
  framebuffer_init(320, 480);
  uint8_t out[FB_RGB565_SIZE];
  int w, h;
  EXPECT_FALSE(framebuffer_get(out, &w, &h));
}

TEST(FramebufferTest, RejectsOversized) {
  EXPECT_FALSE(framebuffer_init(1000, 1000));
}

TEST(FramebufferTest, DoubleBuffering) {
  framebuffer_init(320, 480);

  uint8_t frame_a[32 * 480 * 2] = {};
  std::memset(frame_a, 0xAA, sizeof(frame_a));
  framebuffer_write_tile(0, 0, 32, 480, frame_a);
  framebuffer_finish_frame();

  uint8_t frame_b[32 * 480 * 2] = {};
  std::memset(frame_b, 0xBB, sizeof(frame_b));
  framebuffer_write_tile(0, 0, 32, 480, frame_b);

  uint8_t out[FB_RGB565_SIZE];
  int w, h;
  ASSERT_TRUE(framebuffer_get(out, &w, &h));
  EXPECT_EQ(out[0], 0xAA);
}

TEST(FramebufferTest, MultipleFrames) {
  framebuffer_init(320, 480);

  for (int frame = 0; frame < 5; frame++) {
    uint8_t tile[64 * 64 * 2] = {};
    tile[0] = frame;
    framebuffer_write_tile(0, 0, 64, 64, tile);
    framebuffer_finish_frame();

    uint8_t out[FB_RGB565_SIZE];
    int w, h;
    ASSERT_TRUE(framebuffer_get(out, &w, &h));
  }
}

// -- framebuffer_clear ------------------------------------------------------

TEST(FramebufferTest, ClearPublishesFrame) {
  framebuffer_init(320, 480);
  framebuffer_clear();

  uint8_t out[FB_RGB565_SIZE];
  int w, h;
  EXPECT_TRUE(framebuffer_get(out, &w, &h));
  EXPECT_EQ(w, 320);
  EXPECT_EQ(h, 480);
}

TEST(FramebufferTest, ClearProducesBlack) {
  framebuffer_init(320, 480);

  uint8_t tile[16 * 16 * 2] = {};
  tile[0] = 0xFF;
  tile[1] = 0xFF;
  framebuffer_write_tile(0, 0, 16, 16, tile);
  framebuffer_finish_frame();

  uint8_t tmp[FB_RGB565_SIZE];
  int tmp_w, tmp_h;
  framebuffer_get(tmp, &tmp_w, &tmp_h);  // consume the white frame
  framebuffer_clear();

  uint8_t out[FB_RGB565_SIZE];
  int w, h;
  ASSERT_TRUE(framebuffer_get(out, &w, &h));
  for (int i = 0; i < FB_RGB565_SIZE; i++)
    EXPECT_EQ(out[i], 0) << "byte " << i;
}

TEST(FramebufferTest, ClearConsumedOnlyOnce) {
  framebuffer_init(320, 480);
  framebuffer_clear();

  uint8_t out[FB_RGB565_SIZE];
  int w, h;
  EXPECT_TRUE(framebuffer_get(out, &w, &h));
  EXPECT_FALSE(framebuffer_get(out, &w, &h));
}

TEST(FramebufferTest, ClearAfterClear) {
  framebuffer_init(320, 480);
  framebuffer_clear();
  framebuffer_clear();

  uint8_t out[FB_RGB565_SIZE];
  int w, h;
  EXPECT_TRUE(framebuffer_get(out, &w, &h));
}
