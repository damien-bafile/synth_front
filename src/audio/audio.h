/// @file audio.h
/// @brief SDL3 audio passthrough from the Teensy USB audio device to the default
///        playback device.
///
/// This is a plain C interface for consistent linkage.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Open the Teensy recording device and start streaming to the default playback
/// device.
/// @return 0 on success, -1 on failure (errors are printed to stderr).
int audio_init(void);

/// Stop audio streaming and close all audio devices.
void audio_shutdown(void);

#ifdef __cplusplus
}
#endif
