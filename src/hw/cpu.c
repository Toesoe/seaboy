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

#include "instr.h"
#include "mem.h"

static cpu_t cpu;

static bool enableInterrupts;

static uint64_t cycleCount = 0;

/**
 * @brief reset cpu to initial state
 */
void resetCpu(void)
{
    memset(&cpu, 0, sizeof(cpu));
    enableInterrupts = true;
    instrSetCpuPtr(&cpu);
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
 * @note yeah, I know, I perform an addition with a bool, but the standard specifies it as 1
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

/**
 * @brief change the interrupt master enable flag
 */
void changeIME(bool enable)
{
    enableInterrupts = enable;
}

static void decodeCbPrefix()
{
    uint8_t instr = fetch8(++cpu.reg16.pc); // CB prefix takes next instruction
    uint8_t hi = instr >> 4;
    uint8_t lo = instr & 15;

    Register8 reg = A;

    switch (hi)
    {
        case 0x04:
        {
            if (lo < 0x06)
            {
                // bit 0
                reg += lo + 1;
                bit_n_reg(0, reg);
                cycleCount += 8;
            }
            else if ((lo > 0x07) && (lo < 0x0F))
            {
                // bit 1
                reg += lo - 0x06;
                bit_n_reg(1, reg);
                cycleCount += 8;
            }
            else if (lo == 0x06)
            {
                bit_n_addr(0, cpu.reg16.hl);
                cycleCount += 16;
            }
            else
            {
                bit_n_addr(1, cpu.reg16.hl);
                cycleCount += 16;
            }

            break;
        }
        case 0x05:
        {
            if (lo < 0x06)
            {
                // bit 2
                reg += lo + 1;
                bit_n_reg(2, reg);
                cycleCount += 8;
            }
            else if ((lo > 0x07) && (lo < 0x0F))
            {
                // bit 3
                reg += lo - 0x06;
                bit_n_reg(3, reg);
                cycleCount += 8;
            }
            else if (lo == 0x06)
            {
                bit_n_addr(2, cpu.reg16.hl);
                cycleCount += 16;
            }
            else
            {
                bit_n_addr(3, cpu.reg16.hl);
                cycleCount += 16;
            }

            break;
        }
        case 0x06:
        {
            if (lo < 0x06)
            {
                // bit 4
                reg += lo + 1;
                bit_n_reg(4, reg);
                cycleCount += 8;
            }
            else if ((lo > 0x07) && (lo < 0x0F))
            {
                // bit 5
                reg += lo - 0x06;
                bit_n_reg(5, reg);
                cycleCount += 8;
            }
            else if (lo == 0x06)
            {
                bit_n_addr(4, cpu.reg16.hl);
                cycleCount += 16;
            }
            else
            {
                bit_n_addr(5, cpu.reg16.hl);
                cycleCount += 16;
            }

            break;
        }
        case 0x07:
        {
            if (lo < 0x06)
            {
                // bit 6
                reg += lo + 1;
                bit_n_reg(6, reg);
                cycleCount += 8;
            }
            else if ((lo > 0x07) && (lo < 0x0F))
            {
                // bit 7
                reg += lo - 0x06;
                bit_n_reg(7, reg);
                cycleCount += 8;
            }
            else if (lo == 0x06)
            {
                bit_n_addr(6, cpu.reg16.hl);
                cycleCount += 16;
            }
            else
            {
                bit_n_addr(7, cpu.reg16.hl);
                cycleCount += 16;
            }

            break;
        }
    }
}

void mapInstrToFunc(uint8_t instr)
{
    bool is16 = false;

    uint8_t hi = instr >> 4;
    uint8_t lo = instr & 15;

    // main instruction decode loop
    switch (instr)
    {
        case 0x00: // NOP
        {
            cycleCount += 4;
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
            cycleCount += 12;
            break;
        }
        case 0x02: // LD (BC),A
        case 0x12: // LD (DE),A
        {
            Register16 reg = BC + hi; // use high nibble
            write8(cpu.reg8.a, cpu.reg16_arr[reg]);
            cycleCount += 8;
            break;
        }
        case 0x22: // LD (HL+),A
        {
            write8(cpu.reg8.a, cpu.reg16.hl++);
            cycleCount += 8;
            break;
        }
        case 0x32: // LD (HL-),A
        {
            write8(cpu.reg8.a, cpu.reg16.hl--);
            cycleCount += 8;
            break;
        }
        case 0x06: // LD B,d8
        case 0x16: // LD D,d8
        case 0x26: // LD H,d8
        {
            Register8 reg = B + ((lo - 6) * 2); // use low nibble
            ld_reg8_imm(reg, fetch8(++cpu.reg16.pc));
            cycleCount += 8;
            break;
        }
        case 0x36: // LD (HL),d8
        {
            write8(++cpu.reg16.pc, cpu.reg16.hl);
            cycleCount += 12;
            break;
        }
        case 0x0A: // LD A,(BC)
        case 0x1A: // LD A,(DE)
        {
            Register16 reg = BC + (lo - 0x0A); // use low nibble
            ld_reg8_addr(A, reg);
            break;
        }
        case 0x2A: // LD A,(HL+)
        case 0x3A: // LD A,(HL-)
        {
            ld_reg8_addr(A, HL);
            (hi == 0x02) ? cpu.reg16.hl-- : cpu.reg16.hl++;
            cycleCount += 8;
            break;
        }
        case 0x0E: // LD C,d8
        case 0x1E: // LD E,d8
        case 0x2E: // LD L,d8
        {
            Register8 reg = C + ((lo - 0x0E) * 2); // use low nibble
            ld_reg8_imm(reg, ++cpu.reg16.pc);
            cycleCount += 8;
            break;
        }
        case 0x3E: // LD A,d8
        {
            ld_reg8_imm(A, ++cpu.reg16.pc);
            cycleCount += 8;
            break;
        }
        case 0xE2: // LD (C),A (write to IO-port C)
        {
            write8(cpu.reg8.a, 0xFF00 + cpu.reg8.c);
            cycleCount += 8;
            break;
        }
        case 0xF2: // LD A, (C) (read from IO port C)
        {
            setRegister8(A, fetch8(0xFF00 + cpu.reg8.c));
            cycleCount += 8;
            break;
        }
        case 0x0C: // INC C
        case 0x1C: // INC E
        case 0x2C: // INC L
        case 0x0D: // DEC C
        case 0x1D: // DEC E
        case 0x2D: // DEC L
        {
            Register8 reg = C + (hi * 2); // use high nibble
            if (lo == 0x0C) { inc8_reg(reg); }
            else            { dec8_reg(reg); }
            cycleCount += 4;
            break;
        }
        case 0x3C: // INC A
        case 0x3D: // DEC A
        {
            if (lo == 0x0C) { inc8_reg(A); }
            else            { dec8_reg(A); }
            cycleCount += 4;
            break;
        }
        case 0x04: // INC B
        case 0x14: // INC D
        case 0x24: // INC H
        case 0x05: // DEC B
        case 0x15: // DEC D
        case 0x25: // DEC H
        {
            Register8 reg = B + (hi * 2); // use high nibble
            if (lo == 0x04) { inc8_reg(reg); }
            else            { dec8_reg(reg); }
            cycleCount += 4;
            break;
        }
        case 0x34: // INC (HL)
        case 0x35: // DEC (HL)
        {
            if (lo == 0x04) { inc8_mem(cpu.reg16.hl); }
            else            { dec8_mem(cpu.reg16.hl); }
            cycleCount += 12;
            break;
        }
        case 0xA8: // XOR B
        case 0xA9: // XOR C
        case 0xAA: // XOR D
        case 0xAB: // XOR E
        case 0xAC: // XOR H
        case 0xAD: // XOR L
        {
            Register8 reg = B + (lo - 8); // use low nibble
            xor8_a_n(cpu.reg8_arr[reg]);
            cycleCount += 4;
            break;
        }
        case 0xAF: // XOR A
        {
            xor8_a_n(cpu.reg8.a);
            cycleCount += 4;
            break;
        }
        case 0xAE: // XOR (HL)
        {
            xor8_a_n(fetch8(cpu.reg16.hl));
            cycleCount += 8;
            break;
        }
        case 0xCB: // CB-prefixed
        {
            decodeCbPrefix();
            break;
        }
        case 0x18: // JR r8
        {
            jr_n(fetch8(cpu.reg16.pc + 1));
            cpu.reg16.pc++;
            break;
        }
        case 0x20: // JR NZ,r8
        case 0x28: // JR Z,r8
        {
            jr_n_cond(fetch8(cpu.reg16.pc + 1), FLAG_Z, (hi == 0x8 ? true : false));
            cpu.reg16.pc++;
            break;
        }
        case 0x30: // JR NC,r8
        case 0x38: // JR C,r8
        {
            jr_n_cond(fetch8(cpu.reg16.pc + 1), FLAG_C, (hi == 0x8 ? true : false));
            cpu.reg16.pc++;
            break;
        }
        case 0xC3: // JP a16
        {
            jmp_nn(fetch16(cpu.reg16.pc + 1));
            cpu.reg16.pc += 2;
            break;
        }
        case 0xC2: // JP NZ,a16
        case 0xCA: // JP Z,a16
        {
            jmp_nn_cond(fetch16(cpu.reg16.pc + 1), FLAG_Z, (hi == 0xA ? true : false));
            break;
        }
        case 0xD2: // JP NC,a16
        case 0xDA: // JP C,a16
        {
            jmp_nn_cond(fetch16(cpu.reg16.pc + 1), FLAG_C, (hi == 0xA ? true : false));
            break;
        }
        case 0x76: // HALT
        {
            while(true);
            break;
        }

        default:
        {
            Register8 regL = A;
            Register8 regR = A;
            bool hl = false;

            switch(hi)
            {
                case 0x04: // LD B,x - LD C,x
                {
                    regL  = (lo <= 0x07) ? B : C;
                    
                    if ((lo | ~0x06) == ~0) // middle 2 bits: 0x6 or 0xE == (HL)
                    {
                        hl = true;
                        cycleCount += 4; // 4 extra cycles
                    }
                    else if ((lo & 0x07) == 0) // lower 3 bits not set? not A
                    {
                        regR += (lo > 0x07) ? lo + 1 : lo - 0x06;
                    }

                    if (!hl) { ld_reg8_reg8(regL, regR); }
                    else     { ld_reg8_addr(regL, cpu.reg16.hl); }
                    cycleCount += 4;

                    break;
                }
                case 0x05: // LD D,x - LD E,x
                {
                    regL  = (lo <= 0x07) ? D : E;
                    
                    if ((lo | ~0x06) == ~0) // middle 2 bits: 0x6 or 0xE == (HL)
                    {
                        hl = true;
                        cycleCount += 4; // 4 extra cycles
                    }
                    else if ((lo & 0x07) == 0) // lower 3 bits not set? not A
                    {
                        regR += (lo > 0x07) ? lo + 1 : lo - 0x06;
                    }

                    if (!hl) { ld_reg8_reg8(regL, regR); }
                    else     { ld_reg8_addr(regL, cpu.reg16.hl); }
                    cycleCount += 4;

                    break;
                }
                case 0x06: // LD H,x - LD L,x
                {
                    regL  = (lo <= 0x07) ? H : L;
                    
                    if ((lo | ~0x06) == ~0) // middle 2 bits: 0x6 or 0xE == (HL)
                    {
                        hl = true;
                        cycleCount += 4; // 4 extra cycles
                    }
                    else if ((lo & 0x07) == 0) // lower 3 bits not set? not A
                    {
                        regR += (lo > 0x07) ? lo + 1 : lo - 0x06;
                    }

                    if (!hl) { ld_reg8_reg8(regL, regR); }
                    else     { ld_reg8_addr(regL, cpu.reg16.hl); }
                    cycleCount += 4;

                    break;
                }
                case 0x07: // LD (HL),x - LD A, x
                {
                    bool hlLeft = false;

                    if (lo <= 0x07) // (HL). no need to worry about HALT, already handled above
                    {
                        hlLeft = true;
                        cycleCount += 4; // 4 extra cycles
                    }
                    else if (lo == 0xE) // LD A, (HL)
                    {
                        hl = true;
                        cycleCount += 4; // 4 extra cycles
                    }
                    else if ((lo & 0x07) == 0) // lower 3 bits not set? not A
                    {
                        regR += (lo > 0x07) ? lo + 1 : lo - 0x06;
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

                    cycleCount += 4;
                    break;
                }
                default:
                {
                    while (true);
                }
            }
        }

    }

    stepCpu(is16);
}