/// @file renderer.h
/// @brief OpenGL 3.3 Core renderer that draws the Teensy framebuffer as a
///        textured full-screen quad with aspect-correct letterboxing.

#pragma once
#include <SDL3/SDL.h>

/// OpenGL renderer state. The caller is responsible for zero-initializing the
/// struct before passing it to renderer_init().
struct Renderer {
  SDL_GLContext gl_ctx;  ///< SDL OpenGL context.
  unsigned int program;  ///< Linked shader program.
  unsigned int vao;      ///< Full-screen quad vertex array object.
  unsigned int vbo;      ///< Full-screen quad vertex buffer object.
  unsigned int texture;  ///< RGB888 texture holding the latest frame.
  int tex_width;         ///< Current texture width in pixels.
  int tex_height;        ///< Current texture height in pixels.
  int vp_x;              ///< Letterbox viewport left offset.
  int vp_y;              ///< Letterbox viewport top offset.
  int vp_w;              ///< Letterbox viewport width.
  int vp_h;              ///< Letterbox viewport height.
};

/// Create an OpenGL context, compile shaders, and initialize the full-screen quad
/// and fallback texture.
/// @param r      Renderer state to initialize. Must be zero-initialized.
/// @param window SDL window that will host the GL context.
/// @return       true on success, false on failure (errors are printed to stderr).
bool renderer_init(Renderer* r, SDL_Window* window);

/// Free all OpenGL resources and destroy the GL context.
/// @param r Renderer state. Safe to call on a zero-initialized renderer.
void renderer_destroy(Renderer* r);

/// Upload a new RGB888 frame into the OpenGL texture.
/// @param r      Renderer state.
/// @param data   RGB888 pixel bytes (3 * width * height bytes).
/// @param width  Frame width in pixels.
/// @param height Frame height in pixels.
void renderer_upload_frame(Renderer* r, const uint8_t* data, int width, int height);

/// Clear the window and draw the textured full-screen quad with letterboxing.
/// @param r             Renderer state.
/// @param window_width  Current window width in pixels.
/// @param window_height Current window height in pixels.
void renderer_draw(Renderer* r, int window_width, int window_height);
