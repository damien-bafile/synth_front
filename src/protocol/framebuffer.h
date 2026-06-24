/// @file framebuffer.h
/// @brief Double-buffered RGB565 framebuffer that receives tiled display updates
///        from the Teensy and hands completed frames to the renderer.
///
/// The framebuffer uses a simple lock-free ping-pong scheme: the producer
/// (serial thread) writes tiles into the "back" buffer and calls
/// framebuffer_finish_frame() to publish it; the consumer (main thread) reads
/// the published "front" buffer via framebuffer_get().

#pragma once
#include <cstdint>

/// Display width in pixels. Must match the Teensy firmware's screen dimensions.
constexpr int FB_WIDTH = 320;
/// Display height in pixels.
constexpr int FB_HEIGHT = 480;
/// Maximum pixel count for the statically allocated buffers.
constexpr int FB_MAX_PIXELS = FB_WIDTH * FB_HEIGHT;
/// Size in bytes of one RGB565 frame (2 bytes per pixel).
constexpr int FB_RGB565_SIZE = FB_MAX_PIXELS * 2;
/// Size in bytes of one RGB888 frame (3 bytes per pixel).
constexpr int FB_RGB888_SIZE = FB_MAX_PIXELS * 3;

/// Initialize or resize the framebuffer.
/// @param width  Desired frame width in pixels.
/// @param height Desired frame height in pixels.
/// @return       true if the dimensions fit in the static buffers, false otherwise.
bool framebuffer_init(int width, int height);

/// Copy a tile of RGB565 pixels into the currently active back buffer.
/// @param tx          Tile left edge in pixels.
/// @param ty          Tile top edge in pixels.
/// @param tw          Tile width in pixels.
/// @param th          Tile height in pixels.
/// @param rgb565_data Raw RGB565 pixel bytes (2 * tw * th bytes).
/// @note              The caller must ensure tiles fit within the initialized dimensions.
void framebuffer_write_tile(int tx, int ty, int tw, int th, const uint8_t* rgb565_data);

/// Publish the current back buffer as the readable front buffer.
/// Call this once the last tile of a frame has been written.
void framebuffer_finish_frame();

/// Copy the latest published front frame into an output buffer.
/// @param out        Destination buffer; must hold at least FB_RGB565_SIZE bytes.
/// @param out_width  Receives the frame width in pixels.
/// @param out_height Receives the frame height in pixels.
/// @return           true if a new frame was available and copied, false otherwise.
/// @note             Calling this consumes the frame; subsequent calls return false
///                   until framebuffer_finish_frame() publishes another frame.
bool framebuffer_get(uint8_t* out, int* out_width, int* out_height);

/// Clear both framebuffer buffers to black and publish a frame.
/// Call when the connection to the Teensy is lost so the screen goes dark.
void framebuffer_clear();
