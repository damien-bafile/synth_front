#include "protocol.h"
#include "../serial/serial_port.h"

static uint8_t compute_checksum(const uint8_t* data, int len) {
  uint8_t sum = 0;
  for (int i = 0; i < len; i++) sum ^= data[i];
  return sum;
}

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

void packet_send(int fd, PacketType type, const uint8_t* payload, size_t len) {
  auto data = packet_encode(type, payload, len);
  serial_write(fd, data.data(), (int)data.size());
}
