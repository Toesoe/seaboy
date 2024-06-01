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

#include <string.h>

#include "ppu.h"
#include "mem.h"
#include "../drv/render.h"

#define LCD_VIEWPORT_X 160
#define LCD_VIEWPORT_Y 144
#define LCD_VBLANK_LEN 10 // 10 lines

#define TILE_DIM_X 8
#define TILE_DIM_Y 8

#define BG_TILES_X 32
#define BG_TILES_Y 32

#define WINDOW_TILES_X 32
#define WINDOW_TILES_Y 32

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

#define TILE_SIZE_BYTES 16
#define PIXEL_FIFO_SIZE 8
#define OAM_MAX_SPRITES 10

#define CYCLES_PER_FRAME 70224 // 154 scanlines * 456 cycle

typedef enum
{
    MODE_0, // Hblank
    MODE_1, // Vblank
    MODE_2, // OAM
    MODE_3  // drawing
} EPPUMode_t;

typedef struct
{
    SPixel_t pixels[TILE_DIM_X][TILE_DIM_Y];
} STile_t;

typedef struct
{
    EPPUMode_t mode;
    int cycleCount;
    int mode3CyclesElapsed;
    uint8_t column;
    uint8_t row;
    SFIFO_t pixelFifo;
} SPPUState_t;

static SPPUState_t g_currentPPUState;
static bus_t *g_pMemoryBus = NULL;

/**
 * @brief builds current FIFO buffer, 8 pixels
 * 
 * @param lx current X position
 * @param tileLine "window Y"
 */
static void fillPixelFifo(uint8_t lx, uint8_t tileLine)
{
    (void)tileLine;
    // a tile consists of 8x8 pixels, but is stored in 16 bytes
    // first byte is LSB, second MSB. e.g. addr == 0x3C (00111100b), addr+1 == 0x7E (01111110b):
    // result is 00 10 11 11 11 11 10 00

    uint16_t tileAddr = 0b1001100000000000;
    uint16_t tileRow = 0b1000000000000000;

    // bg mode
    tileAddr = (tileAddr & ~(1 << 10)) | (g_pMemoryBus->map.ioregs.lcd.control.bgTilemap << 10);

    uint8_t tmp = (g_pMemoryBus->map.ioregs.lcd.ly + g_pMemoryBus->map.ioregs.lcd.scy) / 8;
    tileAddr = (tileAddr & ~(0b11111 << 5)) | (tmp << 5);

    tmp = (lx + g_pMemoryBus->map.ioregs.lcd.scx) / 8;
    tileAddr = (tileAddr & ~0b11111) | tmp;

    // window mode
    // tileAddr = (tileAddr & ~(1 << 10)) | (1 << 10);

    // uint8_t tmp = (g_pMemoryBus->map.ioregs.lcd.ly + g_pMemoryBus->map.ioregs.lcd.scy) / 8;
    // tileAddr = (tileAddr & ~(0b11111 << 5)) | (tmp << 5);

    // tileAddr = (tileAddr & ~0b11111) | (tileLine / 8);

    uint8_t tileId = fetch8(tileAddr);

    // only BG for now
    tileRow = (tileRow & ~(1 << 12)) | (g_pMemoryBus->map.ioregs.lcd.control.bgWindowTileData << 12);
    tileRow = (tileRow & ~(255 << 4)) | (tileId << 4);

    uint8_t tmp2 = (g_pMemoryBus->map.ioregs.lcd.ly + g_pMemoryBus->map.ioregs.lcd.scy) % 8;
    tileRow = (tileRow & ~(0b111 << 1)) | (tmp2 << 4);

    uint8_t lsb = fetch8(tileRow);
    uint8_t msb = fetch8(tileRow + 1);

    for (size_t i = 0; i < PIXEL_FIFO_SIZE; i++)
    {
        g_currentPPUState.pixelFifo.pixels[i] = (lsb & (1 << i)) ? 1 : 0;
        g_currentPPUState.pixelFifo.pixels[i] |= ((msb & (1 << i)) ? 2 : 0);
    }

    g_currentPPUState.pixelFifo.len = PIXEL_FIFO_SIZE;
}

void ppuInit(void)
{
    g_pMemoryBus = pGetBusPtr();
    memset(&g_currentPPUState, 0, sizeof(g_currentPPUState));
    g_currentPPUState.mode = MODE_2;
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
        switch(g_currentPPUState.mode)
        {
            case MODE_2:
            {
                // OAM scan
                // search for OBJs which overlap line
                // TODO: implement
                g_currentPPUState.cycleCount++;

                if (g_currentPPUState.cycleCount == 80)
                {
                    g_currentPPUState.mode = MODE_3;
                }
                break;
            }
            case MODE_3:
            {
                if (g_currentPPUState.column < 160)
                {
                    // TODO: check SCX
                    SPixel_t pixel = {g_currentPPUState.column, g_pMemoryBus->map.ioregs.lcd.ly, g_currentPPUState.pixelFifo.pixels[PIXEL_FIFO_SIZE - g_currentPPUState.pixelFifo.len]};
                    setPixel(&pixel);

                    g_currentPPUState.pixelFifo.len--;

                    g_currentPPUState.column++;
                    g_currentPPUState.mode3CyclesElapsed++;
                }
                else
                {
                    g_currentPPUState.mode = MODE_0;
                }
                break;
            }
            case MODE_0: // Hblank for remaining cycles until we hit 456
            {
                g_currentPPUState.cycleCount++;
                g_currentPPUState.mode3CyclesElapsed++;

                if (g_currentPPUState.mode3CyclesElapsed == 456)
                {
                    // line is done. reset counters
                    g_currentPPUState.column = 0;
                    g_pMemoryBus->map.ioregs.lcd.ly++;
                    g_currentPPUState.mode3CyclesElapsed = 0;

                    // go to Vblank if we processed the last line
                    if (g_pMemoryBus->map.ioregs.lcd.ly == LCD_VIEWPORT_Y)
                    {
                        g_currentPPUState.mode = MODE_1;
                    }
                }
                break;
            }
            case MODE_1: // Vblank for the remaining cycles
            {
                g_currentPPUState.cycleCount++;

                if (g_currentPPUState.cycleCount >= CYCLES_PER_FRAME)
                {
                    frameEnd = true;
                    g_currentPPUState.cycleCount = 0;
                    g_currentPPUState.mode = MODE_2;
                    g_pMemoryBus->map.ioregs.lcd.stat.ppuMode = MODE_2;
                }
                break;
            }
        }

        if (g_currentPPUState.pixelFifo.len == 0)
        {
            fillPixelFifo(g_currentPPUState.column, 0);
        }

        cyclesToRun--;
    }

    g_pMemoryBus->map.ioregs.lcd.stat.ppuMode = g_currentPPUState.mode;
    return frameEnd;
}