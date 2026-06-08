#include "audio/audio.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static SDL_AudioStream *g_capture = nullptr;
static SDL_AudioStream *g_playback = nullptr;

static const char *sample_format_name(SDL_AudioFormat fmt) {
    switch (fmt) {
        case SDL_AUDIO_S8:    return "S8";
        case SDL_AUDIO_S16LE: return "S16LE";
        case SDL_AUDIO_S32LE: return "S32LE";
        case SDL_AUDIO_F32LE: return "F32LE";
        default:              return "?";
    }
}

static SDL_AudioDeviceID find_teensy_audio_device(void) {
    int count = 0;
    SDL_AudioDeviceID *devices = SDL_GetAudioRecordingDevices(&count);
    if (!devices) return 0;

    SDL_AudioDeviceID found = 0;
    for (int i = 0; i < count; i++) {
        const char *name = SDL_GetAudioDeviceName(devices[i]);
        if (name && strstr(name, "Teensy")) {
            fprintf(stderr, "Found Teensy audio: %s\n", name);
            found = devices[i];
            break;
        }
    }
    SDL_free(devices);

    if (!found) {
        fprintf(stderr, "No Teensy audio device found. Available recording devices:\n");
        devices = SDL_GetAudioRecordingDevices(&count);
        for (int i = 0; i < count; i++) {
            fprintf(stderr, "  %s\n", SDL_GetAudioDeviceName(devices[i]));
        }
        SDL_free(devices);
    }
    return found;
}

static void SDLCALL capture_callback(void *userdata, SDL_AudioStream *stream,
                                      int additional_amount, int total_amount) {
    (void)total_amount;
    SDL_AudioStream *playback = static_cast<SDL_AudioStream *>(userdata);
    if (additional_amount <= 0) return;

    uint8_t buf[4096];
    int remaining = additional_amount;
    while (remaining > 0) {
        int chunk = remaining;
        if (chunk > (int)sizeof(buf)) chunk = sizeof(buf);
        int got = SDL_GetAudioStreamData(stream, buf, chunk);
        if (got <= 0) break;
        SDL_PutAudioStreamData(playback, buf, got);
        remaining -= got;
    }
}

int audio_init(void) {
    SDL_AudioDeviceID teensy_dev = find_teensy_audio_device();
    if (!teensy_dev) return -1;

    SDL_AudioSpec teensy_spec;
    SDL_zero(teensy_spec);
    if (!SDL_GetAudioDeviceFormat(teensy_dev, &teensy_spec, nullptr)) {
        fprintf(stderr, "Failed to get Teensy audio format: %s\n", SDL_GetError());
        return -1;
    }

    fprintf(stderr, "Teensy audio format: %d Hz, %d ch, %s\n",
            (int)teensy_spec.freq, (int)teensy_spec.channels,
            sample_format_name(teensy_spec.format));

    g_playback = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                            &teensy_spec, nullptr, nullptr);
    if (!g_playback) {
        fprintf(stderr, "Failed to open playback: %s\n", SDL_GetError());
        return -1;
    }

    SDL_AudioSpec pb_spec;
    SDL_zero(pb_spec);
    SDL_GetAudioStreamFormat(g_playback, &pb_spec, nullptr);
    fprintf(stderr, "Playback stream format:  %d Hz, %d ch, %s\n",
            (int)pb_spec.freq, (int)pb_spec.channels,
            sample_format_name(pb_spec.format));

    g_capture = SDL_OpenAudioDeviceStream(teensy_dev, &teensy_spec,
                                           capture_callback, g_playback);
    if (!g_capture) {
        fprintf(stderr, "Failed to open capture: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(g_playback);
        g_playback = nullptr;
        return -1;
    }

    SDL_ResumeAudioStreamDevice(g_playback);
    SDL_ResumeAudioStreamDevice(g_capture);

    fprintf(stderr, "Audio passthrough active: Teensy -> speakers\n");
    return 0;
}

void audio_shutdown(void) {
    if (g_capture) {
        SDL_DestroyAudioStream(g_capture);
        g_capture = nullptr;
    }
    if (g_playback) {
        SDL_DestroyAudioStream(g_playback);
        g_playback = nullptr;
    }
}
