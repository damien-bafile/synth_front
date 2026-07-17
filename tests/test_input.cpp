#include "unity.h"
#include "unity_fixture.h"
#include "input/input.h"

TEST_GROUP(Input);

TEST_SETUP(Input) {}
TEST_TEAR_DOWN(Input) {}

TEST(Input, ArrowKeys)
{
  auto r = input_map_key(SDLK_UP, false);
  TEST_ASSERT_EQUAL_INT((int)InputAction::KEY, (int)r.action);
  TEST_ASSERT_EQUAL_UINT8(0x01, r.value);

  r = input_map_key(SDLK_DOWN, false);
  TEST_ASSERT_EQUAL_UINT8(0x02, r.value);

  r = input_map_key(SDLK_LEFT, false);
  TEST_ASSERT_EQUAL_UINT8(0x03, r.value);

  r = input_map_key(SDLK_RIGHT, false);
  TEST_ASSERT_EQUAL_UINT8(0x04, r.value);
}

TEST(Input, ReturnAndSpace)
{
  auto r = input_map_key(SDLK_RETURN, false);
  TEST_ASSERT_EQUAL_UINT8(0x10, r.value);

  r = input_map_key(SDLK_SPACE, false);
  TEST_ASSERT_EQUAL_UINT8(0x11, r.value);
}

TEST(Input, FunctionKeysF1F8)
{
  TEST_ASSERT_EQUAL_UINT8(0x70, input_map_key(SDLK_F1, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x71, input_map_key(SDLK_F2, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x72, input_map_key(SDLK_F3, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x73, input_map_key(SDLK_F4, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x74, input_map_key(SDLK_F5, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x75, input_map_key(SDLK_F6, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x76, input_map_key(SDLK_F7, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x77, input_map_key(SDLK_F8, false).value);
}

TEST(Input, FunctionKeysF9F12)
{
  TEST_ASSERT_EQUAL_UINT8(0x78, input_map_key(SDLK_F9, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x79, input_map_key(SDLK_F10, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x7A, input_map_key(SDLK_F11, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x7B, input_map_key(SDLK_F12, false).value);
}

TEST(Input, NumberKeys)
{
  TEST_ASSERT_EQUAL_UINT8(0x50, input_map_key(SDLK_1, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x51, input_map_key(SDLK_2, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x52, input_map_key(SDLK_3, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x53, input_map_key(SDLK_4, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x54, input_map_key(SDLK_5, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x55, input_map_key(SDLK_6, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x56, input_map_key(SDLK_7, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x57, input_map_key(SDLK_8, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x58, input_map_key(SDLK_9, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x59, input_map_key(SDLK_0, false).value);
}

TEST(Input, NoteKeys)
{
  auto r = input_map_key(SDLK_Q, false);
  TEST_ASSERT_EQUAL_INT((int)InputAction::NOTE, (int)r.action);
  TEST_ASSERT_EQUAL_UINT8(60, r.value);

  TEST_ASSERT_EQUAL_UINT8(62, input_map_key(SDLK_W, false).value);
  TEST_ASSERT_EQUAL_UINT8(64, input_map_key(SDLK_E, false).value);
  TEST_ASSERT_EQUAL_UINT8(65, input_map_key(SDLK_R, false).value);
  TEST_ASSERT_EQUAL_UINT8(67, input_map_key(SDLK_T, false).value);
  TEST_ASSERT_EQUAL_UINT8(69, input_map_key(SDLK_Y, false).value);
  TEST_ASSERT_EQUAL_UINT8(71, input_map_key(SDLK_U, false).value);
  TEST_ASSERT_EQUAL_UINT8(72, input_map_key(SDLK_I, false).value);
}

TEST(Input, EncoderKeysNoShift)
{
  auto r = input_map_key(SDLK_A, false);
  TEST_ASSERT_EQUAL_INT((int)InputAction::ENCODER, (int)r.action);
  TEST_ASSERT_EQUAL_UINT8(0, r.value);
  TEST_ASSERT_EQUAL_INT16(1, r.encoder_delta);

  r = input_map_key(SDLK_S, false);
  TEST_ASSERT_EQUAL_UINT8(1, r.value);
  TEST_ASSERT_EQUAL_INT16(1, r.encoder_delta);

  r = input_map_key(SDLK_D, false);
  TEST_ASSERT_EQUAL_UINT8(2, r.value);
  TEST_ASSERT_EQUAL_INT16(1, r.encoder_delta);

  r = input_map_key(SDLK_F, false);
  TEST_ASSERT_EQUAL_UINT8(3, r.value);
  TEST_ASSERT_EQUAL_INT16(1, r.encoder_delta);

  r = input_map_key(SDLK_G, false);
  TEST_ASSERT_EQUAL_UINT8(4, r.value);
  TEST_ASSERT_EQUAL_INT16(1, r.encoder_delta);

  r = input_map_key(SDLK_H, false);
  TEST_ASSERT_EQUAL_UINT8(5, r.value);
  TEST_ASSERT_EQUAL_INT16(1, r.encoder_delta);

  r = input_map_key(SDLK_J, false);
  TEST_ASSERT_EQUAL_UINT8(6, r.value);
  TEST_ASSERT_EQUAL_INT16(1, r.encoder_delta);

  r = input_map_key(SDLK_K, false);
  TEST_ASSERT_EQUAL_UINT8(7, r.value);
  TEST_ASSERT_EQUAL_INT16(1, r.encoder_delta);
}

TEST(Input, EncoderKeysWithShift)
{
  auto r = input_map_key(SDLK_A, true);
  TEST_ASSERT_EQUAL_INT((int)InputAction::ENCODER, (int)r.action);
  TEST_ASSERT_EQUAL_UINT8(0, r.value);
  TEST_ASSERT_EQUAL_INT16(-1, r.encoder_delta);

  r = input_map_key(SDLK_K, true);
  TEST_ASSERT_EQUAL_UINT8(7, r.value);
  TEST_ASSERT_EQUAL_INT16(-1, r.encoder_delta);
}

TEST(Input, ExtraButtons)
{
  TEST_ASSERT_EQUAL_UINT8(0x61, input_map_key(SDLK_Z, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x62, input_map_key(SDLK_X, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x63, input_map_key(SDLK_C, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x64, input_map_key(SDLK_V, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x65, input_map_key(SDLK_B, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x66, input_map_key(SDLK_N, false).value);
  TEST_ASSERT_EQUAL_UINT8(0x67, input_map_key(SDLK_M, false).value);
}

TEST(Input, TransportControls)
{
  auto r = input_map_key(SDLK_ESCAPE, false);
  TEST_ASSERT_EQUAL_INT((int)InputAction::TRANSPORT, (int)r.action);
  TEST_ASSERT_EQUAL_UINT8(0xFC, r.value);

  r = input_map_key(SDLK_TAB, false);
  TEST_ASSERT_EQUAL_INT((int)InputAction::TRANSPORT, (int)r.action);
  TEST_ASSERT_EQUAL_UINT8(0xFB, r.value);
}

TEST(Input, CommaKey)
{
  auto r = input_map_key(SDLK_COMMA, false);
  TEST_ASSERT_EQUAL_INT((int)InputAction::KEY, (int)r.action);
  TEST_ASSERT_EQUAL_UINT8(0x68, r.value);

  r = input_map_key(SDLK_COMMA, true);
  TEST_ASSERT_EQUAL_UINT8(0x08, r.value);
}

TEST(Input, PeriodKey)
{
  auto r = input_map_key(SDLK_PERIOD, false);
  TEST_ASSERT_EQUAL_INT((int)InputAction::KEY, (int)r.action);
  TEST_ASSERT_EQUAL_UINT8(0, r.value);

  r = input_map_key(SDLK_PERIOD, true);
  TEST_ASSERT_EQUAL_UINT8(0x08, r.value);
}

TEST(Input, UnknownKey)
{
  auto r = input_map_key(SDLK_UNKNOWN, false);
  TEST_ASSERT_EQUAL_INT((int)InputAction::KEY, (int)r.action);
  TEST_ASSERT_EQUAL_UINT8(0, r.value);
}
