#pragma once
#include <SDL3/SDL.h>
#include <cstdint>

enum class InputAction { KEY, TRANSPORT, NOTE, ENCODER };

struct InputResult {
    InputAction action;
    uint8_t value;
    int16_t encoder_delta;
};

InputResult input_map_key(SDL_Keycode key, bool shift);
