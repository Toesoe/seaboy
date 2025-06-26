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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bus_t    addressBus;
static uint8_t *pRom;
static size_t   romSize;

#define DEBUG_WRITES
#define TEST

#ifndef TEST
static void    bankSwitch(uint8_t, uint16_t);
static uint8_t cartRam[32768];
static bool    cartramEnabled      = false;
static bool    advancedBankingMode = false;
static int     ramBankNo           = 0;
#endif

/**
 * map rom into memmap. copies incoming ptr
 */
void mapRomIntoMem(uint8_t **ppRomLoaded, size_t len)
{
    pRom    = *ppRomLoaded;
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
    memset(&addressBus, 0x00, sizeof(addressBus));
    memset(&addressBus.map.hram, 0xFF, HRAM_SIZE);
    memset(&addressBus.map.wram, 0xFF, WRAM_SIZE);
    memset(&addressBus.map.eram, 0xFF, ERAM_SIZE);
    memset(&addressBus.map.echo, 0xFF, ECHO_SIZE);
    memset(&addressBus.map.vram, 0xFF, VRAM_SIZE);
    memset(&addressBus.map.oam, 0xFF, OAM_SIZE);
    memset(&addressBus.map.ioregs.joypad, 0xFF, 1);
}

void overrideBus(bus_t *pBus)
{
    memcpy(&addressBus, pBus, sizeof(bus_t));
}

uint8_t fetch8(uint16_t addr)
{
    if (addr == 0xFF00) return 0xFF; // NOTE: don't forget to remove!
    return addressBus.bus[addr];
}

uint16_t fetch16(uint16_t addr)
{
    return (uint16_t)(addressBus.bus[addr + 1] << 8) | addressBus.bus[addr];
}

/**
 * @brief write 8-bit value to address
 *
 * @param val value to write
 * @param addr address to write to
 */
void write8(uint8_t val, uint16_t addr)
{
#ifndef TEST
    if (addr < (ROMN_SIZE * 2))
    {
        // we're writing to cart. handle bank switching stuff
        bankSwitch(val, addr);
    }
    else if (addr >= (ROMN_SIZE * 2) && addr < ((ROMN_SIZE * 2) + VRAM_SIZE))
    {
        if (addressBus.map.ioregs.lcd.stat.ppuMode == 3)
        {
            return;
        }
#ifdef DEBUG_WRITES
        printf("VRAM WRITE %02X at %04X\n", val, addr);
#endif
    }
    else if ((addr >= 0xA000) && (addr <= ERAM_SIZE)) //&& cartramEnabled)
    {
#ifdef DEBUG_WRITES
        printf("CARTRAM WRITE %02X at %04X bank %d\n", val, addr, ramBankNo);
#endif
        cartRam[addr + (ramBankNo * ERAM_SIZE)] = val;
    }
    else if (addr == 0xFF46) // OAM DMA
    {
        memcpy(&addressBus.bus[0xFE00], &addressBus.bus[(uint16_t)val * 256], 0x9F);
        // costs 160 mcycles
    }
#endif
    if (addr < ROMN_SIZE * 2)
    {
        return;
    }
    addressBus.bus[addr] = val;
}

/**
 * @brief write 16-bit val to addr
 *
 * @param val value to write
 * @param addr address to write to
 */
void write16(uint16_t val, uint16_t addr)
{
    if (addr < ROMN_SIZE * 2)
    {
        return;
    }
    addressBus.bus[addr]     = (uint8_t)(val & 0xFF);
    addressBus.bus[addr + 1] = (uint8_t)(val >> 8);
}

bus_t *pGetBusPtr(void)
{
    return &addressBus;
}

#ifndef TEST
static void bankSwitch(uint8_t val, uint16_t addr)
{
    if ((addr > 0x0000) && (addr < 0x2000))
    {
        // RAM enable
        if ((val & 0xF) == 0xA)
        {
            // printf("enabling cartram\n");
            cartramEnabled = true;
        }
        else
        {
            // printf("disabling cartram\n");
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
#endif