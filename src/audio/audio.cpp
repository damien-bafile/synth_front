/// @file audio.cpp
/// @brief SDL3 audio passthrough implementation.
///
/// Opens the Teensy as a recording device, opens the default playback device,
/// and copies captured audio chunks from the recording stream callback into the
/// playback stream.

#include "audio.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static constexpr int AUDIO_BUF_SIZE = 4096;

static SDL_AudioStream* g_capture = nullptr;
static SDL_AudioStream* g_playback = nullptr;

// Convert SDL audio format enum to a short human-readable name for logging.
static const char* sample_format_name(SDL_AudioFormat fmt) {
  switch (fmt) {
  case SDL_AUDIO_S8:
    return "S8";
  case SDL_AUDIO_S16LE:
    return "S16LE";
  case SDL_AUDIO_S32LE:
    return "S32LE";
  case SDL_AUDIO_F32LE:
    return "F32LE";
  default:
    return "?";
  }
}

static SDL_AudioDeviceID find_playback_device(const char* hint) {
  int count = 0;
  SDL_AudioDeviceID* devices = SDL_GetAudioPlaybackDevices(&count);
  if (!devices)
    return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;

  if (hint) {
    for (int i = 0; i < count; i++) {
      const char* name = SDL_GetAudioDeviceName(devices[i]);
      if (name && strstr(name, hint)) {
        fprintf(stderr, "Matched playback device: %s\n", name);
        SDL_free(devices);
        return devices[i];
      }
    }
    fprintf(stderr, "No playback device matching \"%s\" found. Available playback devices:\n", hint);
    for (int i = 0; i < count; i++)
      fprintf(stderr, "  %s\n", SDL_GetAudioDeviceName(devices[i]));
  }

  SDL_free(devices);
  return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
}

static SDL_AudioDeviceID find_teensy_audio_device(void) {
  int count = 0;
  SDL_AudioDeviceID* devices = SDL_GetAudioRecordingDevices(&count);
  if (!devices)
    return 0;

  // Prefer a recording device whose name contains "Teensy".
  SDL_AudioDeviceID found = 0;
  for (int i = 0; i < count; i++) {
    const char* name = SDL_GetAudioDeviceName(devices[i]);
    if (name && strstr(name, "Teensy")) {
      fprintf(stderr, "Found Teensy audio: %s\n", name);
      found = devices[i];
      break;
    }
  }
  if (!found) {
    fprintf(stderr, "No Teensy audio device found. Available recording devices:\n");
    for (int i = 0; i < count; i++) {
      fprintf(stderr, "  %s\n", SDL_GetAudioDeviceName(devices[i]));
    }
  }

  SDL_free(devices);
  return found;
}

// SDL audio stream callback: copy captured audio from the Teensy recording
// stream into the default playback stream. Runs on SDL's audio thread.
static void SDLCALL capture_callback(void* userdata, SDL_AudioStream* stream, int additional_amount,
                                     [[maybe_unused]] int total_amount) {
  SDL_AudioStream* playback = static_cast<SDL_AudioStream*>(userdata);
  if (additional_amount <= 0)
    return;

  uint8_t buf[AUDIO_BUF_SIZE];
  int remaining = additional_amount;
  while (remaining > 0) {
    int chunk = remaining;
    if (chunk > static_cast<int>(sizeof(buf)))
      chunk = sizeof(buf);
    int got = SDL_GetAudioStreamData(stream, buf, chunk);
    if (got <= 0)
      break;
    SDL_PutAudioStreamData(playback, buf, got);
    remaining -= got;
  }
}

int audio_init(const char* playback_device_hint) {
  SDL_AudioDeviceID teensy_dev = find_teensy_audio_device();
  if (!teensy_dev)
    return -1;

  SDL_AudioSpec teensy_spec;
  SDL_zero(teensy_spec);
  if (!SDL_GetAudioDeviceFormat(teensy_dev, &teensy_spec, nullptr)) {
    fprintf(stderr, "Failed to get Teensy audio format: %s\n", SDL_GetError());
    return -1;
  }

  fprintf(stderr, "Teensy audio format: %d Hz, %d ch, %s\n", teensy_spec.freq,
          teensy_spec.channels, sample_format_name(teensy_spec.format));

  // Open the playback device (chosen by hint or default) with the same spec as
  // the Teensy so SDL does not need to resample.
  SDL_AudioDeviceID pb_dev = find_playback_device(playback_device_hint);
  g_playback = SDL_OpenAudioDeviceStream(pb_dev, &teensy_spec, nullptr, nullptr);
  if (!g_playback) {
    fprintf(stderr, "Failed to open playback: %s\n", SDL_GetError());
    return -1;
  }

  fprintf(stderr, "Playback device: %s\n",
          pb_dev == SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK
              ? "(default)"
              : SDL_GetAudioDeviceName(pb_dev));

  SDL_AudioSpec pb_spec;
  SDL_zero(pb_spec);
  SDL_GetAudioStreamFormat(g_playback, &pb_spec, nullptr);
  fprintf(stderr, "Playback stream format:  %d Hz, %d ch, %s\n", pb_spec.freq,
          pb_spec.channels, sample_format_name(pb_spec.format));

  // Open the Teensy recording stream; its callback will forward data to playback.
  g_capture = SDL_OpenAudioDeviceStream(teensy_dev, &teensy_spec, capture_callback, g_playback);
  if (!g_capture) {
    fprintf(stderr, "Failed to open capture: %s\n", SDL_GetError());
    SDL_DestroyAudioStream(g_playback);
    g_playback = nullptr;
    return -1;
  }

  // Start both streams once they are successfully opened.
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
