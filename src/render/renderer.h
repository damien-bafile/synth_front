#pragma once
#include <SDL3/SDL.h>

struct Renderer {
  SDL_GLContext gl_ctx;
  unsigned int program;
  unsigned int vao;
  unsigned int vbo;
  unsigned int texture;
  int tex_width;
  int tex_height;
};

bool renderer_init(Renderer* r, SDL_Window* window);
void renderer_destroy(Renderer* r);
void renderer_upload_frame(Renderer* r, const uint8_t* data, int width, int height);
void renderer_draw(Renderer* r, int window_width, int window_height);
