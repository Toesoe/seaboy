/**
 * @file ppu.h
 * @author Toesoe
 * @brief seaboy PPU routines
 * @version 0.1
 * @date 2024-03-15
 *
 * @copyright Copyright (c) 2024
 *
 */

#ifndef _PPU_H_
#define _PPU_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define PPU_FIFO_SIZE       8

typedef enum
{
    PIXEL_COLOR_NONE = -1,
    PIXEL_COLOR_BLACK_TRANSPARENT = 0, // 00; transparent when used in objects
    PIXEL_COLOR_LGRAY = 1, // 01
    PIXEL_COLOR_DGRAY = 2, // 10
    PIXEL_COLOR_WHITE = 3  // 11
} EPixelColor_t;

typedef struct
{
    EPixelColor_t color;
    bool          objToBGPrioBit;
} SPPUPixel_t;

typedef struct
{
    size_t         len;
    SPPUPixel_t    _pixels[PPU_FIFO_SIZE];
} SFIFO_t;

void buildTiles(uint32_t);

void ppuInit(bool);
bool ppuLoop(int);

#endif //!_PPU_H_