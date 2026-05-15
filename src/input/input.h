#pragma once
#include <SDL3/SDL.h>
#include <cstdint>

// Map an SDL physical keycode to the Teensy protocol key code (1-byte).
uint8_t input_map_key(SDL_Keycode key);
