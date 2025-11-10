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

#include "cpu.h"
#include <stdint.h>

void log_instruction_fetch(uint8_t);

/**
 * @brief set the SCPURegisters_t ptr used in instr.c
 *
 */
void instrSetCpuPtr(SCPURegisters_t *);

/**
 * @brief load 8-bit register with immediate value
 *
 */
void ld_reg8_imm(Register8);

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
 * @brief set value at mem address to the following immediate 8-bit value
 *
 */
void ld_addr_imm8(uint16_t);

/**
 * @brief load register A from memory address in HL, decrement HL
 *
 */
void ldd_a_hl(void);

/**
 * @brief load value to memory address in HL from value in register a, decrement HL
 *
 */
void ldd_hl_a(void);

/**
 * @brief load register A from memory address in HL, increment HL
 *
 */
void ldi_a_hl(void);

/**
 * @brief load value to memory address in HL from value in register a, increment HL
 *
 */
void ldi_hl_a(void);

/**
 * @brief set value at (0xFF00 + offset) to value in register a
 *
 * @param uint8_t offset
 */
void ldh_offset_mem_a(uint8_t);

/**
 * @brief set register a to value at (0xFF00 + offset)
 *
 * @param uint8_t offset
 */
void ldh_a_offset_mem(uint8_t);

/**
 * @brief load a 16-bit register using immediate
 *
 */
void ld_reg16_imm(Register16);

/**
 * @brief set value at mem address to immediate 16-bit value
 *
 */
void ld_addr_imm16(uint16_t);

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

/**
 * @brief add 8-bit value to register A
 *
 * @note flags: Z0HC
 */
void add8_a_n(uint8_t);

/**
 * @brief add 8-bit value + carry to register A
 *
 * @note flags: Z0HC
 */
void adc8_a_n(uint8_t);

/**
 * @brief subtract 8-bit value from register A
 *
 * @note flags: Z1HC
 */
void sub8_n_a(uint8_t);

/**
 * @brief subtract 8-bit value + carry from register A
 *
 * @note flags: Z1HC
 */
void sbc8_a_n(uint8_t);

/**
 * @brief bitwise AND register A with 8-bit value
 *
 * @note flags: Z010
 */
void and8_a_n(uint8_t);

/**
 * @brief bitwise OR register A with 8-bit value
 *
 * @note flags: Z000
 */
void or8_a_n(uint8_t);

/**
 * @brief bitwise XOR register A with 8-bit value
 *
 * @note flags: Z000
 */
void xor8_a_n(uint8_t);

/**
 * @brief bitwise compare register A with 8-bit value
 *
 * @note flags: Z0HC
 */
void cp8_a_n(uint8_t);

/**
 * @brief increment 8-bit register
 *
 * @note flags: Z0H-
 */
void inc8_reg(Register8);

/**
 * @brief decrement 8-bit register
 *
 * @note flags: Z1H-
 */
void dec8_reg(Register8);

/**
 * @brief increment value at memory address
 *
 * @note flags: Z0H-
 */
void inc8_mem(uint16_t);

/**
 * @brief decrement value at memory address
 *
 * @note flags: Z1H-
 */
void dec8_mem(uint16_t);

/**
 * @brief add 16-bit value to HL
 *
 * @note flags: -0HC
 */
void add16_hl_n(uint16_t);

/**
 * @brief add 8-bit signed value to SP
 *
 * @note flags: 00HC
 */
void add8_sp_n(int8_t);

/**
 * @brief set HL to value of SP + 8-bit signed
 *
 * @note flags: 00HC
 */
void ldhl_sp_n(int8_t);

/**
 * @brief increment 16-bit register
 *
 * @note flags: ----
 */
void inc16_reg(Register16);

/**
 * @brief decrement 16-bit register
 *
 * @note flags: ----
 */
void dec16_reg(Register16);

/**
 * @brief swap low & high nibble of a register
 *
 * @note flags: Z000
 */
void swap8_reg(Register8);

/**
 * @brief swap low & high nibble of a memory value
 *
 * @note flags: Z000
 */
void swap8_addr(uint16_t);

/**
 * @brief decimal adjust A (bcd conversion of value in reg a)
 * @note  this instruction is weird. stole the implementation from https://forums.nesdev.org/viewtopic.php?t=15944
 *
 * @note flags: Z00C
 */
void daa(void);

/**
 * @brief complement A register (flip all bits)
 *
 * @note flags: -11-
 */
void cpl(void);

/**
 * @brief complement carry flag (invert)
 *
 * @note flags: -00C
 */
void ccf(void);

/**
 * @brief set carry flag
 *
 * @note flags: -001
 */
void scf(void);

/**
 * @brief rotate left carry, register a
 *
 * @note flags: 000C
 */
void rlca(void);

/**
 * @brief rotate left, register a
 *
 * @note flags: 000C
 */
void rla(void);

/**
 * @brief rotate right carry, register a
 *
 * @note flags: 000C
 */
void rrca(void);

/**
 * @brief rotate right, register a
 *
 * @note flags: 000C
 */
void rra(void);

/**
 * @brief rotate left carry, 8 bit register
 *
 * @note flags: Z00C
 */
void rlc_reg(Register8);

/**
 * @brief rotate left carry, memory value
 *
 * @note flags: Z00C
 */
void rlc_addr(uint16_t);

/**
 * @brief rotate left, 8 bit register
 *
 * @note flags: Z00C
 */
void rl_reg(Register8);

/**
 * @brief rotate lefft, memory value
 *
 * @note flags: Z00C
 */
void rl_addr(uint16_t);

/**
 * @brief rotate right carry, 8 bit register
 *
 * @note flags: Z00C
 */
void rrc_reg(Register8);

/**
 * @brief rotate right carry, memory value
 *
 * @note flags: Z00C
 */
void rrc_addr(uint16_t);

/**
 * @brief rotate right, 8 bit register
 *
 * @note flags: Z00C
 */
void rr_reg(Register8);

/**
 * @brief rotate right, memory value
 *
 * @note flags: Z00C
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
 * @note flags: Z00C
 */
void sla_addr(uint16_t);

/**
 * @brief shift right carry, 8 bit register, MSB keep
 *
 * @note flags: Z00C
 */
void sra_reg(Register8);

/**
 * @brief shift right carry, memory value, MSB keep
 *
 * @note flags: Z00C
 */
void sra_addr(uint16_t);

/**
 * @brief shift right carry, 8 bit register, MSB 0
 *
 * @note flags: Z00C
 */
void srl_reg(Register8);

/**
 * @brief shift right carry, memory value, MSB 0
 *
 * @note flags: Z00C
 */
void srl_addr(uint16_t);

/**
 * @brief set Z if bit in register is set
 *
 * @note flags: Z01-
 */
void bit_n_reg(uint8_t, Register8);

/**
 * @brief set Z if bit of value at memory address is set
 *
 * @note flags: Z01-
 */
void bit_n_addr(uint8_t, uint16_t);

/**
 * @brief set bit N of register
 *
 * @note flags: ----
 */
void set_n_reg(uint8_t, Register8);

/**
 * @brief set bit N of value at memory address
 *
 * @note flags: ----
 */
void set_n_addr(uint8_t, uint16_t);

/**
 * @brief reset bit N of register
 *
 * @note flags: ----
 */
void reset_n_reg(uint8_t, Register8);

/**
 * @brief reset bit N of value at memory address
 *
 * @note flags: ----
 */
void reset_n_addr(uint8_t, uint16_t);

/**
 * @brief unconditional jump to immediate value
 *
 */
void jmp_imm16();

/**
 * @brief perform nonrelative conditional jump to 16-bit immediate value
 *
 * @param flag flag to test
 * @param testSet if true, will execute jump if specified flag is set
 * @return true     if jumped
 * @return false    if not (test was false)
 */
bool jmp_imm16_cond(Flag, bool);

/**
 * @brief perform nonrelative unconditional jump to address specified in HL
 *
 */
void jmp_hl(void);

/**
 * @brief perform relative unconditional jump to signed 8-bit immediate
 *
 */
void jr_imm8();

/**
 * @brief perform relative conditional jump to signed 8-bit immediate
 *
 * @param flag      flag to test
 * @param testSet   if true, will test if <flag> is set, otherwise will test for 0
 * @return true     if jumped
 * @return false    if not (test was false)
 */
bool jr_imm8_cond(Flag, bool);

/**
 * @brief call subroutine at unsigned 16-bit immediate
 *
 * @note pushes address of first instr after subroutine to stack, decrements SP by 2
 */
void call_imm16();

/**
 * @brief conditionally call subroutine at unsigned 16-bit immediate
 *
 * @param flag      flag to test
 * @param testSet   if true, will test if <flag> is set, otherwise will test for 0
 * @return true     if jumped
 * @return false    if not (test was false)
 */
bool call_imm16_cond(Flag, bool);

/**
 * @brief call subroutine at specified offset (max 0x60)
 *
 */
void call_irq_subroutine(uint8_t);

/**
 * @brief call RST page between 0 and 8
 *
 * @note stores next valid PC on stack. RSTs are at 0x00, 0x08, 0x10, etc
 *
 * @param addr RST addr to call
 */
void rst_n(uint8_t);

/**
 * @brief return from subroutine. pulls next PC address from stack
 *
 */
void ret(void);

/**
 * @brief conditionally return from subroutine
 *
 * @param flag      flag to test
 * @param testSet   if true, will test if <flag> is set, otherwise will test for 0
 * @return true     if returned
 * @return false    if not (test was false)
 */
bool ret_cond(Flag, bool);

/**
 * @brief decode and execute current cycle state
 *
 */
void decodeAndExecute(SCPUCurrentCycleState_t *);

#endif //!_INSTR_H_