#include "protocol.h"
#include "../serial/serial_port.h"

// XOR-based checksum over a byte range.
static uint8_t compute_checksum(const uint8_t* data, int len) {
  uint8_t sum = 0;
  for (int i = 0; i < len; i++) sum ^= data[i];
  return sum;
}

// Build a binary packet: SYNC | type | 4B BE length | payload | xor_checksum.
std::vector<uint8_t> packet_encode(PacketType type, const uint8_t* payload, size_t len) {
  std::vector<uint8_t> buf;
  buf.reserve(7 + len);
  buf.push_back(SYNC_BYTE);
  buf.push_back(static_cast<uint8_t>(type));
  buf.push_back((len >> 24) & 0xFF);
  buf.push_back((len >> 16) & 0xFF);
  buf.push_back((len >> 8) & 0xFF);
  buf.push_back(len & 0xFF);
  for (size_t i = 0; i < len; i++) buf.push_back(payload[i]);
  buf.push_back(compute_checksum(buf.data() + 1, (int)buf.size() - 1));
  return buf;
}

// Try to parse one complete packet from buf; returns total bytes consumed, 0 (incomplete), or -1 (corrupt).
int packet_parse(const uint8_t* buf, int len, Packet* out) {
  if (len < 7) return 0;
  if (buf[0] != SYNC_BYTE) return -1;

  uint32_t payload_len = ((uint32_t)buf[2] << 24) | ((uint32_t)buf[3] << 16)
                       | ((uint32_t)buf[4] << 8)  |  (uint32_t)buf[5];
  int total_len = 7 + (int)payload_len;
  if (len < total_len) return 0;

  uint8_t checksum = buf[total_len - 1];
  uint8_t expected = compute_checksum(buf + 1, total_len - 2);
  if (checksum != expected) return -1;

  out->type = static_cast<PacketType>(buf[1]);
  out->payload.assign(buf + 6, buf + 6 + payload_len);
  return total_len;
}

// Encode and send a packet over the connection (serial or TCP).
void packet_send(int fd, PacketType type, const uint8_t* payload, size_t len) {
  auto data = packet_encode(type, payload, len);
  serial_write(fd, data.data(), (int)data.size());
}

void packet_send_encoder(int fd, uint8_t index, int16_t delta) {
  uint8_t payload[3] = { index, (uint8_t)((delta >> 8) & 0xFF), (uint8_t)(delta & 0xFF) };
  packet_send(fd, PacketType::ENCODER, payload, 3);
}

void packet_send_transport(int fd, PacketType type) {
  packet_send(fd, type, nullptr, 0);
}
