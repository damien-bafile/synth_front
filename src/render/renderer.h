#pragma once
#include <SDL3/SDL.h>

// OpenGL renderer state: shader program, vertex array, texture, and dimensions.
struct Renderer {
  SDL_GLContext gl_ctx;
  unsigned int program;
  unsigned int vao;
  unsigned int vbo;
  unsigned int texture;
  int tex_width;
  int tex_height;
  int vp_x, vp_y, vp_w, vp_h;
};

// Create GL context, compile shaders, and set up the full-screen quad + texture.
bool renderer_init(Renderer* r, SDL_Window* window);
// Free all OpenGL resources and destroy the GL context.
void renderer_destroy(Renderer* r);
// Upload a new RGB888 frame into the GL texture.
void renderer_upload_frame(Renderer* r, const uint8_t* data, int width, int height);
// Clear and draw the textured quad with letterboxing for the given window size.
void renderer_draw(Renderer* r, int window_width, int window_height);
