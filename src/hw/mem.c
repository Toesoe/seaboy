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

static bus_t addressBus;

void resetBus(void)
{
    memset(&addressBus, 0, sizeof(addressBus));
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
    addressBus.bus[addr] = val;
}

void write16(uint16_t val, uint16_t addr)
{
    addressBus.bus[addr]     = (uint8_t)(val & 0xFF);
    addressBus.bus[addr + 1] = (uint8_t)(val >> 8);
}

bus_t *pGetBusPtr(void)
{
    return &addressBus;
}