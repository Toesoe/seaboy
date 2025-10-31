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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_WRITES
#define BOOTROM_SIZE (0x100)

#define IO_REGISTER_START     (0xFF00)
#define IO_REGISTER_END       (IO_REGISTER_START + IO_SIZE)

#define JOYPAD_INPUT_ADDR     (0xFF00)
#define SERIAL_TRANSFER_ADDR  (0xFF01) // 2 bytes
#define TIMER_ADDR            (0xFF04) // 2 bytes
#define DIVIDER_ADDR          (0xFF06) // 2 bytes
#define BOOT_ROM_MAPPING_ADDR (0xFF50)
#define OAM_DMA_ADDR          (0xFF46)

//=====================================================================================================================
// Typedefs
//=====================================================================================================================

typedef struct
{
    uint16_t addr;
    addressWriteCallback cb;
} SAddressWriteCallback_t;

typedef struct
{
    const SCartridge_t *pCurrentCartridge;
    size_t countRegisteredCallbacks;
    SAddressWriteCallback_t cbRegister[ADDRESS_CALLBACK_TYPE_COUNT];
    bool dmaTransferInProgress;
    bool bootromIsMapped;
} SMemoryBusState_t;

//=====================================================================================================================
// Globals
//=====================================================================================================================

bus_t             g_bus      = { 0 };
SMemoryBusState_t g_busState = { 0 };

const uint8_t     g_bootrom[] = {
  /* 0x00 */ 0x31, 0xfe, 0xff, 0xaf, 0x21, 0xff, 0x9f, 0x32, 0xcb, 0x7c, 0x20, 0xfb, 0x21, 0x26, 0xff, 0x0e,
  /* 0x10 */ 0x11, 0x3e, 0x80, 0x32, 0xe2, 0x0c, 0x3e, 0xf3, 0xe2, 0x32, 0x3e, 0x77, 0x77, 0x3e, 0xfc, 0xe0,
  /* 0x20 */ 0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1a, 0xcd, 0x95, 0x00, 0xcd, 0x96, 0x00, 0x13, 0x7b,
  /* 0x30 */ 0xfe, 0x34, 0x20, 0xf3, 0x11, 0xd8, 0x00, 0x06, 0x08, 0x1a, 0x13, 0x22, 0x23, 0x05, 0x20, 0xf9,
  /* 0x40 */ 0x3e, 0x19, 0xea, 0x10, 0x99, 0x21, 0x2f, 0x99, 0x0e, 0x0c, 0x3d, 0x28, 0x08, 0x32, 0x0d, 0x20,
  /* 0x50 */ 0xf9, 0x2e, 0x0f, 0x18, 0xf3, 0x67, 0x3e, 0x64, 0x57, 0xe0, 0x42, 0x3e, 0x91, 0xe0, 0x40, 0x04,
  /* 0x60 */ 0x1e, 0x02, 0x0e, 0x0c, 0xf0, 0x44, 0xfe, 0x90, 0x20, 0xfa, 0x0d, 0x20, 0xf7, 0x1d, 0x20, 0xf2,
  /* 0x70 */ 0x0e, 0x13, 0x24, 0x7c, 0x1e, 0x83, 0xfe, 0x62, 0x28, 0x06, 0x1e, 0xc1, 0xfe, 0x64, 0x20, 0x06,
  /* 0x80 */ 0x7b, 0xe2, 0x0c, 0x3e, 0x87, 0xe2, 0xf0, 0x42, 0x90, 0xe0, 0x42, 0x15, 0x20, 0xd2, 0x05, 0x20,
  /* 0x90 */ 0x4f, 0x16, 0x20, 0x18, 0xcb, 0x4f, 0x06, 0x04, 0xc5, 0xcb, 0x11, 0x17, 0xc1, 0xcb, 0x11, 0x17,
  /* 0xA0 */ 0x05, 0x20, 0xf5, 0x22, 0x23, 0x22, 0x23, 0xc9, 0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b,
  /* 0xB0 */ 0x03, 0x73, 0x00, 0x83, 0x00, 0x0c, 0x00, 0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
  /* 0xC0 */ 0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63, 0x6e, 0x0e, 0xec, 0xcc,
  /* 0xD0 */ 0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e, 0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0x42, 0x3c,
  /* 0xE0 */ 0x21, 0x04, 0x01, 0x11, 0xa8, 0x00, 0x1a, 0x13, 0xbe, 0x20, 0xfe, 0x23, 0x7d, 0xfe, 0x34, 0x20,
  /* 0xF0 */ 0xf5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xfb, 0x86, 0x20, 0xfe, 0x3e, 0x01, 0xe0, 0x50
};

size_t   g_bootromLen = 0xFF;

//=====================================================================================================================
// Local function prototypes
//=====================================================================================================================

static void handleIORegWrite8(uint8_t, uint16_t);
static void handleIORegWrite16(uint16_t, uint16_t);

//=====================================================================================================================
// Function definitions
//=====================================================================================================================

void bindCallbackToAddress(uint16_t addr, EAddressCallbackType_t type, addressWriteCallback cb)
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

    g_busState.bootromIsMapped = !skipBootrom;

    g_bus.map.pRom0 = g_busState.pCurrentCartridge->pCurrentRomBank0;
    g_bus.map.pRom1 = g_busState.pCurrentCartridge->pCurrentRomBank1;
    g_bus.map.pEram = g_busState.pCurrentCartridge->pCurrentRamBank;
}

void overrideBus(bus_t *pBus)
{
    memcpy(&g_bus, pBus, sizeof(bus_t));
}

const uint8_t fetch8(uint16_t addr)
{
    if (addr == 0xFF00) return 0xFF;

    uint8_t ret = g_bus.bus[addr];

    if (addr <= ROM0_END)
    {
        ret = g_busState.bootromIsMapped ? g_bootrom[addr] : g_bus.map.pRom0[addr];
    }
    else if (addr <= ROM1_END)
    {
        ret = g_bus.map.pRom1[addr - ROMN_SIZE];
    }
    else if (addr >= ERAM_START && addr <= ERAM_END)
    {
        ret = (g_busState.pCurrentCartridge->cartramEnabled && g_bus.map.pEram != nullptr) ? g_bus.map.pEram[addr - ERAM_START] : 0xFF;
    }

    return ret;
}

const uint16_t fetch16(uint16_t addr)
{
    uint8_t *dataSrcPtr = g_bus.bus;

    if (addr <= ROM0_END)
    {
        dataSrcPtr = g_busState.bootromIsMapped ? g_bootrom : g_bus.map.pRom0;
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

bus_t *pGetBusPtr(void)
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
            //memset(&g_bus.map.ioregs.joypad, 0xFF, 1);
            if (g_busState.cbRegister[JOYPAD_REG_CALLBACK].cb != nullptr) { g_busState.cbRegister[JOYPAD_REG_CALLBACK].cb(data); }
            break;
        }
        case BOOT_ROM_MAPPING_ADDR:
        {
            g_bus.map.ioregs.disableBootrom = 0;
            g_busState.bootromIsMapped = false;
            break;
        }
        case OAM_DMA_ADDR:
        {
            memcpy(&g_bus.map.oam, &g_bus.bus[(uint16_t)(data << 8)], 0x9F);
            // costs 160 mcycles! handle in cb
            if (g_busState.cbRegister[OAM_DMA_CALLBACK].cb != nullptr) { g_busState.cbRegister[OAM_DMA_CALLBACK].cb(data); }
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
            //memset(&g_bus.map.ioregs.joypad, 0xFF, 1);
            if (g_busState.cbRegister[JOYPAD_REG_CALLBACK].cb != nullptr) { g_busState.cbRegister[JOYPAD_REG_CALLBACK].cb(data); }
            break;
        }
        case BOOT_ROM_MAPPING_ADDR:
        {
            g_bus.map.ioregs.disableBootrom = 0;
            g_busState.bootromIsMapped = false;
            break;
        }
        case OAM_DMA_ADDR:
        {
            memcpy(&g_bus.map.oam, &g_bus.bus[(uint16_t)(data << 8)], 0x9F);
            // costs 160 mcycles! handle in cb
            if (g_busState.cbRegister[OAM_DMA_CALLBACK].cb != nullptr) { g_busState.cbRegister[OAM_DMA_CALLBACK].cb(data); }
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