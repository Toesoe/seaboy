/**
 * @file audio.c
 * @author Toesoe
 * @brief seaboy sound driver
 * @version 0.1
 * @date 2023-06-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <SDL2/SDL_audio.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "SDL2/SDL.h"

#include "audio.h"

#include "../hw/apu.h"

static SDL_AudioDeviceID outputDev;

void initAudio()
{
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        // Error handling
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_AudioSpec desired, obtained;
    desired.freq     = 44100;
    desired.format   = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples  = 2048;
    desired.callback = generateDownmixCallback;
    desired.userdata = nullptr;

    outputDev = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);

    if (outputDev == 0)
    {
        //error
        fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
    }

    SDL_PauseAudioDevice(outputDev, 0);
}