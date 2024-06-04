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
#include <stdio.h>

#include "instr.h"
#include "mem.h"

static cpu_t cpu;
static bus_t *pBus;

static bool imeFlag = false;

static int getRegisterIndexByOpcodeNibble(uint8_t);

/**
 * @brief reset cpu to initial state
 */
void resetCpu(void)
{
    memset(&cpu, 0, sizeof(cpu));
    imeFlag = false;
    instrSetCpuPtr(&cpu);
    pBus = pGetBusPtr();
}

/**
 * useful for unit testing instructions
*/
void overrideCpu(cpu_t *pCpu)
{
    memcpy(&cpu, pCpu, sizeof(cpu_t));
}

/**
 * @brief set CPU registers to post-bootrom values
 * 
 */
void cpuSkipBootrom(void)
{
    cpu.reg8.a = 0x01;
    cpu.reg8.f = 0xB0;
    cpu.reg8.b = 0x00;
    cpu.reg8.c = 0x13;
    cpu.reg8.d = 0x00;
    cpu.reg8.e = 0xD8;
    cpu.reg8.h = 0x01;
    cpu.reg8.l = 0x4D;
    cpu.reg16.sp = 0xFFFE;
    cpu.reg16.pc = 0x100;
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
 * @note yeah, I know, I perform an addition with a bool, but the standard specifies it as 1, so sshhh
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

void setIME()
{
    imeFlag = true;
}

void resetIME()
{
    imeFlag = false;
}

bool checkIME()
{
    return imeFlag;
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

static int decodeCbPrefix()
{
    uint8_t instr = fetch8(++cpu.reg16.pc); // CB prefix takes next instruction
    uint8_t hi = instr >> 4;
    uint8_t lo = instr & 15;

    int addCycles = 0;

    switch (hi)
    {
        // RLC/RRC
        case 0x00:
        {
            if (lo < 0x06)
            {
                // RLC basic, B to L
                rlc_reg(getRegisterIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                // RLC (HL)
                rlc_addr(cpu.reg16.hl);
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
                rrc_reg(getRegisterIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                // RRC (HL)
                rrc_addr(cpu.reg16.hl);
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
                rl_reg(getRegisterIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                // RL (HL)
                rl_addr(cpu.reg16.hl);
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
                rr_reg(getRegisterIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                // RR (HL)
                rr_addr(cpu.reg16.hl);
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
                sla_reg(getRegisterIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                // SLA (HL)
                sla_addr(cpu.reg16.hl);
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
                sra_reg(getRegisterIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                // SRA (HL)
                sra_addr(cpu.reg16.hl);
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
                swap8_reg(getRegisterIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                // SWAP (HL)
                swap8_addr(cpu.reg16.hl);
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
                srl_reg(getRegisterIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                // SRL (HL)
                srl_addr(cpu.reg16.hl);
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
                bit_n_reg(0, getRegisterIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                bit_n_reg(0, cpu.reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x07)
            {
                bit_n_reg(0, A);
            }
            else if (lo < 0x0E)
            {
                bit_n_reg(1, getRegisterIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                bit_n_reg(1, cpu.reg16.hl);
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
                bit_n_reg(2, getRegisterIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                bit_n_reg(2, cpu.reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x07)
            {
                bit_n_reg(2, A);
            }
            else if (lo < 0x0E)
            {
                bit_n_reg(3, getRegisterIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                bit_n_reg(3, cpu.reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x0F)
            {
                bit_n_reg(3, A);
            }
            break;

            break;
        }
        // BIT 4/5, reg
        case 0x06:
        {
            if (lo < 0x06)
            {
                bit_n_reg(4, getRegisterIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                bit_n_reg(4, cpu.reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x07)
            {
                bit_n_reg(4, A);
            }
            else if (lo < 0x0E)
            {
                bit_n_reg(5, getRegisterIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                bit_n_reg(5, cpu.reg16.hl);
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
                bit_n_reg(6, getRegisterIndexByOpcodeNibble(lo));
            }
            else if (lo == 0x06)
            {
                bit_n_reg(6, cpu.reg16.hl);
                addCycles = 1;
            }
            else if (lo == 0x07)
            {
                bit_n_reg(6, A);
            }
            else if (lo < 0x0E)
            {
                bit_n_reg(7, getRegisterIndexByOpcodeNibble(lo - 0x08));
            }
            else if (lo == 0x0E)
            {
                bit_n_reg(7, cpu.reg16.hl);
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
                reset_n_reg(getRegisterIndexByOpcodeNibble(lo), 0);
            }
            else if (lo == 0x06)
            {
                reset_n_addr(cpu.reg16.hl, 0);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                reset_n_reg(A, 0);
            }
            else if (lo < 0x0E)
            {
                reset_n_reg(getRegisterIndexByOpcodeNibble(lo - 0x08), 1);
            }
            else if (lo == 0x0E)
            {
                reset_n_addr(cpu.reg16.hl, 1);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                reset_n_reg(A, 1);
            }
            break;
        }
        // RES 2/3
        case 0x09:
        {
            if (lo < 0x06)
            {
                reset_n_reg(getRegisterIndexByOpcodeNibble(lo), 2);
            }
            else if (lo == 0x06)
            {
                reset_n_addr(cpu.reg16.hl, 2);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                reset_n_reg(A, 2);
            }
            else if (lo < 0x0E)
            {
                reset_n_reg(getRegisterIndexByOpcodeNibble(lo - 0x08), 3);
            }
            else if (lo == 0x0E)
            {
                reset_n_addr(cpu.reg16.hl, 3);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                reset_n_reg(A, 3);
            }
            break;
        }
        // RES 4/5
        case 0x0A:
        {
            if (lo < 0x06)
            {
                reset_n_reg(getRegisterIndexByOpcodeNibble(lo), 4);
            }
            else if (lo == 0x06)
            {
                reset_n_addr(cpu.reg16.hl, 4);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                reset_n_reg(A, 4);
            }
            else if (lo < 0x0E)
            {
                reset_n_reg(getRegisterIndexByOpcodeNibble(lo - 0x08), 5);
            }
            else if (lo == 0x0E)
            {
                reset_n_addr(cpu.reg16.hl, 5);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                reset_n_reg(A, 5);
            }
            break;
        }
        // RES 6/7
        case 0x0B:
        {
            if (lo < 0x06)
            {
                reset_n_reg(getRegisterIndexByOpcodeNibble(lo), 6);
            }
            else if (lo == 0x06)
            {
                reset_n_addr(cpu.reg16.hl, 6);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                reset_n_reg(A, 6);
            }
            else if (lo < 0x0E)
            {
                reset_n_reg(getRegisterIndexByOpcodeNibble(lo - 0x08), 7);
            }
            else if (lo == 0x0E)
            {
                reset_n_addr(cpu.reg16.hl, 7);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                reset_n_reg(A, 7);
            }
            break;
        }
        // SET 0/1
        case 0x0C:
        {
            if (lo < 0x06)
            {
                set_n_reg(getRegisterIndexByOpcodeNibble(lo), 0);
            }
            else if (lo == 0x06)
            {
                set_n_addr(cpu.reg16.hl, 0);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                set_n_reg(A, 0);
            }
            else if (lo < 0x0E)
            {
                set_n_reg(getRegisterIndexByOpcodeNibble(lo - 0x08), 1);
            }
            else if (lo == 0x0E)
            {
                set_n_addr(cpu.reg16.hl, 1);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                set_n_reg(A, 1);
            }
            break;
        }
        // SET 2/3
        case 0x0D:
        {
            if (lo < 0x06)
            {
                set_n_reg(getRegisterIndexByOpcodeNibble(lo), 2);
            }
            else if (lo == 0x06)
            {
                set_n_addr(cpu.reg16.hl, 2);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                set_n_reg(A, 2);
            }
            else if (lo < 0x0E)
            {
                set_n_reg(getRegisterIndexByOpcodeNibble(lo - 0x08), 3);
            }
            else if (lo == 0x0E)
            {
                set_n_addr(cpu.reg16.hl, 3);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                set_n_reg(A, 3);
            }
            break;
        }
        // SET 4/5
        case 0x0e:
        {
            if (lo < 0x06)
            {
                set_n_reg(getRegisterIndexByOpcodeNibble(lo), 4);
            }
            else if (lo == 0x06)
            {
                set_n_addr(cpu.reg16.hl, 4);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                set_n_reg(A, 4);
            }
            else if (lo < 0x0E)
            {
                set_n_reg(getRegisterIndexByOpcodeNibble(lo - 0x08), 5);
            }
            else if (lo == 0x0E)
            {
                set_n_addr(cpu.reg16.hl, 5);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                set_n_reg(A, 5);
            }
            break;
        }
        // SET 6/7
        case 0x0F:
        {
            if (lo < 0x06)
            {
                set_n_reg(getRegisterIndexByOpcodeNibble(lo), 6);
            }
            else if (lo == 0x06)
            {
                set_n_addr(cpu.reg16.hl, 6);
                addCycles = 2;
            }
            else if (lo == 0x07)
            {
                set_n_reg(A, 6);
            }
            else if (lo < 0x0E)
            {
                set_n_reg(getRegisterIndexByOpcodeNibble(lo - 0x08), 7);
            }
            else if (lo == 0x0E)
            {
                set_n_addr(cpu.reg16.hl, 7);
                addCycles = 2;
            }
            else if (lo == 0x0F)
            {
                set_n_reg(A, 7);
            }
            break;
        }


        default:
        {
            printf("unknown CB instr CB %02X\n", instr);
            while(true);
        }
    }

    return addCycles;
}

int executeInstruction(uint8_t instr)
{
    bool is16 = false;
    int cycleCount = 0;

    uint8_t hi = instr >> 4;
    uint8_t lo = instr & 15;

    // main instruction decode loop
    switch (instr)
    {
        case 0x00: // NOP
        {
            cycleCount = 1;
            break;
        }
        case 0x01: // LD BC,d16
        case 0x11: // LD DE,d16
        case 0x21: // LD HL,d16
        case 0x31: // LD SP,d16
        {
            Register16 reg = BC + hi; // use high nibble
            ld_reg16_imm(reg, fetch16(cpu.reg16.pc + 1));
            cpu.reg16.pc += 2;
            cycleCount = 3;
            break;
        }
        case 0x02: // LD (BC),A
        case 0x12: // LD (DE),A
        {
            Register16 reg = BC + hi; // use high nibble
            write8(cpu.reg8.a, cpu.reg16_arr[reg]);
            cycleCount = 2;
            break;
        }
        case 0x22: // LD (HL+),A
        {
            write8(cpu.reg8.a, cpu.reg16.hl);
            cpu.reg16.hl++;
            cycleCount = 2;
            break;
        }
        case 0x32: // LD (HL-),A
        {
            write8(cpu.reg8.a, cpu.reg16.hl);
            cpu.reg16.hl--;
            cycleCount = 2;
            break;
        }
        case 0x06: // LD B,d8
        case 0x16: // LD D,d8
        case 0x26: // LD H,d8
        {
            Register8 reg = reg = ((hi == 1) ? D : ((hi == 2) ? H : B));
            ld_reg8_imm(reg, fetch8(++cpu.reg16.pc));
            cycleCount = 2;
            break;
        }
        case 0x36: // LD (HL),d8
        {
            write8(fetch8(++cpu.reg16.pc), cpu.reg16.hl);
            cycleCount = 3;
            break;
        }
        case 0x0A: // LD A,(BC)
        {
            ld_reg8_addr(A, cpu.reg16.bc);
            cycleCount = 2;
            break;
        }
        case 0x1A: // LD A,(DE)
        {
            ld_reg8_addr(A, cpu.reg16.de);
            cycleCount = 2;
            break;
        }
        case 0x2A: // LD A,(HL+)
        case 0x3A: // LD A,(HL-)
        {
            ld_reg8_addr(A, HL);
            (hi == 0x02) ? cpu.reg16.hl-- : cpu.reg16.hl++;
            cycleCount = 2;
            break;
        }
        case 0x0E: // LD C,d8
        case 0x1E: // LD E,d8
        case 0x2E: // LD L,d8
        {
            Register8 reg = ((hi == 1) ? E : ((hi == 2) ? L : C));
            ld_reg8_imm(reg, fetch8(++cpu.reg16.pc));
            cycleCount = 2;
            break;
        }
        case 0x3E: // LD A,d8
        {
            ld_reg8_imm(A, fetch8(++cpu.reg16.pc));
            cycleCount = 2;
            break;
        }
        case 0xE2: // LD (0xFF00 + C),A
        {
            write8(cpu.reg8.a, 0xFF00 + cpu.reg8.c);
            cycleCount = 2;
            break;
        }
        case 0xF2: // LD A, (0xFF00 + C)
        {
            setRegister8(A, fetch8(0xFF00 + cpu.reg8.c));
            cycleCount = 2;
            break;
        }
        case 0xE0: // LDH (a8), A (load from A to to 0xFF00+8-bit unsigned value)
        {
            write8(cpu.reg8.a, 0xFF00 + fetch8(++cpu.reg16.pc));
            cycleCount = 3;
            break;
        }
        case 0xF0: // LDH A, (a8) (load from 0xFF00+8-bit unsigned value to A)
        {
            ld_reg8_addr(A, 0xFF00 + fetch8(++cpu.reg16.pc));
            cycleCount = 3;
            break;
        }
        case 0xEA: // LD (a16), A (load from A to addr)
        {
            write8(cpu.reg8.a, fetch16(++cpu.reg16.pc));
            cycleCount = 4;
            is16 = true;
            break;
        }
        case 0xFA: // LD A, (a16) (load from addr to A)
        {
            ld_reg8_addr(A, fetch16(++cpu.reg16.pc));
            cycleCount = 4;
            is16 = true;
            break;
        }
        case 0x0C: // INC C
        case 0x1C: // INC E
        case 0x2C: // INC L
        case 0x0D: // DEC C
        case 0x1D: // DEC E
        case 0x2D: // DEC L
        {
            Register8 reg = ((hi == 1) ? E : ((hi == 2) ? L : C));
            if (lo == 0x0C) { inc8_reg(reg); }
            else            { dec8_reg(reg); }
            cycleCount = 1;
            break;
        }
        case 0x3C: // INC A
        case 0x3D: // DEC A
        {
            if (lo == 0x0C) { inc8_reg(A); }
            else            { dec8_reg(A); }
            cycleCount = 1;
            break;
        }
        case 0x04: // INC B
        case 0x14: // INC D
        case 0x24: // INC H
        case 0x05: // DEC B
        case 0x15: // DEC D
        case 0x25: // DEC H
        {
            Register8 reg = ((hi == 1) ? D : ((hi == 2) ? H : B));
            if (lo == 0x04) { inc8_reg(reg); }
            else            { dec8_reg(reg); }
            cycleCount = 1;
            break;
        }
        case 0x34: // INC (HL)
        case 0x35: // DEC (HL)
        {
            if (lo == 0x04) { inc8_mem(cpu.reg16.hl); }
            else            { dec8_mem(cpu.reg16.hl); }
            cycleCount = 3;
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
            if (lo == 0x03) { inc16_reg(reg); }
            else            { dec16_reg(reg); }
            cycleCount = 2;
            break;
        }
        case 0x07: // RLCA (rotate left circular)
        {
            rlca();
            cycleCount = 1;
            break;
        }
        case 0x17: // RLA (rotate left through carry)
        {
            rla();
            cycleCount = 1;
            break;
        }
        case 0x0F: // RRCA (rotate right circular)
        {
            rrca();
            cycleCount = 1;
            break;
        }
        case 0x1F: // RRA (rotate right through carry)
        {
            rra();
            cycleCount = 1;
            break;
        }
        case 0xCB: // CB-prefixed
        {
            cycleCount = 2; // all CB's use at least 2 cycles
            cycleCount += decodeCbPrefix();
            break;
        }

        // Weirdly positioned instructions in map

        // JUMP instructions

        case 0x18: // JR s8
        {
            jr_n_signed(fetch8(++cpu.reg16.pc));
            //cpu.reg16.pc--; // we jumped to an absolute offset; no increments
            cycleCount = 3;
            break;
        }
        case 0x20: // JR NZ,s8
        case 0x28: // JR Z,s8
        {
            cycleCount = 2;
            if (jr_n_cond_signed(fetch8(++cpu.reg16.pc), FLAG_Z, (lo == 0x8 ? true : false))) { cycleCount++; } //cpu.reg16.pc--; }
            break;
        }
        case 0x30: // JR NC,s8
        case 0x38: // JR C,s8
        {
            cycleCount = 2;
            if (jr_n_cond_signed(fetch8(++cpu.reg16.pc), FLAG_C, (lo == 0x8 ? true : false))) { cycleCount++; } //cpu.reg16.pc--; }
            break;
        }
        case 0xC3: // JP a16
        {
            jmp_nn(fetch16(++cpu.reg16.pc));
            cycleCount = 4;
            break;
        }
        case 0xC2: // JP NZ,a16
        case 0xCA: // JP Z,a16
        {
            cycleCount = 3;
            if (jmp_nn_cond(fetch16(++cpu.reg16.pc), FLAG_Z, (lo == 0xA ? true : false))) { cycleCount++; } //cpu.reg16.pc--; }
            else { is16 = true; } // PC is still at the high byte of uint16
            break;
        }
        case 0xD2: // JP NC,a16
        case 0xDA: // JP C,a16
        {
            cycleCount = 3;
            if (jmp_nn_cond(fetch16(++cpu.reg16.pc), FLAG_C, (lo == 0xA ? true : false))) { cycleCount++; } //cpu.reg16.pc--; }
            else { is16 = true; } // PC is still at the high byte of uint16
            break;
        }

        case 0xE9: // JP HL
        {
            setRegister16(PC, cpu.reg16.hl);
            cycleCount = 1;
            cpu.reg16.pc--;
            break;
        }

        // CALL instructions
        
        case 0xC4: // CALL NZ,a16
        {
            cycleCount = 3;
            if (call_nn_cond(fetch16(++cpu.reg16.pc), FLAG_Z, false)) { cycleCount += 3; }
            else { is16 = true; } // PC is still at the high byte of uint16
            break;
        }
        case 0xD4: // CALL NC,a16
        {
            cycleCount = 3;
            if (call_nn_cond(fetch16(++cpu.reg16.pc), FLAG_C, false)) { cycleCount += 3; }
            else { is16 = true; } // PC is still at the high byte of uint16
            break;
        }
        case 0xCC: // CALL Z,a16
        {
            cycleCount = 3;
            if (call_nn_cond(fetch16(++cpu.reg16.pc), FLAG_Z, true)) { cycleCount += 3; }
            else { is16 = true; } // PC is still at the high byte of uint16
            break;
        }
        case 0xDC: // CALL C,a16
        {
            cycleCount = 3;
            if (call_nn_cond(fetch16(++cpu.reg16.pc), FLAG_C, true)) { cycleCount += 3; }
            else { is16 = true; } // PC is still at the high byte of uint16
            break;
        }
        case 0xCD: // CALL a16
        {
            cycleCount = 6;
            call_nn(fetch16(++cpu.reg16.pc));
            break;
        }

        // POP instructions

        case 0xC1: // POP BC
        {
            pop_reg16(BC);
            cycleCount = 3;
            break;
        }
        case 0xD1: // POP DE
        {
            pop_reg16(DE);
            cycleCount = 3;
            break;
        }
        case 0xE1: // POP HL
        {
            pop_reg16(HL);
            cycleCount = 3;
            break;
        }
        case 0xF1: // POP AF
        {
            pop_reg16(AF);
            cycleCount = 3;
            break;
        }

        // PUSH instructions

        case 0xC5: // PUSH BC
        {
            push_reg16(BC);
            cycleCount = 4;
            break;
        }
        case 0xD5: // PUSH DE
        {
            push_reg16(DE);
            cycleCount = 4;
            break;
        }
        case 0xE5: // PUSH HL
        {
            push_reg16(HL);
            cycleCount = 4;
            break;
        }
        case 0xF5: // PUSH AF
        {
            push_reg16(AF);
            cycleCount = 4;
            break;
        }

        // RET instructions
        case 0xC9: // RET
        {
            ret();
            cycleCount = 4;
            break;
        }
        case 0xD9: // RETI
        {
            ret();
            imeFlag = true;
            cycleCount = 4;
            break;
        }

        case 0xC0: // RET NZ
        {
            if (!testFlag(FLAG_Z)) { ret(); }
            cycleCount = 4;
            break;
        }

        case 0xC8: // RET Z
        {
            if (testFlag(FLAG_Z)) { ret(); }
            cycleCount = 4;
            break;
        }

        case 0xD0: // RET NC
        {
            if (!testFlag(FLAG_C)) { ret(); }
            cycleCount = 4;
            break;
        }

        case 0xD8: // RET C
        {
            if (testFlag(FLAG_C)) { ret(); }
            cycleCount = 4;
            break;
        }

        case 0x76: // HALT
        {
            cycleCount = 1;
            //while(true);
            break;
        }

        case 0x10: // STOP
        {
            cycleCount = 2;
            memset(&pBus->map.interruptEnable, 0, 1);
            memset(&pBus->map.ioregs.intFlags, 0, 1);
            break;
        }

        case 0x27: // DAA
        {
            daa();
            cycleCount = 1;
            break;
        }

        case 0x2F: // CPL
        {
            cpl();
            cycleCount = 1;
            break;
        }

        case 0x3F: // CCF
        {
            ccf();
            cycleCount = 1;
            break;
        }

        case 0x37: // SCF
        {
            scf();
            cycleCount = 1;
            break;
        }

        case 0xF3: // DI
        {
            imeFlag = false;
            cycleCount = 1;
            break;
        }

        case 0xFB: // EI
        {
            imeFlag = true;
            cycleCount = 1;
            break;
        }

        case 0x09: // ADD HL, BC
        case 0x19: // ADD HL, DE
        case 0x29: // ADD HL, HL
        case 0x39: // ADD HL, SP
        {
            add16_hl_n(cpu.reg16_arr[hi + 1]);
            cycleCount = 2;
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
            uint8_t num = (hi - 0xC) * 2;
            if (lo > 0x07)
            {
                num += 1;
            }
            rst_n(num);
            cycleCount = 4;
            break;
        }

        case 0xC6: // ADD a, d8
        {
            add8_a_n(fetch8(++cpu.reg16.pc));
            cycleCount = 2;
            break;
        }

        case 0xD6: // SUB d8
        {
            sub8_n_a(fetch8(++cpu.reg16.pc));
            cycleCount = 2;
            break;
        }

        case 0xE6: // AND d8
        {
            and8_a_n(fetch8(++cpu.reg16.pc));
            cycleCount = 2;
            break;
        }

        case 0xF6: // ADD a, d8
        {
            or8_a_n(fetch8(++cpu.reg16.pc));
            cycleCount = 2;
            break;
        }

        case 0xCE: // ADC a, d8
        {
            adc8_a_n(fetch8(++cpu.reg16.pc));
            cycleCount = 2;
            break;
        }

        case 0xDE: // SBC a, d8
        {
            sbc8_a_n(fetch8(++cpu.reg16.pc));
            cycleCount = 2;
            break;
        }

        case 0xEE: // XOR d8
        {
            xor8_a_n(fetch8(++cpu.reg16.pc));
            cycleCount = 2;
            break;
        }

        case 0xFE: // CP d8
        {
            cp8_a_n(fetch8(++cpu.reg16.pc));
            cycleCount = 2;
            break;
        }

        case 0xE8: // ADD SP, s8
        {
            setRegister16(SP, cpu.reg16.sp + ((int8_t)fetch8(++cpu.reg16.pc)));
            cycleCount = 4;
            break;
        }

        case 0x08: // LD (a16), SP
        {
            write16(fetch16(++cpu.reg16.pc), cpu.reg16.sp);
            cycleCount = 5;
            cpu.reg16.pc += 2;
            break;
        }

        case 0xF8: // LD HL, SP+s8
        {
            setRegister16(HL, cpu.reg16.sp + ((int8_t)fetch8(++cpu.reg16.pc)));
            cycleCount = 3;
            break;
        }

        case 0xF9: // LD SP, HL
        {
            setRegister16(SP, cpu.reg16.hl);
            cycleCount = 2;
            break;
        }

        default:
        {
            Register8 regL = A;
            Register8 regR = A;
            bool hl = false;
            bool hlLeft = false;

            cycleCount = 1; // by default: only (HL) has 2 cycles

            switch(hi)
            {
                // LD functions
                case 0x04: // LD B,x - LD C,x
                {
                    regL  = (lo <= 0x07) ? B : C;
                    
                    if ((lo == 0x06) || (lo == 0x0E)) // 0x6 or 0xE == (HL)
                    {
                        hl = true;
                    }
                    else if ((lo & 0x07) == 0) // lower 3 bits not set? not A
                    {
                        regR = (lo < 0x06) ? getRegisterIndexByOpcodeNibble(lo) : getRegisterIndexByOpcodeNibble(lo - 0x07);
                    }

                    if (!hl) { ld_reg8_reg8(regL, regR); }
                    else     { ld_reg8_addr(regL, cpu.reg16.hl); }

                    break;
                }
                case 0x05: // LD D,x - LD E,x
                {
                    regL  = (lo <= 0x07) ? D : E;
                    
                    if (lo == 0x06 || lo == 0x0E)
                    {
                        hl = true;
                    }
                    else if ((lo & 0x07) == 0) // lower 3 bits not set? not A
                    {
                        regR = (lo < 0x06) ? getRegisterIndexByOpcodeNibble(lo) : getRegisterIndexByOpcodeNibble(lo - 0x07);
                    }

                    if (!hl) { ld_reg8_reg8(regL, regR); }
                    else     { ld_reg8_addr(regL, cpu.reg16.hl); }

                    break;
                }
                case 0x06: // LD H,x - LD L,x
                {
                    regL  = (lo <= 0x07) ? H : L;
                    
                    if (lo == 0x06 || lo == 0x0E)
                    {
                        hl = true;
                    }
                    else if ((lo & 0x07) == 0) // lower 3 bits not set? not A
                    {
                        regR = (lo < 0x06) ? getRegisterIndexByOpcodeNibble(lo) : getRegisterIndexByOpcodeNibble(lo - 0x07);
                    }

                    if (!hl) { ld_reg8_reg8(regL, regR); }
                    else     { ld_reg8_addr(regL, cpu.reg16.hl); }

                    break;
                }
                case 0x07: // LD (HL),x - LD A, x
                {
                    if (lo <= 0x07) // (HL). no need to worry about HALT, already handled above
                    {
                        hlLeft = true;
                    }
                    else if (lo == 0xE) // LD A, (HL)
                    {
                        hl = true;
                    }
                    else if (lo != 0xF) // 0xF == LD A, A
                    {
                        regR = (lo < 0x06) ? getRegisterIndexByOpcodeNibble(lo) : getRegisterIndexByOpcodeNibble(lo - 0x08);
                    }

                    if (hlLeft)
                    {
                        ld_addr_reg8(cpu.reg16.hl, regR);
                    }
                    else if (hl)
                    {
                        ld_reg8_addr(regL, cpu.reg16.hl);
                    }
                    else
                    {
                        ld_reg8_reg8(regL, regR);
                    }

                    break;
                }

                // ADD/ADC A
                case 0x08:
                {
                    if (lo < 0x06)
                    {
                        // ADD, B to L
                        add8_a_n(cpu.reg8_arr[getRegisterIndexByOpcodeNibble(lo)]);
                    }
                    else if (lo == 0x06)
                    {
                        // ADD (HL)
                        add8_a_n(fetch8(cpu.reg16.hl));
                        hl = true;
                    }
                    else if (lo == 0x07)
                    {
                        // ADD A
                        add8_a_n(cpu.reg8_arr[A]);
                    }
                    else if (lo < 0x0E)
                    {
                        // ADC, B to L
                        adc8_a_n(cpu.reg8_arr[getRegisterIndexByOpcodeNibble(lo - 0x08)]);
                    }
                    else if (lo == 0x0E)
                    {
                        // ADC (HL)
                        adc8_a_n(fetch8(cpu.reg16.hl));
                        hl = true;
                    }
                    else if (lo == 0x0F)
                    {
                        // ADC A
                        adc8_a_n(cpu.reg8_arr[A]);
                    }
                    break;
                }

                // SUB/SBC <reg>
                case 0x09:
                {
                    if (lo < 0x06)
                    {
                        // SUB, B to L
                        sub8_n_a(cpu.reg8_arr[getRegisterIndexByOpcodeNibble(lo)]);
                    }
                    else if (lo == 0x06)
                    {
                        // SUB (HL)
                        sub8_n_a(fetch8(cpu.reg16.hl));
                        hl = true;
                    }
                    else if (lo == 0x07)
                    {
                        // SUB A
                        sub8_n_a(cpu.reg8_arr[A]);
                    }
                    else if (lo < 0x0E)
                    {
                        // SBC, B to L
                        sbc8_a_n(cpu.reg8_arr[getRegisterIndexByOpcodeNibble(lo - 0x08)]);
                    }
                    else if (lo == 0x0E)
                    {
                        // SBC (HL)
                        sbc8_a_n(fetch8(cpu.reg16.hl));
                        hl = true;
                    }
                    else if (lo == 0x0F)
                    {
                        // SBC A
                        sbc8_a_n(cpu.reg8_arr[A]);
                    }
                    break;
                }

                // AND/XOR <reg>
                case 0x0A:
                {
                    if (lo < 0x06)
                    {
                        // AND, B to L
                        and8_a_n(cpu.reg8_arr[getRegisterIndexByOpcodeNibble(lo)]);
                        break;
                    }
                    else if (lo == 0x06)
                    {
                        // AND (HL)
                        and8_a_n(fetch8(cpu.reg16.hl));
                        hl = true;
                    }
                    else if (lo == 0x07)
                    {
                        // AND A
                        and8_a_n(cpu.reg8_arr[A]);
                    }
                    else if (lo < 0x0E)
                    {
                        // XOR, B to L
                        xor8_a_n(cpu.reg8_arr[getRegisterIndexByOpcodeNibble(lo - 0x08)]);
                    }
                    else if (lo == 0x0E)
                    {
                        // XOR (HL)
                        xor8_a_n(fetch8(cpu.reg16.hl));
                        hl = true;
                    }
                    else if (lo == 0x0F)
                    {
                        // XOR A
                        xor8_a_n(cpu.reg8_arr[A]);
                    }
                    break;
                }

                // OR/CP <reg>
                case 0x0B:
                {
                    if (lo < 0x06)
                    {
                        // OR, B to L
                        or8_a_n(cpu.reg8_arr[getRegisterIndexByOpcodeNibble(lo)]);
                        break;
                    }
                    else if (lo == 0x06)
                    {
                        // OR (HL)
                        or8_a_n(fetch8(cpu.reg16.hl));
                        hl = true;
                    }
                    else if (lo == 0x07)
                    {
                        // OR A
                        or8_a_n(cpu.reg8_arr[A]);
                    }
                    else if (lo < 0x0E)
                    {
                        // CP, B to L
                        cp8_a_n(cpu.reg8_arr[getRegisterIndexByOpcodeNibble(lo - 0x08)]);
                    }
                    else if (lo == 0x0E)
                    {
                        // CP (HL)
                        cp8_a_n(fetch8(cpu.reg16.hl));
                        hl = true;
                    }
                    else if (lo == 0x0F)
                    {
                        // CP A
                        cp8_a_n(cpu.reg8_arr[A]);
                    }
                    break;
                }
                default:
                {
                    printf("unknown instruction 0x%2x at 0x%04x\n", instr, cpu.reg16.pc);
                    while (true);
                }
            }

            if (hl || hlLeft) { cycleCount += 1; }
        }

    }

    stepCpu(is16);

    return cycleCount;
}

int handleInterrupts(void)
{
    bool fired = false;
    if (pBus->map.interruptEnable.vblank && pBus->map.ioregs.intFlags.vblank)
    {
        fired = true;
        call_nn(0x41);
    }
    else if (pBus->map.interruptEnable.lcd && pBus->map.ioregs.intFlags.lcd)
    {
        if ((pBus->map.ioregs.lcd.stat.LYCIntSel && pBus->map.ioregs.lcd.stat.lycEqLy) ||
            (pBus->map.ioregs.lcd.stat.mode0IntSel && pBus->map.ioregs.lcd.stat.ppuMode == 0) ||
            (pBus->map.ioregs.lcd.stat.mode1IntSel && pBus->map.ioregs.lcd.stat.ppuMode == 1) ||
            (pBus->map.ioregs.lcd.stat.mode2IntSel && pBus->map.ioregs.lcd.stat.ppuMode == 2))
        {
            // STAT int
            fired = true;
            call_nn(0x49);
        }
    }
    else if (pBus->map.interruptEnable.timer && pBus->map.ioregs.intFlags.timer)
    {
        fired = true;
        call_nn(0x51);
    }
    else if (pBus->map.interruptEnable.serial && pBus->map.ioregs.intFlags.serial)
    {
        fired = true;
        call_nn(0x59);
    }
    else if (pBus->map.interruptEnable.joypad && pBus->map.ioregs.intFlags.joypad)
    {
        fired = true;
        call_nn(0x61);
    }

    if (fired)
    {
        imeFlag = false;
        return 5;
    }
    else
    {
        return 0;
    }
}

void handleTimers(int mCycles)
{
    static int localCycles = 0;
    uint16_t localTim = pBus->map.ioregs.timers.TIMA;
    while (mCycles--)
    {
        if (pBus->map.ioregs.timers.TAC.enable)
        {
            localCycles++;
            switch (pBus->map.ioregs.timers.TAC.clockSelect)
            {
                case 0:
                {
                    if ((mCycles % 256) == 0)
                    {
                        localTim++;
                    }
                    break;
                }
                case 1:
                {
                    if ((mCycles % 4) == 0)
                    {
                        localTim++;
                    }
                    break;
                }
                case 2:
                {
                    if ((mCycles % 16) == 0)
                    {
                        localTim++;
                    }
                    break;
                }
                case 3:
                {
                    if ((mCycles % 64) == 0)
                    {
                        localTim++;
                    }
                    break;
                }
            }

            if (localTim > UINT8_MAX)
            {
                pBus->map.ioregs.timers.TIMA = pBus->map.ioregs.timers.TMA;
                pBus->map.ioregs.intFlags.timer = 1;
            }
        }
        if (localCycles == 11)
        {
            pBus->map.ioregs.divRegister++;
            localCycles = 0;
        }
    }
}

static int getRegisterIndexByOpcodeNibble(uint8_t lo)
{
    switch (lo)
    {
        case 0x0: return 3; // B
        case 0x1: return 2; // C
        case 0x2: return 5; // D
        case 0x3: return 4; // E
        case 0x4: return 7; // H
        case 0x5: return 6; // L
        // Add other cases if needed
        default: return -1; // Invalid register
    }
}