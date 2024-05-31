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

#define CHECK_HALF_CARRY_ADD8(a, b) (((((a) & 0xf) + ((b) & 0xf)) & 0x10) == 0x10)
#define CHECK_HALF_CARRY_SUB8(a, b) (((((a) & 0xf) - ((b) & 0xf)) & 0x10) == 0x10)

#define CHECK_HALF_CARRY_ADD16(a, b) (((((a) & 0xfff) + ((b) & 0xfff)) & 0x1000) == 0x1000)
#define CHECK_HALF_CARRY_SUB16(a, b) (((((a) & 0xfff) - ((b) & 0xfff)) & 0x1000) == 0x1000)


// static defs
static uint8_t _rl(uint8_t val);
static uint8_t _rlc(uint8_t val);
static uint8_t _sla(uint8_t val);
static uint8_t _rr(uint8_t val);
static uint8_t _rrc(uint8_t val);
static uint8_t _sra(uint8_t val);
static uint8_t _srl(uint8_t val);

void instrSetCpuPtr(cpu_t *pCpuSet)
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
    setRegister8(left, pCpu->reg8_arr[right]);
}

void ld_addr_reg8(uint16_t addr, Register8 right)
{
    write8(pCpu->reg8_arr[right], addr);
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
    setRegister16(SP,  pCpu->reg16.sp - 2);
    write16(pCpu->reg16_arr[reg], pCpu->reg16.sp);
}

void pop_reg16(Register16 reg)
{
    setRegister16(reg, fetch16(pCpu->reg16.sp));
    setRegister16(SP, pCpu->reg16.sp + 2);
}

// 8-bit arithmetic

void add8_a_n(uint8_t val)
{
    uint16_t sum = pCpu->reg8.a + val;

    (sum & 0xFF) == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    (sum > UINT8_MAX) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    CHECK_HALF_CARRY_ADD8(val, pCpu->reg8.a) ? setFlag(FLAG_H) : resetFlag(FLAG_H);
    
    resetFlag(FLAG_N); // Reset the subtraction flag

    setRegister8(A, (uint8_t)sum);
}

void adc8_a_n(uint8_t val)
{
    uint16_t sum = pCpu->reg8.a + val + testFlag(FLAG_C); // Perform addition with carry

    (sum > UINT8_MAX) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    (sum & 0xFF) == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    CHECK_HALF_CARRY_ADD8(val + testFlag(FLAG_C), pCpu->reg8.a) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    resetFlag(FLAG_N);

    setRegister8(A, (uint8_t)sum);
}

void sub8_n_a(uint8_t val)
{
    int16_t result = pCpu->reg8.a - val;

    (result >= 0) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    (result == 0) ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    CHECK_HALF_CARRY_SUB8(val, pCpu->reg8.a) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    setFlag(FLAG_N);

    setRegister8(A, (uint8_t)result);
}

void sbc8_a_n(uint8_t val)
{
    uint8_t carry = testFlag(FLAG_C) ? 1 : 0; // Determine if there's a carry from a previous operation

    int16_t result = pCpu->reg8.a - val - carry; // Perform the subtraction with borrow

    (result < 0) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    (result == 0) ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    CHECK_HALF_CARRY_SUB8(val + carry, pCpu->reg8.a) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    setFlag(FLAG_N); // Always set the subtraction flag

    setRegister8(A, (uint8_t)result);
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
    setFlag(FLAG_N); // Set the subtraction flag

    (pCpu->reg8.a == val) ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);

    (pCpu->reg8.a < val) ? setFlag(FLAG_C) : resetFlag(FLAG_C);

    // Check for half-carry
    CHECK_HALF_CARRY_SUB8(pCpu->reg8.a, val) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    // Results are thrown away, but flags are set based on comparison
}

void inc8_reg(Register8 reg)
{
    uint8_t value = pCpu->reg8_arr[reg];

    uint8_t result = value + 1;
    result == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    resetFlag(FLAG_N);
    CHECK_HALF_CARRY_ADD8(value, 1) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    setRegister8(reg, result);
}

void dec8_reg(Register8 reg)
{
    uint8_t value = pCpu->reg8_arr[reg];

    uint8_t result = value - 1;
    result == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    setFlag(FLAG_N);
    CHECK_HALF_CARRY_SUB8(value, 1) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    setRegister8(reg, result);
}

void inc8_mem(uint16_t addr)
{
    uint8_t value = fetch8(addr);

    uint8_t result = value + 1;
    result == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    resetFlag(FLAG_N);
    CHECK_HALF_CARRY_ADD8(value, 1) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    write8(result, addr);
}

void dec8_mem(uint16_t addr)
{
    uint8_t value = fetch8(addr);

    uint8_t result = value - 1;
    result == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
    setFlag(FLAG_N); // Set N flag to indicate subtraction
    CHECK_HALF_CARRY_SUB8(value, 1) ? setFlag(FLAG_H) : resetFlag(FLAG_H);

    write8(result, addr);
}

// 16-bit arithmetic

void add16_hl_n(uint16_t val)
{
    uint32_t sum = pCpu->reg16.hl + val;

    (sum > UINT16_MAX) ? setFlag(FLAG_C) : resetFlag(FLAG_C);
    CHECK_HALF_CARRY_ADD16(pCpu->reg16.hl, val) ? setFlag(FLAG_H) : resetFlag(FLAG_H);
    
    resetFlag(FLAG_N);

    setRegister16(HL, (uint16_t)sum);
}

void add16_sp_n(uint16_t val)
{
    uint32_t sum = pCpu->reg16.sp + val;
    (sum > UINT16_MAX) ? setFlag(FLAG_C) : resetFlag(FLAG_C);

    resetFlag(FLAG_N);
    setRegister16(SP, (uint16_t)sum);
}

void inc16_reg(Register16 reg)
{
    setRegister16(reg, pCpu->reg16_arr[reg] + 1);
}

void dec16_reg(Register16 reg)
{
    setRegister16(reg, pCpu->reg16_arr[reg] - 1);
}

// misc

void swap8_reg(Register8 reg)
{
    setRegister8(reg, ((pCpu->reg8_arr[reg] & 0x0F) << 4) | ((pCpu->reg8_arr[reg] & 0xF0) >> 4));
    resetFlag(FLAG_N);
    resetFlag(FLAG_C);
    resetFlag(FLAG_H);

    pCpu->reg8_arr[reg] == 0 ? setFlag(FLAG_Z) : resetFlag(FLAG_Z);
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

/**
 * @brief 0x07, rotate left circular accumulator
 * @note  bit 7 is copied to the CARRY flag, CARRY goes to bit 0
 * 
 */
void rlca(void)
{
    bool zFlag = testFlag(FLAG_Z);
    setRegister8(A, _rlc(pCpu->reg8.a));
    if (zFlag) { setFlag(FLAG_Z); }
}

/**
 * @brief 0x17, rotate left accumulator
 * @note  bit 7 goes to carry and 0. functionally identical to rlca
 * 
 */
void rla(void)
{
    bool zFlag = testFlag(FLAG_Z);
    setRegister8(A, _rl(pCpu->reg8.a));
    if (zFlag) { setFlag(FLAG_Z); }
}

void rrca(void)
{
    setRegister8(A, _rrc(pCpu->reg8.a));
}

void rra(void)
{
    bool zFlag = testFlag(FLAG_Z);
    setRegister8(A, _rr(pCpu->reg8.a));
    if (zFlag) { setFlag(FLAG_Z); }
}

void rlc_reg(Register8 reg)
{
    setRegister8(reg, _rlc(pCpu->reg8_arr[reg]));
    if (reg == A)
    {
        resetFlag(FLAG_Z);
    }
}

void rlc_addr(uint16_t addr)
{
    write8(_rlc(fetch8(addr)), addr);
}

void rl_reg(Register8 reg)
{
    setRegister8(reg, _rl(pCpu->reg8_arr[reg]));
}

void rl_addr(uint16_t addr)
{
    write8(addr, _rl(fetch8(addr)));
}

void rrc_reg(Register8 reg)
{
    setRegister8(reg, _rrc(pCpu->reg8_arr[reg]));
}

void rrc_addr(uint16_t addr)
{
    write8(_rrc(fetch8(addr)), addr);
}

void rr_reg(Register8 reg)
{
    setRegister8(reg, _rr(pCpu->reg8_arr[reg]));
}

void rr_addr(uint16_t addr)
{
    write8(addr, _rr(fetch8(addr)));
}

void sla_reg(Register8 reg)
{
    setRegister8(reg, _sla(pCpu->reg8_arr[reg]));
}

void sla_addr(uint16_t addr)
{
    write8(addr, _sla(fetch8(addr)));
}

void sra_reg(Register8 reg)
{
    setRegister8(reg, _sra(pCpu->reg8_arr[reg]));
}

void sra_addr(uint16_t addr)
{
    write8(_sra(fetch8(addr)), addr);
}

void srl_reg(Register8 reg)
{
    setRegister8(reg, _srl(pCpu->reg8_arr[reg]));
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
 * @return true if we jumped
 */
bool jmp_nn_cond(uint16_t addr, Flag flag, bool testSet)
{
    bool ret = false;

    if (testFlag(flag))
    {
        if (testSet) { jmp_nn(addr); ret = true; }
    }
    else
    {
        if (!testSet) { jmp_nn(addr); ret = true; }
    }

    return ret;
}

void jmp_hl(void)
{
    setRegister16(PC, pCpu->reg16.hl);
}

void jr_n(int8_t val)
{
    setRegister16(PC, pCpu->reg16.pc + val);
}

void jr_n_signed(int8_t val)
{
    setRegister16(PC, pCpu->reg16.pc + (signed char)val);
}

/**
 * @return true if we jumped
 */
bool jr_n_cond(int8_t val, Flag flag, bool testSet)
{
    bool ret = false;

    if (testFlag(flag))
    {
        if (testSet) { jr_n(val); ret = true; }
    }
    else
    {
        if (!testSet) { jr_n(val); ret = true; }
    }

    return ret;
}

/**
 * @return true if we jumped
 */
bool jr_n_cond_signed(int8_t val, Flag flag, bool testSet)
{
    bool ret = false;

    if (testFlag(flag))
    {
        if (testSet) { jr_n_signed(val); ret = true; }
    }
    else
    {
        if (!testSet) { jr_n_signed(val); ret = true; }
    }

    return ret;
}

// calls

void call_nn(uint16_t val)
{
    // push return address onto stack
    // next 2 bytes are the address to call
    pCpu->reg16.sp -= 2;
    write16(pCpu->reg16.pc + 3, pCpu->reg16.sp);
    setRegister16(PC, val - 1); // -1 since we always increment at the end of the main loop
}

bool call_nn_cond(uint16_t val, Flag flag, bool testSet)
{
    bool ret = false;

    if (testFlag(flag))
    {
        if (testSet) 
        {
            call_nn(val);
            ret = true;
        }
    }
    else
    {
        if (!testSet) 
        {
            call_nn(val);
            ret = true;
        }
    }

    return ret;
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
            pCpu->reg16.sp -= 2;
            write16(pCpu->reg16.pc, pCpu->reg16.sp);
            setRegister16(PC, val);
            break;
        default:
            while(true) {} // hang for now
    }
}

void ret(void)
{
    setRegister16(PC, fetch16(pCpu->reg16.sp) - 1); // -1 as we always increment at the end of the main loop
    pCpu->reg16.sp += 2;
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

void reti(void)
{
    ret();
    changeIME(true);
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

    uint8_t ret = (val << 1) | testFlag(FLAG_C);
    if (val & 0x80) { setFlag(FLAG_C); } // get bit 7 of previous value

    if (ret == 0) { setFlag(FLAG_Z); }

    return ret;
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

    if (val & 0x80) { setFlag(FLAG_C); } // get bit 7 of previous value
    uint8_t ret = (val << 1) | testFlag(FLAG_C);

    if (ret == 0) { setFlag(FLAG_Z); }

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

    if (val & 0x80) { setFlag(FLAG_C); } // get bit 7 of previous value
    uint8_t ret = val << 1;

    if (ret == 0) { setFlag(FLAG_Z); }

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

    uint8_t ret = (val >> 1) | (testFlag(FLAG_C) << 7);
    if (val & 0x01) { setFlag(FLAG_C); } // get bit 0 of previous value

    if (ret == 0) { setFlag(FLAG_Z); }

    return ret;
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

    if (val & 0x01) { setFlag(FLAG_C); } // get bit 1 of previous value
    uint8_t ret = (val >> 1) | (testFlag(FLAG_C) << 7);

    if (ret == 0) { setFlag(FLAG_Z); }

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
    val &= ~0x80; // unset bit 7
    if (val & 0x01) { setFlag(FLAG_C); } // get bit 0 of previous value

    uint8_t ret = (val >> 1) | (bit7 << 7);

    if (ret == 0) { setFlag(FLAG_Z); }

    return ret;
}

static uint8_t _srl(uint8_t val)
{
    resetFlag(FLAG_N);
    resetFlag(FLAG_H);

    if (val & 0x01) { setFlag(FLAG_C); } // get bit 7 of previous value
    uint8_t ret = val >> 1;

    if (ret == 0) { setFlag(FLAG_Z); }

    return ret;
}