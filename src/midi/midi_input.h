/// @file midi_input.h
/// @brief Platform-agnostic MIDI input interface.
///
/// Exactly one of the platform-specific implementations (ALSA / CoreMIDI / WinMM)
/// is compiled in by CMake. The callback is invoked on the MIDI input thread.

#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/// Callback signature for incoming MIDI messages.
/// @param status MIDI status byte.
/// @param data1  First data byte.
/// @param data2  Second data byte (may be 0 for 1-byte messages).
using MidiCallback = std::function<void(uint8_t status, uint8_t data1, uint8_t data2)>;

/// Opaque handle to a MIDI input backend. The public fields are owned by the
/// platform-specific backend and must not be modified by callers.
struct MidiInput {
    void* opaque = nullptr;       ///< Platform backend state.
    MidiCallback* cb = nullptr;   ///< Stored callback pointer (backend-owned).
    bool running = false;         ///< True if the backend was opened successfully.
};

/// Open a MIDI input source.
/// @param m           Handle to initialize.
/// @param source_name Optional name substring to match; if nullptr the first
///                    available source is used.
/// @param cb          Callback invoked for each incoming MIDI message.
/// @return            true if the backend opened successfully. If no matching
///                    source is found, the function may still return true and
///                    run without MIDI input (this is platform-specific).
/// @note              The callback may be invoked from a background thread.
bool midi_input_open(MidiInput* m, const char* source_name, MidiCallback cb);

/// Close a previously opened MIDI input and release all backend resources.
/// @param m Handle returned from midi_input_open().
void midi_input_close(MidiInput* m);

/// List human-readable names of available MIDI input sources.
/// @return Vector of source names.
std::vector<std::string> midi_input_list_sources();
