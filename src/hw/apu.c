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
 *
 * @note  https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware:
 *        If a timer's rate is given as a frequency, its period is 4194304/frequency in Hz
 *        Each timer has an internal counter that is decremented on each input clock
 *        When the counter becomes zero, it is reloaded with the period and an output clock is generated.
 *
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "apu.h"
#include "mem.h"
#include <stdint.h>
#include <string.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define WAVEFORM_GENERATOR_TIMER 4194304
#define MAX_WAVEFORM_INDEX       7

#define PULSE_TIMER_PERIOD(freq) (4 * (2048 - (freq)))

//=====================================================================================================================
// Types
//=====================================================================================================================

typedef struct
{
    EAudioChannelType_t         type;
    fnTick                      tick;
    uint8_t                     sample;
    uint32_t                    waveformIndex;
    int32_t                     currentPeriod;
    int32_t                     currentLengthCounter;

    SAudio_ChannelSweep_t      *pNRx0; // unused for channel 2
    SAudio_LengthDutyCycle_t   *pNRx1;
    SAudio_VolumeEnvelope_t    *pNRx2;
    uint8_t                    *pNRx3;
    SAudio_PeriodHighControl_t *pNRx4;
} SPulseAudioChannel_t;

typedef struct
{
    EAudioChannelType_t               type;
    fnTick                            tick;
    uint8_t                           sample;
    uint32_t                          wavetableAddr;
    int32_t                           currentPeriod;
    int32_t                           currentLengthCounter;

    SAudio_WaveChannel_DACEnable_t   *pNRx0;
    uint8_t                          *pNRx1;
    SAudio_WaveChannel_OutputLevel_t *pNRx2;
    uint8_t                          *pNRx3;
    SAudio_PeriodHighControl_t       *pNRx4;
} SWaveAudioChannel_t;

typedef struct
{
    EAudioChannelType_t                        type;
    fnTick                                     tick;
    uint8_t                                    sample;
    uint32_t                                   param0; // unused
    int32_t                                    param1; // unused
    int32_t                                    param2; // unused

    void                                      *pNRx0; // unused
    SAudio_NoiseChannel_LengthTimer_t         *pNRx1;
    SAudio_VolumeEnvelope_t                   *pNRx2;
    SAudio_NoiseChannel_FrequencyRandomness_t *pNRx3;
    SAudio_NoiseChannel_Control_t             *pNRx4;
} SNoiseAudioChannel_t;

typedef bool Waveform_t[8];

//=====================================================================================================================
// Globals
//=====================================================================================================================

static const Waveform_t sc_dutyWaveforms[] = {
    { 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 1, 1, 1 },
    { 0, 1, 1, 1, 1, 1, 1, 0 }
};

static bus_t *g_pBus = nullptr;

static APU_t  g_APU  = { 0 };

static bool hasTicked = false;

#define APU_SAMPLE_RATE 44100
#define APU_CLOCK 1048576

#define SAMPLE_BUFFER_SIZE 2048

static int16_t sampleBuffer[SAMPLE_BUFFER_SIZE];
static volatile size_t sampleWriteIndex = 0;
static volatile size_t sampleReadIndex  = 0;

//=====================================================================================================================
// Function prototypes
//=====================================================================================================================

void tickAudioChannel(SAudioChannel_t *, size_t);
static inline uint16_t getPulseFrequency(const SPulseAudioChannel_t *);
static void updateAPUSampleBuffer(void);

//=====================================================================================================================
// Functions
//=====================================================================================================================

void apuInit(bus_t *pBus)
{
    g_pBus = pBus;

    // channel 1
    g_APU.ch1Pulse.type  = PULSE;
    g_APU.ch1Pulse.pNRx0 = &pBus->map.ioregs.audio.ch1Sweep;
    g_APU.ch1Pulse.pNRx1 = &pBus->map.ioregs.audio.ch1LengthDuty;
    g_APU.ch1Pulse.pNRx2 = &pBus->map.ioregs.audio.ch1VolEnvelope;
    g_APU.ch1Pulse.pNRx3 = &pBus->map.ioregs.audio.ch1FreqLSB;
    g_APU.ch1Pulse.pNRx4 = &pBus->map.ioregs.audio.ch1FreqMSBControl;

    g_APU.ch1Pulse.tick  = tickAudioChannel;
    g_APU.ch1Pulse.data1 = 4 * 2048;                                                // period
    g_APU.ch1Pulse.data2 = 64 - pBus->map.ioregs.audio.ch1LengthDuty.initialLength; // length

    // channel 2
    g_APU.ch2Pulse.type  = PULSE;
    g_APU.ch2Pulse.pNRx0 = nullptr; // does not exist for channel 2
    g_APU.ch2Pulse.pNRx1 = &pBus->map.ioregs.audio.ch1LengthDuty;
    g_APU.ch2Pulse.pNRx2 = &pBus->map.ioregs.audio.ch1VolEnvelope;
    g_APU.ch2Pulse.pNRx3 = &pBus->map.ioregs.audio.ch2FreqLSB;
    g_APU.ch2Pulse.pNRx4 = &pBus->map.ioregs.audio.ch2FreqMSBControl;

    g_APU.ch2Pulse.tick  = tickAudioChannel;
    g_APU.ch2Pulse.data1 = 4 * 2048;                                                // period
    g_APU.ch2Pulse.data2 = 64 - pBus->map.ioregs.audio.ch2LengthDuty.initialLength; // length

    // channel 3
    g_APU.ch3Wave.type  = WAVE;
    g_APU.ch3Wave.pNRx0 = &pBus->map.ioregs.audio.ch3DACEnable;
    g_APU.ch3Wave.pNRx1 = &pBus->map.ioregs.audio.ch3LengthTimer;
    g_APU.ch3Wave.pNRx2 = &pBus->map.ioregs.audio.ch3OutputLevel;
    g_APU.ch3Wave.pNRx3 = &pBus->map.ioregs.audio.ch3FreqLSB;
    g_APU.ch3Wave.pNRx4 = &pBus->map.ioregs.audio.ch3FreqMSBControl;

    g_APU.ch3Wave.tick  = tickAudioChannel;
    g_APU.ch3Wave.data1 = 4 * 2048;                                   // period
    g_APU.ch3Wave.data2 = 64 - pBus->map.ioregs.audio.ch3LengthTimer; // length

    // channel 4
    g_APU.ch4Noise.type  = NOISE;
    g_APU.ch4Noise.pNRx0 = nullptr; // does not exist for channel 4
    g_APU.ch4Noise.pNRx1 = &pBus->map.ioregs.audio.ch4LengthTimer;
    g_APU.ch4Noise.pNRx2 = &pBus->map.ioregs.audio.ch1VolEnvelope;
    g_APU.ch4Noise.pNRx3 = &pBus->map.ioregs.audio.ch4FreqRandom;
    g_APU.ch4Noise.pNRx4 = &pBus->map.ioregs.audio.ch4Control;

    g_APU.ch4Noise.tick  = tickAudioChannel;
    g_APU.ch4Noise.data1 = UINT32_MAX;                                         // unused
    g_APU.ch4Noise.data2 = 64 - pBus->map.ioregs.audio.ch4LengthTimer.initial; // length

    // control
    g_APU.audioControl.pNR50 = &pBus->map.ioregs.audio.masterVolVINControl;
    g_APU.audioControl.pNR51 = &pBus->map.ioregs.audio.channelPanning;
    g_APU.audioControl.pNR52 = &pBus->map.ioregs.audio.masterControl;
}

APU_t *getAPUObject(void)
{
    return &g_APU;
}

/**
 * @brief the frame sequencer runs at 512Hz, or every ~8192 CPU cycles (4194304/512=8192)
 *
 */
void cycleFrameSequencer() {}

void tickAudioChannel(SAudioChannel_t *channelCtx, size_t mCycles)
{
    if (channelCtx->type == PULSE)
    {
        SPulseAudioChannel_t *pulse = (SPulseAudioChannel_t *)channelCtx;

        pulse->currentPeriod -= mCycles;

        while (pulse->currentPeriod <= 0)
        {
            pulse->currentPeriod += PULSE_TIMER_PERIOD(getPulseFrequency(pulse));

            pulse->waveformIndex = (pulse->waveformIndex + 1) & 0x07;
        }

        pulse->sample = sc_dutyWaveforms[pulse->pNRx1->waveDuty][pulse->waveformIndex] ? 1 : 0;
    }
    // else if (channelCtx->type == WAVE)
    // {
    //     SWaveAudioChannel_t *wave = (SWaveAudioChannel_t *)channelCtx;
    //     uint16_t period = ((wave->pNRx4->freqMSB & 0x7) << 11) | *wave->pNRx3;
    // }

    updateAPUSampleBuffer();
}

void generateDownmixCallback(void *userdata, uint8_t *pStream, int len)
{
    int16_t *out     = (int16_t *)pStream;
    size_t   samples = len / sizeof(int16_t);

    for (size_t i = 0; i < samples; i++)
    {
        if (sampleReadIndex != sampleWriteIndex)
        {
            out[i] = sampleBuffer[sampleReadIndex & (SAMPLE_BUFFER_SIZE - 1)];
            sampleReadIndex++;
        }
        else
        {
            out[i] = 0; // buffer underrun → output silence
        }
    }
}

static inline uint16_t getPulseFrequency(const SPulseAudioChannel_t *pulse)
{
    return ((pulse->pNRx4->freqMSB & 0x7) << 8) | *pulse->pNRx3;
}

static void updateAPUSampleBuffer()
{
    static int apuSampleCounter = 0;
    int16_t mix = 0;

    apuSampleCounter += APU_SAMPLE_RATE; // e.g. 44100
    if (apuSampleCounter >= APU_CLOCK)   // e.g. 1048576
    {
        apuSampleCounter -= APU_CLOCK;

        if (g_APU.audioControl.pNR52->ch1Enable) mix += g_APU.ch1Pulse.sample ? 3000 : -3000;
        //if (g_APU.audioControl.pNR52->ch2Enable) mix += g_APU.ch2Pulse.sample ? 3000 : -3000;
        //if (g_APU.audioControl.pNR52->ch3Enable) mix += g_APU.ch3Wave.sample  ? 3000 : -3000;
        //if (g_APU.audioControl.pNR52->ch4Enable) mix += g_APU.ch4Noise.sample ? 3000 : -3000;

        if (mix > 32767) mix = 32767;
        else if (mix < -32768) mix = -32768;

        sampleBuffer[sampleWriteIndex & (SAMPLE_BUFFER_SIZE - 1)] = mix;
        sampleWriteIndex++;
    }
}