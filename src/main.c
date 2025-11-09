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

#include "hw/apu.h"
#include "hw/cart.h"
#include "hw/cpu.h"
#include "hw/mem.h"
#include "hw/ppu.h"
#include "hw/joypad.h"

#include "drv/audio.h"
#include "drv/render.h"

#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>

#define TARGET_FPS        (59.7275)
#define FRAME_DURATION_MS (1000.0 / TARGET_FPS)

static const SAddressBus_t   *pBus = nullptr;
static const SCPURegisters_t *pCpu = nullptr;

int main()
{
    // TODO:
    // handle OAM DMA cycle timing (160), currently not accurate

    //runTests();

    bool skipBootrom               = true;
    bool imePrev                   = false;
    bool frameComplete             = false;

    uint32_t cycleCounter          = 0;

    initializeBus(pLoadRom("roms/Tetris.gb"), skipBootrom);

    pBus = pGetAddressBus();
    pCpu = pGetCPURegisters();

    ppuInit(skipBootrom);
    apuInit(pBus);
    joypadInit();

    resetCpu(skipBootrom);

    initRenderWindow();
    initAudio();

    size_t mCyclesCurrentLoop = 0;

    // frame timing sync
    uint64_t lastFrameTime = SDL_GetPerformanceCounter();
    double   freq          = (double)SDL_GetPerformanceFrequency();

    uint8_t delayedIMECounter = 0;

#ifdef GB_DOCTOR
    FILE *f = fopen("gb.log", "w");
#endif

    while (true)
    {
        // printf("executing 0x%02x at pc 0x%02x\n", opcode, pCpu->reg16.pc);
#ifdef GB_DOCTOR
        fprintf(f, "A:%02x F:%02x B:%02x C:%02x D:%02x E:%02x H:%02x L:%02x SP:%04x PC:%04x PCMEM:%02x,%02x,%02x,%02x\n",
                pCpu->reg8.a, pCpu->reg8.f, pCpu->reg8.b, pCpu->reg8.c, pCpu->reg8.d, pCpu->reg8.e, pCpu->reg8.h, pCpu->reg8.l,
                pCpu->reg16.sp, pCpu->reg16.pc, fetch8(pCpu->reg16.pc), fetch8(pCpu->reg16.pc+1), fetch8(pCpu->reg16.pc+2), fetch8(pCpu->reg16.pc+3));
#endif

        // interrupt handling - fetch - decode - execute + timers
        mCyclesCurrentLoop = stepCPU();
        cycleCounter += mCyclesCurrentLoop;

        if (!checkStopped())
        {
            // clock peripherals with T-cycles
            frameComplete = ppuTick(mCyclesCurrentLoop * 4);
            apuTick(mCyclesCurrentLoop * 4);
        }

        if ((cycleCounter % (CYCLES_PER_FRAME / 4)) == 0)
        {
            joypadEventLoop();
        }

        if (frameComplete)
        {
            frameComplete = false;

            uint64_t now                                     = SDL_GetPerformanceCounter();
            double   elapsedMS                               = (now - lastFrameTime) * 1000.0 / freq;

            // use 59.73fps frame sync
            if (elapsedMS < FRAME_DURATION_MS)
            {
                SDL_Delay((Uint32)(FRAME_DURATION_MS - elapsedMS));
            }

            lastFrameTime = SDL_GetPerformanceCounter(); // Reset for next frame
        }
    }
}