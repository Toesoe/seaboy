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

#include "SDL2/SDL.h"

#include "render.h"

ETilePalette_t example_data[DISP_HEIGHT][DISP_WIDTH] = {
    { BLACK, WHITE, LGRAY, DGRAY, BLACK, WHITE, LGRAY, DGRAY },
    { WHITE, LGRAY, DGRAY, BLACK, WHITE, LGRAY, DGRAY, BLACK },
    { LGRAY, DGRAY, BLACK, WHITE, LGRAY, DGRAY, BLACK, WHITE },
    { DGRAY, BLACK, WHITE, LGRAY, DGRAY, BLACK, WHITE, LGRAY },
    { BLACK, WHITE, LGRAY, DGRAY, BLACK, WHITE, LGRAY, DGRAY },
    { WHITE, LGRAY, DGRAY, BLACK, WHITE, LGRAY, DGRAY, BLACK },
    { LGRAY, DGRAY, BLACK, WHITE, LGRAY, DGRAY, BLACK, WHITE },
    { DGRAY, BLACK, WHITE, LGRAY, DGRAY, BLACK, WHITE, LGRAY }
};

static ETilePalette_t framebuffer[DISP_HEIGHT][DISP_WIDTH] = { 0 };
static uint32_t       pixelbuffer[DISP_WIDTH * DISP_HEIGHT];

static SDL_Window    *g_pRenderWindow = NULL;
static SDL_Renderer  *g_pRenderer     = NULL;
static SDL_Texture   *g_pFbTexture    = NULL;

static uint32_t       map_palette_to_rgba(ETilePalette_t color)
{
    switch (color)
    {
        case BLACK:
            return 0x0f380fFF; // Black
        case LGRAY:
            return 0x8bac0fFF; // Light Gray
        case DGRAY:
            return 0x306230FF; // Dark Gray
        case WHITE:
            return 0x9bbc0fFF; // White
        default:
            return 0x9bbc0fFF; // Default to black
    }
}

/**
 * @brief set a pixel value at a specific screen x/y location
 *
 * @param pPixel pointer to pixel value
 */
void writeFifoToFramebuffer(SFIFO_t *pFifo, uint8_t xStart, uint8_t y)
{
    // Ensure bounds checking
    if (xStart + 8 > DISP_WIDTH || y >= DISP_HEIGHT) return;

    // Copy pixel data from FIFO to framebuffer
    memcpy(&framebuffer[y][xStart], &pFifo->pixels[pFifo->discardLeft], 8 - pFifo->discardLeft);
}

void setPixel(SPixel_t *pPixel)
{
    // Ensure bounds checking
    if (pPixel->x >= DISP_WIDTH || pPixel->y >= DISP_HEIGHT) return;

    // Set pixel value in framebuffer
    framebuffer[pPixel->y][pPixel->x] = pPixel->color;
}

void debugFramebuffer(void)
{
    // Update pixelbuffer from framebuffer
    for (int y = 0; y < DISP_HEIGHT; y++)
    {
        for (int x = 0; x < DISP_WIDTH; x++)
        {
            pixelbuffer[y * DISP_WIDTH + x] = map_palette_to_rgba(framebuffer[y][x]);
        }
    }

    // Update texture with pixelbuffer data
    if (SDL_UpdateTexture(g_pFbTexture, NULL, pixelbuffer, DISP_WIDTH * sizeof(uint32_t)) != 0)
    {
        // Error handling
        fprintf(stderr, "Texture SDL_Error: %s\n", SDL_GetError());
    }

    // Render texture onto window
    SDL_RenderCopy(g_pRenderer, g_pFbTexture, NULL, NULL);
    SDL_RenderPresent(g_pRenderer);
}

void initRenderWindow(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        // Error handling
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    g_pRenderWindow = SDL_CreateWindow("Framebuffer Example", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                       DISP_WIDTH * 6, DISP_HEIGHT * 6, SDL_WINDOW_SHOWN);

    if (!g_pRenderWindow)
    {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        exit(EXIT_FAILURE);
    }

    g_pRenderer = SDL_CreateRenderer(g_pRenderWindow, -1, SDL_RENDERER_ACCELERATED);

    if (!g_pRenderer)
    {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_pRenderWindow);
        SDL_Quit();
        exit(EXIT_FAILURE);
    }

    SDL_RenderSetLogicalSize(g_pRenderer, DISP_WIDTH, DISP_HEIGHT);

    g_pFbTexture = SDL_CreateTexture(g_pRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);

    if (!g_pFbTexture)
    {
        fprintf(stderr, "Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(g_pRenderer);
        SDL_DestroyWindow(g_pRenderWindow);
        SDL_Quit();
        exit(EXIT_FAILURE);
    }

    memset(pixelbuffer, 0xFFFFFFFF, DISP_WIDTH * DISP_HEIGHT * sizeof(uint32_t));
}