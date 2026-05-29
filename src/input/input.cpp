#include "input.h"

static InputResult make_key(uint8_t kc)   { return {InputAction::KEY, kc, 0}; }
static InputResult make_transport(uint8_t t) { return {InputAction::TRANSPORT, t, 0}; }
static InputResult make_note(uint8_t n)   { return {InputAction::NOTE, n, 0}; }
static InputResult make_encoder(uint8_t idx, int16_t delta) {
    return {InputAction::ENCODER, idx, delta};
}

static InputResult empty() { return {InputAction::KEY, 0, 0}; }

InputResult input_map_key(SDL_Keycode key, bool shift) {
    switch (key) {
        case SDLK_UP:       return make_key(0x01);
        case SDLK_DOWN:     return make_key(0x02);
        case SDLK_LEFT:     return make_key(0x03);
        case SDLK_RIGHT:    return make_key(0x04);
        case SDLK_RETURN:   return make_key(0x10);

        case SDLK_F6:
        case SDLK_MINUS:    return make_transport(0xFA);
        case SDLK_F7:
        case SDLK_ESCAPE:   return make_transport(0xFC);
        case SDLK_F8:
        case SDLK_TAB:      return make_transport(0xFB);

        case SDLK_Q: return make_note(60);
        case SDLK_W: return make_note(62);
        case SDLK_E: return make_note(64);
        case SDLK_R: return make_note(65);
        case SDLK_T: return make_note(67);
        case SDLK_Y: return make_note(69);
        case SDLK_U: return make_note(71);
        case SDLK_I: return make_note(72);

        case SDLK_1: return make_encoder(0, shift ? -1 : 1);
        case SDLK_2: return make_encoder(1, shift ? -1 : 1);
        case SDLK_3: return make_encoder(2, shift ? -1 : 1);
        case SDLK_4: return make_encoder(3, shift ? -1 : 1);
        case SDLK_5: return make_encoder(4, shift ? -1 : 1);
        case SDLK_6: return make_encoder(5, shift ? -1 : 1);
        case SDLK_7: return make_encoder(6, shift ? -1 : 1);
        case SDLK_8: return make_encoder(7, shift ? -1 : 1);

        default: return empty();
    }
}
