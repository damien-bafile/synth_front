/// @file protocol.h
/// @brief Binary packet protocol shared between synth_front and the Teensy firmware.
///
/// Packets have the on-wire layout:
///   [SYNC_BYTE] [type] [4-byte big-endian payload length] [payload...] [XOR checksum]
///
/// The checksum is computed over every byte after the sync byte, including the length
/// and payload. The payload length may be zero.

#pragma once
#include <cstdint>
#include <vector>

/// Packet type identifiers. These values are part of the wire protocol and must
/// stay synchronized with the Teensy firmware.
enum class PacketType : uint8_t {
  KEY_DOWN = 0x01,   ///< Physical/front-panel key pressed.
  KEY_UP   = 0x02,   ///< Physical/front-panel key released.
  ENCODER  = 0x05,   ///< Rotary encoder movement: payload [index, delta_hi, delta_lo].
  TOUCH    = 0x08,   ///< Touch-screen event: payload [x_hi, x_lo, y_hi, y_lo, state].

  FRAME = 0x81,      ///< Legacy full-frame debug string (text payload).
  DEBUG = 0x82,      ///< Debug text line from the device.
  READY = 0x83,      ///< Device handshake: synth is ready to receive events.
  FRAME_TILE = 0x88, ///< Display tile update containing RGB565 pixels.

  MIDI_NOTE_ON  = 0x90,  ///< MIDI note on (or note on with velocity 0 treated as off).
  MIDI_NOTE_OFF = 0x91,  ///< MIDI note off.
  MIDI_CC       = 0x92,  ///< MIDI control change.
  MIDI_PITCH_BEND = 0x93,///< MIDI pitch-bend change.

  MIDI_START    = 0xFA,  ///< MIDI transport start.
  MIDI_CONTINUE = 0xFB,  ///< MIDI transport continue.
  MIDI_STOP     = 0xFC,  ///< MIDI transport stop.

  RESET         = 0x0C,  ///< Request a device reset / restart.
};

/// Sync byte that precedes every packet.
static constexpr uint8_t SYNC_BYTE = 0xAA;

/// A fully parsed packet: its type and payload bytes.
struct Packet {
  PacketType type;              ///< Parsed packet type.
  std::vector<uint8_t> payload; ///< Raw payload bytes (may be empty).
};

/// Encode a type and payload into the binary packet format.
/// @param type    Packet type to send.
/// @param payload Pointer to payload bytes; may be nullptr if @p len is zero.
/// @param len     Number of payload bytes.
/// @return        Complete serialized packet, including sync byte and checksum.
std::vector<uint8_t> packet_encode(PacketType type, const uint8_t* payload, size_t len);

/// Parse a buffer for the next complete packet.
/// @param buf Input byte buffer.
/// @param len Number of valid bytes in @p buf.
/// @param out Receives the parsed packet on success.
/// @return    Total bytes consumed on success, 0 if the buffer is incomplete,
///            or -1 if the data is corrupt / out of sync.
/// @note      Callers should advance past consumed bytes and re-parse; on -1 the
///            caller should skip one byte and try again.
int packet_parse(const uint8_t* buf, int len, Packet* out);

/// Encode and write a packet to a connection.
/// @param fd      Open file descriptor (serial or TCP). The descriptor is not owned
///                by this function.
/// @param type    Packet type to send.
/// @param payload Payload bytes; may be nullptr if @p len is zero.
/// @param len     Number of payload bytes.
/// @note          This function is not intrinsically thread-safe; callers must
///                serialize access to @p fd if multiple threads may write.
int packet_send(int fd, PacketType type, const uint8_t* payload, size_t len);

/// Send an encoder delta packet.
/// @param fd    Open connection file descriptor.
/// @param index Encoder index (0-based).
/// @param delta Signed 16-bit rotation delta.
void packet_send_encoder(int fd, uint8_t index, int16_t delta);

/// Send a transport control packet with zero-length payload.
/// @param fd   Open connection file descriptor.
/// @param type Must be one of START, CONTINUE, or STOP.
void packet_send_transport(int fd, PacketType type);

/// Send a touch event packet.
/// @param fd     Open connection file descriptor.
/// @param x      Touch X coordinate.
/// @param y      Touch Y coordinate.
/// @param state  1 for press, 0 for release.
void packet_send_touch(int fd, uint16_t x, uint16_t y, uint8_t state);
