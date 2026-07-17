#include "unity.h"
#include "unity_fixture.h"
#include "protocol/protocol.h"

TEST_GROUP(Protocol);

TEST_SETUP(Protocol) {}
TEST_TEAR_DOWN(Protocol) {}

TEST(Protocol, EncodeDecodeRoundTrip)
{
  uint8_t payload[] = {0x01, 0x02, 0x03};
  auto encoded = packet_encode(PacketType::KEY_DOWN, payload, 3);

  Packet parsed;
  int consumed = packet_parse(encoded.data(), (int)encoded.size(), &parsed);
  TEST_ASSERT_EQUAL_INT((int)encoded.size(), consumed);
  TEST_ASSERT_EQUAL_INT((int)PacketType::KEY_DOWN, (int)parsed.type);
  TEST_ASSERT_EQUAL_INT(3, (int)parsed.payload.size());
  TEST_ASSERT_EQUAL_UINT8(0x01, parsed.payload[0]);
  TEST_ASSERT_EQUAL_UINT8(0x02, parsed.payload[1]);
  TEST_ASSERT_EQUAL_UINT8(0x03, parsed.payload[2]);
}

TEST(Protocol, RejectsBadSync)
{
  uint8_t data[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x42, 0x01};
  Packet p;
  int r = packet_parse(data, (int)sizeof(data), &p);
  TEST_ASSERT_EQUAL_INT(-1, r);
}

TEST(Protocol, RejectsBadChecksum)
{
  uint8_t data[] = {0xAA, 0x01, 0x00, 0x00, 0x00, 0x01, 0x42, 0xFF};
  Packet p;
  int r = packet_parse(data, (int)sizeof(data), &p);
  TEST_ASSERT_EQUAL_INT(-1, r);
}

TEST(Protocol, ReturnsZeroOnIncomplete)
{
  uint8_t data[] = {0xAA, 0x01, 0x00, 0x00, 0x00};
  Packet p;
  int r = packet_parse(data, (int)sizeof(data), &p);
  TEST_ASSERT_EQUAL_INT(0, r);
}

TEST(Protocol, EmptyPayload)
{
  auto encoded = packet_encode(PacketType::MIDI_START, nullptr, 0);
  TEST_ASSERT_EQUAL_INT(7, (int)encoded.size());
  Packet p;
  int r = packet_parse(encoded.data(), (int)encoded.size(), &p);
  TEST_ASSERT_EQUAL_INT(7, r);
  TEST_ASSERT_EQUAL_INT((int)PacketType::MIDI_START, (int)p.type);
  TEST_ASSERT_TRUE(p.payload.empty());
}

TEST(Protocol, LargePayload)
{
  uint8_t payload[256];
  for (int i = 0; i < 256; i++)
    payload[i] = (uint8_t)(i & 0xFF);
  auto encoded = packet_encode(PacketType::FRAME, payload, 256);
  Packet p;
  int r = packet_parse(encoded.data(), (int)encoded.size(), &p);
  TEST_ASSERT_EQUAL_INT((int)encoded.size(), r);
  TEST_ASSERT_EQUAL_INT(256, (int)p.payload.size());
  for (int i = 0; i < 256; i++)
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(i & 0xFF), p.payload[i]);
}

TEST(Protocol, AllPacketTypesRoundTrip)
{
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
    int r = packet_parse(encoded.data(), (int)encoded.size(), &p);
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)encoded.size(), r, "type");
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)type, (int)p.type, "type");
  }
}
