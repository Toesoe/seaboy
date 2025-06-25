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

#include <stdio.h>
#include <string.h>

#include "../drv/render.h"
#include "mem.h"
#include "ppu.h"

#define LCD_VIEWPORT_X      160
#define LCD_VIEWPORT_Y      144
#define LCD_VBLANK_LEN      10 // 10 lines

#define TILE_DIM_X          8
#define TILE_DIM_Y          8

#define BG_TILES_X          32
#define BG_TILES_Y          32

#define WINDOW_TILES_X      32
#define WINDOW_TILES_Y      32

#define CHECK_BIT(var, pos) ((var) & (1 << (pos)))

#define TILE_SIZE_BYTES     16
#define PIXEL_FIFO_SIZE     8
#define OAM_MAX_SPRITES     10

#define CYCLES_PER_FRAME    70224 // 154 scanlines * 456 cycle

typedef struct
{
    SPixel_t pixels[TILE_DIM_X][TILE_DIM_Y];
} STile_t;

typedef struct
{
    EPPUMode_t mode;
    int        cycleCount;
    int        currentLineCycleCount;
    uint8_t    column;
    uint8_t    row;
    SFIFO_t    pixelFifo;
} SPPUState_t;

static SPPUState_t g_currentPPUState;
static bus_t      *g_pMemoryBus = NULL;

/**
 * @brief builds current FIFO buffer, 8 pixels
 *
 * @param lx current X position
 */
static void fillPixelFifo(uint8_t lx)
{
    bool    isWindow         = false;
    uint8_t adjustedScanline = (g_pMemoryBus->map.ioregs.lcd.ly + g_pMemoryBus->map.ioregs.lcd.scy) % 256;

    if (g_pMemoryBus->map.ioregs.lcd.control.bgWindowEnable)
    {
        uint8_t windowEnd =
        g_pMemoryBus->map.ioregs.lcd.wy + ((g_pMemoryBus->map.ioregs.lcd.control.bgWindowTileMap ? 4 : 3) << 3);
        if ((g_pMemoryBus->map.ioregs.lcd.ly >= g_pMemoryBus->map.ioregs.lcd.wy) &&
            g_pMemoryBus->map.ioregs.lcd.ly < windowEnd)
        {
            isWindow = true;
        }
    }

    uint16_t tileMapBase = ((g_pMemoryBus->map.ioregs.lcd.control.bgTilemap && !isWindow) ||
                            (g_pMemoryBus->map.ioregs.lcd.control.bgWindowTileMap && isWindow)) ?
                           0x9C00 :
                           0x9800;
    uint16_t tileAddr    = tileMapBase;

    if (!isWindow)
    {
        uint8_t row = (adjustedScanline / 8);
        uint8_t col = ((lx + g_pMemoryBus->map.ioregs.lcd.scx) / 8);
        tileAddr += (row * 32) + col;
        // printf("BG Mode - TileMapBase: %04X, Row: %d, Col: %d, TileAddr: %04X\n", tileMapBase, row, col, tileAddr);
    }
    else
    {
        uint8_t row = ((g_pMemoryBus->map.ioregs.lcd.ly - g_pMemoryBus->map.ioregs.lcd.wy) / 8);
        uint8_t col = (lx / 8);
        tileAddr += (row * 32) + col;
        // printf("Window Mode - TileMapBase: %04X, Row: %d, Col: %d, TileAddr: %04X\n", tileMapBase, row, col,
        // tileAddr);
    }

    uint8_t tileId = fetch8(tileAddr);

    // Debug: Print the fetched tile ID
    // printf("Tile ID at (lx=%d, ly=%d): %02X\n", lx, g_pMemoryBus->map.ioregs.lcd.ly, tileId);

    uint16_t tileRowAddr = g_pMemoryBus->map.ioregs.lcd.control.bgWindowTileData ? 0x8000 : 0x8800;
    tileRowAddr += tileId * 16; // Each tile occupies 16 bytes (8x8 pixels)

    uint8_t lsb = fetch8(tileRowAddr + (adjustedScanline % 8) * 2);
    uint8_t msb = fetch8(tileRowAddr + (adjustedScanline % 8) * 2 + 1);

    // Debug: Print the fetched tile data
    // printf("Tile data at (addr=%04X): LSB=%02X, MSB=%02X\n", tileRowAddr + (adjustedScanline % 8) * 2, lsb, msb);

    for (size_t i = 0; i < PIXEL_FIFO_SIZE; i++)
    {
        uint8_t pixel = 0;
        pixel |= ((msb & (1 << (7 - i))) ? 2 : 0); // Combine MSB
        pixel |= ((lsb & (1 << (7 - i))) ? 1 : 0); // Combine LSB
        g_currentPPUState.pixelFifo.pixels[i] = pixel;
    }

    g_currentPPUState.pixelFifo.len = PIXEL_FIFO_SIZE;
}

void ppuInit(bool skipBootrom)
{
    g_pMemoryBus = pGetBusPtr();

    if (!skipBootrom)
    {
        g_currentPPUState.mode = MODE_2;
    }
    else
    {
        g_currentPPUState.mode = MODE_1;
    }

    g_pMemoryBus->map.ioregs.lcd.control.lcdPPUEnable = 0;

    memset(&g_currentPPUState, 0, sizeof(g_currentPPUState));
}

/**
 * @brief main PPU loop
 *
 * @param cyclesToRun amount of PPU cycles to run this loop
 * @return true if frame has been completed
 */
bool ppuLoop(int cyclesToRun)
{
    bool frameEnd = false;

    while (g_pMemoryBus->map.ioregs.lcd.control.lcdPPUEnable && (cyclesToRun > 0))
    {
        // Mode 2 -> Mode 3  -> Mode 0         -> Mode 1
        // 80     -> 172/289 -> 376 - (Mode 3) -> 4560
        switch (g_currentPPUState.mode)
        {
            case MODE_2:
            {
                // OAM scan
                // search for OBJs which overlap line
                // TODO: implement

                g_currentPPUState.currentLineCycleCount++;
                if (g_currentPPUState.currentLineCycleCount == 80)
                {
                    g_currentPPUState.mode = MODE_3;
                }
                break;
            }
            case MODE_3:
            {
                if (g_currentPPUState.column < 160)
                {
                    SPixel_t pixel = {
                        g_currentPPUState.column, g_pMemoryBus->map.ioregs.lcd.ly,
                        g_currentPPUState.pixelFifo.pixels[PIXEL_FIFO_SIZE - g_currentPPUState.pixelFifo.len]
                    };
                    setPixel(&pixel);

                    g_currentPPUState.pixelFifo.len--;

                    g_currentPPUState.column++;
                    g_currentPPUState.currentLineCycleCount++;
                }
                else
                {
                    g_currentPPUState.mode = MODE_0;
                }
                break;
            }
            case MODE_0: // Hblank for remaining cycles until we hit 456
            {
                g_currentPPUState.currentLineCycleCount++;

                if (g_currentPPUState.currentLineCycleCount == 456)
                {
                    // line is done. reset counters
                    g_currentPPUState.column = 0;
                    g_pMemoryBus->map.ioregs.lcd.ly++;
                    g_currentPPUState.currentLineCycleCount = 0;

                    if (g_pMemoryBus->map.ioregs.lcd.ly == LCD_VIEWPORT_Y)
                    {
                        // go to Vblank if we processed the last line
                        g_currentPPUState.mode                   = MODE_1;
                        g_pMemoryBus->map.ioregs.intFlags.vblank = 1;
                    }
                    else
                    {
                        // if not, go back to OAM scan
                        g_currentPPUState.mode = MODE_2;
                    }
                }
                break;
            }
            case MODE_1: // Vblank for the remaining lines
            {
                if (g_currentPPUState.cycleCount < CYCLES_PER_FRAME)
                {
                    if (g_currentPPUState.currentLineCycleCount == 456)
                    {
                        // end of line, increase ly
                        g_pMemoryBus->map.ioregs.lcd.ly++;
                        g_currentPPUState.currentLineCycleCount = 0;
                    }
                    g_currentPPUState.currentLineCycleCount++;
                }
                else
                {
                    frameEnd = true;
                    debugFramebuffer();
                    g_currentPPUState.mode                   = MODE_2;
                    g_currentPPUState.currentLineCycleCount  = 0;
                    g_pMemoryBus->map.ioregs.lcd.ly          = 0;
                    g_currentPPUState.cycleCount             = 0;
                    g_pMemoryBus->map.ioregs.intFlags.vblank = 0;
                }

                break;
            }
        }

        if (g_currentPPUState.pixelFifo.len == 0)
        {
            fillPixelFifo(g_currentPPUState.column);
        }

        cyclesToRun--;
        g_currentPPUState.cycleCount++;
    }

    g_pMemoryBus->map.ioregs.lcd.stat.ppuMode = g_currentPPUState.mode;
    return frameEnd;
}