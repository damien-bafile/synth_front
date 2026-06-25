/// @file midi_input_coremidi.cpp
/// @brief CoreMIDI MIDI input backend for macOS.
///
/// Creates a CoreMIDI client and input port, connects to the first available
/// (or user-named) source endpoint, and dispatches 3-byte MIDI messages
/// through the user callback on CoreMIDI's internal callback thread.

#include "midi_input.h"
#include <CoreMIDI/CoreMIDI.h>
#include <cstdio>
#include <cstring>

struct CoreMidiBackend {
  MIDIClientRef client = 0;    ///< CoreMIDI client reference.
  MIDIEndpointRef endpoint = 0;///< Connected source endpoint (or 0).
  MIDIPortRef port = 0;        ///< Input port receiving MIDI packets.
};

// CoreMIDI packet callback: forwards each 3-byte MIDI message to the user
// callback. Invoked on CoreMIDI's internal thread.
static void midi_read_callback(const MIDIPacketList* pktlist, void* readProcRefCon, void*) {
  MidiCallback* cb = reinterpret_cast<MidiCallback*>(readProcRefCon);
  const MIDIPacket* packet = &pktlist->packet[0];
  for (unsigned int i = 0; i < pktlist->numPackets; i++) {
    if (packet->length >= 3) {
      uint8_t status = packet->data[0];
      uint8_t data1 = packet->data[1];
      uint8_t data2 = packet->data[2];
      (*cb)(status, data1, data2);
    }
    packet = MIDIPacketNext(packet);
  }
}

bool midi_input_open(MidiInput* m, const char* source_name, MidiCallback cb) {
  auto* b = new CoreMidiBackend;
  m->opaque = b;
  m->cb = nullptr;
  m->running = false;

  // Create a CoreMIDI client and input port for receiving packets.
  CFStringRef name = CFSTR("synth_front");
  MIDIClientRef client;
  OSStatus err = MIDIClientCreate(name, nullptr, nullptr, &client);
  if (err != noErr) {
    fprintf(stderr, "MIDI: failed to create client (err %d)\n", (int)err);
    delete b;
    m->opaque = nullptr;
    return false;
  }
  b->client = client;

  // Store the callback on the heap so its address remains stable in the C callback.
  auto* cb_ptr = new MidiCallback(std::move(cb));
  m->cb = cb_ptr;

  MIDIPortRef port;
  err = MIDIInputPortCreate(client, CFSTR("Input"), midi_read_callback, cb_ptr, &port);
  if (err != noErr) {
    fprintf(stderr, "MIDI: failed to create input port (err %d)\n", (int)err);
    delete cb_ptr;
    m->cb = nullptr;
    MIDIClientDispose(client);
    delete b;
    m->opaque = nullptr;
    return false;
  }
  b->port = port;

  // Select the first source, or the first source whose display name matches.
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

  // If no source is available we still report success; MIDI input is optional.
  if (!found) {
    fprintf(stderr, "MIDI: no matching source found, running without MIDI input.\n");
    MIDIPortDispose(port);
    b->port = 0;
    m->running = true;
    return true;
  }

  b->endpoint = found;
  err = MIDIPortConnectSource(port, found, nullptr);
  if (err != noErr) {
    fprintf(stderr, "MIDI: failed to connect source (err %d)\n", (int)err);
    delete cb_ptr;
    m->cb = nullptr;
    MIDIPortDispose(port);
    MIDIClientDispose(client);
    delete b;
    m->opaque = nullptr;
    return false;
  }

  m->running = true;
  return true;
}

void midi_input_close(MidiInput* m) {
  auto* b = static_cast<CoreMidiBackend*>(m->opaque);
  if (!b)
    return;

  // Disconnect and dispose in reverse order of creation.
  if (b->endpoint)
    MIDIPortDisconnectSource(b->port, b->endpoint);
  MIDIPortDispose(b->port);
  if (b->client) {
    MIDIClientDispose(b->client);
  }
  if (m->cb) {
    delete m->cb;
    m->cb = nullptr;
  }
  delete b;
  m->opaque = nullptr;
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
