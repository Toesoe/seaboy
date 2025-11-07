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

#include <stdbool.h>
#include <stdint.h>

#define CPU_CLOCK_SPEED_HZ (4194304)

typedef enum _Flag
{
    FLAG_C = 4, // carry
    FLAG_H,     // half-carry
    FLAG_N,     // subtract flag (BCD)
    FLAG_Z      // zero flag
} Flag;

typedef enum _Register16
{
    AF = 0,
    BC,
    DE,
    HL,
    SP,
    PC
} Register16;

// reversed order due to endianness
typedef enum _Register8
{
    F = 0,
    A,
    C,
    B,
    E,
    D,
    L,
    H,
} Register8;

typedef union __attribute__((__packed__))
{
    struct __attribute__((__packed__))
    {
        uint16_t af;
        uint16_t bc;
        uint16_t de;
        uint16_t hl;
        uint16_t sp;
        uint16_t pc;
    } reg16;

    // reversed order due to endianness
    struct __attribute__((__packed__))
    {
        uint8_t  f;
        uint8_t  a;
        uint8_t  c;
        uint8_t  b;
        uint8_t  e;
        uint8_t  d;
        uint8_t  l;
        uint8_t  h;
        uint32_t _nouse_sp_pc;
    } reg8;

    uint16_t reg16_arr[6];
    uint8_t  reg8_arr[12];
} SCPURegisters_t;

// functions
/**
 * @brief reset cpu to initial state
 */
void resetCpu(bool);

void overrideCpu(SCPURegisters_t *);

/**
 * @brief set a specific flag
 */
void setFlag(Flag);

/**
 * @brief reset a specific flag
 */
void resetFlag(Flag);

/**
 * @brief check if a flag is set
 */
bool testFlag(Flag);

/**
 * @brief step the program counter
 */
void stepCpu(int);

/**
 * @brief set PC to a specific address
 */
void jumpCpu(uint16_t);

/**
 * @brief retrieve constptr to cpu object
 */
const SCPURegisters_t *pGetCPURegisters(void);

/**
 * @brief set a specific 16-bit register
 */
void setRegister16(Register16, uint16_t);

/**
 * @brief set a specific 8-bit register
 */
void setRegister8(Register8, uint8_t);

void setIME();
void resetIME();
bool checkIME();

bool checkHalted();
bool checkStopped();

/**
 * @brief map instruction to actual decoding function
 * @return total execution cycles used for the last instruction
 */
int  executeInstruction(uint8_t);

int  handleInterrupts(void);

void handleTimers(int);

#endif // !_CPU_H_