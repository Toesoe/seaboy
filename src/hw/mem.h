/**
 * @file bus.h
 * @author Toesoe
 * @brief seaboy mem (ram, vram)
 * @version 0.1
 * @date 2023-06-15
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _MEM_H_
#define _MEM_H_

#include <stdint.h>

#define GB_WRAM_SIZE_B 8192
#define GB_VRAM_SIZE_B 8192

uint8_t  fetch8(uint16_t addr);
uint16_t fetch16(uint16_t addr);

void write8(uint8_t val, uint16_t addr);
void write16(uint16_t val, uint16_t addr);

#endif // !_MEM_H_