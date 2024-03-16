/**
 * @file ppu.c
 * @author Toesoe
 * @brief seaboy PPU emulation
 * @version 0.1
 * @date 2023-06-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "ppu.h"
#include "mem.h"
#include "../drv/render.h"

#define TILE_SIZE_BYTES 16

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

void buildTile(uint32_t addr)
{
    // a tile consists of 8x8 pixels, but is stored in 16 bytes
    // first byte is LSB, second MSB. e.g. addr == 0x3C (00111100b), addr+1 == 0x7E (01111110b):
    // result is 00 10 11 11 11 11 10 00

    SPixel_t tilePixel = {0};

    for (size_t i = 0; i < TILE_SIZE_BYTES / 2; i++)
    {
        uint8_t msb = fetch8(addr + 1);
        uint8_t lsb = fetch8(addr);

        for (size_t j = 0; j < 8; j++)
        {
            // combine into tile
            tilePixel.x = i;
            tilePixel.y = j;
            tilePixel.color = (CHECK_BIT(msb, j) << 1) | CHECK_BIT(lsb, j);
            setPixel(&tilePixel);
        }
        addr += 2;
    }
}