/**
 * @file apu.c
 * @author Toesoe
 * @brief seaboy audio emulation
 * @version 0.1
 * @date 2025-06-25
 *
 * @copyright Copyright (c) 2025
 *
 * @brief DMG has 4 sound channels: 2 squares, a g_currentAPUState.ch3 channel and a noise generator
 *        sqwaves are rudimentary waveform generators clocked by frequency timers
 *
 * @note  https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware:
 *        If a timer's rate is given as a frequency, its samplePeriod is 4194304/frequency in Hz
 *        Each timer has an internal counter that is decremented on each input clock
 *        When the counter becomes zero, it is reloaded with the samplePeriod and an output clock is generated.
 *
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "apu.h"
#include "mem.h"
#include "cpu.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define WAVEFORM_GENERATOR_TIMER CPU_CLOCK_SPEED_HZ
#define MAX_WAVEFORM_INDEX       7

#define NUM_APU_CHANNELS         (4)

#define WAVE_PATTERN_RAM_START   (0xFF30)
#define WAVE_PATTERN_RAM_END     (0xFF3F)

#define PULSE_TIMER_PERIOD(freq) (((2048 - (freq))))
#define WAVE_TIMER_PERIOD(freq)  (((2048 - (freq)) * 2))

#define SDL_SAMPLE_RATE          44100
#define APU_CLOCK                (CPU_CLOCK_SPEED_HZ)

#define SAMPLE_BUFFER_SIZE       2048

//=====================================================================================================================
// Types
//=====================================================================================================================

typedef bool Waveform_t[8];

typedef struct
{
    bool                        active;

    uint8_t                     volume;
    bool                        envelopeActive;
    uint32_t                    waveformIndex;
    int32_t                     envelopePeriod;
    int32_t                     samplePeriod;
    int32_t                     lengthCounter;

    bool                        sweepEnabled;
    int32_t                     sweepTimer;
    uint16_t                    sweepShadowRegister;

    SAudio_ChannelSweep_t      *pNRx0; // unused for channel 2
    SAudio_LengthDutyCycle_t   *pNRx1;
    SAudio_VolumeEnvelope_t    *pNRx2;
    uint8_t                    *pNRx3;
    SAudio_PeriodHighControl_t *pNRx4;

    SAudio_LengthDutyCycle_t    prevNRx1;
} SPulseAudioChannel_t;

typedef struct
{
    bool                              active;

    uint8_t                           volume;
    uint32_t                          sampleIndex;
    int32_t                           samplePeriod;
    int32_t                           lengthCounter;
    uint8_t                          *pCurrentWaveTableEntry;

    SAudio_WaveChannel_DACEnable_t   *pNR30;
    uint8_t                          *pNR31;
    SAudio_WaveChannel_OutputLevel_t *pNR32;
    uint8_t                          *pNR33;
    SAudio_PeriodHighControl_t       *pNR34;

    uint8_t                           prevNR31;
} SWaveAudioChannel_t;

typedef struct
{
    bool                                       active;

    uint8_t                                    volume;
    bool                                       envelopeActive;
    int32_t                                    envelopePeriod;
    int32_t                                    lengthCounter;

    SAudio_NoiseChannel_LengthTimer_t         *pNR41;
    SAudio_VolumeEnvelope_t                   *pNR42;
    SAudio_NoiseChannel_FrequencyRandomness_t *pNR43;
    SAudio_NoiseChannel_Control_t             *pNR44;

    SAudio_NoiseChannel_LengthTimer_t          prevNR41;
} SNoiseAudioChannel_t;

typedef struct
{
    SAudio_MasterVolumeControl_t *pNR50;
    SAudio_ChannelPanning_t      *pNR51;
    SAudio_MasterControl_t       *pNR52;
} SControlStatusRegisters_t;

typedef struct
{
    SPulseAudioChannel_t      ch1;
    SPulseAudioChannel_t      ch2;
    SWaveAudioChannel_t       ch3;
    SNoiseAudioChannel_t      ch4;

    SControlStatusRegisters_t audioControl;

    size_t                    frameSequencerStep;
} SAPUState_t;

//=====================================================================================================================
// Globals
//=====================================================================================================================

static const Waveform_t sc_dutyWaveforms[] = {
    { 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 1, 1, 1 },
    { 0, 1, 1, 1, 1, 1, 1, 0 }
};

static const SAddressBus_t *g_pBus             = nullptr;

static SAPUState_t          g_currentAPUState  = { 0 };
static SAPUState_t          g_previousAPUState = { 0 };

static bool                 hasTicked          = false;

static uint16_t             sampleBuffer[SAMPLE_BUFFER_SIZE];
static volatile size_t      sampleWriteIndex   = 0;
static volatile size_t      sampleReadIndex    = 0;

static uint64_t             g_cycleAccumulator = 0;

//=====================================================================================================================
// Function prototypes
//=====================================================================================================================

static void            cycleFrameSequencer();
static void            updateSampleBuffer(size_t);

static void            tickPulseChannel(size_t, size_t);
static void            triggerPulseChannel(size_t);
static void            tickWaveChannel(size_t);
static void            triggerWaveChannel(void);
static void            tickNoiseChannel(size_t);
static void            triggerNoiseChannel(void);

static void            clockLengthCounters(void);
static void            clockVolumeEnvelope(bool *, int32_t *, uint8_t *, SAudio_VolumeEnvelope_t *);
static void            clockFrequencySweep();

static inline uint16_t getPulseFreqValue(const SPulseAudioChannel_t *);
static inline uint16_t getWaveFreqValue();

static void            audioControlRegisterCallback(uint8_t, uint16_t);

//=====================================================================================================================
// Functions
//=====================================================================================================================

void debugPulseChannelTrigger(void)
{
    SPulseAudioChannel_t *ch = &g_currentAPUState.ch1;

    printf("\n[APU DEBUG] Trigger fired!\n");
    printf("NR52 = 0x%02X\n", *(uint8_t *)g_currentAPUState.audioControl.pNR52);
    printf("NR12 = 0x%02X\n", *(uint8_t *)ch->pNRx2);
    printf("NR14 = 0x%02X\n", *(uint8_t *)ch->pNRx4);

    printf("Before APU tick:\n");
    printf("  lengthCounter = %d\n", ch->lengthCounter);
    printf("  envelope volume = %d\n", ch->volume);
    printf("  envelope samplePeriod = %d\n", ch->envelopePeriod);
    printf("  dutyStep = %d\n", ch->waveformIndex);
    printf("  enabled = %d\n", g_currentAPUState.audioControl.pNR52->ch1Enable);

    // Tick a few cycles manually
    for (int i = 0; i < 8; i++)
    {
        tickPulseChannel(1, 4); // small number of CPU cycles
        printf("Tick %d: dutyStep=%d volume=%d\n", i, ch->waveformIndex, ch->volume);
    }

    // Frame sequencer step
    cycleFrameSequencer();
    printf("After frame sequencer step: lengthCounter=%d volume=%d\n", ch->lengthCounter, ch->volume);
}

void apuInit(SAddressBus_t *pBus)
{
    g_pBus                                       = pBus;

    g_currentAPUState.ch1.pNRx0                  = &pBus->map.ioregs.audio.ch1Sweep;
    g_currentAPUState.ch1.pNRx1                  = &pBus->map.ioregs.audio.ch1LengthDuty;
    g_currentAPUState.ch1.pNRx2                  = &pBus->map.ioregs.audio.ch1VolEnvelope;
    g_currentAPUState.ch1.pNRx3                  = &pBus->map.ioregs.audio.ch1FreqLSB;
    g_currentAPUState.ch1.pNRx4                  = &pBus->map.ioregs.audio.ch1FreqMSBControl;

    g_currentAPUState.ch1.waveformIndex          = 0;
    g_currentAPUState.ch1.envelopeActive         = false;
    g_currentAPUState.ch1.envelopePeriod         = 7;
    g_currentAPUState.ch1.samplePeriod                 = 4 * 2048;
    g_currentAPUState.ch1.lengthCounter          = 0;
    g_currentAPUState.ch1.volume                 = 0;

    g_currentAPUState.ch1.sweepEnabled           = false;
    g_currentAPUState.ch1.sweepShadowRegister    = g_currentAPUState.ch1.samplePeriod;
    g_currentAPUState.ch1.sweepTimer             = 0;

    g_currentAPUState.ch2.pNRx0                  = nullptr; // does not exist for channel 2
    g_currentAPUState.ch2.pNRx1                  = &pBus->map.ioregs.audio.ch2LengthDuty;
    g_currentAPUState.ch2.pNRx2                  = &pBus->map.ioregs.audio.ch2VolEnvelope;
    g_currentAPUState.ch2.pNRx3                  = &pBus->map.ioregs.audio.ch2FreqLSB;
    g_currentAPUState.ch2.pNRx4                  = &pBus->map.ioregs.audio.ch2FreqMSBControl;

    g_currentAPUState.ch2.waveformIndex          = 0;
    g_currentAPUState.ch2.envelopeActive         = false;
    g_currentAPUState.ch2.envelopePeriod         = 7;
    g_currentAPUState.ch2.samplePeriod                 = 4 * 2048;
    g_currentAPUState.ch2.lengthCounter          = 0;
    g_currentAPUState.ch2.volume                 = 0;

    g_currentAPUState.ch3.pNR30                  = &pBus->map.ioregs.audio.ch3DACEnable;
    g_currentAPUState.ch3.pNR31                  = &pBus->map.ioregs.audio.ch3LengthTimer;
    g_currentAPUState.ch3.pNR32                  = &pBus->map.ioregs.audio.ch3OutputLevel;
    g_currentAPUState.ch3.pNR33                  = &pBus->map.ioregs.audio.ch3FreqLSB;
    g_currentAPUState.ch3.pNR34                  = &pBus->map.ioregs.audio.ch3FreqMSBControl;

    g_currentAPUState.ch3.pCurrentWaveTableEntry = &pBus->map.ioregs.wavetable[0];
    g_currentAPUState.ch3.sampleIndex            = 1; // always start at 1, high nibble of 0
    g_currentAPUState.ch3.samplePeriod                 = 4 * 2048;
    g_currentAPUState.ch3.lengthCounter          = 256 - pBus->map.ioregs.audio.ch3LengthTimer;

    g_currentAPUState.ch4.pNR41                  = &pBus->map.ioregs.audio.ch4LengthTimer;
    g_currentAPUState.ch4.pNR42                  = &pBus->map.ioregs.audio.ch4VolEnvelope;
    g_currentAPUState.ch4.pNR43                  = &pBus->map.ioregs.audio.ch4FreqRandom;
    g_currentAPUState.ch4.pNR44                  = &pBus->map.ioregs.audio.ch4Control;

    g_currentAPUState.ch4.volume                 = 7;
    g_currentAPUState.ch4.envelopeActive         = 0;
    g_currentAPUState.ch4.envelopePeriod         = 0;
    g_currentAPUState.ch4.lengthCounter          = 64 - pBus->map.ioregs.audio.ch4LengthTimer.initialLength;

    // control
    g_currentAPUState.audioControl.pNR50 = &pBus->map.ioregs.audio.masterVolVINControl;
    g_currentAPUState.audioControl.pNR51 = &pBus->map.ioregs.audio.channelPanning;
    g_currentAPUState.audioControl.pNR52 = &pBus->map.ioregs.audio.masterControl;

    g_currentAPUState.ch1.active         = false;
    g_currentAPUState.ch2.active         = false;
    g_currentAPUState.ch3.active         = false;
    g_currentAPUState.ch4.active         = false;

    g_previousAPUState                   = g_currentAPUState;

    registerAddressCallback(AUDIO_MASTER_CONTROL_ADDR, AUDIO_MASTER_CONTROL_CALLBACK, audioControlRegisterCallback);
    registerAddressCallback(AUDIO_CH1_CONTROL_ADDR, AUDIO_CH1_CONTROL_CALLBACK, audioControlRegisterCallback);
    registerAddressCallback(AUDIO_CH2_CONTROL_ADDR, AUDIO_CH2_CONTROL_CALLBACK, audioControlRegisterCallback);
    registerAddressCallback(AUDIO_CH3_CONTROL_ADDR, AUDIO_CH3_CONTROL_CALLBACK, audioControlRegisterCallback);
    registerAddressCallback(AUDIO_CH4_CONTROL_ADDR, AUDIO_CH4_CONTROL_CALLBACK, audioControlRegisterCallback);

    registerAddressCallback(AUDIO_CH1_VOLUME_ENVELOPE_ADDR, AUDIO_CH1_VOLUME_ENVELOPE_CALLBACK,
                            audioControlRegisterCallback);
    registerAddressCallback(AUDIO_CH2_VOLUME_ENVELOPE_ADDR, AUDIO_CH2_VOLUME_ENVELOPE_CALLBACK,
                            audioControlRegisterCallback);
    registerAddressCallback(AUDIO_CH3_VOLUME_ENVELOPE_ADDR, AUDIO_CH3_VOLUME_ENVELOPE_CALLBACK,
                            audioControlRegisterCallback);
    registerAddressCallback(AUDIO_CH4_VOLUME_ENVELOPE_ADDR, AUDIO_CH4_VOLUME_ENVELOPE_CALLBACK,
                            audioControlRegisterCallback);
}

/**
 * @brief audio tick loop
 *
 * @param apuCycles amount of APU cycles to run this loop
 */
void apuTick(size_t apuCycles)
{
    if (!g_currentAPUState.audioControl.pNR52->audioMasterEnable) return;

    static size_t totalCycles = 0;

    totalCycles += apuCycles;

    if (g_currentAPUState.ch1.active && (g_currentAPUState.ch1.lengthCounter > 0))
    {
        tickPulseChannel(1, apuCycles);
    }
    if (g_currentAPUState.ch2.active && (g_currentAPUState.ch2.lengthCounter > 0))
    {
        tickPulseChannel(2, apuCycles);
    }

    // if (*(uint8_t *)g_currentAPUState.ch3.pNR30)
    // {
    //     tickWaveChannel(apuCycles);
    // }
    updateSampleBuffer(apuCycles);

    if (totalCycles >= 8192)
    {
        cycleFrameSequencer();
        totalCycles -= 8192;
    }
}

static void cycleFrameSequencer()
{
    switch (g_currentAPUState.frameSequencerStep)
    {
        case 2:
        case 6:
            clockFrequencySweep(); // 128Hz
        case 0:
        case 4:
            clockLengthCounters(); // 256Hz
            break;

        case 7:
            // volume envelopes are clocked once per frame, so 64Hz
            clockVolumeEnvelope(&g_currentAPUState.ch1.envelopeActive, &g_currentAPUState.ch1.envelopePeriod,
                                &g_currentAPUState.ch1.volume, g_currentAPUState.ch1.pNRx2);
            clockVolumeEnvelope(&g_currentAPUState.ch2.envelopeActive, &g_currentAPUState.ch2.envelopePeriod,
                                &g_currentAPUState.ch2.volume, g_currentAPUState.ch2.pNRx2);
            clockVolumeEnvelope(&g_currentAPUState.ch4.envelopeActive, &g_currentAPUState.ch4.envelopePeriod,
                                &g_currentAPUState.ch4.volume, g_currentAPUState.ch4.pNR42);
            break;
    }

    g_currentAPUState.frameSequencerStep = (g_currentAPUState.frameSequencerStep + 1) & 0x07; // wrap 0-7
}

static void tickPulseChannel(size_t num, size_t cycles)
{
    SPulseAudioChannel_t *pulse = (num == 1 ? &g_currentAPUState.ch1 : &g_currentAPUState.ch2);

    pulse->samplePeriod -= cycles;

    // if samplePeriod goes below 0, we have to step the waveform
    while (pulse->samplePeriod <= 0)
    {
        pulse->samplePeriod += PULSE_TIMER_PERIOD(getPulseFreqValue(pulse));
        pulse->waveformIndex = (pulse->waveformIndex + 1) & 0x07;
    }

    if (*(uint8_t *)pulse->pNRx1 != *(uint8_t *)&pulse->prevNRx1)
    {
        pulse->lengthCounter = 64 - pulse->pNRx1->initialLength;
        memcpy(&pulse->prevNRx1, pulse->pNRx1, sizeof(SAudio_LengthDutyCycle_t));
    }
}

static void triggerPulseChannel(size_t num)
{
    SPulseAudioChannel_t *pulse = (num == 1 ? &g_currentAPUState.ch1 : &g_currentAPUState.ch2);

    pulse->envelopeActive       = true;
    pulse->volume               = pulse->pNRx2->initialVolume;
    pulse->envelopePeriod       = pulse->pNRx2->envelopePeriod;
    pulse->envelopeActive       = pulse->envelopePeriod == 0 ? false : true;

    pulse->lengthCounter = 64 - pulse->pNRx1->initialLength;

    pulse->waveformIndex = 0;
    pulse->samplePeriod        = PULSE_TIMER_PERIOD(getPulseFreqValue(pulse));

    if (num == 1)
    {
        pulse->sweepTimer          = 0;
        pulse->sweepShadowRegister = getPulseFreqValue(pulse);
        pulse->sweepEnabled        = ((pulse->pNRx0->period != 0) || (pulse->pNRx0->shift != 0));

        if (pulse->pNRx0->shift != 0)
        {
            clockFrequencySweep();
        }

        //debugPulseChannelTrigger();

        if ((*(uint8_t *)g_currentAPUState.ch1.pNRx2 & 0xF8) != 0)
        {
            pulse->active = true;
        }
    }
    else if ((*(uint8_t *)g_currentAPUState.ch2.pNRx2 & 0xF8) != 0)
    {
        pulse->active = true;
    }
}

static void tickWaveChannel(size_t cycles)
{
    switch (g_currentAPUState.ch3.pNR32->outputLevel)
    {
        case 0:
        {
            g_currentAPUState.ch3.volume = 0;
            break;
        }
        case 1:
        {
            g_currentAPUState.ch3.volume = 0xF;
            break;
        }
        case 2:
        {
            g_currentAPUState.ch3.volume = 0x8;
            break;
        }
        case 3:
        {
            g_currentAPUState.ch3.volume = 0x4;
            break;
        }
    }

    g_currentAPUState.ch3.samplePeriod -= cycles;

    while (g_currentAPUState.ch3.samplePeriod <= 0)
    {
        g_currentAPUState.ch3.samplePeriod += WAVE_TIMER_PERIOD(getWaveFreqValue());
        g_currentAPUState.ch3.sampleIndex = (g_currentAPUState.ch3.sampleIndex + 1) & 0x07;
    }

    g_currentAPUState.ch3.lengthCounter = 256 - *g_currentAPUState.ch3.pNR31;

    // while (g_currentAPUState.ch3.samplePeriod  <= 0)
    // {
    //     // g_currentAPUState.ch3->currentPeriod += PULSE_TIMER_PERIOD(getChannelFreqValue(pulse));

    //     g_currentAPUState.ch3->waveformIndex = (g_currentAPUState.ch3->waveformIndex + 1) & 0x07;
    // }

    // pulse->sample = sc_dutyWaveforms[pulse->pNRx1->waveDuty][pulse->waveformIndex] ? 1 : 0;

    // if (g_currentAPUState.ch3->pNRx4->trigger)
    // {
    //     if (g_currentAPUState.audioControl.pNR52->ch3Enable == 0)
    //     {
    //         g_currentAPUState.audioControl.pNR52->ch3Enable = 1;
    //     }
    //     if (g_currentAPUState.ch3->currentLengthCounter == 0)
    //     {
    //         g_currentAPUState.ch3->currentLengthCounter = 256 - *g_currentAPUState.ch3->pNRx1;
    //     }
    // }

    // if (*(*(SWaveAudioChannel_t *)&g_currentAPUState.ch3).pNRx1 != *g_currentAPUState.ch3->pNRx1)
    // {
    //     g_currentAPUState.ch3->currentLengthCounter = 256 - *g_currentAPUState.ch3->pNRx1;
    // }
}

static void triggerWaveChannel(void)
{
    g_currentAPUState.ch3.active = true;

    if (!g_currentAPUState.ch3.lengthCounter)
    {
        g_currentAPUState.ch3.lengthCounter = 256 - *g_currentAPUState.ch3.pNR31;
    }

    g_currentAPUState.ch3.sampleIndex = 0;
    g_currentAPUState.ch3.samplePeriod      = WAVE_TIMER_PERIOD(getWaveFreqValue());
}

static void tickNoiseChannel(size_t cycles)
{
    // SNoiseAudioChannel_t *noise = (SNoiseAudioChannel_t *)channelCtx;

    // if (noise->pNRx4->trigger)
    // {
    //     if (g_currentAPUState.audioControl.pNR52->ch4Enable == 0)
    //     {
    //         g_currentAPUState.audioControl.pNR52->ch4Enable = 1;
    //     }
    //     if (noise->currentLengthCounter == 0)
    //     {
    //         noise->currentLengthCounter = 64 - noise->pNRx1->initialLength;
    //     }
    // }

    // if (*(uint8_t *)(*(SNoiseAudioChannel_t *)&g_currentAPUState.ch4).pNRx1 != *(uint8_t *)noise->pNRx1)
    // {
    //     noise->currentLengthCounter = 64 - noise->pNRx1->initialLength;
    // }
}

static void triggerNoiseChannel(void) {}

static void clockLengthCounters(void)
{
    if (g_currentAPUState.ch1.pNRx4->lengthEnable && g_currentAPUState.ch1.lengthCounter > 0)
    {
        if (--g_currentAPUState.ch1.lengthCounter == 0)
        {
            g_currentAPUState.ch1.active = false;
        }
    }

    if (g_currentAPUState.ch2.pNRx4->lengthEnable && g_currentAPUState.ch2.lengthCounter > 0)
    {
        if (--g_currentAPUState.ch2.lengthCounter == 0)
        {
            g_currentAPUState.ch2.active = false;
        }
    }

    if (g_currentAPUState.ch3.pNR34->lengthEnable && g_currentAPUState.ch3.lengthCounter > 0)
    {
        if (--g_currentAPUState.ch3.lengthCounter == 0)
        {
            g_currentAPUState.ch3.active = false;
        }
    }

    if (g_currentAPUState.ch4.pNR44->lengthEnable && g_currentAPUState.ch4.lengthCounter > 0)
    {
        if (--g_currentAPUState.ch4.lengthCounter == 0)
        {
            g_currentAPUState.ch4.active = false;
        }
    }
}

static void clockVolumeEnvelope(bool *pEnvelopeActive, int32_t *pEnvelopePeriod, uint8_t *pVolume,
                                SAudio_VolumeEnvelope_t *pEnvelopeRegister)
{
    // if a clock is generated and envelope samplePeriod is not 0, calculate new volume
    if (*pEnvelopeActive) // && pCH1->pNRx2->envelopePeriod > 0)
    {
        if (--(*pEnvelopePeriod) == 0)
        {
            if ((*pVolume < 0xF) && (pEnvelopeRegister->envDir == 1))
            {
                (*pVolume)++;
            }
            else if ((*pVolume > 0x0) && (pEnvelopeRegister->envDir == 0))
            {
                (*pVolume)--;
            }
            *pEnvelopePeriod = (pEnvelopeRegister->envelopePeriod ? pEnvelopeRegister->envelopePeriod : 8);
        }
    }
}

static void clockFrequencySweep()
{
    if (!g_currentAPUState.ch1.sweepEnabled || !g_currentAPUState.ch1.pNRx0->period) return;

    uint16_t freqAdd = g_currentAPUState.ch1.sweepShadowRegister >> g_currentAPUState.ch1.pNRx0->shift;
    int32_t  newFreq =
    g_currentAPUState.ch1.sweepShadowRegister + (g_currentAPUState.ch1.pNRx0->negate ? -freqAdd : freqAdd);

    if (newFreq > 0x7FF)
    {
        // overflow -> disable channel
        g_currentAPUState.ch1.active = false;
    }
    else
    {
        g_currentAPUState.ch1.sweepShadowRegister = newFreq;

        *(uint8_t *)g_currentAPUState.ch1.pNRx3 = (uint8_t)(newFreq & 0xFF);
        g_currentAPUState.ch1.pNRx4->freqMSB    = (newFreq >> 8) & 0x7;

        freqAdd = g_currentAPUState.ch1.sweepShadowRegister >> g_currentAPUState.ch1.pNRx0->shift;
        newFreq = g_currentAPUState.ch1.sweepShadowRegister + (g_currentAPUState.ch1.pNRx0->negate ? -freqAdd : freqAdd);

        if (newFreq > 0x7FF)
        {
            // overflow -> disable channel
            g_currentAPUState.ch1.active = false;
        }
    }
}

/**
 * @brief the frame sequencer runs at 512Hz, or every ~8192 CPU cycles (4194304/512=8192)
 *
 */

void generateDownmixCallback(void *userdata, uint8_t *pStream, int len)
{
    uint16_t *out     = (uint16_t *)pStream;
    size_t    samples = len / sizeof(int16_t);

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

static inline uint16_t getPulseFreqValue(const SPulseAudioChannel_t *pChannel)
{
    // use the lower 3 bits from NRx4 as MSB's or'ed with NRx3
    return (uint16_t)((*(uint8_t *)(pChannel->pNRx4) & 0x7) << 8) | *(uint8_t *)pChannel->pNRx3;
}

static inline uint16_t getWaveFreqValue()
{
    return (uint16_t)((*(uint8_t *)(g_currentAPUState.ch3.pNR34) & 0x7) << 8) | *(uint8_t *)g_currentAPUState.ch3.pNR33;
}

/**
 * @brief update sample buffer. also saves APU state to g_previousAPUState
 *
 * @param cycles cycle count to update for
 */
void updateSampleBuffer(size_t cycles)
{
    static double sampleCounter   = 0.0;
    const double  cyclesPerSample = (double)APU_CLOCK / (double)SDL_SAMPLE_RATE;
    static float  prev            = 0.0f;

    sampleCounter += (double)cycles;

    while (sampleCounter >= cyclesPerSample)
    {
        sampleCounter -= cyclesPerSample;

        int16_t mix = 0;

        if (g_currentAPUState.audioControl.pNR52->audioMasterEnable)
        {
            if (g_currentAPUState.ch1.active)
            {
                int16_t s =
                sc_dutyWaveforms[g_currentAPUState.ch1.pNRx1->waveDuty][g_currentAPUState.ch1.waveformIndex] ?
                (int16_t)g_currentAPUState.ch1.volume :
                -(int16_t)g_currentAPUState.ch1.volume;
                mix += s * 200;
            }
            if (g_currentAPUState.ch2.active)
            {
                int16_t s =
                sc_dutyWaveforms[g_currentAPUState.ch2.pNRx1->waveDuty][g_currentAPUState.ch2.waveformIndex] ?
                (int16_t)g_currentAPUState.ch2.volume :
                -(int16_t)g_currentAPUState.ch2.volume;
                mix += s * 200;
            }
            // if (g_currentAPUState.audioControl.pNR52->ch3Enable)
            // {
            //     mix += *(uint8_t *)&g_pBus->map.ioregs.wavepattern[g_currentAPUState.ch3.sampleIndex / 2] &
            //            (0xF << g_currentAPUState.ch3.sampleIndex % 2);
            // }
        }

        if ((sampleWriteIndex - sampleReadIndex) < SAMPLE_BUFFER_SIZE)
        {
            sampleBuffer[sampleWriteIndex & (SAMPLE_BUFFER_SIZE - 1)] = mix;
            sampleWriteIndex++;
        }
    }

    g_previousAPUState = g_currentAPUState;
}

static void audioControlRegisterCallback(uint8_t data, uint16_t addr)
{
    bool isTrigger = data & 0x80;

    switch (addr)
    {
        case AUDIO_MASTER_CONTROL_ADDR:
        {
            if ((g_previousAPUState.audioControl.pNR52->ch1Enable !=
                 g_currentAPUState.audioControl.pNR52->ch1Enable) &&
                (g_currentAPUState.audioControl.pNR52->ch1Enable == 0))
            {
                g_currentAPUState.ch1.active         = false;
                g_currentAPUState.ch1.envelopeActive = 0;
                g_currentAPUState.ch1.waveformIndex  = 0;
                g_currentAPUState.ch1.lengthCounter  = 0;
                g_currentAPUState.ch1.volume         = 0;
            }
            if ((g_previousAPUState.audioControl.pNR52->ch2Enable !=
                 g_currentAPUState.audioControl.pNR52->ch2Enable) &&
                (g_currentAPUState.audioControl.pNR52->ch2Enable == 0))
            {
                g_currentAPUState.ch2.active         = false;
                g_currentAPUState.ch2.envelopeActive = 0;
                g_currentAPUState.ch2.waveformIndex  = 0;
                g_currentAPUState.ch2.lengthCounter  = 0;
                g_currentAPUState.ch2.volume         = 0;
            }
            if ((g_previousAPUState.audioControl.pNR52->ch3Enable !=
                 g_currentAPUState.audioControl.pNR52->ch3Enable) &&
                (g_currentAPUState.audioControl.pNR52->ch3Enable == 0))
            {
                g_currentAPUState.ch3.active        = false;
                g_currentAPUState.ch3.sampleIndex   = 0;
                g_currentAPUState.ch3.lengthCounter = 0;
                g_currentAPUState.ch3.volume        = 0;
            }
            if ((g_previousAPUState.audioControl.pNR52->ch4Enable !=
                 g_currentAPUState.audioControl.pNR52->ch4Enable) &&
                (g_currentAPUState.audioControl.pNR52->ch4Enable == 0))
            {
                g_currentAPUState.ch4.active         = false;
                g_currentAPUState.ch4.envelopeActive = 0;
                g_currentAPUState.ch4.lengthCounter  = 0;
                g_currentAPUState.ch4.volume         = 0;
            }
            break;
        }
        case AUDIO_CH1_CONTROL_ADDR:
        {
            g_currentAPUState.ch1.pNRx4->lengthEnable = (data >> 6) & 1;
            *(uint8_t *)g_currentAPUState.ch1.pNRx4   = data & 0x78; // store value without trigger bit & samplePeriod
            if (isTrigger)
            {
                triggerPulseChannel(1);
            }
            break;
        }
        case AUDIO_CH2_CONTROL_ADDR:
        {
            g_currentAPUState.ch2.pNRx4->lengthEnable = (data >> 6) & 1;
            *(uint8_t *)g_currentAPUState.ch2.pNRx4   = data & 0x7F;
            if (isTrigger)
            {
                triggerPulseChannel(2);
            }
            break;
        }
        case AUDIO_CH3_CONTROL_ADDR:
        {
            g_currentAPUState.ch1.pNRx4->lengthEnable = (data >> 6) & 1;
            *(uint8_t *)g_currentAPUState.ch3.pNR34   = data & 0x7F;
            if (isTrigger)
            {
                triggerWaveChannel();
            }
            break;
        }
        case AUDIO_CH4_CONTROL_ADDR:
        {
            g_currentAPUState.ch4.pNR44->lengthEnable = (data >> 6) & 1;
            *(uint8_t *)g_currentAPUState.ch4.pNR44   = data & 0x7F;
            if (isTrigger)
            {
                triggerNoiseChannel();
            }
            break;
        }
        case AUDIO_CH1_VOLUME_ENVELOPE_ADDR:
        {
            if ((*(uint8_t *)g_currentAPUState.ch1.pNRx2 & 0xF8) == 0)
            {
                g_currentAPUState.ch1.active = false;
            }
            break;
        }
        case AUDIO_CH2_VOLUME_ENVELOPE_ADDR:
        {
            if ((*(uint8_t *)g_currentAPUState.ch2.pNRx2 & 0xF8) == 0)
            {
                g_currentAPUState.ch2.active = false;
            }
            break;
        }
        case AUDIO_CH3_VOLUME_ENVELOPE_ADDR:
        {
            if (g_currentAPUState.ch3.pNR30->dacOnOff == 0)
            {
                g_currentAPUState.ch3.active = false;
            }
            break;
        }
        case AUDIO_CH4_VOLUME_ENVELOPE_ADDR:
        {
            if ((*(uint8_t *)g_currentAPUState.ch4.pNR42 & 0xF8) == 0)
            {
                g_currentAPUState.ch4.active = false;
            }
            break;
        }
    }
}