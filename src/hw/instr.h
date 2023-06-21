/**
 * @file instr.h
 * @author Toesoe
 * @brief seaboy SM83 instructions
 * @version 0.1
 * @date 2023-06-13
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef _INSTR_H_
#define _INSTR_H_

#include <stdint.h>
#include "cpu.h"

/**
 * @brief set the cpu_t ptr used in instr.c
 * 
 */
void setCpuPtr(const cpu_t *);

/**
 * @brief load 8-bit register with immediate value
 * 
 */
void ld_reg8_imm(Register8, uint8_t);

/**
 * @brief load 8-bit register from mem address
 * 
 */
void ld_reg8_addr(Register8, uint16_t);

/**
 * @brief load 8-bit register from another 8-bit register
 * 
 */
void ld_reg8_reg8(Register8, Register8);

/**
 * @brief set value at mem address to 8-bit reg value
 * 
 */
void ld_addr_reg8(uint16_t, Register8);

/**
 * @brief set value at mem address to immediate value
 * 
 */
void ld_addr_imm(uint16_t, uint8_t);

/**
 * @brief load-and-decrement register a from memory address in HL
 * 
 */
void ldd_a_hl(void);

/**
 * @brief load-and-decrement value at memory address in h from value in register a
 * 
 */
void ldd_hl_a(void);

/**
 * @brief load-and-increment register a from memory address in HL
 * 
 */
void ldi_a_hl(void);

/**
 * @brief load-and-increment value at memory address in h from value in register a
 * 
 */
void ldi_hl_a(void);

/**
 * @brief set value at (mem address + offset) to value in register a
 * 
 */
void ldh_offset_mem_a(uint8_t);

/**
 * @brief set register a to value at (mem address + offset)
 * 
 */
void ldh_a_offset_mem(uint8_t);