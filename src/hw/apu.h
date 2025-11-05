/**
 * @file apu.h
 * @author Toesoe
 * @brief seaboy audio emulation
 * @version 0.1
 * @date 2025-06-25
 *
 */

#ifndef _APU_H_
#define _APU_H_

#include <stdint.h>
#include <stdlib.h>

#include "mem.h"

void apuInit(SAddressBus_t *);
void apuTick(size_t);

void generateDownmixCallback(void *, uint8_t *, int);

#endif //!_APU_H_