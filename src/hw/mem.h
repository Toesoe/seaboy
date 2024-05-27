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

#include <stdint.h>

#define GB_BUS_SIZE     0xFFFF

#define ROMN_SIZE       0x3FFF
#define VRAM_SIZE       0x1FFF
#define ERAM_SIZE       0x1FFF
#define WRAM_SIZE       0x1FFF
#define ECHO_SIZE       0x1DFF
#define OAM_SIZE        0x9F
#define IO_SIZE         0x7F
#define HRAM_SIZE       0x7E

// VRAM related
#define TILEBLOCK_SIZE  0x800
#define TILEMAP_SIZE    0x400

typedef struct
{
    uint8_t aRight : 1;
    uint8_t bLeft : 1;
    uint8_t selectUp : 1;
    uint8_t startDown : 1;
    uint8_t dpadSelect : 1; // if 0, input can be read from first 4 bits
    uint8_t buttonSelect : 1; // if 0, first 4 bits are a/b/sel/start instead
    uint8_t _none : 2;
} sRegJOYP_t;

typedef struct
{
    uint8_t clockSelect : 1;
    uint8_t clockSpeed : 1;
    uint8_t _none : 5;
    uint8_t transferEnable : 1;
} sRegSC_t;

/** @brief timer control register */
typedef struct
{
    uint8_t clockSelect : 2;
    uint8_t enable : 1;
    uint8_t _none : 5;
} sRegTAC_t;

typedef struct
{
    uint8_t TIMA;
    uint8_t TMA;
    sRegTAC_t TAC;
} STimers_t;

typedef struct
{
    uint8_t bgWindowEnablePrio : 1;
    uint8_t objEnable : 1;
    uint8_t objSize : 1;
    uint8_t bgTilemap : 1;
    uint8_t bgWindowTileData : 1;
    uint8_t bgWindowEnable : 1;
    uint8_t bgWindowTileMap : 1;
    uint8_t lcdPPUEnable : 1;
} SRegLCDC_t;

typedef struct
{
    uint8_t ppuMode : 2;
    uint8_t lycEqLy : 1;
    uint8_t mode0IntSel : 1;
    uint8_t mode1IntSel : 1;
    uint8_t mode2IntSel : 1;
    uint8_t LYCIntSel : 1;
    uint8_t _none : 1;
} SRegLCDStat_t;

typedef struct
{
    uint8_t id0 : 2;
    uint8_t id1 : 2;
    uint8_t id2 : 2;
    uint8_t id3 : 2;
} SRegPaletteData_t;

typedef union __attribute__((__packed__))
{
    uint8_t bus[GB_BUS_SIZE];

    struct __attribute__((__packed__))
    {
        uint8_t rom0[ROMN_SIZE]; // 0x0000 -> 0x3FFF
        uint8_t romn[ROMN_SIZE]; // 0x4000 -> 0x7FFF
        union __attribute__((__packed__))
        {
            uint8_t all[VRAM_SIZE];             // 0x8000 -> 0x9FFF
            struct __attribute__((__packed__))
            {
                uint8_t block0[TILEBLOCK_SIZE]; // 0x8000 -> 0x87FF
                uint8_t block1[TILEBLOCK_SIZE]; // 0x8800 -> 0x8FFF
                uint8_t block2[TILEBLOCK_SIZE]; // 0x9000 -> 0x97FF
                uint8_t map0[TILEMAP_SIZE];     // 0x9800 -> 0x9BFF
                uint8_t map1[TILEMAP_SIZE];     // 0x9C00 -> 0x9FFF
            } tiles;
        } vram;
        uint8_t eram[ERAM_SIZE];                // 0xA000 -> 0xBFFF
        union __attribute__((__packed__))
        {
            uint8_t all[WRAM_SIZE];             // 0xC000 -> 0xCFFF
            struct __attribute__((__packed__))
            {
                uint8_t bank0[WRAM_SIZE / 2];   // 0xC000 -> 0xC7FF
                uint8_t bankn[WRAM_SIZE / 2];   // 0xC800 -> 0xCFFF
            } banks;
        } wram;
        uint8_t echo[ECHO_SIZE];                // 0xE000 -> 0xFDFF
        uint8_t oam[OAM_SIZE];                  // 0xFE00 -> 0xFE9F
        uint8_t _NOUSE[0x5F];                   // 0xFEA0 -> 0xFEFF
        union __attribute__((__packed__))
        {
            uint8_t all[IO_SIZE];               // 0xFF00 -> 0xFF7F
            struct __attribute__((__packed__))
            {
                sRegJOYP_t joypad;
                uint8_t serData;                // 0xFF01
                sRegSC_t serControl;                  // 0xFF02
                uint8_t divRegister;            // 0xFF04
                STimers_t timers;               // 0xFF05 -> 0xFF07
                uint8_t intFlag;                // 0xFF0F
                uint8_t audio[22];              // 0xFF10 -> 0xFF26 TODO: implement audio
                uint8_t wavepattern[0xF];       // 0xFF30 -> 0xFF3F TODO: implement
                union __attribute__((__packed__))
                {
                    uint8_t all[0xB];           // 0xFF40 -> 0xFF4B
                    struct __attribute__((__packed__))
                    {
                        SRegLCDC_t lcdc;        // 0xFF40
                        SRegLCDStat_t lcdStat;  // 0xFF41
                        uint8_t scy;            // 0xFF42
                        uint8_t scx;            // 0xFF43
                        uint8_t ly;             // 0xFF44
                        uint8_t lyc;            // 0xFF45
                        uint8_t dma;            // 0xFF46
                        SRegPaletteData_t bgp;  // 0xFF47
                        SRegPaletteData_t obp0; // 0xFF48
                        SRegPaletteData_t obp1; // 0xFF49
                        uint8_t wy;             // 0xFF4A
                        uint8_t wx;             // 0xFF4B
                    } regs;
                } lcdcontrol;
                uint8_t vramBankSelect;         // 0xFF4F: CGB only
                uint8_t disableBootrom;         // 0xFF50
                // TODO: extend with CGB DMA stuff
                uint8_t _unusedForNow[0x2F];    // 0xFF51 -> 0xFF7F
            } regs;
        } ioreg;
        uint8_t hram[HRAM_SIZE];                // 0xFF80 -> 0xFFFE
        uint8_t iereg;                          // 0xFFFF
    } mem;
} bus_t;

void resetBus(void);

uint8_t  fetch8(uint16_t);
uint16_t fetch16(uint16_t);

void write8(uint8_t, uint16_t);
void write16(uint16_t, uint16_t);

bus_t *pGetBusPtr(void);

#endif // !_MEM_H_