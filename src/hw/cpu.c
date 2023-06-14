/**
 * @file cpu.c
 * @author Toesoe
 * @brief seaboy SM83 emulation
 * @version 0.1
 * @date 2023-06-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "cpu.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

static cpu_t cpu;

static bool enableInterrupts;

/**
 * @brief reset cpu to initial state
 */
void resetCpu(void)
{
    memset(&cpu, 0, sizeof(cpu));
    enableInterrupts = true;
}

/**
 * @brief set a specific flag
 * 
 * @param flag flag_t type to set
 */
void setFlag(Flag flag)
{
    cpu.af |= (1 << flag);
}

/**
 * @brief reset a specific flag
 * 
 * @param flag flag_t type to set
 */
void resetFlag(Flag flag)
{
    cpu.af &= ~(1 << flag);
}

/**
 * @brief step the program counter
 * 
 * @param is16 if true, will increment PC by 2 when a 16-bit instruction has been executed
 * @note yeah, I know, I perform an addition with a bool, but the standard specifies it as 1
 */
void stepCpu(bool is16)
{
    cpu.pc += 1 + is16;
}

/**
 * @brief set PC to a specific address
 * 
 * @param addr address to set PC to
 */
void jumpCpu(uint16_t addr)
{
    cpu.pc = addr;
}

/**
 * @brief retrieve constptr to cpu object
 */
const cpu_t *getCpuObject(void)
{
    return (const cpu_t *)&cpu;
}

/**
 * @brief set a specific 16-bit register
 * 
 * @param reg register to set
 * @param value value to write to the register
 */
void setRegister16(Register16 reg, uint16_t value)
{
    memcpy(&cpu + (reg * sizeof(uint16_t)), &value, sizeof(uint16_t));
}

/**
 * @brief set a specific 8-bit register
 * 
 * @param reg register to set
 * @param value value to write to the register
 */
void setRegister8(Register8 reg, uint8_t value)
{
    memcpy(&cpu + (reg * sizeof(uint8_t)), &value, sizeof(uint8_t));
}

/**
 * @brief change the interrupt master enable flag
 */
void changeIME(bool enable)
{
    enableInterrupts = enable;
}