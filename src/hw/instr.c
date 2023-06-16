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

#include "cpu.h"
#include "mem.h"

static cpu_t *pCpu;

#define REG_A_VALUE ((uint8_t)(pCpu->af & 0xFF))

#define CHECK_HALF_CARRY_ADD8(a, b) ((((a & 0xf) + (b & 0xf)) & 0x10) == 0x10)
#define CHECK_HALF_CARRY_SUB8(a, b) ((((a & 0xf) - (b & 0xf)) & 0x10) == 0x10)

#define CHECK_HALF_CARRY_ADD16(a, b) ((((a & 0xfff) + (b & 0xfff)) & 0x1000) == 0x1000)
#define CHECK_HALF_CARRY_SUB16(a, b) ((((a & 0xfff) - (b & 0xfff)) & 0x1000) == 0x1000)

void getCpuPtr(const cpu_t *pCpuSet)
{
    pCpu = pCpuSet;
}

// 8 bit loads

void ld_reg8_imm(Register8 reg, uint8_t val)
{
    setRegister8(reg, val);
}

void ld_reg8_addr(Register8 left, uint16_t addr)
{
    setRegister8(left, fetch8(addr));
}

void ld_reg8_reg8(Register8 left, Register8 right)
{
    uint8_t rval = (uint8_t)pCpu + (right * sizeof(uint8_t));
    setRegister8(left, rval);
}

void ld_addr_reg8(uint16_t addr, Register8 right)
{
    uint8_t rval = (uint8_t)pCpu + (right * sizeof(uint8_t));
    write8(addr, rval);
}

void ld_addr_imm(uint16_t addr, uint8_t val)
{
    write8(val, addr);
}

void ldd_a_hl()
{
    setRegister8(A, fetch8(pCpu->hl--));
}

void ldd_hl_a()
{
    write8(REG_A_VALUE, pCpu->hl--);
}

void ldi_a_hl()
{
    setRegister8(A, fetch8(pCpu->hl++));
}

void ldi_hl_a()
{
    write8(REG_A_VALUE, pCpu->hl++);
}

void ldh_offset_mem_a(uint8_t offset)
{
    write8(REG_A_VALUE, (uint16_t)(0xFF00 + offset));
}

void ldh_a_offset_mem(uint8_t offset)
{
    setRegister8(A, (uint16_t)(0xFF00 + offset));
}

// 16-bit loads

void ld_reg16_imm(Register16 reg, uint16_t val)
{
    setRegister16(reg, val);
}

void ldhl_sp_offset(uint8_t offset)
{
    setRegister16(HL, (uint16_t)(pCpu->sp + offset));
}

void push_reg16(Register16 reg)
{
    uint8_t rval = (uint8_t)pCpu + (reg * sizeof(uint16_t));
    write16(rval, pCpu->sp);
    pCpu->sp -= 2;
}

void pop_reg16(Register16 reg)
{
    setRegister16(reg, fetch16(pCpu->sp));
    pCpu->sp += 2;
}

// 8-bit arithmetic

void add8_a_n(uint8_t val)
{
    REG_A_VALUE + val == 0x100 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    REG_A_VALUE + val > UINT8_MAX ? setFlag(FLAG_C) : resetFlag(FLAG_C);

    CHECK_HALF_CARRY_ADD8(val, REG_A_VALUE) ? setFlag(FLAG_H) : resetFlag(FLAG_H);
    
    resetFlag(FLAG_N);

    setRegister8(A, (uint8_t)REG_A_VALUE + val);
}

void adc8_a_n(uint8_t val)
{
    REG_A_VALUE + val + 1 > UINT8_MAX ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    REG_A_VALUE + val + 1 == 0x100 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);

    CHECK_HALF_CARRY_ADD8(val + 1, REG_A_VALUE) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    resetFlag(FLAG_N);

    setRegister8(A, (uint8_t)REG_A_VALUE + val + 1);
}

void sub8_n_a(uint8_t val)
{
    setFlag(FLAG_N);

    REG_A_VALUE - val > 0 ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    REG_A_VALUE - val == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);

    CHECK_HALF_CARRY_SUB8(val, REG_A_VALUE) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    setRegister8(A, (uint8_t)(REG_A_VALUE - val));
}

void sbc8_a_n(uint8_t val)
{
    setFlag(FLAG_N);

    REG_A_VALUE - val - 1 > 0 ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    REG_A_VALUE - val - 1 == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);

    CHECK_HALF_CARRY_SUB8(val - 1, REG_A_VALUE) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    setRegister8(A, (uint8_t)(REG_A_VALUE - val - 1));
}

void and8_a_n(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_C);
    setFlag(FLAG_H);
    
    setRegister8(A, REG_A_VALUE & val);

    REG_A_VALUE == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
}

void or8_a_n(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_C);
    resetFlag(FLAG_H);

    setRegister8(A, REG_A_VALUE | val);

    REG_A_VALUE == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
}

void xor8_a_n(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_C);
    resetFlag(FLAG_H);

    setRegister8(A, REG_A_VALUE ^ val);

    REG_A_VALUE == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
}

void cp8_a_n(uint8_t val)
{
    setFlag(FLAG_N);

    REG_A_VALUE == val ? setFlag(FLAG_Z); resetFlag(FLAG_Z);

    REG_A_VALUE - val - 1 > 0 ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    CHECK_HALF_CARRY_SUB8(val, REG_A_VALUE) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    // results are thrown away
}

void inc8_reg(Register8 reg)
{
    uint8_t rval = (uint8_t)pCpu + (reg * sizeof(uint8_t));

    resetFlag(FLAG_N);
    CHECK_HALF_CARRY_ADD8(rval, 1) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    rval + 1 == 0x100 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);

    setRegister8(reg, (uint8_t)(rval + 1));
}

void dec8_reg(Register8 reg)
{
    uint8_t rval = (uint8_t)pCpu + (reg * sizeof(uint8_t));

    resetFlag(FLAG_N);
    CHECK_HALF_CARRY_SUB8(rval, 1) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    rval - 1 == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);

    setRegister8(reg, (uint8_t)(rval + 1));
}

// 16-bit arithmetic

void add16_hl_n(uint16_t val)
{
    resetFlag(FLAG_N);

    CHECK_HALF_CARRY_ADD16(val + 1, REG_A_VALUE) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    pCpu->hl + val + 1 > UINT16_MAX ? setFlag(FLAG_C) : resetFlag(FLAG_C);

    setRegister16(HL, (uint16_t)(pCpu->hl + val));
}

void add16_sp_n(uint16_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_Z);

    setRegister16(SP, (uint16_t)(pCpu->sp + val));
}

void inc16_reg(Register16 reg)
{
    uint16_t rval = (uint16_t)pCpu + (reg * sizeof(uint16_t));
    setRegister16(reg, rval + 1);
}

void dec16_reg(Register16 reg)
{
    uint16_t rval = (uint16_t)pCpu + (reg * sizeof(uint16_t));
    setRegister16(reg, rval - 1);
}