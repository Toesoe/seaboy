/**
 * @file cpu.h
 * @author Toesoe
 * @brief seaboy SM83 emulation
 * @version 0.1
 * @date 2023-06-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _CPU_H_
#define _CPU_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum _Flag
{
    c = 4,
    h,
    n,
    z
} Flag;

typedef enum _Register16
{
    AF = 0,
    BC,
    DE,
    HL
} Register16;

typedef enum _Register8
{
    A = 0,
    B = 2,
    C,
    D,
    E,
    H,
    L
} Register8;

typedef struct
{
    uint16_t  af;
    uint16_t  bc;
    uint16_t  de;
    uint16_t  hl;
    uint16_t  sp;
    uint16_t  pc;
} cpu_t;

// functions
/**
 * @brief reset cpu to initial state
 */
void resetCpu(void);

/**
 * @brief set a specific flag
*/
void setFlag(Flag);

/**
 * @brief reset a specific flag
 */
void resetFlag(Flag);

/**
 * @brief step the program counter
 */
void stepCpu(bool);

/**
 * @brief set PC to a specific address
 */
void jumpCpu(uint16_t);

/**
 * @brief retrieve constptr to cpu object
 */
const cpu_t *getCpuObject(void);

/**
 * @brief set a specific 16-bit register
 */
void setRegister16(Register16, uint16_t);

/**
 * @brief set a specific 8-bit register
 */
void setRegister8(Register8, uint8_t);

/**
 * @brief change the interrupt master enable flag
 */
void changeIME(bool);

#endif // !_CPU_H_