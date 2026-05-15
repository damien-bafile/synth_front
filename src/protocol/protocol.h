#pragma once
#include <cstdint>
#include <vector>

// Binary packet type identifiers shared between synth_front and Teensy.
enum class PacketType : uint8_t {
  KEY_DOWN = 0x01,
  KEY_UP   = 0x02,
  PING     = 0x04,

  FRAME = 0x81,
  DEBUG = 0x82,
  READY = 0x83,
  FRAME_TILE = 0x88,

  MIDI_NOTE_ON  = 0x90,
  MIDI_NOTE_OFF = 0x91,
  MIDI_CC       = 0x92,
  MIDI_PITCH_BEND = 0x93,
};

static constexpr uint8_t SYNC_BYTE = 0xAA;

// Parsed binary packet: a type code and its payload bytes.
struct Packet {
  PacketType type;
  std::vector<uint8_t> payload;
};

// Serialize a type + payload into a binary packet with sync byte and XOR checksum.
std::vector<uint8_t> packet_encode(PacketType type, const uint8_t* payload, size_t len);

// Parse a buffer for the next complete packet; returns bytes consumed, 0 (need more), or -1 (bad).
int packet_parse(const uint8_t* buf, int len, Packet* out);

// Encode a packet and write it to a file descriptor (serial or TCP).
void packet_send(int fd, PacketType type, const uint8_t* payload, size_t len);
