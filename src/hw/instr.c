/**
 * @file instr.c
 * @author Toesoe
 * @brief seaboy SM83 instructions
 * @version 0.1
 * @date 2023-06-13
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "instr.h"

#include "cpu.h"
#include "mem.h"

#include <assert.h>
#include <stdio.h>

static SCPURegisters_t *g_pCPURegisters;

#define CHECK_HALF_CARRY_ADD8(a, b)  ((((a) & 0xF) + ((b) & 0xF)) & 0x10)
#define CHECK_HALF_CARRY_SUB8(a, b)  ((((a) & 0xF) - ((b) & 0xF)) & 0x10)
#define CHECK_HALF_CARRY_ADC8(a, b, c)  ((((a) & 0xF) + ((b) & 0xF) + ((c) & 0xF)) & 0x10)
#define CHECK_HALF_CARRY_SBC8(a, b, c)  ((((a) & 0xF) - ((b) & 0xF) - ((c) & 0xF)) & 0x10)

#define CHECK_HALF_CARRY_ADD16(a, b) (((((a) & 0xFFF) + ((b) & 0xFFF)) & 0x1000) == 0x1000)
#define CHECK_HALF_CARRY_SUB16(a, b) (((a) & 0xFFF) < ((b) & 0xFFF))

// static defs
static uint8_t _rl(uint8_t val);
static uint8_t _rlc(uint8_t val);
static uint8_t _sla(uint8_t val);
static uint8_t _rr(uint8_t val);
static uint8_t _rrc(uint8_t val);
static uint8_t _sra(uint8_t val);
static uint8_t _srl(uint8_t val);

static size_t  decodeAndExecuteCBPrefix(void);
static size_t  getRegIndexByOpcodeNibble(uint8_t);

void           instrSetCpuPtr(SCPURegisters_t *pCpuSet)
{
    g_pCPURegisters = pCpuSet;
}

// 8 bit loads

void ld_reg8_imm(Register8 reg)
{
    setRegister8(reg, fetch8(g_pCPURegisters->reg16.pc + 1));
}

void ld_reg8_addr(Register8 left, uint16_t addr)
{
    setRegister8(left, fetch8(addr));
}

void ld_reg8_reg8(Register8 left, Register8 right)
{
    setRegister8(left, g_pCPURegisters->reg8_arr[right]);
}

/**
 * @brief set value at mem address to 8-bit reg value
 *
 * @param addr address to write to
 * @param reg register containing value to write
 */
void ld_addr_reg8(uint16_t addr, Register8 reg)
{
    write8(g_pCPURegisters->reg8_arr[reg], addr);
}

void ld_addr_imm8(uint16_t addr)
{
    write8(fetch8(g_pCPURegisters->reg16.pc + 1), addr);
}

void ldd_a_hl(void)
{
    setRegister8(A, fetch8(g_pCPURegisters->reg16.hl--));
}

void ldd_hl_a(void)
{
    write8(g_pCPURegisters->reg8.a, g_pCPURegisters->reg16.hl--);
}

void ldi_a_hl(void)
{
    setRegister8(A, fetch8(g_pCPURegisters->reg16.hl++));
}

void ldi_hl_a(void)
{
    write8(g_pCPURegisters->reg8.a, g_pCPURegisters->reg16.hl++);
}

void ldh_offset_mem_a(uint8_t offset)
{
    write8(g_pCPURegisters->reg8.a, (uint16_t)(0xFF00 + offset));
}

void ldh_a_offset_mem(uint8_t offset)
{
    setRegister8(A, fetch8(0xFF00 + offset));
}

// 16-bit loads

/**
 * @brief load 16-bit immediate value to register reg
 *
 * @param reg register to load into
 */
void ld_reg16_imm(Register16 reg)
{
    setRegister16(reg, fetch16(g_pCPURegisters->reg16.pc + 1));
}

/**
 * @brief load 16-bit immediate value to memory address addr
 *
 * @param addr memory address to load to
 */
void ld_addr_imm16(uint16_t addr)
{
    write16(fetch16(g_pCPURegisters->reg16.pc + 1), addr);
}

/**
 * @brief push a 16-bit register's value to the stack
 *
 * @note takes care of stack pointer decrease
 *
 * @param reg register which needs its value pushed on the stack
 */
void push_reg16(Register16 reg)
{
    setRegister16(SP, g_pCPURegisters->reg16.sp - 2);
    write16(g_pCPURegisters->reg16_arr[reg], g_pCPURegisters->reg16.sp);
}

/**
 * @brief pop a 16-bit value from the stack to register reg
 *
 * @note takes care of stack pointer increase
 *
 * @param reg register which gains a value from the stack
 */
void pop_reg16(Register16 reg)
{
    setRegister16(reg, fetch16(g_pCPURegisters->reg16.sp));
    setRegister16(SP, g_pCPURegisters->reg16.sp + 2);
}

// 8-bit arithmetic

void add8_a_n(uint8_t val)
{
    uint16_t sum = g_pCPURegisters->reg8.a + val;

    (sum & 0xFF) == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    (sum > UINT8_MAX) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    CHECK_HALF_CARRY_ADD8(g_pCPURegisters->reg8.a, val) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    resetFlag(FLAG_N);

    setRegister8(A, (uint8_t)sum);
}

void adc8_a_n(uint8_t val)
{
    uint16_t sum = g_pCPURegisters->reg8.a + val + testFlag(FLAG_C);
    uint8_t  carryFlag = testFlag(FLAG_C) ? 1 : 0;

    (sum > UINT8_MAX) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    (sum & 0xFF) == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    CHECK_HALF_CARRY_ADC8(g_pCPURegisters->reg8.a, val, carryFlag) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    resetFlag(FLAG_N);

    setRegister8(A, (uint8_t)sum);
}

void sub8_n_a(uint8_t val)
{
    int16_t result = g_pCPURegisters->reg8.a - val;

    (g_pCPURegisters->reg8.a < val) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    (result == 0) ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    CHECK_HALF_CARRY_SUB8(g_pCPURegisters->reg8.a, val) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    setFlag(FLAG_N);

    setRegister8(A, (uint8_t)result);
}

void sbc8_a_n(uint8_t val)
{
    uint8_t carry  = testFlag(FLAG_C) ? 1 : 0;

    int16_t result = g_pCPURegisters->reg8.a - val - carry;

    (result < 0) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    ((uint8_t)result == 0) ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    CHECK_HALF_CARRY_SBC8(g_pCPURegisters->reg8.a, val,  carry) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    setFlag(FLAG_N);

    setRegister8(A, (uint8_t)result);
}

void and8_a_n(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_C);
    setFlag(FLAG_H);

    setRegister8(A, g_pCPURegisters->reg8.a & val);

    (g_pCPURegisters->reg8.a == 0) ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
}

void or8_a_n(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_C);
    resetFlag(FLAG_H);

    setRegister8(A, g_pCPURegisters->reg8.a | val);

    (g_pCPURegisters->reg8.a == 0) ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
}

void xor8_a_n(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_C);
    resetFlag(FLAG_H);

    setRegister8(A, g_pCPURegisters->reg8.a ^ val);

    (g_pCPURegisters->reg8.a == 0) ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
}

void cp8_a_n(uint8_t val)
{
    setFlag(FLAG_N);

    (g_pCPURegisters->reg8.a == val) ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    (g_pCPURegisters->reg8.a < val) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    CHECK_HALF_CARRY_SUB8(g_pCPURegisters->reg8.a, val) ? setFlag(FLAG_H) : resetFlag(FLAG_H);
}

void inc8_reg(Register8 reg)
{
    uint8_t value  = g_pCPURegisters->reg8_arr[reg];

    uint8_t result = value + 1;
    result == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    resetFlag(FLAG_N);
    CHECK_HALF_CARRY_ADD8(value, 1) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    setRegister8(reg, result);
}

void dec8_reg(Register8 reg)
{
    uint8_t value  = g_pCPURegisters->reg8_arr[reg];

    uint8_t result = value - 1;
    result == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    setFlag(FLAG_N);
    CHECK_HALF_CARRY_SUB8(value, 1) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    setRegister8(reg, result);
}

void inc8_mem(uint16_t addr)
{
    uint8_t value  = fetch8(addr);

    uint8_t result = value + 1;
    result == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    resetFlag(FLAG_N);
    CHECK_HALF_CARRY_ADD8(value, 1) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    write8(result, addr);
}

void dec8_mem(uint16_t addr)
{
    uint8_t value  = fetch8(addr);

    uint8_t result = value - 1;
    result == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    setFlag(FLAG_N);
    CHECK_HALF_CARRY_SUB8(value, 1) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    write8(result, addr);
}

// 16-bit arithmetic

/**
 * @brief add val to HL
 *
 * @param val
 */
void add16_hl_n(uint16_t val)
{
    uint32_t sum = g_pCPURegisters->reg16.hl + val;

    (sum > UINT16_MAX) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    CHECK_HALF_CARRY_ADD16(g_pCPURegisters->reg16.hl, val) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    resetFlag(FLAG_N);

    setRegister16(HL, (uint16_t)sum);
}

void add16_sp_n(int16_t val)
{
    int32_t sum = g_pCPURegisters->reg16.sp + val;

    (((uint8_t)(g_pCPURegisters->reg16.sp & 0xFF)+ (uint8_t)val) > UINT8_MAX) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    CHECK_HALF_CARRY_ADD8((uint8_t)(g_pCPURegisters->reg16.sp & 0xFF), val) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    resetFlag(FLAG_N);
    resetFlag(FLAG_Z);
    setRegister16(SP, (uint16_t)sum);
}

void ldhl_sp_n(int16_t offset)
{
    int32_t sum = g_pCPURegisters->reg16.sp + offset;

    (((uint8_t)(g_pCPURegisters->reg16.sp & 0xFF)+ (uint8_t)offset) > UINT8_MAX) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    CHECK_HALF_CARRY_ADD8((uint8_t)(g_pCPURegisters->reg16.sp & 0xFF), offset) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    resetFlag(FLAG_N);
    resetFlag(FLAG_Z);
    setRegister16(HL, (uint16_t)sum);
}

void inc16_reg(Register16 reg)
{
    setRegister16(reg, g_pCPURegisters->reg16_arr[reg] + 1);
}

void dec16_reg(Register16 reg)
{
    setRegister16(reg, g_pCPURegisters->reg16_arr[reg] - 1);
}

// misc

void swap8_reg(Register8 reg)
{
    setRegister8(reg, ((g_pCPURegisters->reg8_arr[reg] & 0x0F) << 4) | ((g_pCPURegisters->reg8_arr[reg] & 0xF0) >> 4));
    resetFlag(FLAG_N);
    resetFlag(FLAG_C);
    resetFlag(FLAG_H);

    g_pCPURegisters->reg8_arr[reg] == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
}

void swap8_addr(uint16_t addr)
{
    uint8_t val = fetch8(addr);
    write8(((val & 0x0F) << 4) | ((val & 0xF0) >> 4), addr);
    resetFlag(FLAG_N);
    resetFlag(FLAG_C);
    resetFlag(FLAG_H);

    fetch8(addr) == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
}

/**
 * @brief bcd conversion of value in reg a
 * @note  this instruction is weird. stole the implementation from https://forums.nesdev.org/viewtopic.php?t=15944
 *
 */
void daa(void)
{
    uint8_t aVal = g_pCPURegisters->reg8.a;

    if (!testFlag(FLAG_N))
    {
        // after an addition, adjust if (half-)carry occurred or if result is out of bounds
        if (testFlag(FLAG_C) || g_pCPURegisters->reg8.a > 0x99)
        {
            aVal += 0x60;
            setFlag(FLAG_C);
        }
        if (testFlag(FLAG_H) || (g_pCPURegisters->reg8.a & 0x0f) > 0x09)
        {
            aVal += 0x6;
        }
    }
    else
    {
        // after a subtraction, only adjust if (half-)carry occurred
        if (testFlag(FLAG_C))
        {
            aVal -= 0x60;
        }
        if (testFlag(FLAG_H))
        {
            aVal -= 0x6;
        }
    }

    (aVal == 0) ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    resetFlag(FLAG_H);

    setRegister8(A, aVal);
}

void cpl(void)
{
    setRegister8(A, ~g_pCPURegisters->reg8.a);
    setFlag(FLAG_N);
    setFlag(FLAG_H);
}

void ccf(void)
{
    if (testFlag(FLAG_C))
    {
        resetFlag(FLAG_C);
    }
    else
    {
        setFlag(FLAG_C);
    }
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);
}

void scf(void)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);
    setFlag(FLAG_C);
}

// rotates & shifts

/**
 * @brief 0x07, rotate left circular accumulator
 * @note  bit 7 is copied to the CARRY flag, CARRY goes to bit 0
 *
 */
void rlca(void)
{
    setRegister8(A, _rlc(g_pCPURegisters->reg8.a));
    resetFlag(FLAG_Z);
}

void rla(void)
{
    setRegister8(A, _rl(g_pCPURegisters->reg8.a));
    resetFlag(FLAG_Z);
}

void rrca(void)
{
    setRegister8(A, _rrc(g_pCPURegisters->reg8.a));
    resetFlag(FLAG_Z);
}

void rra(void)
{
    setRegister8(A, _rr(g_pCPURegisters->reg8.a));
    resetFlag(FLAG_Z);
}

void rlc_reg(Register8 reg)
{
    setRegister8(reg, _rlc(g_pCPURegisters->reg8_arr[reg]));
}

void rlc_addr(uint16_t addr)
{
    write8(_rlc(fetch8(addr)), addr);
}

void rl_reg(Register8 reg)
{
    setRegister8(reg, _rl(g_pCPURegisters->reg8_arr[reg]));
}

void rl_addr(uint16_t addr)
{
    write8(_rl(fetch8(addr)), addr);
}

void rrc_reg(Register8 reg)
{
    setRegister8(reg, _rrc(g_pCPURegisters->reg8_arr[reg]));
}

void rrc_addr(uint16_t addr)
{
    write8(_rrc(fetch8(addr)), addr);
}

void rr_reg(Register8 reg)
{
    setRegister8(reg, _rr(g_pCPURegisters->reg8_arr[reg]));
}

void rr_addr(uint16_t addr)
{
    write8(_rr(fetch8(addr)), addr);
}

void sla_reg(Register8 reg)
{
    setRegister8(reg, _sla(g_pCPURegisters->reg8_arr[reg]));
}

void sla_addr(uint16_t addr)
{
    write8(_sla(fetch8(addr)), addr);
}

void sra_reg(Register8 reg)
{
    setRegister8(reg, _sra(g_pCPURegisters->reg8_arr[reg]));
}

void sra_addr(uint16_t addr)
{
    write8(_sra(fetch8(addr)), addr);
}

void srl_reg(Register8 reg)
{
    setRegister8(reg, _srl(g_pCPURegisters->reg8_arr[reg]));
}

void srl_addr(uint16_t addr)
{
    write8(_srl(fetch8(addr)), addr);
}

// single bit stuff

void bit_n_reg(uint8_t bit, Register8 reg)
{
    resetFlag(FLAG_N);
    setFlag(FLAG_H);

    // Z set if bit set
    g_pCPURegisters->reg8_arr[reg] & (1 << bit) ? resetFlag(FLAG_Z) : setFlag(FLAG_Z);
}

void bit_n_addr(uint8_t bit, uint16_t addr)
{
    resetFlag(FLAG_N);
    setFlag(FLAG_H);

    // Z set if bit set
    fetch8(addr) & (1 << bit) ? resetFlag(FLAG_Z) : setFlag(FLAG_Z);
}

void set_n_reg(uint8_t bit, Register8 reg)
{
    g_pCPURegisters->reg8_arr[reg] |= (1 << bit);
}

void set_n_addr(uint8_t bit, uint16_t addr)
{
    write8(fetch8(addr) | (1 << bit), addr);
}

void reset_n_reg(uint8_t bit, Register8 reg)
{
    g_pCPURegisters->reg8_arr[reg] &= ~(1 << bit);
}

void reset_n_addr(uint8_t bit, uint16_t addr)
{
    write8(fetch8(addr) & ~(1 << bit), addr);
}

// jumps

/**
 * @brief perform nonrelative unconditional jump to 16-bit immediate value
 *
 */
void jmp_imm16()
{
    setRegister16(PC, fetch16(g_pCPURegisters->reg16.pc + 1));
}

/**
 * @brief perform nonrelative conditional jump to 16-bit immediate value
 *
 * @param flag flag to test
 * @param testSet if true, will execute jump if specified flag is set
 * @return true     if jumped
 * @return false    if not (test was false)
 */
bool jmp_imm16_cond(Flag flag, bool testSet)
{
    bool ret = false;

    if (testFlag(flag))
    {
        if (testSet)
        {
            jmp_imm16();
            ret = true;
        }
    }
    else
    {
        if (!testSet)
        {
            jmp_imm16();
            ret = true;
        }
    }

    return ret;
}

/**
 * @brief perform nonrelative unconditional jump to address specified in HL
 *
 */
void jmp_hl(void)
{
    setRegister16(PC, g_pCPURegisters->reg16.hl);
}

/**
 * @brief perform relative unconditional jump to signed 8-bit immediate
 *
 */
void jr_imm8()
{
    // add 2 for JR size
    setRegister16(PC, g_pCPURegisters->reg16.pc + 2 + (int8_t)fetch8(g_pCPURegisters->reg16.pc + 1));
}

/**
 * @brief perform relative conditional jump to signed 8-bit immediate
 *
 * @param flag      flag to test
 * @param testSet   if true, will test if <flag> is set, otherwise will test for 0
 * @return true     if jumped
 * @return false    if not (test was false)
 */
bool jr_imm8_cond(Flag flag, bool testSet)
{
    bool ret = false;

    if (testFlag(flag))
    {
        if (testSet)
        {
            jr_imm8();
            ret = true;
        }
    }
    else
    {
        if (!testSet)
        {
            jr_imm8();
            ret = true;
        }
    }

    return ret;
}

// calls

/**
 * @brief call subroutine at unsigned 16-bit immediate
 *
 * @note pushes address of first instr after subroutine to stack, decrements SP by 2
 */
void call_imm16()
{
    // decrement SP
    setRegister16(SP, g_pCPURegisters->reg16.sp - 2);

    // save PC + 3
    write16(g_pCPURegisters->reg16.pc + 3, g_pCPURegisters->reg16.sp);

    // set new PC to imm16
    setRegister16(PC, fetch16(g_pCPURegisters->reg16.pc + 1));
}

/**
 * @brief conditionally call subroutine at unsigned 16-bit immediate
 *
 * @param flag      flag to test
 * @param testSet   if true, will test if <flag> is set, otherwise will test for 0
 * @return true     if jumped
 * @return false    if not (test was false)
 */
bool call_imm16_cond(Flag flag, bool testSet)
{
    bool ret = false;

    if (testFlag(flag))
    {
        if (testSet)
        {
            call_imm16();
            ret = true;
        }
    }
    else
    {
        if (!testSet)
        {
            call_imm16();
            ret = true;
        }
    }

    return ret;
}

void call_irq_subroutine(uint8_t addr)
{
    // decrement SP
    setRegister16(SP, g_pCPURegisters->reg16.sp - 2);

    // save PC to new SP addr (not executed yet)
    write16(g_pCPURegisters->reg16.pc, g_pCPURegisters->reg16.sp);

    // set new PC to addr
    setRegister16(PC, addr);
}

/**
 * @brief call RST page between 0 and 8
 *
 * @note stores next valid PC on stack. RSTs are at 0x00, 0x08, 0x10, etc
 *
 * @param addr RST addr to call
 */
void rst_n(uint8_t addr)
{
    g_pCPURegisters->reg16.sp -= 2;
    write16(g_pCPURegisters->reg16.pc + 1, g_pCPURegisters->reg16.sp);
    setRegister16(PC, addr);
}

/**
 * @brief return from subroutine. pulls next PC address from stack
 *
 */
void ret(void)
{
    setRegister16(PC, fetch16(g_pCPURegisters->reg16.sp));
    setRegister16(SP, g_pCPURegisters->reg16.sp + 2);
}

bool ret_cond(Flag flag, bool testSet)
{
    bool retval = false;

    if (testFlag(flag))
    {
        if (testSet)
        {
            ret();
            retval = true;
        }
    }
    else
    {
        if (!testSet)
        {
            ret();
            retval = true;
        }
    }

    return retval;
}

void decodeAndExecute(SCPUCurrentCycleState_t *pCurrentInstruction)
{
    uint8_t hi = pCurrentInstruction->instruction >> 4;
    uint8_t lo = pCurrentInstruction->instruction & 15;

    pCurrentInstruction->programCounterSteps = 1;

    // main instruction decode loop
    switch (pCurrentInstruction->instruction)
    {
        case 0x00: // NOP
        {
            pCurrentInstruction->mCyclesExecuted     += 1;
            break;
        }
        case 0x01: // LD BC,d16
        case 0x11: // LD DE,d16
        case 0x21: // LD HL,d16
        case 0x31: // LD SP,d16
        {
            ld_reg16_imm(BC + hi); // use high nibble
            pCurrentInstruction->programCounterSteps = 3;
            pCurrentInstruction->mCyclesExecuted     += 3;
            break;
        }
        case 0x02: // LD (BC),A
        case 0x12: // LD (DE),A
        {
            Register16 reg = BC + hi; // use high nibble
            ld_addr_reg8(pGetCPURegisters()->reg16_arr[reg], A);
            pCurrentInstruction->mCyclesExecuted += 2;
            break;
        }
        case 0x22: // LD (HL+),A
        {
            pCurrentInstruction->mCyclesExecuted += 2;
            ld_addr_reg8(pGetCPURegisters()->reg16.hl, A);
            inc16_reg(HL);
            break;
        }
        case 0x32: // LD (HL-),A
        {
            pCurrentInstruction->mCyclesExecuted += 2;
            ld_addr_reg8(pGetCPURegisters()->reg16.hl, A);
            dec16_reg(HL);
            break;
        }
        case 0x06: // LD B,d8
        {
            ld_reg8_imm(B);
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 2;
            break;
        }
        case 0x16: // LD D,d8
        {
            ld_reg8_imm(D);
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 2;
            break;
        }
        case 0x26: // LD H,d8
        {
            ld_reg8_imm(H);
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 2;
            break;
        }
        case 0x36: // LD (HL),d8
        {
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 3;
            ld_addr_imm8(pGetCPURegisters()->reg16.hl);
            break;
        }
        case 0x0A: // LD A,(BC)
        {
            ld_reg8_addr(A, pGetCPURegisters()->reg16.bc);
            pCurrentInstruction->mCyclesExecuted += 2;
            break;
        }
        case 0x1A: // LD A,(DE)
        {
            ld_reg8_addr(A, pGetCPURegisters()->reg16.de);
            pCurrentInstruction->mCyclesExecuted += 2;
            break;
        }
        case 0x2A: // LD A,(HL+)
        {
            ld_reg8_addr(A, pGetCPURegisters()->reg16.hl);
            inc16_reg(HL);
            pCurrentInstruction->mCyclesExecuted += 2;
            break;
        }
        case 0x3A: // LD A,(HL-)
        {
            ld_reg8_addr(A, pGetCPURegisters()->reg16.hl);
            dec16_reg(HL);
            pCurrentInstruction->mCyclesExecuted += 2;
            break;
        }
        case 0x0E: // LD C,d8
        {
            ld_reg8_imm(C);
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 2;
            break;
        }
        case 0x1E: // LD E,d8
        {
            ld_reg8_imm(E);
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 2;
            break;
        }
        case 0x2E: // LD L,d8
        {
            ld_reg8_imm(L);
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 2;
            break;
        }
        case 0x3E: // LD A,d8
        {
            ld_reg8_imm(A);
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 2;
            break;
        }
        case 0xE2: // LD (0xFF00 + C),A
        {
            pCurrentInstruction->mCyclesExecuted += 2;
            ld_addr_reg8(0xFF00 + pGetCPURegisters()->reg8.c, A);
            break;
        }
        case 0xF2: // LD A, (0xFF00 + C)
        {
            ld_reg8_addr(A, 0xFF00 + pGetCPURegisters()->reg8.c);
            pCurrentInstruction->mCyclesExecuted += 2;
            break;
        }
        case 0xE0: // LDH (a8), A (load from A to to 0xFF00+8-bit imm unsigned)
        {
            ld_addr_reg8(0xFF00 + fetch8(pGetCPURegisters()->reg16.pc + 1), A);
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 3;
            break;
        }
        case 0xF0: // LDH A, (a8) (load from 0xFF00+8-bit unsigned value to A)
        {
            ld_reg8_addr(A, 0xFF00 + fetch8(pGetCPURegisters()->reg16.pc + 1));
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 3;
            break;
        }
        case 0xEA: // LD (a16), A (load from A to addr)
        {
            ld_addr_reg8(fetch16(pGetCPURegisters()->reg16.pc + 1), A);
            pCurrentInstruction->programCounterSteps = 3;
            pCurrentInstruction->mCyclesExecuted     += 4;
            break;
        }
        case 0xFA: // LD A, (a16) (load from addr to A)
        {
            ld_reg8_addr(A, fetch16(pGetCPURegisters()->reg16.pc + 1));
            pCurrentInstruction->programCounterSteps = 3;
            pCurrentInstruction->mCyclesExecuted     += 4;
            break;
        }
        case 0x0C: // INC C
        case 0x0D: // DEC C
        {
            if (lo == 0x0C)
            {
                inc8_reg(C);
            }
            else
            {
                dec8_reg(C);
            }
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }
        case 0x1C: // INC E
        case 0x1D: // DEC E
        {
            if (lo == 0x0C)
            {
                inc8_reg(E);
            }
            else
            {
                dec8_reg(E);
            }
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }
        case 0x2C: // INC L
        case 0x2D: // DEC L
        {
            if (lo == 0x0C)
            {
                inc8_reg(L);
            }
            else
            {
                dec8_reg(L);
            }
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }
        case 0x3C: // INC A
        case 0x3D: // DEC A
        {
            if (lo == 0x0C)
            {
                inc8_reg(A);
            }
            else
            {
                dec8_reg(A);
            }
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            ;
            break;
        }
        case 0x04: // INC B
        case 0x05: // DEC B
        {
            if (lo == 0x04)
            {
                inc8_reg(B);
            }
            else
            {
                dec8_reg(B);
            }
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }
        case 0x14: // INC D
        case 0x15: // DEC D
        {
            if (lo == 0x04)
            {
                inc8_reg(D);
            }
            else
            {
                dec8_reg(D);
            }
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }
        case 0x24: // INC H
        case 0x25: // DEC H
        {
            if (lo == 0x04)
            {
                inc8_reg(H);
            }
            else
            {
                dec8_reg(H);
            }
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }
        case 0x34: // INC (HL)
        case 0x35: // DEC (HL)
        {
            if (lo == 0x04)
            {
                inc8_mem(pGetCPURegisters()->reg16.hl);
            }
            else
            {
                dec8_mem(pGetCPURegisters()->reg16.hl);
            }
            pCurrentInstruction->mCyclesExecuted     += 3;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }
        case 0x03: // INC BC
        case 0x13: // INC DE
        case 0x23: // INC HL
        case 0x33: // INC SP
        case 0x0B: // DEC BC
        case 0x1B: // DEC DE
        case 0x2B: // DEC HL
        case 0x3B: // DEC SP
        {
            Register16 reg = BC + hi; // use high nibble
            if (lo == 0x03)
            {
                inc16_reg(reg);
            }
            else
            {
                dec16_reg(reg);
            }
            pCurrentInstruction->mCyclesExecuted     += 2;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }
        case 0x07: // RLCA (rotate left circular A)
        {
            rlca();
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }
        case 0x17: // RLA (rotate left through carry A)
        {
            rla();
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }
        case 0x0F: // RRCA (rotate right circular A)
        {
            rrca();
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }
        case 0x1F: // RRA (rotate right through carry A)
        {
            rra();
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }
        case 0xCB: // CB-prefixed
        {
            pCurrentInstruction->mCyclesExecuted     += 2 + decodeAndExecuteCBPrefix(); // all CB's use at least 2 cycles
            pCurrentInstruction->programCounterSteps = 2; // CB doesn't take immediates, always 2 bytes
            break;
        }

            // JUMP instructions
            // for JUMPs we do not increment the current PC after the loop!

        case 0x18: // JR s8: relative jump
        {
            jr_imm8();
            pCurrentInstruction->programCounterSteps = 0;
            pCurrentInstruction->mCyclesExecuted     += 3;
            break;
        }
        case 0x20: // JR NZ,s8
        case 0x28: // JR Z,s8
        {
            pCurrentInstruction->mCyclesExecuted += 2;
            if (jr_imm8_cond(FLAG_Z, (lo == 0x8 ? true : false)))
            {
                pCurrentInstruction->mCyclesExecuted++;
                pCurrentInstruction->programCounterSteps = 0;
            }
            else
            {
                pCurrentInstruction->programCounterSteps = 2;
            }
            break;
        }
        case 0x30: // JR NC,s8
        case 0x38: // JR C,s8
        {
            pCurrentInstruction->mCyclesExecuted += 2;
            if (jr_imm8_cond(FLAG_C, (lo == 0x8 ? true : false)))
            {
                pCurrentInstruction->mCyclesExecuted++;
                pCurrentInstruction->programCounterSteps = 0;
            }
            else
            {
                pCurrentInstruction->programCounterSteps = 2;
            }
            break;
        }
        case 0xC3: // JP a16
        {
            jmp_imm16();
            pCurrentInstruction->programCounterSteps = 0;
            pCurrentInstruction->mCyclesExecuted     += 4;
            break;
        }
        case 0xC2: // JP NZ,a16
        case 0xCA: // JP Z,a16
        {
            pCurrentInstruction->mCyclesExecuted += 3;
            if (jmp_imm16_cond(FLAG_Z, (lo == 0xA ? true : false)))
            {
                pCurrentInstruction->mCyclesExecuted++;
                pCurrentInstruction->programCounterSteps = 0;
            }
            else
            {
                pCurrentInstruction->programCounterSteps = 3;
            }
            break;
        }
        case 0xD2: // JP NC,a16
        case 0xDA: // JP C,a16
        {
            pCurrentInstruction->mCyclesExecuted += 3;
            if (jmp_imm16_cond(FLAG_C, (lo == 0xA ? true : false)))
            {
                pCurrentInstruction->mCyclesExecuted++;
                pCurrentInstruction->programCounterSteps = 0;
            }
            else
            {
                pCurrentInstruction->programCounterSteps = 3;
            }
            break;
        }

        case 0xE9: // JP HL
        {
            jmp_hl();
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 0;
            break;
        }

            // CALL instructions

        case 0xC4: // CALL NZ,a16
        case 0xCC: // CALL Z,a16
        {
            pCurrentInstruction->mCyclesExecuted += 3;
            if (call_imm16_cond(FLAG_Z, (lo == 0xC ? true : false)))
            {
                pCurrentInstruction->mCyclesExecuted += 3;
                pCurrentInstruction->programCounterSteps = 0;
            }
            else
            {
                pCurrentInstruction->programCounterSteps = 3;
            }
            break;
        }
        case 0xD4: // CALL NC,a16
        case 0xDC: // CALL C,a16
        {
            pCurrentInstruction->mCyclesExecuted += 3;
            if (call_imm16_cond(FLAG_C, (lo == 0xC ? true : false)))
            {
                pCurrentInstruction->mCyclesExecuted += 3;
                pCurrentInstruction->programCounterSteps = 0;
            }
            else
            {
                pCurrentInstruction->programCounterSteps = 3;
            }
            break;
        }
        case 0xCD: // CALL a16
        {
            pCurrentInstruction->mCyclesExecuted     += 6;
            pCurrentInstruction->programCounterSteps = 0;
            call_imm16();
            break;
        }

            // POP instructions

        case 0xC1: // POP BC
        {
            pop_reg16(BC);
            pCurrentInstruction->programCounterSteps = 1;
            pCurrentInstruction->mCyclesExecuted     += 3;
            break;
        }
        case 0xD1: // POP DE
        {
            pop_reg16(DE);
            pCurrentInstruction->programCounterSteps = 1;
            pCurrentInstruction->mCyclesExecuted     += 3;
            break;
        }
        case 0xE1: // POP HL
        {
            pop_reg16(HL);
            pCurrentInstruction->programCounterSteps = 1;
            pCurrentInstruction->mCyclesExecuted     += 3;
            break;
        }
        case 0xF1: // POP AF
        {
            pop_reg16(AF);
            // zero unused nibble of flags register
            setRegister8(F, pGetCPURegisters()->reg8.f & 0xF0);
            pCurrentInstruction->programCounterSteps = 1;
            pCurrentInstruction->mCyclesExecuted     += 3;
            break;
        }

            // PUSH instructions

        case 0xC5: // PUSH BC
        {
            push_reg16(BC);
            pCurrentInstruction->programCounterSteps = 1;
            pCurrentInstruction->mCyclesExecuted     += 4;
            break;
        }
        case 0xD5: // PUSH DE
        {
            push_reg16(DE);
            pCurrentInstruction->programCounterSteps = 1;
            pCurrentInstruction->mCyclesExecuted     += 4;
            break;
        }
        case 0xE5: // PUSH HL
        {
            push_reg16(HL);
            pCurrentInstruction->programCounterSteps = 1;
            pCurrentInstruction->mCyclesExecuted     += 4;
            break;
        }
        case 0xF5: // PUSH AF
        {
            push_reg16(AF);
            pCurrentInstruction->programCounterSteps = 1;
            pCurrentInstruction->mCyclesExecuted     += 4;
            break;
        }

        // RET instructions
        case 0xC9: // RET
        {
            ret();
            pCurrentInstruction->programCounterSteps = 0;
            pCurrentInstruction->mCyclesExecuted     += 4;
            break;
        }
        case 0xD9: // RETI
        {
            setIME();
            ret();
            pCurrentInstruction->programCounterSteps = 0;
            pCurrentInstruction->mCyclesExecuted     += 4;
            break;
        }

        case 0xC0: // RET NZ
        {
            if (!testFlag(FLAG_Z))
            {
                ret();
                pCurrentInstruction->mCyclesExecuted     += 5;
                pCurrentInstruction->programCounterSteps = 0;
            }
            else
            {
                pCurrentInstruction->mCyclesExecuted     += 2;
                pCurrentInstruction->programCounterSteps = 1;
            }
            break;
        }

        case 0xC8: // RET Z
        {
            if (testFlag(FLAG_Z))
            {
                ret();
                pCurrentInstruction->mCyclesExecuted     += 5;
                pCurrentInstruction->programCounterSteps = 0;
            }
            else
            {
                pCurrentInstruction->mCyclesExecuted     += 2;
                pCurrentInstruction->programCounterSteps = 1;
            }
            break;
        }

        case 0xD0: // RET NC
        {
            if (!testFlag(FLAG_C))
            {
                ret();
                pCurrentInstruction->mCyclesExecuted     += 5;
                pCurrentInstruction->programCounterSteps = 0;
            }
            else
            {
                pCurrentInstruction->mCyclesExecuted     += 2;
                pCurrentInstruction->programCounterSteps = 1;
            }
            break;
        }

        case 0xD8: // RET C
        {
            if (testFlag(FLAG_C))
            {
                ret();
                pCurrentInstruction->mCyclesExecuted     += 5;
                pCurrentInstruction->programCounterSteps = 0;
            }
            else
            {
                pCurrentInstruction->mCyclesExecuted     += 2;
                pCurrentInstruction->programCounterSteps = 1;
            }
            break;
        }

        case 0x76: // HALT
        {
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            setHaltRequested();
            break;
        }

        case 0x10: // STOP
        {
            pCurrentInstruction->mCyclesExecuted     += 2;
            pCurrentInstruction->programCounterSteps = 1;
            setStopped();
            resetIME();
            write8(0, DIVIDER_ADDR); // set DIV to 0
            break;
        }

        case 0x27: // DAA
        {
            daa();
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }

        case 0x2F: // CPL
        {
            cpl();
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }

        case 0x3F: // CCF
        {
            ccf();
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }

        case 0x37: // SCF
        {
            scf();
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }

        case 0xF3: // DI
        {
            resetIME();
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }

        case 0xFB: // EI
        {
            setDelayedIMELatch();
            pCurrentInstruction->mCyclesExecuted     += 1;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }

        case 0x09: // ADD HL, BC
        case 0x19: // ADD HL, DE
        case 0x29: // ADD HL, HL
        case 0x39: // ADD HL, SP
        {
            add16_hl_n(pGetCPURegisters()->reg16_arr[BC + hi]); // AF = 0, BC = 1, etc
            pCurrentInstruction->mCyclesExecuted     += 2;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }

        case 0xC7: // RST 0
        case 0xD7: // RST 2
        case 0xE7: // RST 4
        case 0xF7: // RST 6
        case 0xCF: // RST 1
        case 0xDF: // RST 3
        case 0xEF: // RST 5
        case 0xFF: // RST 7
        {
            rst_n(pCurrentInstruction->instruction & 0x38);
            pCurrentInstruction->mCyclesExecuted     += 4;
            pCurrentInstruction->programCounterSteps = 0; // PC is set explicitly
            break;
        }

        case 0xC6: // ADD a, d8
        {
            add8_a_n(fetch8(pGetCPURegisters()->reg16.pc + 1));
            pCurrentInstruction->mCyclesExecuted     += 2;
            pCurrentInstruction->programCounterSteps = 2;
            break;
        }

        case 0xD6: // SUB d8
        {
            sub8_n_a(fetch8(pGetCPURegisters()->reg16.pc + 1));
            pCurrentInstruction->mCyclesExecuted     += 2;
            pCurrentInstruction->programCounterSteps = 2;
            break;
        }

        case 0xE6: // AND d8
        {
            and8_a_n(fetch8(pGetCPURegisters()->reg16.pc + 1));
            pCurrentInstruction->mCyclesExecuted     += 2;
            pCurrentInstruction->programCounterSteps = 2;
            break;
        }

        case 0xF6: // ADD a, d8
        {
            or8_a_n(fetch8(pGetCPURegisters()->reg16.pc + 1));
            pCurrentInstruction->mCyclesExecuted     += 2;
            pCurrentInstruction->programCounterSteps = 2;
            break;
        }

        case 0xCE: // ADC a, d8
        {
            adc8_a_n(fetch8(pGetCPURegisters()->reg16.pc + 1));
            pCurrentInstruction->mCyclesExecuted     += 2;
            pCurrentInstruction->programCounterSteps = 2;
            break;
        }

        case 0xDE: // SBC a, d8
        {
            sbc8_a_n(fetch8(pGetCPURegisters()->reg16.pc + 1));
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 2;
            break;
        }

        case 0xEE: // XOR d8
        {
            xor8_a_n(fetch8(pGetCPURegisters()->reg16.pc + 1));
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 2;
            break;
        }

        case 0xFE: // CP d8
        {
            cp8_a_n(fetch8(pGetCPURegisters()->reg16.pc + 1));
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 2;
            break;
        }

        case 0xE8: // ADD SP, s8
        {
            add16_sp_n((int8_t)fetch8(pGetCPURegisters()->reg16.pc + 1));
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 4;
            break;
        }

        case 0x08: // LD (a16), SP
        {
            write16(pGetCPURegisters()->reg16.sp, fetch16(pGetCPURegisters()->reg16.pc + 1));
            pCurrentInstruction->programCounterSteps = 3;
            pCurrentInstruction->mCyclesExecuted     += 5;
            break;
        }

        case 0xF8: // LD HL, SP+s8
        {
            ldhl_sp_n((int8_t)fetch8((pGetCPURegisters()->reg16.pc + 1)));
            pCurrentInstruction->programCounterSteps = 2;
            pCurrentInstruction->mCyclesExecuted     += 3;
            break;
        }

        case 0xF9: // LD SP, HL
        {
            setRegister16(SP, pGetCPURegisters()->reg16.hl);
            pCurrentInstruction->mCyclesExecuted     += 2;
            pCurrentInstruction->programCounterSteps = 1;
            break;
        }

        default:
        {
            Register8 regL                           = A;
            Register8 regR                           = A;
            bool      hl                             = false;
            bool      hlLeft                         = false;

            pCurrentInstruction->mCyclesExecuted     += 1; // by default: only (HL) has 2 cycles
            pCurrentInstruction->programCounterSteps = 1;

            switch (hi)
            {
                // LD functions
                case 0x04: // LD B,x - LD C,x
                {
                    regL = (lo <= 0x07) ? B : C;

                    if ((lo == 0x06) || (lo == 0x0E)) // 0x6 or 0xE == (HL)
                    {
                        hl = true;
                    }
                    else
                    {
                        regR = getRegIndexByOpcodeNibble(lo);
                    }

                    if (!hl)
                    {
                        ld_reg8_reg8(regL, regR);
                    }
                    else
                    {
                        pCurrentInstruction->mCyclesExecuted += 2;
                        ld_reg8_addr(regL, pGetCPURegisters()->reg16.hl);
                    }

                    break;
                }
                case 0x05: // LD D,x - LD E,x
                {
                    regL = (lo <= 0x07) ? D : E;

                    if ((lo == 0x06) || (lo == 0x0E)) // 0x6 or 0xE == (HL)
                    {
                        hl = true;
                    }
                    else
                    {
                        regR = getRegIndexByOpcodeNibble(lo);
                    }

                    if (!hl)
                    {
                        ld_reg8_reg8(regL, regR);
                    }
                    else
                    {
                        pCurrentInstruction->mCyclesExecuted += 2;
                        ld_reg8_addr(regL, pGetCPURegisters()->reg16.hl);
                    }

                    break;
                }
                case 0x06: // LD H,x - LD L,x
                {
                    regL = (lo <= 0x07) ? H : L;

                    if ((lo == 0x06) || (lo == 0x0E)) // 0x6 or 0xE == (HL)
                    {
                        hl = true;
                    }
                    else
                    {
                        regR = getRegIndexByOpcodeNibble(lo);
                    }

                    if (!hl)
                    {
                        ld_reg8_reg8(regL, regR);
                    }
                    else
                    {
                        pCurrentInstruction->mCyclesExecuted += 2;
                        ld_reg8_addr(regL, pGetCPURegisters()->reg16.hl);
                    }

                    break;
                }
                case 0x07: // LD (HL),x - LD A, x
                {
                    if (lo <= 0x07) // (HL). no need to worry about HALT, already handled above
                    {
                        pCurrentInstruction->mCyclesExecuted += 2;
                        if (lo != 0x07)
                        {
                            regR = getRegIndexByOpcodeNibble(lo);
                        }
                        ld_addr_reg8(pGetCPURegisters()->reg16.hl, regR);
                        break;
                    }

                    if (lo == 0x0E)
                    {
                        hl = true;
                    }
                    else
                    {
                        regR = getRegIndexByOpcodeNibble(lo);
                    }

                    if (!hl)
                    {
                        ld_reg8_reg8(regL, regR);
                    }
                    else
                    {
                        pCurrentInstruction->mCyclesExecuted += 2;
                        ld_reg8_addr(regL, pGetCPURegisters()->reg16.hl);
                    }

                    break;
                }

                // ADD/ADC A
                case 0x08:
                {
                    if (lo < 0x06)
                    {
                        // ADD, B to L
                        add8_a_n(pGetCPURegisters()->reg8_arr[getRegIndexByOpcodeNibble(lo)]);
                    }
                    else if (lo == 0x06)
                    {
                        // ADD (HL)
                        pCurrentInstruction->mCyclesExecuted += 2;
                        add8_a_n(fetch8(pGetCPURegisters()->reg16.hl));
                    }
                    else if (lo == 0x07)
                    {
                        // ADD A
                        add8_a_n(pGetCPURegisters()->reg8_arr[A]);
                    }
                    else if (lo < 0x0E)
                    {
                        // ADC, B to L
                        adc8_a_n(pGetCPURegisters()->reg8_arr[getRegIndexByOpcodeNibble(lo - 0x08)]);
                    }
                    else if (lo == 0x0E)
                    {
                        // ADC (HL)
                        pCurrentInstruction->mCyclesExecuted += 2;
                        adc8_a_n(fetch8(pGetCPURegisters()->reg16.hl));
                    }
                    else if (lo == 0x0F)
                    {
                        // ADC A
                        adc8_a_n(pGetCPURegisters()->reg8_arr[A]);
                    }
                    break;
                }

                // SUB/SBC <reg>
                case 0x09:
                {
                    if (lo < 0x06)
                    {
                        // SUB, B to L
                        sub8_n_a(pGetCPURegisters()->reg8_arr[getRegIndexByOpcodeNibble(lo)]);
                    }
                    else if (lo == 0x06)
                    {
                        // SUB (HL)
                        pCurrentInstruction->mCyclesExecuted += 2;
                        sub8_n_a(fetch8(pGetCPURegisters()->reg16.hl));
                    }
                    else if (lo == 0x07)
                    {
                        // SUB A
                        sub8_n_a(pGetCPURegisters()->reg8_arr[A]);
                    }
                    else if (lo < 0x0E)
                    {
                        // SBC, B to L
                        sbc8_a_n(pGetCPURegisters()->reg8_arr[getRegIndexByOpcodeNibble(lo - 0x08)]);
                    }
                    else if (lo == 0x0E)
                    {
                        // SBC (HL)
                        pCurrentInstruction->mCyclesExecuted += 2;
                        sbc8_a_n(fetch8(pGetCPURegisters()->reg16.hl));
                    }
                    else if (lo == 0x0F)
                    {
                        // SBC A
                        sbc8_a_n(pGetCPURegisters()->reg8_arr[A]);
                    }
                    break;
                }

                // AND/XOR <reg>
                case 0x0A:
                {
                    if (lo < 0x06)
                    {
                        // AND, B to L
                        and8_a_n(pGetCPURegisters()->reg8_arr[getRegIndexByOpcodeNibble(lo)]);
                        break;
                    }
                    else if (lo == 0x06)
                    {
                        // AND (HL)
                        pCurrentInstruction->mCyclesExecuted += 2;
                        and8_a_n(fetch8(pGetCPURegisters()->reg16.hl));
                    }
                    else if (lo == 0x07)
                    {
                        // AND A
                        and8_a_n(pGetCPURegisters()->reg8_arr[A]);
                    }
                    else if (lo < 0x0E)
                    {
                        // XOR, B to L
                        xor8_a_n(pGetCPURegisters()->reg8_arr[getRegIndexByOpcodeNibble(lo - 0x08)]);
                    }
                    else if (lo == 0x0E)
                    {
                        // XOR (HL)
                        pCurrentInstruction->mCyclesExecuted += 2;
                        xor8_a_n(fetch8(pGetCPURegisters()->reg16.hl));
                    }
                    else if (lo == 0x0F)
                    {
                        // XOR A
                        xor8_a_n(pGetCPURegisters()->reg8_arr[A]);
                    }
                    break;
                }

                // OR/CP <reg>
                case 0x0B:
                {
                    if (lo < 0x06)
                    {
                        // OR, B to L
                        or8_a_n(pGetCPURegisters()->reg8_arr[getRegIndexByOpcodeNibble(lo)]);
                        break;
                    }
                    else if (lo == 0x06)
                    {
                        // OR (HL)
                        pCurrentInstruction->mCyclesExecuted += 2;
                        or8_a_n(fetch8(pGetCPURegisters()->reg16.hl));
                    }
                    else if (lo == 0x07)
                    {
                        // OR A
                        or8_a_n(pGetCPURegisters()->reg8_arr[A]);
                    }
                    else if (lo < 0x0E)
                    {
                        // CP, B to L
                        cp8_a_n(pGetCPURegisters()->reg8_arr[getRegIndexByOpcodeNibble(lo - 0x08)]);
                    }
                    else if (lo == 0x0E)
                    {
                        // CP (HL)
                        pCurrentInstruction->mCyclesExecuted += 2;
                        cp8_a_n(fetch8(pGetCPURegisters()->reg16.hl));
                    }
                    else if (lo == 0x0F)
                    {
                        // CP A
                        cp8_a_n(pGetCPURegisters()->reg8_arr[A]);
                    }
                    break;
                }
                default:
                {
                    printf("unknown instruction 0x%2x at 0x%04x\n", pCurrentInstruction->instruction,
                           pGetCPURegisters()->reg16.pc);
                    assert(1 == 0);
                }
            }
        }
    }
}

/**
 * @brief Rotate Left through Carry
 * @note  carry becomes 0, and bit 7 is copied to carry
 *
 * @param val
 */
static uint8_t _rl(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    bool toggleFlag = val & 0x80;

    val <<= 1;
    val |= testFlag(FLAG_C);
    toggleFlag ? setFlag(FLAG_C) : resetFlag(FLAG_C);

    if (val == 0)
    {
        setFlag(FLAG_Z);
    }
    else
    {
        resetFlag(FLAG_Z);
    }

    return val;
}

/**
 * @brief Rotate Left Circular
 * @note  bit 7 is copied to carry and bit 0
 * @param val value to rotate
 */
static uint8_t _rlc(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (val & 0x80)
    {
        setFlag(FLAG_C);
    }
    else
    {
        resetFlag(FLAG_C);
    }

    uint8_t ret = (val << 1) | testFlag(FLAG_C);

    if (ret == 0)
    {
        setFlag(FLAG_Z);
    }
    else
    {
        resetFlag(FLAG_Z);
    }

    return ret;
}

/**
 * @brief Shift Left Arithmetic
 * @note  leftshift one bit, zero 0, and copy bit 7 to carry
 * @param val value to shift
 * @return uint8_t shifted value
 */
static uint8_t _sla(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (val & 0x80)
    {
        setFlag(FLAG_C);
    }
    else
    {
        resetFlag(FLAG_C);
    }
    uint8_t ret = val << 1;

    if (ret == 0)
    {
        setFlag(FLAG_Z);
    }
    else
    {
        resetFlag(FLAG_Z);
    }

    return ret;
}

/**
 * @brief Rotate Right through Carry
 * @note  carry becomes 7, and bit 0 is copied to carry
 *
 * @param val
 */
static uint8_t _rr(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    bool toggleFlag = val & 0x01;

    val >>= 1;
    val |= (testFlag(FLAG_C) << 7);
    toggleFlag ? setFlag(FLAG_C) : resetFlag(FLAG_C);

    if (val == 0)
    {
        setFlag(FLAG_Z);
    }
    else
    {
        resetFlag(FLAG_Z);
    }

    return val;
}

/**
 * @brief Rotate Right Circular
 * @note  bit 0 is copied to carry and bit 7
 * @param val value to rotate
 */
static uint8_t _rrc(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (val & 0x01)
    {
        setFlag(FLAG_C);
    }
    else
    {
        resetFlag(FLAG_C);
    }

    // get bit 7 of previous value
    uint8_t ret = (val >> 1) | (testFlag(FLAG_C) << 7);

    if (ret == 0)
    {
        setFlag(FLAG_Z);
    }
    else
    {
        resetFlag(FLAG_Z);
    }

    return ret;
}

/**
 * @brief Shift Right Arithmetic
 * @note  rightshift one bit without changing sign bit (7), and copy bit 0 to carry
 * @param val value to shift
 * @return uint8_t shifted value
 */
static uint8_t _sra(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    uint8_t bit7 = val & 0x80;

    if (val & 0x01)
    {
        setFlag(FLAG_C);
    }
    else
    {
        resetFlag(FLAG_C);
    }

    uint8_t ret = (val >> 1) | bit7;

    if (ret == 0)
    {
        setFlag(FLAG_Z);
    }
    else
    {
        resetFlag(FLAG_Z);
    }

    return ret;
}

/**
 * @brief shift right logical: >>1, bit 7 is 0, bit 0 to carry
 * 
 * @param val 
 * @return uint8_t 
 */
static uint8_t _srl(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (val & 0x01)
    {
        setFlag(FLAG_C);
    }
    else
    {
        resetFlag(FLAG_C);
    }
    uint8_t ret = val >> 1;

    if (ret == 0)
    {
        setFlag(FLAG_Z);
    }
    else
    {
        resetFlag(FLAG_Z);
    }

    return ret;
}

static size_t decodeAndExecuteCBPrefix(void)
{
    uint8_t instr     = fetch8(pGetCPURegisters()->reg16.pc + 1); // CB prefix takes next instruction
    uint8_t hi        = instr >> 4;
    uint8_t lo        = instr & 15;

    size_t  addCycles = 0;

    switch (hi)
    {
        // RLC/RRC
        case 0x00:
        {
            if (lo < 0x06)
            {
                // RLC basic, B to L
                rlc_reg(getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                // RLC (HL)
                rlc_addr(pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                // RLC A
                rlc_reg(A);
            }
            else if (lo < 0x0E)
            {
                // RRC basic, B to L
                rrc_reg(getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                // RRC (HL)
                rrc_addr(pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                // RRC A
                rrc_reg(A);
            }
            break;
        }
        // RL/RR
        case 0x01:
        {
            if (lo < 0x06)
            {
                // RL basic, B to L
                rl_reg(getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                // RL (HL)
                rl_addr(pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                // RL A
                rl_reg(A);
            }
            else if (lo < 0x0E)
            {
                // RR basic, B to L
                rr_reg(getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                // RR (HL)
                rr_addr(pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                // RR A
                rr_reg(A);
            }
            break;
        }
        // SLA/SRA
        case 0x02:
        {
            if (lo < 0x06)
            {
                // SLA basic, B to L
                sla_reg(getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                // SLA (HL)
                sla_addr(pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                // SLA A
                sla_reg(A);
            }
            else if (lo < 0x0E)
            {
                // SRA basic, B to L
                sra_reg(getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                // SRA (HL)
                sra_addr(pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                // SRA A
                sra_reg(A);
            }
            break;
        }
        // SWAP/SRL
        case 0x03:
        {
            if (lo < 0x06)
            {
                // SWAP basic, B to L
                swap8_reg(getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                // SWAP (HL)
                swap8_addr(pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                // SWAP A
                swap8_reg(A);
            }
            else if (lo < 0x0E)
            {
                // SRL basic, B to L
                srl_reg(getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                // SRL (HL)
                srl_addr(pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                // SRL A
                srl_reg(A);
            }
            break;
        }
        // BIT 0/1, reg
        case 0x04:
        {
            if (lo < 0x06)
            {
                bit_n_reg(0, getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                bit_n_addr(0, pGetCPURegisters()->reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x07)
            {
                bit_n_reg(0, A);
            }
            else if (lo < 0x0E)
            {
                bit_n_reg(1, getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                bit_n_addr(1, pGetCPURegisters()->reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x0F)
            {
                bit_n_reg(1, A);
            }
            break;
        }
        // BIT 2/3, reg
        case 0x05:
        {
            if (lo < 0x06)
            {
                bit_n_reg(2, getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                bit_n_addr(2, pGetCPURegisters()->reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x07)
            {
                bit_n_reg(2, A);
            }
            else if (lo < 0x0E)
            {
                bit_n_reg(3, getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                bit_n_addr(3, pGetCPURegisters()->reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x0F)
            {
                bit_n_reg(3, A);
            }
            break;
        }
        // BIT 4/5, reg
        case 0x06:
        {
            if (lo < 0x06)
            {
                bit_n_reg(4, getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                bit_n_addr(4, pGetCPURegisters()->reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x07)
            {
                bit_n_reg(4, A);
            }
            else if (lo < 0x0E)
            {
                bit_n_reg(5, getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                bit_n_addr(5, pGetCPURegisters()->reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x0F)
            {
                bit_n_reg(5, A);
            }
            break;
        }
        // BIT 6/7, reg
        case 0x07:
        {
            if (lo < 0x06)
            {
                bit_n_reg(6, getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                bit_n_addr(6, pGetCPURegisters()->reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x07)
            {
                bit_n_reg(6, A);
            }
            else if (lo < 0x0E)
            {
                bit_n_reg(7, getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                bit_n_addr(7, pGetCPURegisters()->reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x0F)
            {
                bit_n_reg(7, A);
            }
            break;
        }
        // RES 0/1
        case 0x08:
        {
            if (lo < 0x06)
            {
                reset_n_reg(0, getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                reset_n_addr(0, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                reset_n_reg(0, A);
            }
            else if (lo < 0x0E)
            {
                reset_n_reg(1, getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                reset_n_addr(1, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                reset_n_reg(1, A);
            }
            break;
        }
        // RES 2/3
        case 0x09:
        {
            if (lo < 0x06)
            {
                reset_n_reg(2, getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                reset_n_addr(2, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                reset_n_reg(2, A);
            }
            else if (lo < 0x0E)
            {
                reset_n_reg(3, getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                reset_n_addr(3, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                reset_n_reg(3, A);
            }
            break;
        }
        // RES 4/5
        case 0x0A:
        {
            if (lo < 0x06)
            {
                reset_n_reg(4, getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                reset_n_addr(4, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                reset_n_reg(4, A);
            }
            else if (lo < 0x0E)
            {
                reset_n_reg(5, getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                reset_n_addr(5, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                reset_n_reg(5, A);
            }
            break;
        }
        // RES 6/7
        case 0x0B:
        {
            if (lo < 0x06)
            {
                reset_n_reg(6, getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                reset_n_addr(6, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                reset_n_reg(6, A);
            }
            else if (lo < 0x0E)
            {
                reset_n_reg(7, getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                reset_n_addr(7, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                reset_n_reg(7, A);
            }
            break;
        }
        // SET 0/1
        case 0x0C:
        {
            if (lo < 0x06)
            {
                set_n_reg(0, getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                set_n_addr(0, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                set_n_reg(0, A);
            }
            else if (lo < 0x0E)
            {
                set_n_reg(1, getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                set_n_addr(1, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                set_n_reg(1, A);
            }
            break;
        }
        // SET 2/3
        case 0x0D:
        {
            if (lo < 0x06)
            {
                set_n_reg(2, getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                set_n_addr(2, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                set_n_reg(2, A);
            }
            else if (lo < 0x0E)
            {
                set_n_reg(3, getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                set_n_addr(3, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                set_n_reg(3, A);
            }
            break;
        }
        // SET 4/5
        case 0x0E:
        {
            if (lo < 0x06)
            {
                set_n_reg(4, getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                set_n_addr(4, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                set_n_reg(4, A);
            }
            else if (lo < 0x0E)
            {
                set_n_reg(5, getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                set_n_addr(5, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                set_n_reg(5, A);
            }
            break;
        }
        // SET 6/7
        case 0x0F:
        {
            if (lo < 0x06)
            {
                set_n_reg(6, getRegIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                set_n_addr(6, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                set_n_reg(6, A);
            }
            else if (lo < 0x0E)
            {
                set_n_reg(7, getRegIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                set_n_addr(7, pGetCPURegisters()->reg16.hl);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                set_n_reg(7, A);
            }
            break;
        }

        default:
        {
            printf("unknown CB instr CB %02X\n", instr);
            while (true);
        }
    }

    return addCycles;
}

static size_t getRegIndexByOpcodeNibble(uint8_t lo)
{
    switch (lo)
    {
        case 0x0:
        case 0x8:
            return 3; // B
        case 0x1:
        case 0x9:
            return 2; // C
        case 0x2:
        case 0xA:
            return 5; // D
        case 0x3:
        case 0xB:
            return 4; // E
        case 0x4:
        case 0xC:
            return 7; // H
        case 0x5:
        case 0xD:
            return 6; // L
        case 0x7:
        case 0xF:
            return 1; // A
        default:
            return -1;
    }
}