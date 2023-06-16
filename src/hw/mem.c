/**
 * @file mem.h
 * @author Toesoe
 * @brief seaboy mem (ram, vram)
 * @version 0.1
 * @date 2023-06-15
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "mem.h"

#include <stdint.h>

static uint8_t wram[GB_WRAM_SIZE_B];
static uint8_t vram[GB_VRAM_SIZE_B];

uint8_t fetch8(uint16_t addr)
{
    return wram[addr];
}
uint16_t fetch16(uint16_t addr)
{
    return (uint16_t)(wram[addr] << 8 | wram[addr + 1]);
}

void write8(uint8_t val, uint16_t addr)
{
    wram[addr] = val;
}

void write16(uint16_t val, uint16_t addr)
{
    wram[addr]     = (uint8_t)(val & 0xFF);
    wram[addr + 1] = (uint8_t)(val >> 8);
}