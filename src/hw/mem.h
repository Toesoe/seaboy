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

typedef union __attribute__((__packed__))
{
    uint8_t bus[GB_BUS_SIZE];

    struct __attribute__((__packed__))
    {
        uint8_t rom0[ROMN_SIZE];
        uint8_t romn[ROMN_SIZE];
        uint8_t vram[VRAM_SIZE];
        union __attribute__((__packed__))
        {
            uint8_t all[WRAM_SIZE];
            struct __attribute__((__packed__))
            {
                uint8_t bank0[WRAM_SIZE / 2];
                uint8_t bankn[WRAM_SIZE / 2];
            } banks;
        } wram;
        uint8_t echo[ECHO_SIZE];
        uint8_t oam[OAM_SIZE];
        uint8_t _NOUSE[0x5F];
        uint8_t ioreg[IO_SIZE];
        uint8_t hram[HRAM_SIZE];
    } mem;
} bus_t;

uint8_t  fetch8(uint16_t addr);
uint16_t fetch16(uint16_t addr);

void write8(uint8_t val, uint16_t addr);
void write16(uint16_t val, uint16_t addr);

#endif // !_MEM_H_