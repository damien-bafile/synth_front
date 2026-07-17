#include "unity.h"
#include "unity_fixture.h"
#include <cstring>
#include "protocol/framebuffer.h"

TEST_GROUP(Framebuffer);

TEST_SETUP(Framebuffer) {}
TEST_TEAR_DOWN(Framebuffer) {}

TEST(Framebuffer, InitAndGetFrame)
{
  TEST_ASSERT_TRUE(framebuffer_init(320, 480));

  uint8_t tile_data[32 * 32 * 2] = {};
  framebuffer_write_tile(0, 0, 32, 32, tile_data);
  framebuffer_write_tile(0, 32, 32, 32, tile_data);
  framebuffer_finish_frame();

  uint8_t out[FB_RGB565_SIZE];
  int w, h;
  bool got = framebuffer_get(out, &w, &h);
  TEST_ASSERT_TRUE(got);
  TEST_ASSERT_EQUAL_INT(320, w);
  TEST_ASSERT_EQUAL_INT(480, h);
}

TEST(Framebuffer, NoFrameAvailable)
{
  framebuffer_init(320, 480);
  uint8_t out[FB_RGB565_SIZE];
  int w, h;
  TEST_ASSERT_FALSE(framebuffer_get(out, &w, &h));
}

TEST(Framebuffer, RejectsOversized)
{
  TEST_ASSERT_FALSE(framebuffer_init(1000, 1000));
}

TEST(Framebuffer, DoubleBuffering)
{
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
  TEST_ASSERT_TRUE(framebuffer_get(out, &w, &h));
  TEST_ASSERT_EQUAL_UINT8(0xAA, out[0]);
}

TEST(Framebuffer, MultipleFrames)
{
  framebuffer_init(320, 480);

  for (int frame = 0; frame < 5; frame++) {
    uint8_t tile[64 * 64 * 2] = {};
    tile[0] = (uint8_t)frame;
    framebuffer_write_tile(0, 0, 64, 64, tile);
    framebuffer_finish_frame();

    uint8_t out[FB_RGB565_SIZE];
    int w, h;
    TEST_ASSERT_TRUE(framebuffer_get(out, &w, &h));
  }
}
