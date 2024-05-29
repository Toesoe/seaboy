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
void instrSetCpuPtr(cpu_t *);

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

/**
 * @brief load a 16-bit register with an immediate value
 * 
 */
void ld_reg16_imm(Register16, uint16_t);

/**
 * @brief set HL register to SP + an offfset
 * 
 */
void ldhl_sp_offset(uint8_t);

/**
 * @brief push a specific register onto the stack
 * 
 */
void push_reg16(Register16);

/**
 * @brief pop a specific register from the stack
 * 
 */
void pop_reg16(Register16);

void add8_a_n(uint8_t);
void adc8_a_n(uint8_t);
void sub8_n_a(uint8_t);
void sbc8_a_n(uint8_t);
void and8_a_n(uint8_t);
void or8_a_n(uint8_t);
void xor8_a_n(uint8_t);
void cp8_a_n(uint8_t);
void inc8_reg(Register8);
void dec8_reg(Register8);

void inc8_mem(uint16_t);
void dec8_mem(uint16_t);

void add16_hl_n(uint16_t);
void dec16_hl_n(uint16_t);
void add16_sp_n(uint16_t);
void inc16_reg(Register16);
void dec16_reg(Register16);

/**
 * @brief swap low & high nibble of a register
 */
void swap8_reg(Register8);

/**
 * @brief swap low & high nibble of a memory value
 * 
 */
void swap8_addr(uint16_t);

/**
 * @brief decimal adjust A register (bcd conversion)
 * 
 */
void daa(void);

/**
 * @brief complement A register (flip all bits)
 * 
 */
void cpl(void);

/**
 * @brief complement carry flag (invert)
 * 
 */
void ccf(void);

/**
 * @brief set carry flag
 * 
 */
void scf(void);

/**
 * @brief rotate left carry, register a
 * 
 */
void rlca(void);

/**
 * @brief rotate left, register a
 * 
 */
void rla(void);

/**
 * @brief rotate right carry, register a
 * 
 */
void rrca(void);

/**
 * @brief rotate right, register a
 * 
 */
void rra(void);

/**
 * @brief rotate left carry, 8 bit register
 * 
 */
void rlc_reg(Register8);

/**
 * @brief rotate left carry, memory value
 * 
 */
void rlc_addr(uint16_t);

/**
 * @brief rotate left, 8 bit register
 * 
 */
void rl_reg(Register8);

/**
 * @brief rotate lefft, memory value
 * 
 */
void rl_addr(uint16_t);

/**
 * @brief rotate right carry, 8 bit register
 * 
 */
void rrc_reg(Register8);

/**
 * @brief rotate right carry, memory value
 * 
 */
void rrc_addr(uint16_t);

/**
 * @brief rotate right, 8 bit register
 * 
 */
void rr_reg(Register8);

/**
 * @brief rotate right, memory value
 * 
 */
void rr_addr(uint16_t);

/**
 * @brief shift left carry, 8 bit register, LSB 0
 * 
 */
void sla_reg(Register8);

/**
 * @brief shift left carry, memory value, LSB 0
 * 
 */
void sla_addr(uint16_t);

/**
 * @brief shift right carry, 8 bit register, MSB keep
 * 
 */
void sra_reg(Register8);

/**
 * @brief shift right carry, memory value, MSB keep
 * 
 */
void sra_addr(uint16_t);

/**
 * @brief shift right carry, 8 bit register, MSB 0
 * 
 */
void srl_reg(Register8);

/**
 * @brief shift right carry, memory value, MSB 0
 * 
 */
void srl_addr(uint16_t);

void bit_n_reg(uint8_t, Register8);
void bit_n_addr(uint8_t, uint16_t);
void set_n_reg(uint8_t, Register8);
void set_n_addr(uint8_t, uint16_t);
void reset_n_reg(uint8_t, Register8);
void reset_n_addr(uint8_t, uint16_t);

void jmp_nn(uint16_t);
bool jmp_nn_cond(uint16_t, Flag, bool);
void jmp_hl(void);
void jr_n(int8_t);
void jr_n_signed(int8_t);
bool jr_n_cond(int8_t, Flag, bool);
bool jr_n_cond_signed(int8_t, Flag, bool);

void call_nn(uint16_t);
bool call_nn_cond(uint16_t, Flag, bool);
void rst_n(uint8_t);
void ret(void);
bool ret_cond(Flag, bool);
void reti(void);

#endif //!_INSTR_H_