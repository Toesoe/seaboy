/**
 * @file joypad.h
 * @author Toesoe
 * @brief seaboy input handling
 * @version 0.1
 * @date 31-10-2025
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <stdint.h>

void joypadInit(void);
void joypadEventLoop(void);
void joypadShiftValuesToReg(uint8_t, uint16_t);