#include <gtest/gtest.h>
#include "input/input.h"

TEST(InputTest, ArrowKeys) {
  auto r = input_map_key(SDLK_UP, false);
  EXPECT_EQ(r.action, InputAction::KEY);
  EXPECT_EQ(r.value, 0x01);

  r = input_map_key(SDLK_DOWN, false);
  EXPECT_EQ(r.value, 0x02);

  r = input_map_key(SDLK_LEFT, false);
  EXPECT_EQ(r.value, 0x03);

  r = input_map_key(SDLK_RIGHT, false);
  EXPECT_EQ(r.value, 0x04);
}

TEST(InputTest, ReturnAndSpace) {
  EXPECT_EQ(input_map_key(SDLK_RETURN, false).value, 0x10);
  EXPECT_EQ(input_map_key(SDLK_SPACE, false).value, 0x11);
}

TEST(InputTest, FunctionKeysF1F8) {
  EXPECT_EQ(input_map_key(SDLK_F1, false).value, 0x70);
  EXPECT_EQ(input_map_key(SDLK_F2, false).value, 0x71);
  EXPECT_EQ(input_map_key(SDLK_F3, false).value, 0x72);
  EXPECT_EQ(input_map_key(SDLK_F4, false).value, 0x73);
  EXPECT_EQ(input_map_key(SDLK_F5, false).value, 0x74);
  EXPECT_EQ(input_map_key(SDLK_F6, false).value, 0x75);
  EXPECT_EQ(input_map_key(SDLK_F7, false).value, 0x76);
  EXPECT_EQ(input_map_key(SDLK_F8, false).value, 0x77);
}

TEST(InputTest, FunctionKeysF9F12) {
  EXPECT_EQ(input_map_key(SDLK_F9, false).value, 0x78);
  EXPECT_EQ(input_map_key(SDLK_F10, false).value, 0x79);
  EXPECT_EQ(input_map_key(SDLK_F11, false).value, 0x7A);
  EXPECT_EQ(input_map_key(SDLK_F12, false).value, 0x7B);
}

TEST(InputTest, NumberKeys) {
  EXPECT_EQ(input_map_key(SDLK_1, false).value, 0x50);
  EXPECT_EQ(input_map_key(SDLK_2, false).value, 0x51);
  EXPECT_EQ(input_map_key(SDLK_3, false).value, 0x52);
  EXPECT_EQ(input_map_key(SDLK_4, false).value, 0x53);
  EXPECT_EQ(input_map_key(SDLK_5, false).value, 0x54);
  EXPECT_EQ(input_map_key(SDLK_6, false).value, 0x55);
  EXPECT_EQ(input_map_key(SDLK_7, false).value, 0x56);
  EXPECT_EQ(input_map_key(SDLK_8, false).value, 0x57);
  EXPECT_EQ(input_map_key(SDLK_9, false).value, 0x58);
  EXPECT_EQ(input_map_key(SDLK_0, false).value, 0x59);
}

TEST(InputTest, NoteKeys) {
  auto r = input_map_key(SDLK_Q, false);
  EXPECT_EQ(r.action, InputAction::NOTE);
  EXPECT_EQ(r.value, 60);

  EXPECT_EQ(input_map_key(SDLK_W, false).value, 62);
  EXPECT_EQ(input_map_key(SDLK_E, false).value, 64);
  EXPECT_EQ(input_map_key(SDLK_R, false).value, 65);
  EXPECT_EQ(input_map_key(SDLK_T, false).value, 67);
  EXPECT_EQ(input_map_key(SDLK_Y, false).value, 69);
  EXPECT_EQ(input_map_key(SDLK_U, false).value, 71);
  EXPECT_EQ(input_map_key(SDLK_I, false).value, 72);
}

TEST(InputTest, EncoderKeysNoShift) {
  auto r = input_map_key(SDLK_A, false);
  EXPECT_EQ(r.action, InputAction::ENCODER);
  EXPECT_EQ(r.value, 0);
  EXPECT_EQ(r.encoder_delta, 1);

  r = input_map_key(SDLK_S, false);
  EXPECT_EQ(r.value, 1);
  EXPECT_EQ(r.encoder_delta, 1);

  r = input_map_key(SDLK_D, false);
  EXPECT_EQ(r.value, 2);
  EXPECT_EQ(r.encoder_delta, 1);

  r = input_map_key(SDLK_F, false);
  EXPECT_EQ(r.value, 3);
  EXPECT_EQ(r.encoder_delta, 1);

  r = input_map_key(SDLK_G, false);
  EXPECT_EQ(r.value, 4);
  EXPECT_EQ(r.encoder_delta, 1);

  r = input_map_key(SDLK_H, false);
  EXPECT_EQ(r.value, 5);
  EXPECT_EQ(r.encoder_delta, 1);

  r = input_map_key(SDLK_J, false);
  EXPECT_EQ(r.value, 6);
  EXPECT_EQ(r.encoder_delta, 1);

  r = input_map_key(SDLK_K, false);
  EXPECT_EQ(r.value, 7);
  EXPECT_EQ(r.encoder_delta, 1);
}

TEST(InputTest, EncoderKeysWithShift) {
  auto r = input_map_key(SDLK_A, true);
  EXPECT_EQ(r.action, InputAction::ENCODER);
  EXPECT_EQ(r.value, 0);
  EXPECT_EQ(r.encoder_delta, -1);

  r = input_map_key(SDLK_K, true);
  EXPECT_EQ(r.value, 7);
  EXPECT_EQ(r.encoder_delta, -1);
}

TEST(InputTest, ExtraButtons) {
  EXPECT_EQ(input_map_key(SDLK_Z, false).value, 0x61);
  EXPECT_EQ(input_map_key(SDLK_X, false).value, 0x62);
  EXPECT_EQ(input_map_key(SDLK_C, false).value, 0x63);
  EXPECT_EQ(input_map_key(SDLK_V, false).value, 0x64);
  EXPECT_EQ(input_map_key(SDLK_B, false).value, 0x65);
  EXPECT_EQ(input_map_key(SDLK_N, false).value, 0x66);
  EXPECT_EQ(input_map_key(SDLK_M, false).value, 0x67);
}

TEST(InputTest, TransportControls) {
  auto r = input_map_key(SDLK_ESCAPE, false);
  EXPECT_EQ(r.action, InputAction::TRANSPORT);
  EXPECT_EQ(r.value, 0xFC);

  r = input_map_key(SDLK_TAB, false);
  EXPECT_EQ(r.action, InputAction::TRANSPORT);
  EXPECT_EQ(r.value, 0xFB);
}

TEST(InputTest, CommaKey) {
  auto r = input_map_key(SDLK_COMMA, false);
  EXPECT_EQ(r.action, InputAction::KEY);
  EXPECT_EQ(r.value, 0x68);

  r = input_map_key(SDLK_COMMA, true);
  EXPECT_EQ(r.value, 0x08);
}

TEST(InputTest, PeriodKey) {
  auto r = input_map_key(SDLK_PERIOD, false);
  EXPECT_EQ(r.action, InputAction::KEY);
  EXPECT_EQ(r.value, 0);

  r = input_map_key(SDLK_PERIOD, true);
  EXPECT_EQ(r.value, 0x08);
}

TEST(InputTest, UnknownKey) {
  auto r = input_map_key(SDLK_UNKNOWN, false);
  EXPECT_EQ(r.action, InputAction::KEY);
  EXPECT_EQ(r.value, 0);
}
