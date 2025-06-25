/**
 * @file apu.c
 * @author Toesoe
 * @brief seaboy audio emulation
 * @version 0.1
 * @date 2025-06-25
 * 
 * @copyright Copyright (c) 2025
 *
 * @brief DMG has 4 sound channels: 2 squares, a wave channel and a noise generator
 *        sqwaves are rudimentary waveform generators clocked by frequency timers
 *        sounds like PWM? well it is. NRx3 is period low, NRx4 is period high.
 *
 * @note  https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware:
 *        If a timer's rate is given as a frequency, its period is 4194304/frequency in Hz
 *        Each timer has an internal counter that is decremented on each input clock
 *        When the counter becomes zero, it is reloaded with the period and an output clock is generated.
 * 
 */

#include "apu.h"
#include "mem.h"

#include <stdint.h>
#include <stdlib.h>

#define PERIOD 4194304

typedef bool WaveForm_t[8];

static const WaveForm_t sc_dutyWaveforms[] =
{
    { 0, 0, 0, 0, 0, 0, 0, 1},
    { 1, 0, 0, 0, 0, 0, 0, 1},
    { 1, 0, 0, 0, 0, 1, 1, 1},
    { 0, 1, 1, 1, 1, 1, 1, 0}
};

static bus_t *g_pBus = nullptr;

static size_t g_currentPtr_Square1;
static size_t g_currentPtr_Square2;

void initAPU(bus_t *pBus)
{
    g_pBus = pBus;
}

/**
 * @brief the frame sequencer runs at 512Hz, or every ~8192 CPU cycles (4194304/512=8192)
 * 
 */
void cycleFrameSequencer()
{

}

void clockFrequencyRegisters(size_t mCycles)
{
    uint16_t ch1Period = (g_pBus->map.ioregs.audio.ch1PeriodHighControl.period << 11) & g_pBus->map.ioregs.audio.ch1PeriodLow;
    uint16_t ch2Period = (g_pBus->map.ioregs.audio.ch2PeriodHighControl.period << 11) & g_pBus->map.ioregs.audio.ch2PeriodLow;

    if (ch1Period + )
}

void handleSquareChannel()
{
    // the waveDuty field in NRx1 is 2 bits, and can contain 4 duty cycle values corresponding to specific waveforms:

    // Duty   Waveform    Ratio
    // -------------------------
    // 0      00000001    12.5%
    // 1      10000001    25%
    // 2      10000111    50%
    // 3      01111110    75%
}