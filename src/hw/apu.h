/**
 * @file apu.h
 * @author Toesoe
 * @brief seaboy audio emulation
 * @version 0.1
 * @date 2025-06-25
 *
 */

#ifndef _APU_H_
#define _APU_H_

#include <stdint.h>
#include <stdlib.h>

#include "mem.h"

typedef struct SAudioChannel_t SAudioChannel_t;

typedef void (*fnTick)(SAudioChannel_t *, size_t);

typedef enum
{
    PULSE,
    WAVE,
    NOISE
} EAudioChannelType_t;

struct SAudioChannel_t
{
    EAudioChannelType_t type;
    size_t              num;
    fnTick              tick;
    uint8_t             sample;
    uint32_t            data0;
    uint32_t            data1;
    int32_t             data2;
    int32_t             data3;

    void               *pNRx0;
    void               *pNRx1;
    void               *pNRx2;
    void               *pNRx3;
    void               *pNRx4;
};

typedef struct
{
    SAudio_MasterVolumeControl_t *pNR50;
    SAudio_ChannelPanning_t      *pNR51;
    SAudio_MasterControl_t       *pNR52;
} SControlStatusRegisters_t;

typedef struct
{
    SAudioChannel_t           ch1Pulse;
    SAudioChannel_t           ch2Pulse;
    SAudioChannel_t           ch3Wave;
    SAudioChannel_t           ch4Noise;

    SControlStatusRegisters_t audioControl;

    size_t                    frameSequencerStep;
} APU_t;

void apuInit(bus_t *);

APU_t *getAPUObject(void);

void generateDownmixCallback(void *, uint8_t *, int);

void cycleFrameSequencer();
void updateAPUSampleBuffer(size_t);

#endif //!_APU_H_