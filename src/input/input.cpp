#include "input.h"

uint8_t input_map_key(SDL_Keycode key) {
  switch (key) {
    case SDLK_UP:       return 0x01;
    case SDLK_DOWN:     return 0x02;
    case SDLK_LEFT:     return 0x03;
    case SDLK_RIGHT:    return 0x04;
    case SDLK_RETURN:   return 0x10;
    case SDLK_SPACE:    return 0x11;
    case SDLK_ESCAPE:   return 0x12;
    case SDLK_TAB:      return 0x13;
    case SDLK_BACKSPACE:return 0x14;
    case SDLK_DELETE:   return 0x15;

    case SDLK_Q: return 0x20;
    case SDLK_W: return 0x21;
    case SDLK_E: return 0x22;
    case SDLK_R: return 0x23;
    case SDLK_T: return 0x24;
    case SDLK_Y: return 0x25;
    case SDLK_U: return 0x26;
    case SDLK_I: return 0x27;
    case SDLK_O: return 0x28;
    case SDLK_P: return 0x29;

    case SDLK_A: return 0x30;
    case SDLK_S: return 0x31;
    case SDLK_D: return 0x32;
    case SDLK_F: return 0x33;
    case SDLK_G: return 0x34;
    case SDLK_H: return 0x35;
    case SDLK_J: return 0x36;
    case SDLK_K: return 0x37;
    case SDLK_L: return 0x38;

    case SDLK_Z: return 0x40;
    case SDLK_X: return 0x41;
    case SDLK_C: return 0x42;
    case SDLK_V: return 0x43;
    case SDLK_B: return 0x44;
    case SDLK_N: return 0x45;
    case SDLK_M: return 0x46;

    case SDLK_1: return 0x50;
    case SDLK_2: return 0x51;
    case SDLK_3: return 0x52;
    case SDLK_4: return 0x53;
    case SDLK_5: return 0x54;
    case SDLK_6: return 0x55;
    case SDLK_7: return 0x56;
    case SDLK_8: return 0x57;

    case SDLK_F1:  return 0x60;
    case SDLK_F2:  return 0x61;
    case SDLK_F3:  return 0x62;
    case SDLK_F4:  return 0x63;
    case SDLK_F5:  return 0x64;

    default: return 0;
  }
}
