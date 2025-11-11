/**
 * @file audio.h
 * @author Toesoe
 * @brief seaboy audio output
 * @version 0.1
 * @date 2025-06-26
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <SDL2/SDL_audio.h>

#define SDL_SAMPLE_RATE 44100
#define SDL_SAMPLE_COUNT 4096

void initAudio();

SDL_AudioDeviceID getAudioDevice(void);

#endif //!_AUDIO_H_