/**
 * @file cart.h
 * @author Toesoe
 * @brief seaboy cart routines
 * @version 0.1
 * @date 2024-03-15
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef _CART_H_
#define _CART_H_

#include <stdint.h>
#include <stdlib.h>

#define MAX_CARTRAM_SIZE_BYTES (131072)

#define ROM0_END              (ROMN_SIZE - 1)
#define ROM1_END              ((2 * ROMN_SIZE) - 1)
#define ERAM_START            (0xA000)
#define ERAM_END              (0xBFFF)

typedef enum
{
    MAPPER_NONE = -1,
    MBC1,
    MMM01,
    MBC2,
    MBC3,
    MBC4,
    MBC5,
    // ...
} EMapperType_t;

typedef struct
{
    const uint8_t *pRom;    // pointer to full ROM data
    size_t         romSize; // length of mapped ROM

    EMapperType_t  mapperType;
    bool           mapperHasRam;
    bool           mapperHasBattery;
    bool           mapperHasRTC;  // >=MBC3
    bool           mapperHasRumble; // MBC5
    bool           mapperHasSensor; // MBC7
    bool           cartramEnabled;
    bool           advancedBankingModeEnabled;

    size_t         selectedRomBankNum;
    const uint8_t *pCurrentRomBank0; // pointer to start of currently selected ROM bank
    const uint8_t *pCurrentRomBank1;

    uint8_t       *pCartRam;
    size_t         cartRamSize;
    size_t         selectedRamBankNum;
    uint8_t       *pCurrentRamBank;
} SCartridge_t;

const SCartridge_t *pLoadRom(const char *);

void                performBankSwitch(uint16_t, uint16_t);

void                writeCartRam8(uint8_t, uint16_t);
void                writeCartRam16(uint16_t, uint16_t);

uint8_t             readCartRam8(uint16_t);
uint16_t            readCartRam16(uint16_t);

#endif //!_CART_H_