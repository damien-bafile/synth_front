#include <gtest/gtest.h>
#include "protocol/protocol.h"

TEST(ProtocolTest, EncodeDecodeRoundTrip) {
  uint8_t payload[] = {0x01, 0x02, 0x03};
  auto encoded = packet_encode(PacketType::KEY_DOWN, payload, 3);

  Packet parsed;
  int consumed = packet_parse(encoded.data(), encoded.size(), &parsed);
  EXPECT_EQ(consumed, (int)encoded.size());
  EXPECT_EQ(parsed.type, PacketType::KEY_DOWN);
  ASSERT_EQ(parsed.payload.size(), 3);
  EXPECT_EQ(parsed.payload[0], 0x01);
  EXPECT_EQ(parsed.payload[1], 0x02);
  EXPECT_EQ(parsed.payload[2], 0x03);
}

TEST(ProtocolTest, RejectsBadSync) {
  uint8_t data[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x42, 0x01};
  Packet p;
  int r = packet_parse(data, sizeof(data), &p);
  EXPECT_EQ(r, -1);
}

TEST(ProtocolTest, RejectsBadChecksum) {
  uint8_t data[] = {0xAA, 0x01, 0x00, 0x00, 0x00, 0x01, 0x42, 0xFF};
  Packet p;
  int r = packet_parse(data, sizeof(data), &p);
  EXPECT_EQ(r, -1);
}

TEST(ProtocolTest, ReturnsZeroOnIncomplete) {
  uint8_t data[] = {0xAA, 0x01, 0x00, 0x00, 0x00};
  Packet p;
  int r = packet_parse(data, sizeof(data), &p);
  EXPECT_EQ(r, 0);
}

TEST(ProtocolTest, EmptyPayload) {
  auto encoded = packet_encode(PacketType::MIDI_START, nullptr, 0);
  ASSERT_EQ(encoded.size(), 7);
  Packet p;
  int r = packet_parse(encoded.data(), encoded.size(), &p);
  EXPECT_EQ(r, 7);
  EXPECT_EQ(p.type, PacketType::MIDI_START);
  EXPECT_TRUE(p.payload.empty());
}

TEST(ProtocolTest, LargePayload) {
  uint8_t payload[256];
  for (int i = 0; i < 256; i++)
    payload[i] = i & 0xFF;
  auto encoded = packet_encode(PacketType::FRAME, payload, 256);
  Packet p;
  int r = packet_parse(encoded.data(), encoded.size(), &p);
  EXPECT_EQ(r, (int)encoded.size());
  ASSERT_EQ(p.payload.size(), 256);
  for (int i = 0; i < 256; i++)
    EXPECT_EQ(p.payload[i], i & 0xFF);
}

TEST(ProtocolTest, AllPacketTypesRoundTrip) {
  PacketType types[] = {
      PacketType::KEY_DOWN,   PacketType::KEY_UP,
      PacketType::ENCODER,    PacketType::TOUCH,  PacketType::FRAME,
      PacketType::DEBUG,      PacketType::READY,  PacketType::FRAME_TILE,
      PacketType::MIDI_NOTE_ON, PacketType::MIDI_NOTE_OFF, PacketType::MIDI_CC,
      PacketType::MIDI_PITCH_BEND, PacketType::MIDI_START, PacketType::MIDI_CONTINUE,
      PacketType::MIDI_STOP,
  };
  uint8_t payload[] = {0xAB, 0xCD};
  for (auto type : types) {
    auto encoded = packet_encode(type, payload, 2);
    Packet p;
    int r = packet_parse(encoded.data(), encoded.size(), &p);
    EXPECT_EQ(r, (int)encoded.size()) << "type=" << (int)type;
    EXPECT_EQ(p.type, type) << "type=" << (int)type;
  }
}
