#include <SDL3/SDL.h>

int main() {
  const char window_title[] = "teensy synth";
  auto sdl_window = SDL_CreateWindow(window_title, 800, 600, SDL_WINDOW_OPENGL);
  return 0;
}
