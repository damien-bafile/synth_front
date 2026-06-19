#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

using MidiCallback = std::function<void(uint8_t status, uint8_t data1, uint8_t data2)>;

struct MidiInput {
    void* opaque = nullptr;
    MidiCallback* cb = nullptr;
    bool running = false;
};

bool midi_input_open(MidiInput* m, const char* source_name, MidiCallback cb);
void midi_input_close(MidiInput* m);
std::vector<std::string> midi_input_list_sources();
