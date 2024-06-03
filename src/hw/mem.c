/**
 * @file mem.h
 * @author Toesoe
 * @brief seaboy bus (ram, vram)
 * @version 0.1
 * @date 2023-06-15
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "mem.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

static bus_t addressBus;
static uint8_t *pRom;
static size_t romSize;

static bool cartramEnabled = false;
static bool advancedBankingMode = false;
static int ramBankNo = 0;
static uint8_t cartRam[32768];

#define DEBUG_WRITES

static void bankSwitch(uint8_t, uint16_t);

/**
 * map rom into memmap. copies incoming ptr
*/
void mapRomIntoMem(uint8_t **ppRomLoaded, size_t len)
{
    pRom = *ppRomLoaded;
    romSize = len;

    // map loaded rom to bus
    memcpy(&addressBus.map.rom0, pRom, ROMN_SIZE);
    memcpy(&addressBus.map.romn, pRom + ROMN_SIZE, ROMN_SIZE);
}

void unmapBootrom(void)
{
    memcpy(&addressBus.map.rom0, pRom, 0x100);
    addressBus.map.ioregs.disableBootrom = 0;
}

void resetBus(void)
{
    memset(&addressBus, 0, sizeof(addressBus));
    memset(&addressBus.map.ioregs.joypad, 0xFF, 1);
}

uint8_t fetch8(uint16_t addr)
{
    return addressBus.bus[addr];
}
uint16_t fetch16(uint16_t addr)
{
    return (uint16_t)(addressBus.bus[addr + 1] << 8) | addressBus.bus[addr];
}

void write8(uint8_t val, uint16_t addr)
{
    if (addr < (ROMN_SIZE * 2))
    {
        // we're writing to cart. handle bank switching stuff
        bankSwitch(val, addr);
    }
    else if (addr >= (ROMN_SIZE * 2) && addr < ((ROMN_SIZE * 2) + VRAM_SIZE))
    {
        #ifdef DEBUG_WRITES
        printf("VRAM WRITE %02X at %04X\n", val, addr);
        #endif
    }
    else if ((addr >= 0xA000) && (addr <= ERAM_SIZE) && cartramEnabled)
    {
        #ifdef DEBUG_WRITES
        printf("CARTRAM WRITE %02X at %04X bank %d\n", val, addr, ramBankNo);
        #endif
        cartRam[addr + (ramBankNo * ERAM_SIZE)] = val;
    }
    addressBus.bus[addr] = val;
}

void write16(uint16_t val, uint16_t addr)
{
    if (addr < ROMN_SIZE)
    {
        __asm("nop");
    }
    addressBus.bus[addr]     = (uint8_t)(val & 0xFF);
    addressBus.bus[addr + 1] = (uint8_t)(val >> 8);
}

bus_t *pGetBusPtr(void)
{
    return &addressBus;
}

static void bankSwitch(uint8_t val, uint16_t addr)
{
    if ((addr > 0x0000) && (addr < 0x2000))
    {
        // RAM enable
        if ((val & 0xF) == 0xA)
        {
            printf("enabling cartram\n");
            cartramEnabled = true;
        }
        else
        {
            printf("disabling cartram\n");
            cartramEnabled = false;
        }
    }
    else if ((addr >= 0x2000) && (addr < ROMN_SIZE))
    {
        // switch bank. 0x1 -> 0x1F
        val &= 0x1F;
        printf("switching rombank to %d\n", val);
        memcpy(&addressBus.map.romn, pRom + (val * ROMN_SIZE), ROMN_SIZE);
    }
    else if ((addr >= ROMN_SIZE) && (addr < 0x6000))
    {
        // ram bank no
        printf("switching rambank to %d\n", val);
        ramBankNo = val;
    }
    else if ((addr >= 0x6000) && (addr <= 0x8000))
    {
        // banking mode select
        if (val & 0x01)
        {
            // advanced banking
            printf("enabling advanced banking mode");
            advancedBankingMode = true;
        }
        else
        {
            printf("disabling advanced banking mode");
            advancedBankingMode = false;
        }
    }
}