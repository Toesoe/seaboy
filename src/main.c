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

#define CYCLES_PER_FRAME  70224
#define FRAME_DURATION_NS 16666667L // ~16.67ms in nanoseconds

#include "hw/apu.h"
#include "hw/cart.h"
#include "hw/cpu.h"
#include "hw/mem.h"
#include "hw/ppu.h"
#include "hw/joypad.h"

#include "hw/instr.h"

#include "drv/audio.h"
#include "drv/render.h"

#include "cputest.h"

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
    bool previousIME               = false;
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

    // frame timing sync
    uint64_t lastFrameTime = SDL_GetPerformanceCounter();
    double   freq          = (double)SDL_GetPerformanceFrequency();

    while (true)
    {
        int     mCycles = 0;
        uint8_t opcode  = fetch8(pCpu->reg16.pc);

        // printf("executing 0x%02x at pc 0x%02x\n", opcode, pCpu->reg16.pc);

        if (!checkHalted() || !checkStopped())
        {
            //log_instruction_fetch(opcode);
            mCycles += executeInstruction(opcode);
            if (opcode == 0xFB) { previousIME = true; }
        }

        if (!checkStopped())
        {
            // clock peripherals with T-cycles
            handleTimers(mCycles * 4);
            frameComplete = ppuTick(mCycles * 4);
            apuTick(mCycles * 4);
        }

        if (!previousIME)
        {
            mCycles += handleInterrupts();
        }

        if ((cycleCounter % (CYCLES_PER_FRAME / 4)) == 0)
        {
            joypadEventLoop();
        }

        previousIME = false;

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

static void oamDmaCallback(uint8_t data, uint16_t addr)
{

}