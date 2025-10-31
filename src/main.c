/**
 * @file main.c
 * @author Toesoe
 * @brief seaboy's main entry point
 * @version 0.1
 * @date 2023-06-15
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#define CYCLES_PER_FRAME 70224
#define FRAME_DURATION_NS 16666667L  // ~16.67ms in nanoseconds


#include "hw/mem.h"
#include "hw/cpu.h"
#include "hw/ppu.h"
#include "hw/apu.h"
#include "hw/cart.h"

#include "drv/render.h"
#include "drv/audio.h"

#include "cputest.h"

#include <string.h>
#include <stdio.h>

#include <SDL2/SDL.h>

#define TARGET_FPS (59.7275)
#define FRAME_DURATION_MS (1000.0 / TARGET_FPS)

static bus_t *pBus            = nullptr;
static const cpu_t *pCpu            = nullptr;
static APU_t *pApu            = nullptr;

static void tickAudioChannels(size_t);

int main()
{
    // TODO:
    // handle OAM DMA cycle timing (160), currently not accurate

    // runTests();

    bool         skipBootrom               = true;
    bool         previousInstructionSetIME = false;

    initializeBus(pLoadRom("Tetris.gb"), skipBootrom);

    pBus = pGetBusPtr();
    pCpu = getCpuObject();
    pApu = getAPUObject();

    ppuInit(skipBootrom);
    apuInit(pBus);

    resetCpu(skipBootrom);

    initRenderWindow();
    initAudio();

    // frame timing sync
    uint64_t lastFrameTime = SDL_GetPerformanceCounter();
    double freq = (double)SDL_GetPerformanceFrequency();

    while (true)
    {
        int     mCycles = 0;
        uint8_t opcode  = fetch8(pCpu->reg16.pc);

        //printf("executing 0x%02x at pc 0x%02x\n", opcode, pCpu->reg16.pc);

        // this is done to delay executing interrupts by one cycle: EI sets IME after delay
        if (pBus->bus[pCpu->reg16.pc] == 0xFB)
        {
            previousInstructionSetIME = true;
        }

        if (!checkHalted())
        {
            mCycles += executeInstruction(opcode);
        }

        if (!previousInstructionSetIME)
        {
            mCycles += handleInterrupts();
        }

        previousInstructionSetIME = false;

        handleTimers(mCycles);
        bool frameComplete = ppuLoop(mCycles * 4); // 1 CPU cycle = 4 PPU cycles
        tickAudioChannels(mCycles * 4);

        if (frameComplete)
        {
            frameComplete = false;

            uint64_t now = SDL_GetPerformanceCounter();
            double elapsedMS = (now - lastFrameTime) * 1000.0 / freq;

            // use 59.73fps frame sync
            if (elapsedMS < FRAME_DURATION_MS)
            {
                SDL_Delay((Uint32)(FRAME_DURATION_MS - elapsedMS));
            }

            lastFrameTime = SDL_GetPerformanceCounter(); // Reset for next frame
        }
    }
}

static void tickAudioChannels(size_t cycles)
{
    static size_t totalCycles = 0;

    totalCycles += cycles;

    pApu->ch1Pulse.tick(&pApu->ch1Pulse, cycles);
    pApu->ch2Pulse.tick(&pApu->ch2Pulse, cycles);
    pApu->ch3Wave.tick(&pApu->ch3Wave, cycles);
    pApu->ch4Noise.tick(&pApu->ch4Noise, cycles);

    updateAPUSampleBuffer(cycles);

    if (totalCycles >= 8192)
    {
        cycleFrameSequencer();
        totalCycles -= 8192;
    }
}

static void handleInput()
{

}