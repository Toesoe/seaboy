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

#include "mem.h"

static cpu_t *pCpu;

#define CHECK_HALF_CARRY_ADD8(a, b) ((((a & 0xf) + (b & 0xf)) & 0x10) == 0x10)
#define CHECK_HALF_CARRY_SUB8(a, b) ((((a & 0xf) - (b & 0xf)) & 0x10) == 0x10)

#define CHECK_HALF_CARRY_ADD16(a, b) ((((a & 0xfff) + (b & 0xfff)) & 0x1000) == 0x1000)
#define CHECK_HALF_CARRY_SUB16(a, b) ((((a & 0xfff) - (b & 0xfff)) & 0x1000) == 0x1000)

void setCpuPtr(const cpu_t *pCpuSet)
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

void ldd_a_hl(void)
{
    setRegister8(A, fetch8(pCpu->reg16.hl--));
}

void ldd_hl_a(void)
{
    write8(pCpu->reg8.a, pCpu->reg16.hl--);
}

void ldi_a_hl(void)
{
    setRegister8(A, fetch8(pCpu->reg16.hl++));
}

void ldi_hl_a(void)
{
    write8(pCpu->reg8.a, pCpu->reg16.hl++);
}

void ldh_offset_mem_a(uint8_t offset)
{
    write8(pCpu->reg8.a, (uint16_t)(0xFF00 + offset));
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
    setRegister16(HL, (uint16_t)(pCpu->reg16.sp + offset));
}

void push_reg16(Register16 reg)
{
    uint8_t rval = (uint8_t)pCpu + (reg * sizeof(uint16_t));
    write16(pCpu->reg16_arr[reg], pCpu->reg16.sp);
    setRegister16(SP,  pCpu->reg16.sp - 2);
}

void pop_reg16(Register16 reg)
{
    setRegister16(reg, fetch16(pCpu->reg16.sp));
    setRegister16(SP, pCpu->reg16.sp + 2);
}

// 8-bit arithmetic

void add8_a_n(uint8_t val)
{
    pCpu->reg8.a + val == 0x100 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    pCpu->reg8.a + val > UINT8_MAX ? setFlag(FLAG_C) : resetFlag(FLAG_C);

    CHECK_HALF_CARRY_ADD8(val, pCpu->reg8.a) ? setFlag(FLAG_H) : resetFlag(FLAG_H);
    
    resetFlag(FLAG_N);

    setRegister8(A, (uint8_t)pCpu->reg8.a + val);
}

void adc8_a_n(uint8_t val)
{
    pCpu->reg8.a + val + 1 > UINT8_MAX ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    pCpu->reg8.a + val + 1 == 0x100 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);

    CHECK_HALF_CARRY_ADD8(val + 1, pCpu->reg8.a) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    resetFlag(FLAG_N);

    setRegister8(A, (uint8_t)pCpu->reg8.a + val + 1);
}

void sub8_n_a(uint8_t val)
{
    setFlag(FLAG_N);

    pCpu->reg8.a - val > 0 ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    pCpu->reg8.a - val == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);

    CHECK_HALF_CARRY_SUB8(val, pCpu->reg8.a) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    setRegister8(A, (uint8_t)(pCpu->reg8.a - val));
}

void sbc8_a_n(uint8_t val)
{
    setFlag(FLAG_N);

    pCpu->reg8.a - val - 1 > 0 ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    pCpu->reg8.a - val - 1 == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);

    CHECK_HALF_CARRY_SUB8(val - 1, pCpu->reg8.a) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    setRegister8(A, (uint8_t)(pCpu->reg8.a - val - 1));
}

void and8_a_n(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_C);
    setFlag(FLAG_H);
    
    setRegister8(A, pCpu->reg8.a & val);

    pCpu->reg8.a == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
}

void or8_a_n(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_C);
    resetFlag(FLAG_H);

    setRegister8(A, pCpu->reg8.a | val);

    pCpu->reg8.a == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
}

void xor8_a_n(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_C);
    resetFlag(FLAG_H);

    setRegister8(A, pCpu->reg8.a ^ val);

    pCpu->reg8.a == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
}

void cp8_a_n(uint8_t val)
{
    setFlag(FLAG_N);

    pCpu->reg8.a == val ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);

    pCpu->reg8.a - val - 1 > 0 ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    CHECK_HALF_CARRY_SUB8(val, pCpu->reg8.a) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

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

    CHECK_HALF_CARRY_ADD16(val + 1, pCpu->reg8.a) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    pCpu->reg16.hl + val + 1 > UINT16_MAX ? setFlag(FLAG_C) : resetFlag(FLAG_C);

    setRegister16(HL, (uint16_t)(pCpu->reg16.hl + val));
}

void add16_sp_n(uint16_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_Z);

    setRegister16(SP, (uint16_t)(pCpu->reg16.sp + val));
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

// misc

uint8_t swap8_n(uint8_t val)
{
    return ((val & 0x0F) << 4) | ((val & 0xF0) >> 4);
}

/**
 * @brief bcd conversion of value in reg a
 * @note  this instruction is weird. stole the implementation from https://forums.nesdev.org/viewtopic.php?t=15944
 * 
 */
void daa(void)
{
    uint8_t aVal = pCpu->reg8.a;

    if (!testFlag(FLAG_N))
    {
        // after an addition, adjust if (half-)carry occurred or if result is out of bounds
        if (testFlag(FLAG_C) || pCpu->reg8.a > 0x99) { aVal += 0x60; setFlag(FLAG_C); }
        if (testFlag(FLAG_H) || (pCpu->reg8.a & 0x0f) > 0x09) { aVal += 0x6; }
    }
    else
    {
        // after a subtraction, only adjust if (half-)carry occurred
        if (testFlag(FLAG_C)) { aVal -= 0x60; }
        if (testFlag(FLAG_H)) { aVal -= 0x6; }
    }

    if (aVal == 0) setFlag(FLAG_Z);
    resetFlag(FLAG_H);

    setRegister8(A, aVal);
}

void cpl(void)
{
    setRegister8(A, ~pCpu->reg8.a);
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);
}

void ccf(void)
{
    if (testFlag(FLAG_C)) { resetFlag(FLAG_C); }
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

void rlca(void)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (pCpu->reg8.a & 0x80) { setFlag(FLAG_C); } // get bit 7
    setRegister8(A, (pCpu->reg8.a << 1 | pCpu->reg8.a >> 7));

    if (pCpu->reg8.a == 0) { setFlag(FLAG_Z); }
}

void rla(void)
{
    uint8_t regVal = pCpu->reg8.a;

    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    setRegister8(A, (pCpu->reg8.a << 1 | testFlag(FLAG_C)));

    if (regVal & 0x80) { setFlag(FLAG_C); } // get bit 7 of previous value

    if (pCpu->reg8.a == 0) { setFlag(FLAG_Z); }
}

void rrca(void)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (pCpu->reg8.a & 0x01) { setFlag(FLAG_C); } // get bit 0
    setRegister8(A, (pCpu->reg8.a >> 1 | pCpu->reg8.a << 7));

    if (pCpu->reg8.a == 0) { setFlag(FLAG_Z); }
}

void rra(void)
{
    uint8_t regVal = pCpu->reg8.a;

    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    setRegister8(A, (pCpu->reg8.a >> 1 | testFlag(FLAG_C)));

    if (regVal & 0x01) { setFlag(FLAG_C); } // get bit 0 of previous value

    if (pCpu->reg8.a == 0) { setFlag(FLAG_Z); }
}

void rlc_reg(Register8 reg)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (pCpu->reg8_arr[reg] & 0x80) { setFlag(FLAG_C); } // get bit 7
    setRegister8(reg, (pCpu->reg8_arr[reg] << 1 | pCpu->reg8_arr[reg] >> 7));

    if (pCpu->reg8_arr[reg] == 0) { setFlag(FLAG_Z); }
}

void rlc_addr(uint16_t addr)
{
    uint8_t val = fetch8(addr);
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (val & 0x80) { setFlag(FLAG_C); } // get bit 7
    write8((val << 1 | val >> 7), addr);

    if (fetch8(addr) == 0) { setFlag(FLAG_Z); }
}

void rl_reg(Register8 reg)
{
    uint8_t regVal = pCpu->reg8_arr[reg];

    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    setRegister8(reg, (pCpu->reg8_arr[reg] << 1 | testFlag(FLAG_C)));

    if (regVal & 0x80) { setFlag(FLAG_C); } // get bit 7 of previous value

    if (pCpu->reg8_arr[reg] == 0) { setFlag(FLAG_Z); }
}

void rl_addr(uint16_t addr)
{
    uint8_t val = fetch8(addr);

    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    write8(addr, (val << 1 | testFlag(FLAG_C)));

    if (val & 0x80) { setFlag(FLAG_C); } // get bit 7 of previous value

    if (fetch8(addr) == 0) { setFlag(FLAG_Z); }
}

void rrc_reg(Register8 reg)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (pCpu->reg8_arr[reg] & 0x80) { setFlag(FLAG_C); } // get bit 7
    setRegister8(reg, (pCpu->reg8_arr[reg] >> 1 | pCpu->reg8_arr[reg] << 7));

    if (pCpu->reg8_arr[reg] == 0) { setFlag(FLAG_Z); }
}

void rrc_addr(uint16_t addr)
{
    uint8_t val = fetch8(addr);
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (val & 0x01) { setFlag(FLAG_C); } // get bit 0
    write8((val >> 1 | val << 7), addr);

    if (fetch8(addr) == 0) { setFlag(FLAG_Z); }
}

void rr_reg(Register8 reg)
{
    uint8_t regVal = pCpu->reg8_arr[reg];

    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    setRegister8(reg, (pCpu->reg8_arr[reg] >> 1 | testFlag(FLAG_C)));

    if (regVal & 0x01) { setFlag(FLAG_C); } // get bit 0 of previous value

    if (pCpu->reg8_arr[reg] == 0) { setFlag(FLAG_Z); }
}

void rr_addr(uint16_t addr)
{
    uint8_t val = fetch8(addr);

    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    write8(addr, (val >> 1 | testFlag(FLAG_C)));

    if (val & 0x01) { setFlag(FLAG_C); } // get bit 7 of previous value

    if (fetch8(addr) == 0) { setFlag(FLAG_Z); }
}

void sla_reg(Register8 reg)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (pCpu->reg8_arr[reg] & 0x80) { setFlag(FLAG_C); }

    setRegister8(reg, pCpu->reg8_arr[reg] << 1);
}

void sla_addr(uint16_t addr)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (fetch8(addr) & 0x80) { setFlag(FLAG_C); }

    write8(fetch8(addr) << 1, addr);
}

void sra_reg(Register8 reg)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (pCpu->reg8_arr[reg] & 0x01) { setFlag(FLAG_C); }

    setRegister8(reg, (pCpu->reg8_arr[reg] >> 1) | (pCpu->reg8_arr[reg] & 0x80));
}

void sra_addr(uint16_t addr)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (fetch8(addr) & 0x01) { setFlag(FLAG_C); }

    write8(fetch8(addr) >> 1, addr);
}

void sla_reg(Register8 reg)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (pCpu->reg8_arr[reg] & 0x01) { setFlag(FLAG_C); }

    setRegister8(reg, (pCpu->reg8_arr[reg] >> 1) | (pCpu->reg8_arr[reg] & 0x80));
}

void sla_addr(uint16_t addr)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (fetch8(addr) & 0x01) { setFlag(FLAG_C); }

    write8(fetch8(addr) >> 1, addr);
}

// single bit stuff

void bit_n_reg(uint8_t bit, Register8 reg)
{
    resetFlag(FLAG_N);
    setFlag(FLAG_H);

    // Z set if bit set
    pCpu->reg8_arr[reg] & (1 << bit) ? resetFlag(FLAG_Z) : setFlag(FLAG_Z);
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
    pCpu->reg8_arr[reg] |= (1 << bit);
}

void set_n_addr(uint8_t bit, uint16_t addr)
{
    write8(fetch8(addr) | (1 << bit), addr);
}

void reset_n_reg(uint8_t bit, Register8 reg)
{
    pCpu->reg8_arr[reg] &= ~(1 << bit);
}

void reset_n_addr(uint8_t bit, uint16_t addr)
{
    write8(fetch8(addr) & ~(1 << bit), addr);
}

// jumps

void jmp_nn(uint16_t addr)
{
    setRegister16(PC, addr);
}

/**
 * @brief perform conditional jump
 * 
 * @param addr addr to jump to
 * @param flag flag to test
 * @param testSet if true, will execute jump if specified flag is set
 */
void jmp_nn_cond(uint16_t addr, Flag flag, bool testSet)
{
    if (testFlag(flag))
    {
        if (testSet) { jmp_nn(addr); }
    }
    else
    {
        if (!testSet) { jmp_nn(addr); }
    }
}

void jmp_hl(void)
{
    setRegister16(PC, pCpu->reg16.hl);
}

void jr_n(int8_t val)
{
    setRegister16(PC, pCpu->reg16.pc + val);
}

void jr_n_cond(int8_t val, Flag flag, bool testSet)
{
    if (testFlag(flag))
    {
        if (testSet) { jr_n(val); }
    }
    else
    {
        if (!testSet) { jr_n(val); }
    }
}

// calls

void call_nn(uint16_t val)
{
    write16(pCpu->reg16.pc + 2, pCpu->reg16.sp);
    pCpu->reg16.sp -= 2;
    setRegister16(PC, val);
}

void call_nn_cond(uint16_t val, Flag flag, bool testSet)
{
    if (testFlag(flag))
    {
        if (testSet) 
        {
            call_nn(val);
        }
    }
    else
    {
        if (!testSet) 
        {
            call_nn(val);
        }
    }
}

void rst_n(uint8_t val)
{
    switch(val)
    {
        case 0x00:
        case 0x08:
        case 0x10:
        case 0x18:
        case 0x20:
        case 0x28:
        case 0x30:
        case 0x38:
            write16(pCpu->reg16.pc, pCpu->reg16.sp);
            pCpu->reg16.sp -= 2;
            setRegister16(PC, val);
            break;
        default:
            while(true) {} // hang for now
    }
}

void ret(void)
{
    setRegister16(PC, fetch16(pCpu->reg16.sp));
    pCpu->reg16.sp += 2;
}

void ret_cond(Flag flag, bool testSet)
{
    if (testFlag(flag))
    {
        if (testSet) 
        {
            ret();
        }
    }
    else
    {
        if (!testSet) 
        {
            ret();
        }
    }
}

void reti(void)
{
    ret();
    changeIME(true);
}