/**
 * @file input.c
 * @author Toesoe
 * @brief seaboy input handling
 * @version 0.1
 * @date 2023-06-13
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>

#include "joypad.h"
#include "mem.h"

static sRegJOYP_t g_joypad;
static sRegJOYP_t g_select;

void              joypadInit(void)
{
    memset(&g_joypad, 0xFF, sizeof(sRegJOYP_t));
    memset(&g_select, 0xFF, sizeof(sRegJOYP_t));

    registerAddressCallback(JOYPAD_INPUT_ADDR, JOYPAD_REG_CALLBACK, joypadShiftValuesToReg);
}

void joypadEventLoop(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_KEYUP:
                /* Check the SDLKey values and move change the coords */
                switch (event.key.keysym.sym)
                {
                    case SDLK_LEFT:
                        g_joypad.bLeft = 1;
                        break;
                    case SDLK_RIGHT:
                        g_joypad.aRight = 1;
                        break;
                    case SDLK_UP:
                        g_joypad.selectUp = 1;
                        break;
                    case SDLK_DOWN:
                        g_joypad.startDown = 1;
                        break;
                    case SDLK_RETURN:
                        g_select.startDown = 1;
                        break;
                    case SDLK_BACKSPACE:
                        g_select.selectUp = 1;
                        break;
                    case SDLK_a:
                        g_select.aRight = 1;
                        break;
                    case SDLK_z:
                        g_select.bLeft = 1;
                        break;
                    default:
                        break;
                }
                break;
            /* We must also use the SDL_KEYUP events to zero the x */
            /* and y velocity variables. But we must also be       */
            /* careful not to zero the velocities when we shouldn't*/
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_LEFT:
                        g_joypad.bLeft = 0;
                        break;
                    case SDLK_RIGHT:
                        g_joypad.aRight = 0;
                        break;
                    case SDLK_UP:
                        g_joypad.selectUp = 0;
                        break;
                    case SDLK_DOWN:
                        g_joypad.startDown = 0;
                        break;
                    case SDLK_RETURN:
                        g_select.startDown = 0;
                        break;
                    case SDLK_BACKSPACE:
                        g_select.selectUp = 0;
                        break;
                    case SDLK_a:
                        g_select.aRight = 0;
                        break;
                    case SDLK_z:
                        g_select.bLeft = 0;
                        break;
                    default:
                        break;
                }
                break;

            default:
                break;
        }
    }
}

void joypadShiftValuesToReg(uint8_t regValue, uint16_t addr)
{
    (void)addr;

    SAddressBus_t *pBus = pGetAddressBus();

    if (regValue & 0x20)
    {
        pBus->map.ioregs.joypad.aRight = g_joypad.aRight;
        pBus->map.ioregs.joypad.bLeft = g_joypad.bLeft;
        pBus->map.ioregs.joypad.selectUp = g_joypad.selectUp;
        pBus->map.ioregs.joypad.startDown = g_joypad.startDown;
        pBus->map.ioregs.joypad.dpadSelect = 1;
    }
    else if (regValue & 0x10)
    {
        pBus->map.ioregs.joypad.aRight = g_select.aRight;
        pBus->map.ioregs.joypad.bLeft = g_select.bLeft;
        pBus->map.ioregs.joypad.selectUp = g_select.selectUp;
        pBus->map.ioregs.joypad.startDown = g_select.startDown;
        pBus->map.ioregs.joypad.buttonSelect = 1;
    }
    else if (regValue & 0x30)
    {
        pBus->map.ioregs.joypad.aRight = 1;
        pBus->map.ioregs.joypad.bLeft = 1;
        pBus->map.ioregs.joypad.selectUp = 1;
        pBus->map.ioregs.joypad.startDown = 1;
        pBus->map.ioregs.joypad.dpadSelect = 1;
        pBus->map.ioregs.joypad.buttonSelect = 1;
    }
}