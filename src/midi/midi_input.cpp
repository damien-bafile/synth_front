#include "midi_input.h"
#include <cstdio>
#include <cstring>

static void midi_read_callback(const MIDIPacketList* pktlist, void* readProcRefCon, void*) {
    MidiCallback* cb = reinterpret_cast<MidiCallback*>(readProcRefCon);
    const MIDIPacket* packet = &pktlist->packet[0];
    for (unsigned int i = 0; i < pktlist->numPackets; i++) {
        if (packet->length >= 3) {
            uint8_t status = packet->data[0];
            uint8_t data1  = packet->data[1];
            uint8_t data2  = packet->data[2];
            (*cb)(status, data1, data2);
        }
        packet = MIDIPacketNext(packet);
    }
}

bool midi_input_open(MidiInput* m, const char* source_name, MidiCallback cb) {
    m->client = 0;
    m->endpoint = 0;
    m->port = 0;
    m->cb = nullptr;
    m->running = false;

    CFStringRef name = CFSTR("synth_front");
    MIDIClientRef client;
    OSStatus err = MIDIClientCreate(name, nullptr, nullptr, &client);
    if (err != noErr) {
        fprintf(stderr, "MIDI: failed to create client (err %d)\n", (int)err);
        return false;
    }
    m->client = client;

    auto* cb_ptr = new MidiCallback(std::move(cb));
    m->cb = cb_ptr;

    MIDIPortRef port;
    err = MIDIInputPortCreate(client, CFSTR("Input"), midi_read_callback, cb_ptr, &port);
    if (err != noErr) {
        fprintf(stderr, "MIDI: failed to create input port (err %d)\n", (int)err);
        delete cb_ptr;
        m->cb = nullptr;
        MIDIClientDispose(client);
        m->client = 0;
        return false;
    }
    m->port = port;

    ItemCount n = MIDIGetNumberOfSources();
    MIDIEndpointRef found = 0;
    for (ItemCount i = 0; i < n; i++) {
        MIDIEndpointRef src = MIDIGetSource(i);
        if (!source_name) {
            found = src;
            break;
        }
        CFStringRef displayName;
        if (MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &displayName) == noErr) {
            char buf[256];
            CFStringGetCString(displayName, buf, sizeof(buf), kCFStringEncodingUTF8);
            CFRelease(displayName);
            if (strstr(buf, source_name)) {
                found = src;
                break;
            }
        }
    }

    if (!found) {
        fprintf(stderr, "MIDI: no matching source found, running without MIDI input.\n");
        MIDIPortDispose(port);
        m->port = 0;
        m->running = true;
        return true;
    }

    m->endpoint = found;
    err = MIDIPortConnectSource(port, found, nullptr);
    if (err != noErr) {
        fprintf(stderr, "MIDI: failed to connect source (err %d)\n", (int)err);
        delete cb_ptr;
        m->cb = nullptr;
        MIDIPortDispose(port);
        m->port = 0;
        MIDIClientDispose(client);
        m->client = 0;
        return false;
    }

    m->running = true;
    return true;
}

void midi_input_close(MidiInput* m) {
    if (m->port) {
        if (m->endpoint) {
            MIDIPortDisconnectSource(m->port, m->endpoint);
        }
        MIDIPortDispose(m->port);
        m->port = 0;
    }
    if (m->client) {
        MIDIClientDispose(m->client);
        m->client = 0;
    }
    if (m->cb) {
        delete m->cb;
        m->cb = nullptr;
    }
    m->running = false;
}

std::vector<std::string> midi_input_list_sources() {
    std::vector<std::string> names;
    ItemCount n = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < n; i++) {
        MIDIEndpointRef src = MIDIGetSource(i);
        CFStringRef displayName;
        if (MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &displayName) == noErr) {
            char buf[256];
            CFStringGetCString(displayName, buf, sizeof(buf), kCFStringEncodingUTF8);
            CFRelease(displayName);
            names.push_back(buf);
        }
    }
    return names;
}
