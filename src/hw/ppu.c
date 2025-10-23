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

#define LCD_VIEWPORT_X           160
#define LCD_VIEWPORT_Y           144
#define LCD_VBLANK_LEN_LINES     10

#define TILE_DIM_X               8
#define TILE_DIM_Y               8

#define BG_TILES_X               32
#define BG_TILES_Y               32

#define WINDOW_TILES_X           32
#define WINDOW_TILES_Y           32

#define CHECK_BIT(var, pos)      ((var) & (1 << (pos)))
#define SET_BIT(var, pos)        ((var) |= (1 << (pos)))
#define CLEAR_BIT(var, pos)      ((var) &= ~(1 << (pos)))
#define TOGGLE_BIT(var, pos)     ((var) ^= (1 << (pos)))

#define OAM_ENTRY_SIZE_BYTES     4
#define TILE_SIZE_BYTES          16
#define OAM_MAX_SPRITES_PER_LINE 10
#define OAM_TOTAL_ENTRIES        40
#define OAM_ENTRY_Y_OFFSET_LINES 16
#define OAM_ENTRY_X_OFFSET_LINES 8

#define CYCLES_PER_FRAME         70224 // 154 scanlines * 456 cycle

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
    uint8_t yPos; // offset by -16, so yPos 16 == pos 0
    uint8_t xPos; // offset by -8
    uint8_t tileIdx;

    struct __attribute__((__packed__)) flags
    {
        uint8_t cgbOnlyPaletteNum      : 3; // 0-2
        uint8_t cgbOnlyTileVRAMBankNum : 1; // 3
        uint8_t gbOnlyPaletteNum       : 1; // 4
        uint8_t xFlip                  : 1; // 5
        uint8_t yFlip                  : 1; // 6
        uint8_t bgOverObjPrio          : 1; // 7: if 1, BG and Window color indices 1–3 are drawn over this OBJ
    } flags;
} SOAMSpriteAttributeObject_t;

typedef struct
{
    EPPUMode_t                  mode;
    int                         cycleCount;
    int                         currentLineCycleCount;
    bool                        discardCalculatedForCurrentLine;
    size_t                      spriteCountCurrentLine;
    SOAMSpriteAttributeObject_t spritesForCurrentLine[OAM_MAX_SPRITES_PER_LINE];
    size_t                      currentOAMEntry;
    size_t                      fifoDiscardLeft;
    uint8_t                     column;
    uint8_t                     row;
    SFIFO_t                     pixelFifo;
    SFIFO_t                     spriteFifo;
} SPPUState_t;

static SPPUState_t g_currentPPUState = { 0 };
static bus_t      *g_pMemoryBus      = NULL;

static void        fifoPush(SFIFO_t *, SPPUPixel_t);
static SPPUPixel_t fifoPop(SFIFO_t *);

/**
 * @brief builds current FIFO buffer, 8 pixels
 *
 * @param lx current X position
 */
static void fillPixelFifos(uint8_t lx)
{
    bool    isWindow         = false;
    uint8_t adjustedScanline = (g_pMemoryBus->map.ioregs.lcd.ly + g_pMemoryBus->map.ioregs.lcd.scy) % 256;
    bool    objToBGPriority  = false;

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
    uint8_t tileLineInTile; // 0..7 : which row inside the tile we must fetch

    if (!isWindow)
    {
        uint8_t row = (adjustedScanline / 8);
        uint8_t col = (((lx + g_pMemoryBus->map.ioregs.lcd.scx) % 256) / 8);
        tileAddr += (row * 32) + col;
        tileLineInTile = adjustedScanline % 8;
    }
    else
    {
        uint8_t row = ((g_pMemoryBus->map.ioregs.lcd.ly - g_pMemoryBus->map.ioregs.lcd.wy) / 8);
        uint8_t col = (lx / 8);
        tileAddr += (row * 32) + col;
        tileLineInTile = (g_pMemoryBus->map.ioregs.lcd.ly - g_pMemoryBus->map.ioregs.lcd.wy) % 8;
    }

    uint8_t tileId = fetch8(tileAddr);

    uint16_t tileRowAddr;
    if (g_pMemoryBus->map.ioregs.lcd.control.bgWindowTileData)
    {
        tileRowAddr = 0x8000 + (tileId * 16);
    }
    else
    {
        // 0x8800 mode uses signed tile data
        tileRowAddr = 0x8800 + ((int8_t)tileId * 16);
    }

    bool spriteFound = false;
    uint16_t spriteTileAddr = 0;
    uint8_t spriteLineInTile = 0;

    for (size_t i = 0; i < g_currentPPUState.spriteCountCurrentLine; i++)
    {
        if (g_currentPPUState.spritesForCurrentLine[i].xPos == lx)
        {
            // compute y within sprite
            uint8_t spriteY = g_pMemoryBus->map.ioregs.lcd.ly + 16 - g_currentPPUState.spritesForCurrentLine[i].yPos;
            uint8_t spriteHeight = (g_pMemoryBus->map.ioregs.lcd.control.objSize ? 16 : 8);

            if (g_currentPPUState.spritesForCurrentLine[i].flags.yFlip)
            {
                // flip within sprite height
                spriteY = (spriteHeight - 1) - spriteY;
            }

            spriteLineInTile = spriteY % 8; // each tile is 8 rows tall; for 16-high sprites the second tile is handled by tileIdx +/- 1
            // For 8x16 mode, tile index selection may need (tileIdx & ~1) + (spriteY / 8) — preserve original intent if different.
            spriteTileAddr = 0x8000 + (g_currentPPUState.spritesForCurrentLine[i].tileIdx * 16) + (spriteY / 8) * 16;

            if (g_currentPPUState.spritesForCurrentLine[i].flags.bgOverObjPrio)
            {
                objToBGPriority = true;
            }
            spriteFound = true;
            break;
        }
    }

    uint8_t lsb       = fetch8(tileRowAddr + tileLineInTile * 2);
    uint8_t msb       = fetch8(tileRowAddr + tileLineInTile * 2 + 1);

    uint8_t lsbSprite = 0;
    uint8_t msbSprite = 0;

    if (spriteFound)
    {
        lsbSprite = fetch8(spriteTileAddr + spriteLineInTile * 2);
        msbSprite = fetch8(spriteTileAddr + spriteLineInTile * 2 + 1);
    }

    // Debug: Print the fetched tile data
    // printf("Tile data at (addr=%04X): LSB=%02X, MSB=%02X\n", tileRowAddr + (adjustedScanline % 8) * 2, lsb, msb);

    for (size_t i = 0; i < PPU_FIFO_SIZE; i++)
    {
        uint8_t pixel = 0, pixelSprite = 0;
        pixel |= ((msb & (1 << (7 - i))) ? 2 : 0);
        pixel |= ((lsb & (1 << (7 - i))) ? 1 : 0);
        fifoPush(&g_currentPPUState.pixelFifo, (SPPUPixel_t){ pixel, false });

        pixelSprite |= ((msbSprite & (1 << (7 - i))) ? 2 : 0);
        pixelSprite |= ((lsbSprite & (1 << (7 - i))) ? 1 : 0);
        fifoPush(&g_currentPPUState.spriteFifo, (SPPUPixel_t){ pixelSprite, objToBGPriority });
    }

    g_currentPPUState.pixelFifo.len  = PPU_FIFO_SIZE;
    g_currentPPUState.spriteFifo.len = PPU_FIFO_SIZE;
}

void ppuInit(bool skipBootrom)
{
    g_pMemoryBus = pGetBusPtr();
    memset(&g_currentPPUState, 0, sizeof(SPPUState_t));

    if (!skipBootrom)
    {
        g_currentPPUState.mode = MODE_2;
    }
    else
    {
        g_currentPPUState.mode = MODE_1;
    }
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

                if (g_currentPPUState.currentLineCycleCount == 0)
                {
                    g_currentPPUState.column                          = 0; // reset column at start of line
                    g_currentPPUState.discardCalculatedForCurrentLine = false;
                    memset(g_currentPPUState.spritesForCurrentLine, 0, OAM_MAX_SPRITES_PER_LINE * OAM_ENTRY_SIZE_BYTES);
                    g_currentPPUState.spriteCountCurrentLine = 0;
                    g_currentPPUState.currentOAMEntry        = 0;
                }

                // start OAM scan
                // compare LY to Y for every sprite
                // find the 10 sprites that appear first in OAM (FE00 - FE03 are first)
                // discard the rest

                // depends on:
                // Sprite X-Position must be greater than 0
                // LY + 16 must be greater than or equal to Sprite Y-Position
                // LY + 16 must be less than Sprite Y-Position + Sprite Height (8 in Normal Mode, 16 in
                // Tall-Sprite-Mode)

                // OAM object size is 4 bytes.

                // only do 1 OAM scan every 2 cycles to keep accuracy
                if ((g_currentPPUState.currentLineCycleCount % 2) == 0)
                {
                    SOAMSpriteAttributeObject_t currentOAMObject =
                    *(SOAMSpriteAttributeObject_t *)&g_pMemoryBus->map.oam[g_currentPPUState.currentOAMEntry * OAM_ENTRY_SIZE_BYTES];

                    if (g_currentPPUState.spriteCountCurrentLine < OAM_MAX_SPRITES_PER_LINE)
                    {
                        uint8_t spriteHeight = g_pMemoryBus->map.ioregs.lcd.control.objSize ? 16 : 8;

                        if (currentOAMObject.xPos > 0 && currentOAMObject.xPos < 168)
                        {
                            if ((g_pMemoryBus->map.ioregs.lcd.ly + 16) >= currentOAMObject.yPos &&
                                (g_pMemoryBus->map.ioregs.lcd.ly + 16) < (currentOAMObject.yPos + spriteHeight))
                            {
                                g_currentPPUState.spritesForCurrentLine[g_currentPPUState.spriteCountCurrentLine++] =
                                currentOAMObject;
                            }
                        }
                    }
                    g_currentPPUState.currentOAMEntry++;
                }

                if (++g_currentPPUState.currentLineCycleCount == 80)
                {
                    g_currentPPUState.mode = MODE_3;
                }
                break;
            }
            case MODE_3: // drawing mode
            {
                if (g_currentPPUState.column < 160)
                {
                    if (g_currentPPUState.pixelFifo.len == 0)
                    {
                        if (!g_currentPPUState.discardCalculatedForCurrentLine)
                        {
                            g_currentPPUState.fifoDiscardLeft                 = g_pMemoryBus->map.ioregs.lcd.scx % 8;
                            g_currentPPUState.discardCalculatedForCurrentLine = true;
                        }
                        fillPixelFifos(g_currentPPUState.column);
                    }

                    if (g_currentPPUState.fifoDiscardLeft > 0)
                    {
                        // discard current pixels
                        g_currentPPUState.fifoDiscardLeft--;
                        (void)fifoPop(&g_currentPPUState.pixelFifo);
                        (void)fifoPop(&g_currentPPUState.spriteFifo);
                    }
                    else
                    {
                        SPPUPixel_t outputPixel = fifoPop(&g_currentPPUState.pixelFifo);
                        SPPUPixel_t spritePixel = fifoPop(&g_currentPPUState.spriteFifo);

                        if (spritePixel.color != PIXEL_COLOR_BLACK_TRANSPARENT &&
                            (!spritePixel.objToBGPrioBit || outputPixel.color == PIXEL_COLOR_BLACK_TRANSPARENT) &&
                            g_pMemoryBus->map.ioregs.lcd.control.objEnable)
                        {
                            outputPixel = spritePixel;
                        }

                        SPixel_t pixel = { g_currentPPUState.column, g_pMemoryBus->map.ioregs.lcd.ly,
                                           outputPixel.color };
                        setPixel(&pixel);
                        g_currentPPUState.column++;
                    }
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
                if (++g_currentPPUState.currentLineCycleCount == 456)
                {
                    // line is done. reset counters
                    g_currentPPUState.currentLineCycleCount = 0;

                    if (++g_pMemoryBus->map.ioregs.lcd.ly == LCD_VIEWPORT_Y)
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
                    if (++g_currentPPUState.currentLineCycleCount == 456)
                    {
                        // end of line, increase ly
                        g_pMemoryBus->map.ioregs.lcd.ly++;
                        g_currentPPUState.currentLineCycleCount = 0;
                    }
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

        cyclesToRun--;
        g_currentPPUState.cycleCount++;
    }

    g_pMemoryBus->map.ioregs.lcd.stat.ppuMode = g_currentPPUState.mode;
    return frameEnd;
}

static inline void fifoPush(SFIFO_t *pFifo, SPPUPixel_t pixel)
{
    if (pFifo->len < PPU_FIFO_SIZE)
    {
        pFifo->_pixels[pFifo->len++] = pixel;
    }
}

static inline SPPUPixel_t fifoPop(SFIFO_t *pFifo)
{
    SPPUPixel_t ret = { 0 };

    if (pFifo->len > 0)
    {
        ret = pFifo->_pixels[PPU_FIFO_SIZE - pFifo->len--];
    }

    return ret;
}