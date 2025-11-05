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
    memset(&g_bus.map.hram, 0xFF, HRAM_SIZE);
    memset(&g_bus.map.wram, 0xFF, WRAM_SIZE);
    memset(&g_bus.map.echo, 0xFF, ECHO_SIZE);
    memset(&g_bus.map.vram, 0xFF, VRAM_SIZE);
    memset(&g_bus.map.oam, 0x00, OAM_SIZE);
    memset(&g_bus.map.ioregs.joypad, 0xFF, 1);

    g_busState.pCurrentCartridge = pCartridge;

    g_busState.bootromIsMapped   = !skipBootrom;

    g_bus.map.pRom0              = g_busState.pCurrentCartridge->pCurrentRomBank0;
    g_bus.map.pRom1              = g_busState.pCurrentCartridge->pCurrentRomBank1;
    g_bus.map.pEram              = g_busState.pCurrentCartridge->pCurrentRamBank;
}

void overrideBus(SAddressBus_t *pBus)
{
    memcpy(&g_bus, pBus, sizeof(SAddressBus_t));
}

const uint8_t fetch8(uint16_t addr)
{
    if (addr == 0xFF00) return 0xff;
    uint8_t ret = g_bus.bus[addr];

    if (addr <= ROM0_END)
    {
        ret = (g_busState.bootromIsMapped && (addr <= BOOTROM_SIZE)) ? g_bootrom[addr] : g_bus.map.pRom0[addr];
    }
    else if (addr <= ROM1_END)
    {
        ret = g_bus.map.pRom1[addr - ROMN_SIZE];
    }
    else if (addr >= ERAM_START && addr <= ERAM_END)
    {
        ret = (g_busState.pCurrentCartridge->cartramEnabled && g_bus.map.pEram != nullptr) ?
              g_bus.map.pEram[addr - ERAM_START] :
              0xFF;
    }

    return ret;
}

const uint16_t fetch16(uint16_t addr)
{
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
}

/**
 * @brief write 8-bit value to address
 *
 * @param val value to write
 * @param addr address to write to
 */
void write8(uint8_t val, uint16_t addr)
{
    // cart write
    if (addr < (ROMN_SIZE * 2))
    {
        performBankSwitch(val, addr);
    }

    // VRAM write
    else if (addr >= (ROMN_SIZE * 2) && addr < ((ROMN_SIZE * 2) + VRAM_SIZE))
    {
        if (g_bus.map.ioregs.lcd.stat.ppuMode == 3)
        {
            return;
        }
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
        g_bus.bus[addr] = val;
    }
}

/**
 * @brief write 16-bit val to addr
 *
 * @param val value to write
 * @param addr address to write to
 */
void write16(uint16_t val, uint16_t addr)
{
    if (addr < (ROMN_SIZE * 2))
    {
        // we're writing to cart. handle bank switching stuff
        performBankSwitch(val, addr);
    }
    else if (addr >= (ROMN_SIZE * 2) && addr < ((ROMN_SIZE * 2) + VRAM_SIZE))
    {
        if (g_bus.map.ioregs.lcd.stat.ppuMode == 3)
        {
            return;
        }
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
    switch (addr)
    {
        case JOYPAD_INPUT_ADDR:
        {
            // memset(&g_bus.map.ioregs.joypad, 0xFF, 1);
            if (g_busState.cbRegister[JOYPAD_REG_CALLBACK].cb != nullptr)
            {
                g_busState.cbRegister[JOYPAD_REG_CALLBACK].cb(data, addr);
            }
            break;
        }
        case BOOT_ROM_MAPPER_CONTROL_ADDR:
        {
            g_bus.map.ioregs.disableBootrom = 0;
            g_busState.bootromIsMapped      = false;
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

            g_busState.cbRegister[AUDIO_MASTER_CONTROL_CALLBACK].cb(data, addr);
            break;
        }
        case AUDIO_CH1_CONTROL_ADDR:
        {
            g_busState.cbRegister[AUDIO_CH1_CONTROL_CALLBACK].cb(data, addr);
            break;
        }
        case AUDIO_CH2_CONTROL_ADDR:
        {
            g_busState.cbRegister[AUDIO_CH2_CONTROL_CALLBACK].cb(data, addr);
            break;
        }
        case AUDIO_CH3_CONTROL_ADDR:
        {
            g_busState.cbRegister[AUDIO_CH3_CONTROL_CALLBACK].cb(data, addr);
            break;
        }
        case AUDIO_CH4_CONTROL_ADDR:
        {
            g_busState.cbRegister[AUDIO_CH4_CONTROL_CALLBACK].cb(data, addr);
            break;
        }
        case SERIAL_TRANSFER_ADDR:
        case TIMER_ADDR:
        case DIVIDER_ADDR:
        default:
        {
            break;
        }
    }
    g_bus.bus[addr] = data;
}

static void handleIORegWrite16(uint16_t data, uint16_t addr)
{
    switch (addr)
    {
        case JOYPAD_INPUT_ADDR:
        {
            // memset(&g_bus.map.ioregs.joypad, 0xFF, 1);
            if (g_busState.cbRegister[JOYPAD_REG_CALLBACK].cb != nullptr)
            {
                g_busState.cbRegister[JOYPAD_REG_CALLBACK].cb(data, addr);
            }
            break;
        }
        case BOOT_ROM_MAPPER_CONTROL_ADDR:
        {
            g_bus.map.ioregs.disableBootrom = 0;
            g_busState.bootromIsMapped      = false;
            break;
        }
        case SERIAL_TRANSFER_ADDR:
        case TIMER_ADDR:
        case DIVIDER_ADDR:
        default:
        {
            break;
        }
    }
    g_bus.bus[addr]     = (uint8_t)(data & 0xFF);
    g_bus.bus[addr + 1] = (uint8_t)(data >> 8);
}