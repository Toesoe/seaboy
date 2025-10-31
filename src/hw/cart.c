/**
 * @file cart.c
 * @author Toesoe
 * @brief seaboy cartridge emulation
 * @version 0.1
 * @date 2023-06-13
 *
 * @copyright Copyright (c) 2023

 // TODO: dynamically allocate cartram based on mapper type ($0147)
 *
 */

#include <fcntl.h> // For open
#include <stdio.h>
#include <sys/mman.h> // For mmap, munmap
#include <sys/stat.h> // For fstat
#include <unistd.h>   // For close

#include "cart.h"
#include "mem.h"

#define DEBUG_WRITES
#define MAPPER_CARTRIDGE_TYPE_ADDRESS (0x147)
#define ROM_SIZE_ADDRESS (0x148)
#define RAM_SIZE_ADDRESS (0x149)

SCartridge_t g_cartridge = {0};

/**
 * map a romfile into memory
 */
const SCartridge_t *pLoadRom(const char *filename)
{
    struct stat sb;

    int         fd = open(filename, O_RDONLY);

    if (fd == -1)
    {
        printf("cannot open file %s\n", filename);
        close(fd);
        return nullptr;
    }

    if (fstat(fd, &sb) == -1)
    {
        printf("cannot retrieve filesize\n");
        close(fd);
        return nullptr;
    }

    g_cartridge.pRom = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (g_cartridge.pRom == MAP_FAILED)
    {
        printf("cannot map rom in memory\n");
        close(fd);
        return nullptr;
    }

    close(fd);

    g_cartridge.pCurrentRomBank0 = g_cartridge.pRom;
    g_cartridge.pCurrentRomBank1 = &g_cartridge.pRom[ROMN_SIZE];
    g_cartridge.pCurrentRamBank = nullptr;
    g_cartridge.mapperType = MAPPER_NONE;

    // determine mapper type
    switch (g_cartridge.pRom[MAPPER_CARTRIDGE_TYPE_ADDRESS])
    {
        case 0x01:
        {
            g_cartridge.mapperType = MBC1;
            break;
        }
        case 0x02:
        {
            g_cartridge.mapperType = MBC1;
            g_cartridge.mapperHasRam = true;
            break;
        }
        case 0x03:
        {
            g_cartridge.mapperType = MBC1;
            g_cartridge.mapperHasRam = true;
            g_cartridge.mapperHasBattery = true;
            break;
        }
        case 0x05:
        {
            g_cartridge.mapperType = MBC2;
            break;
        }
        case 0x06:
        {
            g_cartridge.mapperType = MBC2;
            g_cartridge.mapperHasBattery = true;
            break;
        }
        case 0x0B:
        {
            g_cartridge.mapperType = MMM01;
            break;
        }
        case 0xC:
        {
            g_cartridge.mapperType = MMM01;
            g_cartridge.mapperHasRam = true;
            break;
        }
        case 0x0D:
        {
            g_cartridge.mapperType = MMM01;
            g_cartridge.mapperHasRam = true;
            g_cartridge.mapperHasBattery = true;
            break;
        }
        case 0x0F:
        {
            g_cartridge.mapperType = MBC3;
            g_cartridge.mapperHasRTC = true;
            g_cartridge.mapperHasBattery = true;
            break;
        }
        case 0x10:
        {
            g_cartridge.mapperType = MBC3;
            g_cartridge.mapperHasRTC = true;
            g_cartridge.mapperHasRam = true;
            g_cartridge.mapperHasBattery = true;
            break;
        }
        case 0x11:
        {
            g_cartridge.mapperType = MBC3;
            break;
        }
        case 0x12:
        {
            g_cartridge.mapperType = MBC3;
            g_cartridge.mapperHasRam = true;
            break;
        }
        case 0x13:
        {
            g_cartridge.mapperType = MBC3;
            g_cartridge.mapperHasRam = true;
            g_cartridge.mapperHasBattery = true;
            break;
        }
        case 0x19:
        {
            g_cartridge.mapperType = MBC5;
            break;
        }
        case 0x1A:
        {
            g_cartridge.mapperType = MBC5;
            g_cartridge.mapperHasRam = true;
            break;
        }
        case 0x1B:
        {
            g_cartridge.mapperType = MBC5;
            g_cartridge.mapperHasRam = true;
            g_cartridge.mapperHasBattery = true;
            break;
        }
        case 0x1C:
        {
            g_cartridge.mapperType = MBC5;
            g_cartridge.mapperHasRumble = true;
            break;
        }
        case 0x1D:
        {
            g_cartridge.mapperType = MBC5;
            g_cartridge.mapperHasRumble = true;
            g_cartridge.mapperHasRam = true;
            break;
        }
        case 0x1E:
        {
            g_cartridge.mapperType = MBC5;
            g_cartridge.mapperHasRumble = true;
            g_cartridge.mapperHasRam = true;
            g_cartridge.mapperHasBattery = true;
            break;
        }
        default: break;
    }

    // determine ROM size
    g_cartridge.romSize = 32768 * (1 << g_cartridge.pRom[ROM_SIZE_ADDRESS]);

    // determine cartram size
    switch (g_cartridge.pRom[RAM_SIZE_ADDRESS])
    {
        case 0x02:
        {
            g_cartridge.cartRamSize = 8192;
            g_cartridge.pCartRam = calloc(g_cartridge.cartRamSize, sizeof(uint8_t));
            break;
        }
        case 0x03:
        {
            g_cartridge.cartRamSize = 32768;
            g_cartridge.pCartRam = calloc(g_cartridge.cartRamSize, sizeof(uint8_t));
            break;
        }
        case 0x04:
        {
            g_cartridge.cartRamSize = 131072;
            g_cartridge.pCartRam = calloc(g_cartridge.cartRamSize, sizeof(uint8_t));
            break;
        }
        case 0x05:
        {
            g_cartridge.cartRamSize = 65536;
            g_cartridge.pCartRam = calloc(g_cartridge.cartRamSize, sizeof(uint8_t));
            break;
        }
        default: break;
    }

    return &g_cartridge;
}

void performBankSwitch(uint16_t val, uint16_t addr)
{
    if ((addr > 0x0000) && (addr < 0x2000))
    {
        // RAM enable
        if ((val & 0xF) == 0xA)
        {
            printf("enabling cartram\n");
            g_cartridge.cartramEnabled = true;
        }
        else
        {
            printf("disabling cartram\n");
            g_cartridge.cartramEnabled = false;
        }
    }
    else if ((addr >= 0x2000) && (addr < ROMN_SIZE))
    {
        // MBC1: set low bank, all others: set high bank

        // if (MBC1) {}
        // switch bank. 0x1 -> 0x1F
        val &= 0x1F;
        printf("switching rombank to %d\n", val);
        g_cartridge.selectedRomBankNum = val;
        g_cartridge.pCurrentRomBank1 = &(g_cartridge.pRom[val * ROMN_SIZE]);
    }
    else if ((addr >= ROMN_SIZE) && (addr < 0x6000))
    {
        if (g_cartridge.mapperHasRam && g_cartridge.pCartRam)
        {
            size_t bankCount = g_cartridge.cartRamSize / ERAM_SIZE;
            val %= bankCount;
            g_cartridge.selectedRamBankNum = val;
            g_cartridge.pCurrentRamBank = &(g_cartridge.pCartRam[val * ERAM_SIZE]);
        }
    }
    else if ((addr >= 0x6000) && (addr <= 0x8000))
    {
        // banking mode select
        if (val & 0x01)
        {
            // advanced banking
            printf("enabling advanced banking mode");
            g_cartridge.advancedBankingModeEnabled = true;
        }
        else
        {
            printf("disabling advanced banking mode");
            g_cartridge.advancedBankingModeEnabled = false;
        }
    }
}

void writeCartRam8(uint8_t val, uint16_t addr)
{
    if (!g_cartridge.cartramEnabled || ((addr - ERAM_START) > g_cartridge.cartRamSize)) return;
#ifdef DEBUG_WRITES
    printf("CARTRAM WRITE %02X at %04X bank %zu\n", val, addr, g_cartridge.selectedRamBankNum);
#endif
    *(g_cartridge.pCurrentRamBank + (addr - ERAM_START)) = val;
}

void writeCartRam16(uint16_t val, uint16_t addr)
{
    if (!g_cartridge.cartramEnabled || ((addr - ERAM_START) > g_cartridge.cartRamSize)) return;
#ifdef DEBUG_WRITES
    printf("CARTRAM WRITE %04X at %04X bank %zu\n", val, addr, g_cartridge.selectedRamBankNum);
#endif
    *(uint16_t *)(g_cartridge.pCurrentRamBank + (addr - ERAM_START)) = val;
}

uint8_t readCartRam8(uint16_t addr)
{
    if (!g_cartridge.cartramEnabled || ((addr - ERAM_START) > g_cartridge.cartRamSize)) return 0xFF;
    return *(uint8_t *)(g_cartridge.pCurrentRamBank + (addr - ERAM_START));
}

uint16_t readCartRam16(uint16_t addr)
{
    if (!g_cartridge.cartramEnabled || ((addr - ERAM_START) > g_cartridge.cartRamSize)) return 0xFFFF;
    return *(uint16_t *)(g_cartridge.pCurrentRamBank + (addr - ERAM_START));
}