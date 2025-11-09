/**
 * @file mem.h
 * @author Toesoe
 * @brief seaboy bus (ram, vram)
 * @version 0.1
 * @date 2023-06-15
 *
 * @copyright Copyright (c) 2023
 *
  TODO: improve bus by undoing union access, use separate structs for mapping in cartridge
 */

#include "mem.h"
#include "cart.h"

#include "cpu.h" // for debugging only

#include "bootrom.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_WRITES
#define BOOTROM_SIZE      (0x100)

#define IO_REGISTER_START (0xFF00)
#define IO_REGISTER_END   (IO_REGISTER_START + IO_SIZE)

//=====================================================================================================================
// Typedefs
//=====================================================================================================================

typedef struct
{
    uint16_t             addr;
    addressWriteCallback cb;
} SAddressWriteCallback_t;

typedef struct
{
    const SCartridge_t     *pCurrentCartridge;
    size_t                  countRegisteredCallbacks;
    SAddressWriteCallback_t cbRegister[ADDRESS_CALLBACK_TYPE_COUNT];
    bool                    dmaTransferInProgress;
    bool                    bootromIsMapped;
} SAddressBusState;

//=====================================================================================================================
// Globals
//=====================================================================================================================

SAddressBus_t    g_bus       = { 0 };
SAddressBusState g_busState  = { 0 };

//=====================================================================================================================
// Local function prototypes
//=====================================================================================================================

static void handleIORegWrite8(uint8_t, uint16_t);
static void handleIORegWrite16(uint16_t, uint16_t);

static uint8_t handleIORegRead8(uint16_t);

//=====================================================================================================================
// Function definitions
//=====================================================================================================================

void registerAddressCallback(uint16_t addr, EAddressCallbackType_t type, addressWriteCallback cb)
{
    assert(cb != nullptr);
    g_busState.cbRegister[type] = (SAddressWriteCallback_t){ addr, cb };
}

void initializeBus(const SCartridge_t *pCartridge, bool skipBootrom)
{
    assert(pCartridge != nullptr);

    memset(&g_bus, 0x00, sizeof(g_bus));
    memset(&g_bus.map.hram, 0x00, HRAM_SIZE);
    memset(&g_bus.map.wram, 0x00, WRAM_SIZE);
    memset(&g_bus.map.echo, 0x00, ECHO_SIZE);
    memset(&g_bus.map.vram, 0x00, VRAM_SIZE);
    memset(&g_bus.map.oam, 0x00, OAM_SIZE);
    memset(&g_bus.map.ioregs.joypad, 0xFF, 1);

    g_busState.pCurrentCartridge = pCartridge;

    g_busState.bootromIsMapped   = !skipBootrom;

    g_bus.map.pRom0              = g_busState.pCurrentCartridge->pCurrentRomBank0;
    g_bus.map.pRom1              = g_busState.pCurrentCartridge->pCurrentRomBank1;
    g_bus.map.pEram              = g_busState.pCurrentCartridge->pCurrentRamBank;

    if (skipBootrom)
    {
        memset(&g_bus.map.ioregs.lcd.control, 0x91, 1);
        memset(&g_bus.map.ioregs.lcd.stat, 0x85, 1);
        memset(&g_bus.map.ioregs.lcd.dma, 0xFF, 1);
        memset(&g_bus.map.ioregs.lcd.bgp, 0xFC, 1);
        memset(&g_bus.map.ioregs.joypad, 0xCF, 1);
        memset(&g_bus.map.ioregs.divRegister, 0x18, 1);
        memset(&g_bus.map.ioregs.timers.TAC, 0xF8, 1);
        memset(&g_bus.map.ioregs.intFlags, 0xE1, 1);
    }
}

void overrideBus(SAddressBus_t *pBus)
{
    memcpy(&g_bus, pBus, sizeof(SAddressBus_t));
}

const uint8_t fetch8(uint16_t addr)
{

#ifndef TEST
    uint8_t ret = g_bus.bus[addr];

#ifdef GB_DOCTOR
    if (addr == 0xFF44) { return 0x90; } // hardcode LY
#endif

    if (addr <= ROM0_END)
    {
        ret = (g_busState.bootromIsMapped && (addr <= BOOTROM_SIZE)) ? g_bootrom[addr] : g_bus.map.pRom0[addr];
    }
    else if (addr <= ROM1_END)
    {
        ret = g_bus.map.pRom1[addr - ROMN_SIZE];
    }
    else if ((addr >= ERAM_START) && (addr <= ERAM_END))
    {
        ret = (g_busState.pCurrentCartridge->cartramEnabled && g_bus.map.pEram != nullptr) ?
              g_bus.map.pEram[addr - ERAM_START] :
              0xFF;
    }
    else if ((addr >= IO_REGISTER_START) && (addr < IO_REGISTER_END))
    {
        ret = handleIORegRead8(addr);
    }

    return ret;
#else
    return g_bus.bus[addr];
#endif
}

const uint16_t fetch16(uint16_t addr)
{
#ifndef TEST
    const uint8_t *dataSrcPtr = g_bus.bus;

    if (addr <= ROM0_END)
    {
        dataSrcPtr = (g_busState.bootromIsMapped && (addr <= BOOTROM_SIZE)) ? g_bootrom : g_bus.map.pRom0;
    }
    else if (addr <= ROM1_END)
    {
        dataSrcPtr = g_bus.map.pRom1;
        addr -= ROMN_SIZE;
    }
    else if (addr >= ERAM_START && addr <= ERAM_END)
    {
        dataSrcPtr = g_bus.map.pEram;
        addr -= ERAM_START;
    }

    return (uint16_t)((dataSrcPtr[addr + 1] << 8) | dataSrcPtr[addr]);
#else
    return (uint16_t)((g_bus.bus[addr + 1] << 8) | g_bus.bus[addr]);
#endif
}

/**
 * @brief write 8-bit value to address
 *
 * @param val value to write
 * @param addr address to write to
 */
void write8(uint8_t val, uint16_t addr)
{
#ifndef TEST
    // cart write
    if (addr < (ROMN_SIZE * 2))
    {
        cartWriteHandler(val, addr);
    }

    // VRAM write
    else if (addr < ((ROMN_SIZE * 2) + VRAM_SIZE))
    {
        // if (g_bus.map.ioregs.lcd.stat.ppuMode == 3)
        // {
        //     return;
        // }
        g_bus.bus[addr] = val;
    }

    // cartram write
    else if ((addr >= ERAM_START) && (addr <= ERAM_END))
    {
        writeCartRam8(val, addr);
    }

    // IO reg stuff
    else if ((addr >= IO_REGISTER_START) && (addr < IO_REGISTER_END))
    {
        handleIORegWrite8(val, addr);
    }

    // regular bus write
    else
    {
        if (addr == 0xFF82)
        {
            if (val == 0xC0)
            {
                __asm("nop");
            }
        }
        g_bus.bus[addr] = val;
    }
#else
    g_bus.bus[addr] = val;
#endif
}

/**
 * @brief write 16-bit val to addr
 *
 * @param val value to write
 * @param addr address to write to
 */
void write16(uint16_t val, uint16_t addr)
{
#ifndef TEST
    if (addr < (ROMN_SIZE * 2))
    {
        // we're writing to cart. handle bank switching stuff
        cartWriteHandler(val, addr);
    }
    else if (addr < ((ROMN_SIZE * 2) + VRAM_SIZE))
    {
        // no VRAM writing in mode 3
        // if (g_bus.map.ioregs.lcd.stat.ppuMode == 3)
        // {
        //     return;
        // }
        g_bus.bus[addr] = val;
    }
    else if ((addr >= ERAM_START) && (addr <= ERAM_END))
    {
        writeCartRam16(val, addr);
    }
    else if ((addr >= IO_REGISTER_START) && (addr < IO_REGISTER_END))
    {
        handleIORegWrite16(val, addr);
    }
    else
    {
        g_bus.bus[addr]     = (uint8_t)(val & 0xFF);
        g_bus.bus[addr + 1] = (uint8_t)(val >> 8);
    }
#else
    g_bus.bus[addr]     = (uint8_t)(val & 0xFF);
    g_bus.bus[addr + 1] = (uint8_t)(val >> 8);
#endif
}

SAddressBus_t *const pGetAddressBus(void)
{
    return &g_bus;
}

//=====================================================================================================================
// Local functions
//=====================================================================================================================

static void handleIORegWrite8(uint8_t data, uint16_t addr)
{
    // when true, will write <data> to bus at <addr> after cb execution
    bool writeToBus = true;

    switch (addr)
    {
        case JOYPAD_INPUT_ADDR:
        {
            // set joypad state from local registers
            if (g_busState.cbRegister[JOYPAD_REG_CALLBACK].cb)
            {
                g_busState.cbRegister[JOYPAD_REG_CALLBACK].cb(data, addr);
            }
            writeToBus = false;
            break;
        }
        case BOOT_ROM_MAPPER_CONTROL_ADDR:
        {
            g_bus.map.ioregs.disableBootrom = 0;
            g_busState.bootromIsMapped      = false;
            if (g_busState.cbRegister[BOOTROM_UNMAP_CALLBACK].cb)
            {
                g_busState.cbRegister[BOOTROM_UNMAP_CALLBACK].cb(data, addr);
            }
            writeToBus = false;
            break;
        }
        case OAM_DMA_ADDR:
        {
            memcpy(&g_bus.map.oam, &g_bus.bus[(uint16_t)(data << 8)], 0x9F);
            // costs 160 mcycles! handle in cb
            if (g_busState.cbRegister[OAM_DMA_CALLBACK].cb != nullptr)
            {
                g_busState.cbRegister[OAM_DMA_CALLBACK].cb(data, addr);
            }
            break;
        }
        case AUDIO_MASTER_CONTROL_ADDR:
        {
            writeToBus = false;
            if ((data & 0x80) == 0)
            {
                // APU disabled! reset regs..
                memset(&g_bus.map.ioregs.audio, 0x00, sizeof(g_bus.map.ioregs.audio));
            }
            else
            {
                // reenabling APU. but set regs to 0 to simulate real behaviour of ignored writes during disable
                memset(&g_bus.map.ioregs.audio, 0x00, sizeof(g_bus.map.ioregs.audio));
                g_bus.map.ioregs.audio.masterControl.audioMasterEnable = 1;
            }

            if (g_busState.cbRegister[AUDIO_MASTER_CONTROL_CALLBACK].cb)
            {
                g_busState.cbRegister[AUDIO_MASTER_CONTROL_CALLBACK].cb(data, addr);
            }
            break;
        }
        case AUDIO_CH1_CONTROL_ADDR:
        {
            writeToBus = false;
            if (g_busState.cbRegister[AUDIO_CH1_CONTROL_CALLBACK].cb)
            {
                g_busState.cbRegister[AUDIO_CH1_CONTROL_CALLBACK].cb(data, addr);
            }
            break;
        }
        case AUDIO_CH2_CONTROL_ADDR:
        {
            writeToBus = false;
            if (g_busState.cbRegister[AUDIO_CH2_CONTROL_CALLBACK].cb)
            {
                g_busState.cbRegister[AUDIO_CH2_CONTROL_CALLBACK].cb(data, addr);
            }
            break;
        }
        case AUDIO_CH3_CONTROL_ADDR:
        {
            writeToBus = false;
            if (g_busState.cbRegister[AUDIO_CH3_CONTROL_CALLBACK].cb)
            {
                g_busState.cbRegister[AUDIO_CH3_CONTROL_CALLBACK].cb(data, addr);
            }
            break;
            break;
        }
        case AUDIO_CH4_CONTROL_ADDR:
        {
            writeToBus = false;
            if (g_busState.cbRegister[AUDIO_CH4_CONTROL_CALLBACK].cb)
            {
                g_busState.cbRegister[AUDIO_CH4_CONTROL_CALLBACK].cb(data, addr);
            }
            break;
            break;
        }
        case DIVIDER_ADDR:
        {
            writeToBus = false;
            if (g_busState.cbRegister[DIV_CALLBACK].cb)
            {
                g_busState.cbRegister[DIV_CALLBACK].cb(data, addr);
            }
            break;
        }
        case SERIAL_TRANSFER_ADDR:
        case TIMER_ADDR:
        default:
        {
            break;
        }
    }
    if (writeToBus) { g_bus.bus[addr] = data; }
}

static void handleIORegWrite16(uint16_t data, uint16_t addr)
{
    bool writeToBus = true;
    switch (addr)
    {
        default:
        {
            break;
        }
    }

    if (writeToBus)
    {
        g_bus.bus[addr]     = (uint8_t)(data & 0xFF);
        g_bus.bus[addr + 1] = (uint8_t)(data >> 8);
    }
}


static uint8_t handleIORegRead8(uint16_t addr)
{
    // switch (addr)
    // {
    //     case JOYPAD_INPUT_ADDR:
    //     {
    //         // set joypad state from local registers
    //         g_busState.cbRegister[JOYPAD_REG_CALLBACK].cb(0, addr);
    //         break;
    //     }
    //     default:
    //     {
    //         break;
    //     }
    // }

    return g_bus.bus[addr];
}