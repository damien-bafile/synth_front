#pragma once
#include <cstdint>

// Initialize double-buffered framebuffer; returns false if dimensions exceed max.
bool framebuffer_init(int width, int height);
// Copy a received tile into the active framebuffer at (tx, ty).
void framebuffer_write_tile(int tx, int ty, int tw, int th, const uint8_t* rgb565_data);
// Mark the current buffer as ready and swap to the pending buffer.
void framebuffer_finish_frame();
// Retrieve the latest complete frame; returns false if no new frame is available.
bool framebuffer_get(uint8_t* out, int* out_width, int* out_height);
