/**
 * @file render.c
 * @author Toesoe
 * @brief seaboy render driver
 * @version 0.1
 * @date 2023-06-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "render.h"

static ETilePalette_t framebuffer[DISP_HEIGHT][DISP_WIDTH] = { 0 };

/**
 * @brief set a pixel value at a specific screen x/y location
 * 
 * @param pPixel pointer to pixel value
 */
void writeFifoToFramebuffer(SFIFO_t *pFifo, uint8_t xStart, uint8_t y)
{
    memcpy(&framebuffer[y][xStart], pFifo->pixels, 8);
}

void debugFramebuffer(void)
{
    for (size_t y = 0; y < DISP_HEIGHT; y++)
    {
        for (size_t x = 0; x < DISP_WIDTH; x++)
        {
            printf("%c", framebuffer[y][x] == 3 ? '.' : ' ');
        }
        printf("\n");
    }
}

// TODO: implement SDL for rendering games