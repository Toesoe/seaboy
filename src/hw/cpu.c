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
 * @param flag flag to set
 */
void setFlag(Flag flag)
{
    cpu.reg8.f |= (1 << flag);
}

/**
 * @brief reset a specific flag
 * 
 * @param flag flag to reset
 */
void resetFlag(Flag flag)
{
    cpu.reg8.f &= ~(1 << flag);
}

/**
 * @brief check if a flag is set
 * 
 * @param flag flag to check
 * @return true if set
 * @return false if unset
 */
bool testFlag(Flag flag)
{
    return cpu.reg8.f & (1 << flag);
}

/**
 * @brief step the program counter
 * 
 * @param is16 if true, will increment PC by 2 when a 16-bit instruction has been executed
 * @note yeah, I know, I perform an addition with a bool, but the standard specifies it as 1
 */
void stepCpu(bool is16)
{
    cpu.reg16.pc += 1 + is16;
}

/**
 * @brief set PC to a specific address
 * 
 * @param addr address to set PC to
 */
void jumpCpu(uint16_t addr)
{
    cpu.reg16.sp = addr;
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
    cpu.reg16_arr[reg] = value;
}

/**
 * @brief set a specific 8-bit register
 * 
 * @param reg register to set
 * @param value value to write to the register
 */
void setRegister8(Register8 reg, uint8_t value)
{
    cpu.reg8_arr[reg] = value;
}

/**
 * @brief change the interrupt master enable flag
 */
void changeIME(bool enable)
{
    enableInterrupts = enable;
}