#pragma once
#include <CoreMIDI/CoreMIDI.h>
#include <functional>
#include <string>
#include <vector>

using MidiCallback = std::function<void(uint8_t status, uint8_t data1, uint8_t data2)>;

// CoreMIDI client that reads from a single MIDI input source.
struct MidiInput {
    MIDIClientRef client = 0;
    MIDIEndpointRef endpoint = 0;
    MIDIPortRef port = 0;
    MidiCallback* cb = nullptr;
    bool running = false;
};

// Connect to the first available external MIDI source (or match by name).
bool midi_input_open(MidiInput* m, const char* source_name, MidiCallback cb);

// Disconnect and release CoreMIDI resources.
void midi_input_close(MidiInput* m);

// Return human-readable names of all MIDI input sources.
std::vector<std::string> midi_input_list_sources();
