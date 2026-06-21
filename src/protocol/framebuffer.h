#pragma once
#include <cstdint>

constexpr int FB_WIDTH = 320;
constexpr int FB_HEIGHT = 480;
constexpr int FB_MAX_PIXELS = FB_WIDTH * FB_HEIGHT;
constexpr int FB_RGB565_SIZE = FB_MAX_PIXELS * 2;
constexpr int FB_RGB888_SIZE = FB_MAX_PIXELS * 3;

bool framebuffer_init(int width, int height);
void framebuffer_write_tile(int tx, int ty, int tw, int th, const uint8_t* rgb565_data);
void framebuffer_finish_frame();
bool framebuffer_get(uint8_t* out, int* out_width, int* out_height);
