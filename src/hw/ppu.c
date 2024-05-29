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

typedef struct
{
    SPixel_t pixels[TILE_DIM_X][TILE_DIM_Y];
} STile_t;

static int cycleCount = 0;

/**
 * @brief builds current FIFO buffer, 8 pixels
 * 
 * @param addr tilemap address
 */
static SFIFO_t tileFetcher(uint16_t tilemapAddr, uint8_t tileLine)
{
    // a tile consists of 8x8 pixels, but is stored in 16 bytes
    // first byte is LSB, second MSB. e.g. addr == 0x3C (00111100b), addr+1 == 0x7E (01111110b):
    // result is 00 10 11 11 11 11 10 00

    SFIFO_t newFifo = {{3, 3, 3, 3, 3, 3, 3, 3}};
    static uint16_t tileIndex = 0;

    uint8_t tileId = fetch8(tilemapAddr + tileIndex);

    // a tile is 16 bytes. first get its offset
    uint16_t offset = 0x8000 + ((uint16_t)tileId * 16);

    // calculate address from offset; see which line we want
    uint16_t addr = offset + (tileLine * 2);

    uint8_t msb = fetch8(addr + 1);
    uint8_t lsb = fetch8(addr);

    for (size_t i = 0; i < PIXEL_FIFO_SIZE; i++)
    {
        newFifo.pixels[i] = (lsb & (1 << i)) ? 1 : 0;
        newFifo.pixels[i] |= ((msb & (1 << i)) ? 2 : 0);
    }

    return newFifo;
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

    bus_t *pBus = pGetBusPtr();

    if (pBus->map.ioregs.lcd.control.lcdPPUEnable)
    {
        size_t winX = pBus->map.ioregs.lcd.wx - 7;

        static int mode1Length = 4560;
        static bool mode3Done = false;
        static bool mode0Done = false;
        static int mode0Length = 0;

        // one line is 456 cycles

        static int currentLineLength = 456;
        int currentModeOverflow = 0;

        if (currentLineLength >= 376)
        {
            if ((currentLineLength - cyclesToRun) < 376)
            {
                currentModeOverflow = 376 - (currentLineLength - cyclesToRun);
            }
            cycleCount += cyclesToRun - currentModeOverflow;
            currentLineLength -= cyclesToRun + currentModeOverflow;
            pBus->map.ioregs.lcd.stat.ppuMode = 0x02; // signal Mode 2

            // OAM SCAN (mode 2)
            // Sprite X-Position must be greater than 0
            // LY + 16 must be greater than or equal to Sprite Y-Position
            // LY + 16 must be less than Sprite Y-Position + Sprite Height (8 in Normal Mode, 16 in Tall-Sprite-Mode)
            // The amount of sprites already stored in the OAM Buffer must be less than 10
        }
        if ((currentModeOverflow > 0) ||
            ((currentLineLength < 376) && (currentModeOverflow == 0)))
        {
            // Drawing (mode 3)
            // here we push stuff to the framebuffer
            // sprites have prio over bg
            // first, build tiles
            // then push to display
            // TODO implement. for now use 0x9800 as default
            pBus->map.ioregs.lcd.stat.ppuMode = 0x03; // signal Mode 3

            // check which tilemap to use


            for (size_t y = 0; y < LCD_VIEWPORT_Y; y++)
            {
                for (size_t x = 0; x < LCD_VIEWPORT_X; x += (LCD_VIEWPORT_X / PIXEL_FIFO_SIZE))
                {
                    uint16_t tileAddr = 0x9800;
                    if (((pBus->map.ioregs.lcd.control.bgTilemap) &&
                        (x <= winX)) ||
                        ((pBus->map.ioregs.lcd.control.bgWindowTileMap) &&
                        (x >= winX)))
                    {
                        tileAddr = 0x9C00;
                    }
                    SFIFO_t currentFifo = tileFetcher(tileAddr, y);
                    writeFifoToFramebuffer(&currentFifo, x, y);
                }
            }
            mode3Done = true;
        }

        if (mode3Done)
        {
            if (mode0Length == 0) { mode0Length = currentLineLength; }
            pBus->map.ioregs.lcd.stat.ppuMode = 0x00; // signal Mode 0, hblank

            // ...

            mode0Done = true;
        }

        if (mode0Done)
        {
            // Vblank (mode 1)
            pBus->map.ioregs.lcd.stat.ppuMode = 0x01; // signal Mode 1

            if (mode1Length > 0)
            {
                mode1Length -= cyclesToRun;
            }
            else
            {
                frameEnd = true;
                mode3Done = false;
                mode0Done = false;
                mode0Length = 0;
                mode1Length = 4560;
                currentLineLength = 476;
            }
        }
    }

    return frameEnd;
}