#pragma once
#include <cstdint>

bool framebuffer_init(int width, int height);
void framebuffer_write_tile(int tx, int ty, int tw, int th, const uint8_t* rgb565_data);
void framebuffer_finish_frame();
bool framebuffer_get(uint8_t* out, int* out_width, int* out_height);
