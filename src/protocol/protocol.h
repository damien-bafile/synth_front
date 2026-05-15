#pragma once
#include <cstdint>
#include <vector>

enum class PacketType : uint8_t {
  KEY_DOWN = 0x01,
  KEY_UP   = 0x02,
  PING     = 0x04,

  FRAME = 0x81,
  DEBUG = 0x82,
  READY = 0x83,
  FRAME_TILE = 0x88,
};

static constexpr uint8_t SYNC_BYTE = 0xAA;

struct Packet {
  PacketType type;
  std::vector<uint8_t> payload;
};

std::vector<uint8_t> packet_encode(PacketType type, const uint8_t* payload, size_t len);

int packet_parse(const uint8_t* buf, int len, Packet* out);

void packet_send(int fd, PacketType type, const uint8_t* payload, size_t len);
