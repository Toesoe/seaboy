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

#include <stdint.h>

typedef enum {
    BLACK = 0, // 00; transparent when used in objects
    LGRAY = 1, // 01
    DGRAY = 2, // 10
    WHITE = 3  // 11
} ETilePalette_t;

void buildTile(uint32_t);

#endif //!_PPU_H_