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

#include "render.h"

static ETilePalette_t framebuffer[DISP_WIDTH][DISP_HEIGHT] = { 0 };

void setPixel(SPixel_t *pPixel)
{
    framebuffer[pPixel->x][pPixel->y] = pPixel->color;
}

void debugFramebuffer(void)
{
    for (size_t y = 0; y < DISP_HEIGHT; y++)
    {
        for (size_t x = 0; x < DISP_WIDTH; x++)
        {
            printf("%d ", framebuffer[x][y]);
        }
        printf("\n");
    }
}

// TODO: implement SDL for rendering games