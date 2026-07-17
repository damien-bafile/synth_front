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

// -- compute_checksum -------------------------------------------------------

TEST(ProtocolTest, ChecksumEmpty) {
  EXPECT_EQ(compute_checksum(nullptr, 0), 0);
}

TEST(ProtocolTest, ChecksumSingleByte) {
  uint8_t data[] = {0x42};
  EXPECT_EQ(compute_checksum(data, 1), 0x42);
}

TEST(ProtocolTest, ChecksumTwoBytes) {
  uint8_t data[] = {0xAB, 0xCD};
  EXPECT_EQ(compute_checksum(data, 2), 0xAB ^ 0xCD);
}

TEST(ProtocolTest, ChecksumSelfInverse) {
  uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
  uint8_t c = compute_checksum(data, 4);
  uint8_t with_csum[] = {0x12, 0x34, 0x56, 0x78, c};
  EXPECT_EQ(compute_checksum(with_csum, 5), 0);
}

#ifndef _WIN32
// -- packet_send pipe tests (POSIX only) ------------------------------------

#include <unistd.h>

static std::vector<uint8_t> read_pipe(int fd) {
  std::vector<uint8_t> result;
  uint8_t buf[4096];
  int n;
  while ((n = read(fd, buf, sizeof(buf))) > 0)
    result.insert(result.end(), buf, buf + n);
  return result;
}

TEST(ProtocolTest, PacketSendViaPipe) {
  int pipefd[2];
  ASSERT_EQ(pipe(pipefd), 0);
  uint8_t payload[] = {0xDE, 0xAD};
  int ret = packet_send(pipefd[1], PacketType::KEY_DOWN, payload, 2);
  ASSERT_GT(ret, 0);
  close(pipefd[1]);

  auto written = read_pipe(pipefd[0]);
  close(pipefd[0]);

  auto expected = packet_encode(PacketType::KEY_DOWN, payload, 2);
  ASSERT_EQ(written.size(), expected.size());
  for (size_t i = 0; i < written.size(); i++)
    EXPECT_EQ(written[i], expected[i]) << "byte " << i;
}

TEST(ProtocolTest, PacketSendEncoderViaPipe) {
  int pipefd[2];
  ASSERT_EQ(pipe(pipefd), 0);
  packet_send_encoder(pipefd[1], 3, -256);
  close(pipefd[1]);

  auto written = read_pipe(pipefd[0]);
  close(pipefd[0]);

  uint8_t payload[] = {3, 0xFF, 0x00};
  auto expected = packet_encode(PacketType::ENCODER, payload, 3);
  ASSERT_EQ(written.size(), expected.size());
  for (size_t i = 0; i < written.size(); i++)
    EXPECT_EQ(written[i], expected[i]) << "byte " << i;
}

TEST(ProtocolTest, PacketSendTransportViaPipe) {
  int pipefd[2];
  ASSERT_EQ(pipe(pipefd), 0);
  packet_send_transport(pipefd[1], PacketType::MIDI_START);
  close(pipefd[1]);

  auto written = read_pipe(pipefd[0]);
  close(pipefd[0]);

  auto expected = packet_encode(PacketType::MIDI_START, nullptr, 0);
  ASSERT_EQ(written.size(), expected.size());
  for (size_t i = 0; i < written.size(); i++)
    EXPECT_EQ(written[i], expected[i]) << "byte " << i;
}

TEST(ProtocolTest, PacketSendTouchViaPipe) {
  int pipefd[2];
  ASSERT_EQ(pipe(pipefd), 0);
  packet_send_touch(pipefd[1], 0xABCD, 0x1234, 1);
  close(pipefd[1]);

  auto written = read_pipe(pipefd[0]);
  close(pipefd[0]);

  uint8_t payload[] = {0xAB, 0xCD, 0x12, 0x34, 1};
  auto expected = packet_encode(PacketType::TOUCH, payload, 5);
  ASSERT_EQ(written.size(), expected.size());
  for (size_t i = 0; i < written.size(); i++)
    EXPECT_EQ(written[i], expected[i]) << "byte " << i;
}
#endif
