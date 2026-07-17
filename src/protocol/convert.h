/// @file convert.h
/// @brief RGB565↔RGB888 conversion and coordinate mapping utilities.

#pragma once
#include <cstdint>

struct Renderer;

/// Convert an RGB565 pixel buffer to RGB888.
/// @param src    Input RGB565 pixels (2 bytes per pixel).
/// @param dst    Output RGB888 pixels (3 bytes per pixel).
/// @param pixels Number of pixels to convert.
void convert_rgb565_to_rgb888(const uint8_t* src, uint8_t* dst, int pixels);

/// Map window-logical coordinates to framebuffer coordinates, correcting for
/// the letterbox viewport.
/// @param mx  Mouse X in window-logical coordinates.
/// @param my  Mouse Y in window-logical coordinates.
/// @param r   Renderer containing the letterbox viewport parameters.
/// @param tx  Output framebuffer X coordinate.
/// @param ty  Output framebuffer Y coordinate.
void window_to_fb(float mx, float my, const Renderer& r, uint16_t& tx, uint16_t& ty);
