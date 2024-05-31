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
static int currentColumn = 0;
static int currentMode3Elapsed = 0;
static SFIFO_t currentFifo;
static int fifoCounter = 0;

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

    SFIFO_t newFifo = {8, {3, 3, 3, 3, 3, 3, 3, 3}, 0};
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

    while (pBus->map.ioregs.lcd.control.lcdPPUEnable && (cyclesToRun-- > 0))
    {
        // Mode 2 -> Mode 3  -> Mode 0         -> Mode 1
        // 80     -> 172/289 -> 376 - (Mode 3) -> 4560

        pBus->map.ioregs.lcd.stat.ppuMode = 0x02; // signal Mode 2

        if (cycleCount < 80)
        {
            // Mode 2, OAM scan
            // search for OBJs which overlap line
            // TODO: implement
            cycleCount++;
        }
        else if (cycleCount < 65664)
        {
            if (currentColumn < 160)
            {
                pBus->map.ioregs.lcd.stat.ppuMode = 0x03; // signal Mode 3

                if (currentFifo.len == 0)
                {
                    uint16_t tileAddr = 0x9800;
                    if (((pBus->map.ioregs.lcd.control.bgTilemap) &&
                        (currentColumn <= (pBus->map.ioregs.lcd.wx - 7))) ||
                        ((pBus->map.ioregs.lcd.control.bgWindowTileMap) &&
                        (currentColumn >= (pBus->map.ioregs.lcd.wx - 7))))
                    {
                        tileAddr = 0x9C00;
                    }
                    currentFifo = tileFetcher(tileAddr, pBus->map.ioregs.lcd.ly);
                }

                // TODO: check SCX
                uint8_t scxDrop = (pBus->map.ioregs.lcd.scx % 8);
                if ((scxDrop == 0) || (currentColumn > scxDrop))
                {
                    SPixel_t pixel = {currentColumn,  pBus->map.ioregs.lcd.ly, currentFifo.pixels[fifoCounter]};
                    setPixel(&pixel);
                }
                currentFifo.len--;
                if (currentFifo.len == 0)
                {
                    fifoCounter = 0;
                }
                else
                {
                    fifoCounter++;
                }
                currentColumn++;
                currentMode3Elapsed++;
            }
            else if ((currentColumn >= 160) && (currentMode3Elapsed < 456))
            {
                // Mode 0, hblank
                pBus->map.ioregs.lcd.stat.ppuMode = 0x00; // signal Mode 0
                cycleCount++;
                currentMode3Elapsed++;
            }
            else
            {
                // line is done.
                currentColumn = 0;
                pBus->map.ioregs.lcd.ly++;
                currentMode3Elapsed = 0;
            }
        }
        else if (cycleCount < CYCLES_PER_FRAME)
        {
            // Mode 1, vblank, do nothing at all
            cycleCount++;
        }
        else
        {
            // frame done
            frameEnd = true;
            cycleCount = 0;
            currentMode3Elapsed = 0;
        }
    }

    return frameEnd;
}