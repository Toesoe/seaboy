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

typedef enum
{
    BLACK = 0, // 00; transparent when used in objects
    LGRAY = 1, // 01
    DGRAY = 2, // 10
    WHITE = 3  // 11
} ETilePalette_t;

typedef enum
{
    MODE_0, // Hblank
    MODE_1, // Vblank
    MODE_2, // OAM
    MODE_3  // drawing
} EPPUMode_t;

typedef struct
{
    size_t         len;
    ETilePalette_t pixels[8];
    size_t         discardLeft;
} SFIFO_t;

void buildTiles(uint32_t);

void ppuInit(bool);
bool ppuLoop(int);

#endif //!_PPU_H_