#include "input.h"

static InputResult make_key(uint8_t kc) {
  return {InputAction::KEY, kc, 0};
}
static InputResult make_transport(uint8_t t) {
  return {InputAction::TRANSPORT, t, 0};
}
static InputResult make_note(uint8_t n) {
  return {InputAction::NOTE, n, 0};
}
static InputResult make_encoder(uint8_t idx, int16_t delta) {
  return {InputAction::ENCODER, idx, delta};
}

static InputResult empty() {
  return {InputAction::KEY, 0, 0};
}

InputResult input_map_key(SDL_Keycode key, bool shift) {
  switch (key) {
  case SDLK_UP:
    return make_key(0x01);
  case SDLK_DOWN:
    return make_key(0x02);
  case SDLK_LEFT:
    return make_key(0x03);
  case SDLK_RIGHT:
    return make_key(0x04);
  case SDLK_RETURN:
    return make_key(0x10);
  case SDLK_SPACE:
    return make_key(0x11);
  case SDLK_COMMA:
    if (shift) return make_key(0x08);  // < → V/H toggle
    return make_key(0x68);             // , → encoder 7 push
  case SDLK_PERIOD:
    if (shift) return make_key(0x08);  // > → V/H toggle
    return empty();                    // . → nothing
  case SDLK_F1:
    return make_key(0x70);
  case SDLK_F2:
    return make_key(0x71);
  case SDLK_F3:
    return make_key(0x72);
  case SDLK_F4:
    return make_key(0x73);
  case SDLK_F5:
    return make_key(0x74);
  case SDLK_F6:
    return make_key(0x75);
  case SDLK_F7:
    return make_key(0x76);
  case SDLK_F8:
    return make_key(0x77);

  case SDLK_1:
    return make_key(0x50);
  case SDLK_2:
    return make_key(0x51);
  case SDLK_3:
    return make_key(0x52);
  case SDLK_4:
    return make_key(0x53);
  case SDLK_5:
    return make_key(0x54);
  case SDLK_6:
    return make_key(0x55);
  case SDLK_7:
    return make_key(0x56);
  case SDLK_8:
    return make_key(0x57);
  case SDLK_9:
    return make_key(0x58);
  case SDLK_0:
    return make_key(0x59);

  case SDLK_ESCAPE:
    return make_transport(0xFC);
  case SDLK_TAB:
    return make_transport(0xFB);
  case SDLK_F9:
    return make_key(0x78);
  case SDLK_F10:
    return make_key(0x79);
  case SDLK_F11:
    return make_key(0x7A);
  case SDLK_F12:
    return make_key(0x7B);

  case SDLK_Q:
    return make_note(60);
  case SDLK_W:
    return make_note(62);
  case SDLK_E:
    return make_note(64);
  case SDLK_R:
    return make_note(65);
  case SDLK_T:
    return make_note(67);
  case SDLK_Y:
    return make_note(69);
  case SDLK_U:
    return make_note(71);
  case SDLK_I:
    return make_note(72);

  case SDLK_A:
    return make_encoder(0, shift ? -1 : 1);
  case SDLK_S:
    return make_encoder(1, shift ? -1 : 1);
  case SDLK_D:
    return make_encoder(2, shift ? -1 : 1);
  case SDLK_F:
    return make_encoder(3, shift ? -1 : 1);
  case SDLK_G:
    return make_encoder(4, shift ? -1 : 1);
  case SDLK_H:
    return make_encoder(5, shift ? -1 : 1);
  case SDLK_J:
    return make_encoder(6, shift ? -1 : 1);
  case SDLK_K:
    return make_encoder(7, shift ? -1 : 1);

  case SDLK_Z:     return make_key(0x61);
  case SDLK_X:     return make_key(0x62);
  case SDLK_C:     return make_key(0x63);
  case SDLK_V:     return make_key(0x64);
  case SDLK_B:     return make_key(0x65);
  case SDLK_N:     return make_key(0x66);
  case SDLK_M:     return make_key(0x67);

  default:
    return empty();
  }
}
