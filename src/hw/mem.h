/**
 * @file bus.h
 * @author Toesoe
 * @brief seaboy mem (ram, vram)
 * @version 0.1
 * @date 2023-06-15
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef _MEM_H_
#define _MEM_H_

#include <stddef.h>
#include <stdint.h>

#include "cart.h"

#define GB_BUS_SIZE               0x10000

#define ROMN_SIZE                 0x4000
#define VRAM_SIZE                 0x2000
#define ERAM_SIZE                 0x2000
#define WRAM_SIZE                 0x2000
#define ECHO_SIZE                 0x1E00
#define OAM_SIZE                  0xA0
#define IO_SIZE                   0x80
#define HRAM_SIZE                 0x7F

#define JOYPAD_INPUT_ADDR              (0xFF00)
#define SERIAL_TRANSFER_ADDR           (0xFF01) // 2 bytes
#define TIMER_ADDR                     (0xFF05) // 2 bytes
#define DIVIDER_ADDR                   (0xFF04) // 2 bytes
#define AUDIO_MASTER_CONTROL_ADDR      (0xFF26)
#define AUDIO_CH1_VOLUME_ENVELOPE_ADDR (0xFF12)
#define AUDIO_CH1_CONTROL_ADDR         (0xFF14)
#define AUDIO_CH2_VOLUME_ENVELOPE_ADDR (0xFF17)
#define AUDIO_CH2_CONTROL_ADDR         (0xFF19)
#define AUDIO_CH3_VOLUME_ENVELOPE_ADDR (0xFF1A)
#define AUDIO_CH3_CONTROL_ADDR         (0xFF1E)
#define AUDIO_CH4_VOLUME_ENVELOPE_ADDR (0xFF21)
#define AUDIO_CH4_CONTROL_ADDR         (0xFF23)
#define OAM_DMA_ADDR                   (0xFF46)
#define BOOT_ROM_MAPPER_CONTROL_ADDR   (0xFF50)

#define AUDIO_REGS_START_ADDR          (0xFF10)
#define AUDIO_REGS_END_ADDR            (0xFF26)

// VRAM related
#define TILEBLOCK_SIZE            0x800
#define TILEMAP_SIZE              0x400

typedef enum
{
    NONE = -1,
    BOOTROM_UNMAP_CALLBACK,
    OAM_DMA_CALLBACK,
    JOYPAD_REG_CALLBACK,
    AUDIO_MASTER_CONTROL_CALLBACK,
    AUDIO_CH1_CONTROL_CALLBACK,
    AUDIO_CH2_CONTROL_CALLBACK,
    AUDIO_CH3_CONTROL_CALLBACK,
    AUDIO_CH4_CONTROL_CALLBACK,
    AUDIO_CH1_VOLUME_ENVELOPE_CALLBACK,
    AUDIO_CH2_VOLUME_ENVELOPE_CALLBACK,
    AUDIO_CH3_VOLUME_ENVELOPE_CALLBACK,
    AUDIO_CH4_VOLUME_ENVELOPE_CALLBACK,
    DIV_CALLBACK,
    ADDRESS_CALLBACK_TYPE_COUNT
} EAddressCallbackType_t;

typedef void (*addressWriteCallback)(uint8_t, uint16_t);

typedef struct __attribute__((__packed__))
{
    uint8_t aRight       : 1;
    uint8_t bLeft        : 1;
    uint8_t selectUp     : 1;
    uint8_t startDown    : 1;
    uint8_t dpadSelect   : 1; // if 0, input can be read from first 4 bits
    uint8_t buttonSelect : 1; // if 0, first 4 bits are a/b/sel/start instead
    uint8_t _none        : 2;
} sRegJOYP_t;

typedef struct __attribute__((__packed__))
{
    uint8_t clockSelect    : 1;
    uint8_t clockSpeed     : 1;
    uint8_t _none          : 5;
    uint8_t transferEnable : 1;
} sRegSC_t;

typedef struct __attribute__((__packed__))
{
    uint8_t clockSelect : 2;
    uint8_t enable      : 1;
    uint8_t _none       : 5;
} sRegTAC_t;

typedef struct __attribute__((__packed__))
{
    uint8_t   TIMA;
    uint8_t   TMA;
    sRegTAC_t TAC;
} STimers_t;

typedef struct __attribute__((__packed__))
{
    uint8_t bgWindowEnablePrio : 1; // 0
    uint8_t objEnable          : 1; // 1
    uint8_t objSize            : 1; // 2
    uint8_t bgTilemap          : 1; // 3
    uint8_t bgWindowTileData   : 1; // 4
    uint8_t bgWindowEnable     : 1; // 5
    uint8_t bgWindowTileMap    : 1; // 6
    uint8_t lcdPPUEnable       : 1; // 7
} SRegLCDC_t;

typedef struct __attribute__((__packed__))
{
    uint8_t ppuMode     : 2;
    uint8_t lycEqLy     : 1;
    uint8_t mode0IntSel : 1;
    uint8_t mode1IntSel : 1;
    uint8_t mode2IntSel : 1;
    uint8_t LYCIntSel   : 1;
    uint8_t _none       : 1;
} SRegLCDStat_t;

typedef struct __attribute__((__packed__))
{
    uint8_t id0 : 2;
    uint8_t id1 : 2;
    uint8_t id2 : 2;
    uint8_t id3 : 2;
} SRegPaletteData_t;

typedef struct __attribute__((__packed__))
{
    uint8_t vblank : 1; // 0
    uint8_t lcd    : 1; // 1
    uint8_t timer  : 1; // 2
    uint8_t serial : 1; // 3
    uint8_t joypad : 1; // 4
    uint8_t _none  : 3; // 567
} SInterruptFlags_t;

// Audio regs

typedef struct __attribute__((__packed__))
{
    uint8_t rightVol       : 3;
    uint8_t rightVINEnable : 1;
    uint8_t leftVol        : 3;
    uint8_t leftVINEnable  : 1;
} SAudio_MasterVolumeControl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t ch1Right : 1;
    uint8_t ch2Right : 1;
    uint8_t ch3Right : 1;
    uint8_t ch4Right : 1;
    uint8_t ch1Left  : 1;
    uint8_t ch2Left  : 1;
    uint8_t ch3Left  : 1;
    uint8_t ch4Left  : 1;
} SAudio_ChannelPanning_t;

typedef struct __attribute__((__packed__))
{
    uint8_t ch1Enable         : 1;
    uint8_t ch2Enable         : 1;
    uint8_t ch3Enable         : 1;
    uint8_t ch4Enable         : 1;
    uint8_t _unused           : 3;
    uint8_t audioMasterEnable : 1;
} SAudio_MasterControl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t shift   : 3;
    uint8_t negate  : 1;
    uint8_t period  : 3;
    uint8_t _unused : 1;
} SAudio_ChannelSweep_t;

typedef struct __attribute__((__packed__))
{
    uint8_t initialLength : 6; // 0-5
    uint8_t waveDuty      : 2; // 6-7
} SAudio_LengthDutyCycle_t;

typedef struct __attribute__((__packed__))
{
    uint8_t envelopePeriod : 3; // 0-2
    uint8_t envDir         : 1; // 3
    uint8_t initialVolume  : 4; // 4-7
} SAudio_VolumeEnvelope_t;

typedef struct __attribute__((__packed__))
{
    uint8_t freqMSB      : 3; // 0-2
    uint8_t _unused      : 3; // 3-5
    uint8_t lengthEnable : 1; // 6
    uint8_t trigger      : 1; // 7
} SAudio_PeriodHighControl_t;

typedef struct __attribute__((__packed__))
{
    uint8_t _unused  : 7;
    uint8_t dacOnOff : 1;
} SAudio_WaveChannel_DACEnable_t;

typedef struct __attribute__((__packed__))
{
    uint8_t _unused     : 5;
    uint8_t outputLevel : 2; // 5-6
    uint8_t _unused2    : 1;
} SAudio_WaveChannel_OutputLevel_t;

typedef struct __attribute__((__packed__))
{
    uint8_t initialLength : 6; // 0-5
    uint8_t _unused       : 2; // 6-7
} SAudio_NoiseChannel_LengthTimer_t;

typedef struct __attribute__((__packed__))
{
    uint8_t clockDiv   : 3; // 0-2
    uint8_t lfsrWidth  : 1; // 3
    uint8_t clockShift : 4;
} SAudio_NoiseChannel_FrequencyRandomness_t;

typedef struct __attribute__((__packed__))
{
    uint8_t _unused      : 5; // 0-5
    uint8_t lengthEnable : 1; // 6
    uint8_t trigger      : 1; // 7
} SAudio_NoiseChannel_Control_t;

typedef union __attribute__((__packed__))
{
    uint8_t bus[GB_BUS_SIZE];

    struct __attribute__((__packed__))
    {
        const uint8_t *pRom0; // 0x0000 -> 0x3FFF
        const uint8_t *pRom1; // 0x4000 -> 0x7FFF
        uint8_t        __rompadding__[(ROMN_SIZE * 2) - (2 * sizeof(uint8_t *))];

        union __attribute__((__packed__))
        {
            uint8_t all[VRAM_SIZE]; // 0x8000 -> 0x9FFF

            struct __attribute__((__packed__))
            {
                uint8_t block0[TILEBLOCK_SIZE]; // 0x8000 -> 0x87FF
                uint8_t block1[TILEBLOCK_SIZE]; // 0x8800 -> 0x8FFF
                uint8_t block2[TILEBLOCK_SIZE]; // 0x9000 -> 0x97FF
                uint8_t map0[TILEMAP_SIZE];     // 0x9800 -> 0x9BFF
                uint8_t map1[TILEMAP_SIZE];     // 0x9C00 -> 0x9FFF
            } tiles;
        } vram;

        uint8_t *pEram; // 0xA000 -> 0xBFFF
        uint8_t  __erampadding__[(ERAM_SIZE) - sizeof(uint8_t *)];

        union __attribute__((__packed__))
        {
            uint8_t all[WRAM_SIZE]; // 0xC000 -> 0xDFFF

            struct __attribute__((__packed__))
            {
                uint8_t bank0[WRAM_SIZE / 2]; // 0xC000 -> 0xCFFF
                uint8_t bankn[WRAM_SIZE / 2]; // 0xD000 -> 0xDFFF
            } banks;
        } wram;

        uint8_t echo[ECHO_SIZE]; // 0xE000 -> 0xFDFF
        uint8_t oam[OAM_SIZE];   // 0xFE00 -> 0xFE9F
        uint8_t _NOUSE[0x60];    // 0xFEA0 -> 0xFEFF

        struct __attribute__((__packed__))
        {
            sRegJOYP_t        joypad;       // 0xFF00
            uint8_t           serData;      // 0xFF01
            sRegSC_t          serControl;   // 0xFF02
            uint8_t           _padding1;    // 0xFF03
            uint8_t           divRegister;  // 0xFF04
            STimers_t         timers;       // 0xFF05 -> 0xFF07
            uint8_t           _padding2[7]; // 0xFF08 -> 0xFF0E
            SInterruptFlags_t intFlags;     // 0xFF0F

            struct __attribute__((__packed__))
            {
                SAudio_ChannelSweep_t                     ch1Sweep;            // 0xFF10 | NR10
                SAudio_LengthDutyCycle_t                  ch1LengthDuty;       // 0xFF11 | NR11
                SAudio_VolumeEnvelope_t                   ch1VolEnvelope;      // 0xFF12 | NR12
                uint8_t                                   ch1FreqLSB;          // 0xFF13 | NR13
                SAudio_PeriodHighControl_t                ch1FreqMSBControl;   // 0xFF14 | NR14
                uint8_t                                   _unused;             // 0xFF15
                SAudio_LengthDutyCycle_t                  ch2LengthDuty;       // 0xFF16 | NR21
                SAudio_VolumeEnvelope_t                   ch2VolEnvelope;      // 0xFF17 | NR22
                uint8_t                                   ch2FreqLSB;          // 0xFF18 | NR23
                SAudio_PeriodHighControl_t                ch2FreqMSBControl;   // 0xFF19 | NR24
                SAudio_WaveChannel_DACEnable_t            ch3DACEnable;        // 0xFF1A | NR30
                uint8_t                                   ch3LengthTimer;      // 0xFF1B | NR31
                SAudio_WaveChannel_OutputLevel_t          ch3OutputLevel;      // 0xFF1C | NR32
                uint8_t                                   ch3FreqLSB;          // 0xFF1D | NR33
                SAudio_PeriodHighControl_t                ch3FreqMSBControl;   // 0xFF1E | NR34
                uint8_t                                   _unused2;            // 0xFF1F
                SAudio_NoiseChannel_LengthTimer_t         ch4LengthTimer;      // 0xFF20 | NR41
                SAudio_VolumeEnvelope_t                   ch4VolEnvelope;      // 0xFF21 | NR42
                SAudio_NoiseChannel_FrequencyRandomness_t ch4FreqRandom;       // 0xFF22 | NR43
                SAudio_NoiseChannel_Control_t             ch4Control;          // 0xFF23 | NR44
                SAudio_MasterVolumeControl_t              masterVolVINControl; // 0xFF24
                SAudio_ChannelPanning_t                   channelPanning;      // 0xFF25
                SAudio_MasterControl_t                    masterControl;       // 0xFF26
            } audio;

            uint8_t _padding3[9];      // 0xFF27 -> 0xFF2F
            uint8_t wavetable[0x10];   // 0xFF30 -> 0xFF3F

            struct __attribute__((__packed__))
            {
                SRegLCDC_t        control; // 0xFF40
                SRegLCDStat_t     stat;    // 0xFF41
                uint8_t           scy;     // 0xFF42
                uint8_t           scx;     // 0xFF43
                uint8_t           ly;      // 0xFF44
                uint8_t           lyc;     // 0xFF45
                uint8_t           dma;     // 0xFF46
                SRegPaletteData_t bgp;     // 0xFF47
                SRegPaletteData_t obp0;    // 0xFF48
                SRegPaletteData_t obp1;    // 0xFF49
                uint8_t           wy;      // 0xFF4A
                uint8_t           wx;      // 0xFF4B
            } lcd;

            uint8_t _padding4[3];        // 0xFF4C -> 0xFF4E
            uint8_t vramBankSelect;      // 0xFF4F
            uint8_t disableBootrom;      // 0xFF50
            uint8_t _unusedForNow[0x2F]; // 0xFF51 -> 0xFF7F
        } ioregs;

        uint8_t           hram[HRAM_SIZE]; // 0xFF80 -> 0xFFFE
        SInterruptFlags_t interruptEnable; // 0xFFFF
    } map;
} SAddressBus_t;

void                 initializeBus(const SCartridge_t *, bool);
void                 overrideBus(SAddressBus_t *);
void                 mapRomIntoMem(uint8_t **, size_t);
void                 setNewRomBanks(uint8_t *, uint8_t *);

void                 registerAddressCallback(uint16_t, EAddressCallbackType_t, addressWriteCallback);

const uint8_t        fetch8(uint16_t);
const uint16_t       fetch16(uint16_t);

void                 write8(uint8_t, uint16_t);
void                 write16(uint16_t, uint16_t);

SAddressBus_t *const pGetAddressBus(void);

#endif // !_MEM_H_