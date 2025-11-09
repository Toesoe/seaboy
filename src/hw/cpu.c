/**
 * @file g_cpu.registers.c
 * @author Toesoe
 * @brief seaboy SM83 emulation
 * @version 0.1
 * @date 2023-06-13
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "cpu.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "instr.h"
#include "mem.h"

#define DIV_REGISTER_UPDATE_RATE_HZ  (16384)
#define DIV_REGISTER_TICK_INTERVAL   (CPU_CLOCK_SPEED_HZ / DIV_REGISTER_UPDATE_RATE_HZ)

typedef struct
{
    SCPURegisters_t registers;
    bool interruptMasterEnable;
    bool delayedIMELatch;
    bool isHalted;  // HALT
    bool isStopped; // STOP

    uint8_t  delayedIMECounter;
    uint16_t divCounter;
    uint8_t  timaPendingReloadDelay;
    uint8_t  timaPreviousSignalLevels[4];
} SCPU_t;

static SCPU_t g_cpu;
static SAddressBus_t *g_pBus;

static bool   haltOnUnknown = false;

static size_t handleInterrupts(void);
static void   handleTimers(size_t);

static void   cpuSkipBootrom(void);
static void   bootromUnmapCallback(uint8_t, uint16_t);
static void   divWriteCallback(uint8_t, uint16_t);

/**
 * @brief reset g_cpu.registers to initial state
 */
void resetCpu(bool skipBootrom)
{
    memset(&g_cpu.registers, 0x00, sizeof(g_cpu.registers));
    g_cpu.interruptMasterEnable  = false;
    instrSetCpuPtr(&g_cpu.registers);
    g_pBus = pGetAddressBus();

    if (skipBootrom)
    {
        cpuSkipBootrom();
    }

    registerAddressCallback(BOOT_ROM_MAPPER_CONTROL_ADDR, BOOTROM_UNMAP_CALLBACK, bootromUnmapCallback);
    registerAddressCallback(DIVIDER_ADDR, DIV_CALLBACK, divWriteCallback);
}

/**
 * useful for unit testing instructions
 */
void overrideCpu(SCPURegisters_t *pCpu)
{
    memcpy(&g_cpu.registers, pCpu, sizeof(SCPURegisters_t));
}

/**
 * @brief set a specific flag
 *
 * @param flag flag to set
 */
void setFlag(Flag flag)
{
    g_cpu.registers.reg8.f |= (1 << flag);
}

/**
 * @brief reset a specific flag
 *
 * @param flag flag to reset
 */
void resetFlag(Flag flag)
{
    g_cpu.registers.reg8.f &= ~(1 << flag);
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
    return g_cpu.registers.reg8.f & (1 << flag);
}

/**
 * @brief step the program counter
 *
 */
void incrementProgramCounter(int incrementBy)
{
    g_cpu.registers.reg16.pc += incrementBy;
}

/**
 * @brief set PC to a specific address
 *
 * @param addr address to set PC to
 */
void jumpCpu(uint16_t addr)
{
    g_cpu.registers.reg16.sp = addr;
}

/**
 * @brief retrieve constptr to g_cpu.registers object
 */
const SCPURegisters_t *pGetCPURegisters(void)
{
    return (const SCPURegisters_t *)&g_cpu.registers;
}

void setIME()
{
    g_cpu.interruptMasterEnable = true;
}

void resetIME()
{
    g_cpu.interruptMasterEnable = false;
}

bool checkIME()
{
    return g_cpu.interruptMasterEnable;
}

void setDelayedIMELatch()
{
    g_cpu.delayedIMELatch = true;
}

void resetDelayedIMELatch()
{
    g_cpu.delayedIMELatch = false;
}

bool checkDelayedIMELatch()
{
    return g_cpu.delayedIMELatch;
}

void setHalted()
{
    g_cpu.isHalted = true;
}

void resetHalted()
{
    g_cpu.isHalted = false;
}

bool checkHalted()
{
    return g_cpu.isHalted;
}

void setStopped()
{
    g_cpu.isStopped = true;
}

void resetStopped()
{
    g_cpu.isStopped = false;
}

bool checkStopped()
{
    return g_cpu.isStopped;
}

/**
 * @brief set a specific 16-bit register
 *
 * @param reg register to set
 * @param value value to write to the register
 */
void setRegister16(Register16 reg, uint16_t value)
{
    g_cpu.registers.reg16_arr[reg] = value;
}

/**
 * @brief set a specific 8-bit register
 *
 * @param reg register to set
 * @param value value to write to the register
 */
void setRegister8(Register8 reg, uint8_t value)
{
    g_cpu.registers.reg8_arr[reg] = value;
}

/**
 * @brief fetch-decode-execute loop
 * 
 * @return size_t mCycles consumed during this iteration
 */
size_t stepCPU()
{
    SCPUCurrentCycleState_t thisCycle = { 0 };

    thisCycle.mCyclesExecuted = handleInterrupts();

    if (!checkHalted() || !checkStopped())
    {
        // fetch-decode-execute
        thisCycle.instruction = fetch8(g_cpu.registers.reg16.pc);
        decodeAndExecute(&thisCycle);
    }

    handleTimers(thisCycle.mCyclesExecuted * 4);

    if (checkDelayedIMELatch())
    {
        if (++g_cpu.delayedIMECounter == 2)
        {
            setIME();
            resetDelayedIMELatch();
            g_cpu.delayedIMECounter = 0;
        }
    }

    if (!checkHalted())
    {
        incrementProgramCounter(thisCycle.programCounterSteps);
    }

    return thisCycle.mCyclesExecuted;
}

static size_t handleInterrupts(void)
{
    bool fired = false;
    bool intPending = (*(uint8_t *)&g_pBus->map.interruptEnable & *(uint8_t *)&g_pBus->map.ioregs.intFlags);

    if (g_cpu.isHalted && intPending)
    {
        g_cpu.isHalted = false; // wake CPU
    }

    if (g_cpu.interruptMasterEnable && intPending)
    {
        if (g_pBus->map.interruptEnable.vblank && g_pBus->map.ioregs.intFlags.vblank)
        {
            fired                            = true;
            g_pBus->map.ioregs.intFlags.vblank = 0;
            call_irq_subroutine(0x40);
        }
        else if (g_pBus->map.interruptEnable.lcd && g_pBus->map.ioregs.intFlags.lcd)
        {
            if ((g_pBus->map.ioregs.lcd.stat.LYCIntSel && g_pBus->map.ioregs.lcd.stat.lycEqLy) ||
                (g_pBus->map.ioregs.lcd.stat.mode0IntSel && (g_pBus->map.ioregs.lcd.stat.ppuMode == 0)) ||
                (g_pBus->map.ioregs.lcd.stat.mode1IntSel && (g_pBus->map.ioregs.lcd.stat.ppuMode == 1)) ||
                (g_pBus->map.ioregs.lcd.stat.mode2IntSel && (g_pBus->map.ioregs.lcd.stat.ppuMode == 2)))
            {
                // STAT int
                fired                         = true;
                g_pBus->map.ioregs.intFlags.lcd = 0;
                call_irq_subroutine(0x48);
            }
        }
        else if (g_pBus->map.interruptEnable.timer && g_pBus->map.ioregs.intFlags.timer)
        {
            fired                           = true;
            g_pBus->map.ioregs.intFlags.timer = 0;
            call_irq_subroutine(0x50);
        }
        else if (g_pBus->map.interruptEnable.serial && g_pBus->map.ioregs.intFlags.serial)
        {
            fired                            = true;
            g_pBus->map.ioregs.intFlags.serial = 0;
            call_irq_subroutine(0x58);
        }
        else if (g_pBus->map.interruptEnable.joypad && g_pBus->map.ioregs.intFlags.joypad)
        {
            fired                            = true;
            g_pBus->map.ioregs.intFlags.joypad = 0;
            call_irq_subroutine(0x60);
        }

        if (fired)
        {
            g_cpu.interruptMasterEnable = false;
            return 5;
        }
    }
    return 0;
}

static void handleTimers(size_t cpuCycles)
{
    uint8_t bitPos = 0;
    static bool remainingCyclesTIMAReset = 4;

    if (g_pBus->map.ioregs.timers.TAC.enable)
    {
        switch (g_pBus->map.ioregs.timers.TAC.clockSelect)
        {
            case 0:
                bitPos = 9;
                break; // 4096 Hz   -> 1024 T-cycles
            case 1:
                bitPos = 3;
                break; // 262144 Hz -> 16 T-cycles
            case 2:
                bitPos = 5;
                break; // 65536 Hz  -> 64 T-cycles
            case 3:
                bitPos = 7;
                break; // 16384 Hz  -> 256 T-cycles
        }
    }

    while (cpuCycles--)
    {
        g_cpu.divCounter++;

        if (g_pBus->map.ioregs.timers.TAC.enable)
        {
            bool signalNow = g_pBus->map.ioregs.timers.TAC.enable && (g_cpu.divCounter & (1 << bitPos));

            // check if we have a falling edge
            if (!signalNow && g_cpu.timaPreviousSignalLevels[g_pBus->map.ioregs.timers.TAC.clockSelect])
            {
                g_pBus->map.ioregs.timers.TIMA++;
            }

            if (g_pBus->map.ioregs.timers.TIMA == 0x00)
            {
                remainingCyclesTIMAReset--;

                if (remainingCyclesTIMAReset == 0)
                {
                    g_pBus->map.ioregs.timers.TIMA    = g_pBus->map.ioregs.timers.TMA;
                    g_pBus->map.ioregs.intFlags.timer = 1;
                    remainingCyclesTIMAReset = 4;
                }
            }

           g_cpu.timaPreviousSignalLevels[g_pBus->map.ioregs.timers.TAC.clockSelect] = signalNow;
        }
    }

    g_pBus->map.ioregs.divRegister = (uint8_t)(g_cpu.divCounter >> 8) & 0xFF;
}

/**
 * @brief set CPU registers to post-bootrom values
 *
 */
static void cpuSkipBootrom(void)
{
    g_cpu.registers.reg8.a   = 0x01;
    g_cpu.registers.reg8.f   = 0xB0;
    g_cpu.registers.reg8.b   = 0x00;
    g_cpu.registers.reg8.c   = 0x13;
    g_cpu.registers.reg8.d   = 0x00;
    g_cpu.registers.reg8.e   = 0xD8;
    g_cpu.registers.reg8.h   = 0x01;
    g_cpu.registers.reg8.l   = 0x4D;
    g_cpu.registers.reg16.sp = 0xFFFE;
    g_cpu.registers.reg16.pc = 0x100;
}

static void bootromUnmapCallback(uint8_t data, uint16_t addr)
{
    //g_cpu.interruptMasterEnable = false;
}

static void divWriteCallback(uint8_t data, uint16_t addr)
{
    g_cpu.divCounter = 0;
    memset(g_cpu.timaPreviousSignalLevels, 0, 4);
}