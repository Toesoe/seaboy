/**
 * @file render.h
 * @author Toesoe
 * @brief seaboy display renderer
 * @version 0.1
 * @date 2024-03-15
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef _RENDER_H_
#define _RENDER_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../hw/ppu.h"

#define DISP_WIDTH  160
#define DISP_HEIGHT 144

typedef struct {
    size_t x;
    size_t y;
    ETilePalette_t color;
} SPixel_t;

void writeFifoToFramebuffer(SFIFO_t *, uint8_t, uint8_t);
void setPixel(SPixel_t *);

void debugFramebuffer(void);

void renderWindow(void);

#endif //!_RENDER_H_