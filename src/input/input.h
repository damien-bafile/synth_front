/// @file input.h
/// @brief Keyboard-to-synth input mapping.
///
/// Translates SDL key events into the actions the Teensy firmware understands:
/// front-panel keys, transport controls, musical notes, and encoder rotations.

#pragma once
#include <SDL3/SDL.h>
#include <cstdint>

/// High-level action produced by pressing or releasing a key.
enum class InputAction {
  KEY,       ///< A front-panel or UI key code (value holds the keycode).
  TRANSPORT, ///< A MIDI transport command (value holds the PacketType byte).
  NOTE,      ///< A musical note (value holds the MIDI note number).
  ENCODER    ///< A rotary encoder tick (value holds encoder index, encoder_delta the direction).
};

/// Result of mapping a single SDL key event.
struct InputResult {
    InputAction action;      ///< Action category.
    uint8_t value;           ///< Keycode, transport command, note number, or encoder index.
    int16_t encoder_delta;   ///< For ENCODER actions: +1 or -1 ticks.
};

/// Map an SDL keycode to a synth action.
/// @param key   The SDL key symbol.
/// @param shift Whether the Shift modifier is held (reverses encoder direction).
/// @return      The mapped input result.
InputResult input_map_key(SDL_Keycode key, bool shift);
