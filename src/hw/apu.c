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
#include <stddef.h>
#include <stdint.h>
#include <string.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define WAVEFORM_GENERATOR_TIMER 4194304
#define MAX_WAVEFORM_INDEX       7

#define PULSE_TIMER_PERIOD(freq) (((2048 - (freq))))

//=====================================================================================================================
// Types
//=====================================================================================================================

typedef struct
{
    EAudioChannelType_t         type;
    size_t                      num;
    fnTick                      tick;
    uint8_t                     volume;
    uint32_t                    waveformIndex;
    int32_t                     currentEnvelopePeriod;
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
    size_t                            unused;
    fnTick                            tick;
    uint8_t                           volume;
    uint32_t                          wavetableAddr;
    int32_t                           currentEnvelopePeriod;
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
    size_t                                     unused;
    fnTick                                     tick;
    uint8_t                                    volume;
    uint32_t                                   param0; // unused
    int32_t                                    currentEnvelopePeriod;
    int32_t                                    param2; // unused
    int32_t                                    currentLengthCounter;

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

#define SDL_SAMPLE_RATE 44100
#define APU_CLOCK 1048576

#define SAMPLE_BUFFER_SIZE 2048

static uint16_t sampleBuffer[SAMPLE_BUFFER_SIZE];
static volatile size_t sampleWriteIndex = 0;
static volatile size_t sampleReadIndex  = 0;

SPulseAudioChannel_t *pCH1 = (SPulseAudioChannel_t *)&g_APU.ch1Pulse;
SPulseAudioChannel_t *pCH2 = (SPulseAudioChannel_t *)&g_APU.ch1Pulse;
SWaveAudioChannel_t  *pCH3 = (SWaveAudioChannel_t *)&g_APU.ch1Pulse;
SNoiseAudioChannel_t *pCH4 = (SNoiseAudioChannel_t *)&g_APU.ch1Pulse;

//=====================================================================================================================
// Function prototypes
//=====================================================================================================================

void tickAudioChannel(SAudioChannel_t *, size_t);
static inline uint16_t periodToFrequency(const SAudioChannel_t *);

//=====================================================================================================================
// Functions
//=====================================================================================================================

void apuInit(bus_t *pBus)
{
    g_pBus = pBus;

    // channel 1
    g_APU.ch1Pulse.type  = PULSE;
    g_APU.ch1Pulse.num   = 1;
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
    g_APU.ch2Pulse.num   = 2;
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

void clockLengthCounters(void)
{
    if (pCH1->pNRx4->lengthEnable && pCH1->currentLengthCounter > 0)
    {
        pCH1->currentLengthCounter--;
        if (pCH1->currentLengthCounter == 0) { g_APU.audioControl.pNR52->ch1Enable = false; }
    }

    if (pCH2->pNRx4->lengthEnable && pCH2->currentLengthCounter > 0)
    {
        pCH2->currentLengthCounter--;
        if (pCH2->currentLengthCounter == 0) { g_APU.audioControl.pNR52->ch2Enable = false; }
    }

    if (pCH3->pNRx4->lengthEnable && pCH3->currentLengthCounter > 0)
    {
        pCH3->currentLengthCounter--;
        if (pCH3->currentLengthCounter == 0) { g_APU.audioControl.pNR52->ch3Enable = false; }
    }

    if (pCH4->pNRx4->lengthEnable && pCH4->currentLengthCounter > 0)
    {
        pCH4->currentLengthCounter--;
        if (pCH4->currentLengthCounter == 0) { g_APU.audioControl.pNR52->ch4Enable = false; }
    }
}

void clockVolumeEnvelopes(void)
{
    if (pCH1->currentEnvelopePeriod > 0)
    {
        pCH1->currentEnvelopePeriod--;
        if ((pCH1->pNRx2->sweepPeriod != 0) && (pCH1->currentEnvelopePeriod == 0))
        {
            pCH1->currentEnvelopePeriod = pCH1->pNRx2->sweepPeriod;

            if (((pCH1->volume < 0xF) && (pCH1->pNRx2->envDir == 1)) ||
                ((pCH1->volume > 0x0) && (pCH1->pNRx2->envDir == 0)))
            {
                if (pCH1->pNRx2->envDir == 1) { pCH1->volume++; }
                else { pCH1->volume--; }
            }
        }
    }
    if (pCH2->currentEnvelopePeriod > 0)
    {
        pCH2->currentEnvelopePeriod--;
        if ((pCH2->pNRx2->sweepPeriod != 0) && (pCH2->currentEnvelopePeriod == 0))
        {
            pCH2->currentEnvelopePeriod = pCH2->pNRx2->sweepPeriod;

            if (((pCH2->volume < 0xF) && (pCH2->pNRx2->envDir == 1)) ||
                ((pCH2->volume > 0x0) && (pCH2->pNRx2->envDir == 0)))
            {
                if (pCH2->pNRx2->envDir == 1) { pCH2->volume++; }
                else { pCH2->volume--; }
            }
        }
    }
    if (pCH4->currentEnvelopePeriod > 0)
    {
        pCH4->currentEnvelopePeriod--;
        if ((pCH4->pNRx2->sweepPeriod != 0) && (pCH4->currentEnvelopePeriod == 0))
        {
            pCH4->currentEnvelopePeriod = pCH4->pNRx2->sweepPeriod;

            if (((pCH4->volume < 0xF) && (pCH4->pNRx2->envDir == 1)) ||
                ((pCH4->volume > 0x0) && (pCH4->pNRx2->envDir == 0)))
            {
                if (pCH4->pNRx2->envDir == 1) { pCH4->volume++; }
                else { pCH4->volume--; }
            }
        }
    }
}

void clockFrequencySweeps(void)
{

}

/**
 * @brief the frame sequencer runs at 512Hz, or every ~8192 CPU cycles (4194304/512=8192)
 *
 */
void cycleFrameSequencer()
{
    switch (g_APU.frameSequencerStep)
    {
        case 0:
        case 2:
        case 4:
        case 6:
            clockLengthCounters();
            break;

        case 7:
            // Volume envelope (only once per full frame)
            clockVolumeEnvelopes();
            break;
    }

    if (g_APU.frameSequencerStep == 2 || g_APU.frameSequencerStep == 6)
    {
        // Sweep happens on steps 2 and 6
        clockFrequencySweeps();
    }

    g_APU.frameSequencerStep = (g_APU.frameSequencerStep + 1) & 0x07; // wrap 0-7
}

void tickAudioChannel(SAudioChannel_t *channelCtx, size_t cycles)
{
    if (channelCtx->type == PULSE)
    {
        SPulseAudioChannel_t *pulse = (SPulseAudioChannel_t *)channelCtx;

        pulse->currentPeriod -= cycles;

        while (pulse->currentPeriod <= 0)
        {
            pulse->currentPeriod += PULSE_TIMER_PERIOD(periodToFrequency(channelCtx));
            pulse->waveformIndex = (pulse->waveformIndex + 1) & 0x07;
        }

        if (pulse->pNRx4->trigger)
        {
            if (pulse->num == 1)
            {
                if (g_APU.audioControl.pNR52->ch1Enable == 0)
                {
                    g_APU.audioControl.pNR52->ch1Enable = 1;
                }

                pCH1->currentEnvelopePeriod = pCH1->pNRx2->sweepPeriod;
                pCH1->volume                = pCH1->pNRx2->initialVolume;
            }
            else if (pulse->num == 2)
            {
                if (g_APU.audioControl.pNR52->ch2Enable == 0)
                {
                    g_APU.audioControl.pNR52->ch2Enable = 1;
                }
                pCH2->currentEnvelopePeriod = pCH2->pNRx2->sweepPeriod;
                pCH2->volume                = pCH2->pNRx2->initialVolume;
            }

            if (pulse->currentLengthCounter == 0) { pulse->currentLengthCounter = 64; }

        }
    }
    else if (channelCtx->type == WAVE)
    {
        SWaveAudioChannel_t *wave = (SWaveAudioChannel_t *)channelCtx;
     
        wave->currentPeriod -= cycles;

        // while (wave->currentPeriod <= 0)
        // {
        //     wave->currentPeriod += PULSE_TIMER_PERIOD(periodToFrequency(pulse));

        //     wave->waveformIndex = (wave->waveformIndex + 1) & 0x07;
        // }

        // pulse->sample = sc_dutyWaveforms[pulse->pNRx1->waveDuty][pulse->waveformIndex] ? 1 : 0;

        if (wave->pNRx4->trigger)
        {
            if (g_APU.audioControl.pNR52->ch3Enable == 0) {g_APU.audioControl.pNR52->ch3Enable = 1; }
            if (wave->currentLengthCounter == 0) { wave->currentLengthCounter = 256; }
        }
    }
    else if (channelCtx->type == NOISE)
    {
        SNoiseAudioChannel_t *noise = (SNoiseAudioChannel_t *)channelCtx;

        if (noise->pNRx4->trigger)
        {
            if (g_APU.audioControl.pNR52->ch4Enable == 0) {g_APU.audioControl.pNR52->ch4Enable = 1; }
            if (noise->currentLengthCounter == 0) { noise->currentLengthCounter = 64; }
        }
    }
}

void generateDownmixCallback(void *userdata, uint8_t *pStream, int len)
{
    uint16_t *out     = (uint16_t *)pStream;
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

static inline uint16_t periodToFrequency(const SAudioChannel_t *pChannel)
{
    // use the lower 3 bits from NRx4 as MSB's or'ed with NRx3
    return (uint16_t)((*(uint8_t *)(pChannel->pNRx4) & 0x7) << 8) | *(uint8_t *)pChannel->pNRx3;
}

void updateAPUSampleBuffer(size_t cycles)
{
    static double sampleCounter = 0.0;
    const double cyclesPerSample = (double)APU_CLOCK / (double)SDL_SAMPLE_RATE;

    sampleCounter += (double)cycles;

    while (sampleCounter >= cyclesPerSample)
    {
        sampleCounter -= cyclesPerSample;

        uint16_t mix = 0;

        if (g_APU.audioControl.pNR52->audioMasterEnable)
        {
            if (g_APU.audioControl.pNR52->ch1Enable) mix += sc_dutyWaveforms[(*(SAudio_LengthDutyCycle_t *)g_APU.ch1Pulse.pNRx1).waveDuty][g_APU.ch1Pulse.data0] ? (pCH1->volume * 200) : 0;
            if (g_APU.audioControl.pNR52->ch2Enable) mix += sc_dutyWaveforms[(*(SAudio_LengthDutyCycle_t *)g_APU.ch2Pulse.pNRx1).waveDuty][g_APU.ch2Pulse.data0] ? (pCH2->volume * 200) : 0;
            // if (g_APU.audioControl.pNR52->ch3Enable) mix += g_APU.ch3Wave.sample  ? 3000 : -3000;
            // if (g_APU.audioControl.pNR52->ch4Enable) mix += g_APU.ch4Noise.sample ? 3000 : -3000;
        }

        if ((sampleWriteIndex - sampleReadIndex) < SAMPLE_BUFFER_SIZE)
        {
            sampleBuffer[sampleWriteIndex & (SAMPLE_BUFFER_SIZE - 1)] = mix;
            sampleWriteIndex++;
        }
    }
}