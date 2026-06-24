/// @file renderer.cpp
/// @brief OpenGL 3.3 Core renderer implementation.
///
/// Renders the Teensy display as a full-screen textured quad. The shaders are
/// embedded as string literals; the vertex layout is a simple interleaved
/// position + texcoord buffer.

#include "renderer.h"
#include <cstdio>

#define GL_GLEXT_PROTOTYPES 1
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#elif defined(_WIN32)
#include <GL/glew.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/glcorearb.h>
#endif

static const char* vertex_shader_src = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

static const char* fragment_shader_src = R"(#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
void main() {
    FragColor = texture(uTexture, TexCoord);
}
)";

// Compile a vertex or fragment shader from GLSL source; logs errors to stderr.
static unsigned int compile_shader(unsigned int type, const char* src) {
  unsigned int shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);

  int success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info[512];
    glGetShaderInfoLog(shader, 512, nullptr, info);
    fprintf(stderr, "Shader compile error: %s\n", info);
  }
  return shader;
}

// Initialise OpenGL: create context, compile/link shaders, set up full-screen quad VAO and texture.
bool renderer_init(Renderer* r, SDL_Window* window) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

  r->gl_ctx = SDL_GL_CreateContext(window);
  if (!r->gl_ctx) {
    fprintf(stderr, "Failed to create GL context: %s\n", SDL_GetError());
    return false;
  }
  SDL_GL_MakeCurrent(window, r->gl_ctx);
  SDL_GL_SetSwapInterval(1);

#ifdef _WIN32
  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW\n");
    return false;
  }
#endif

  unsigned int vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
  unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);

  r->program = glCreateProgram();
  glAttachShader(r->program, vs);
  glAttachShader(r->program, fs);
  glLinkProgram(r->program);
  glDeleteShader(vs);
  glDeleteShader(fs);

  int success;
  glGetProgramiv(r->program, GL_LINK_STATUS, &success);
  if (!success) {
    char info[512];
    glGetProgramInfoLog(r->program, 512, nullptr, info);
    fprintf(stderr, "Program link error: %s\n", info);
    glDeleteProgram(r->program);
    return false;
  }

  float vertices[] = {
      -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,

      -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,  -1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
  };

  glGenVertexArrays(1, &r->vao);
  glGenBuffers(1, &r->vbo);
  glBindVertexArray(r->vao);
  glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);

  glGenTextures(1, &r->texture);
  glBindTexture(GL_TEXTURE_2D, r->texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  uint8_t black[3] = {0, 0, 0};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, black);

  r->tex_width = 0;
  r->tex_height = 0;
  return true;
}

// Tear down all OpenGL resources.
void renderer_destroy(Renderer* r) {
  if (r->gl_ctx) {
    glDeleteTextures(1, &r->texture);
    glDeleteVertexArrays(1, &r->vao);
    glDeleteBuffers(1, &r->vbo);
    glDeleteProgram(r->program);
    SDL_GL_DestroyContext(r->gl_ctx);
    r->gl_ctx = nullptr;
  }
}

// Upload an RGB888 pixel buffer to the GL texture; re-allocates if dimensions change.
void renderer_upload_frame(Renderer* r, const uint8_t* data, int width, int height) {
  glBindTexture(GL_TEXTURE_2D, r->texture);
  if (width == r->tex_width && height == r->tex_height) {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
  } else {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    r->tex_width = width;
    r->tex_height = height;
  }
}

// Render the textured quad with aspect-correct letterboxing into the window.
void renderer_draw(Renderer* r, int window_width, int window_height) {
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(r->program);
  glBindVertexArray(r->vao);
  glBindTexture(GL_TEXTURE_2D, r->texture);

  float tex_aspect = r->tex_width > 0 ? (float)r->tex_width / (float)r->tex_height : 1.0f;
  float win_aspect = window_height > 0 ? (float)window_width / (float)window_height : 1.0f;

  if (tex_aspect > win_aspect) {
    r->vp_w = window_width;
    r->vp_h = (int)(window_width / tex_aspect);
    r->vp_x = 0;
    r->vp_y = (window_height - r->vp_h) / 2;
  } else {
    r->vp_h = window_height;
    r->vp_w = (int)(window_height * tex_aspect);
    r->vp_y = 0;
    r->vp_x = (window_width - r->vp_w) / 2;
  }

  glViewport(r->vp_x, r->vp_y, r->vp_w, r->vp_h);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}
