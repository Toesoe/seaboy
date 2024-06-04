#include "cputest.h"

#include "hw/cpu.h"
#include "hw/mem.h"
#include "hw/instr.h"

#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static void set_state(cJSON *, cpu_t *, bus_t *);

void runTests(void)
{
    resetBus();

    DIR *pTestDir = opendir("./src/instructions");

    if (pTestDir == NULL)
    {
        return;
    }

    struct dirent *pEntry;

    cpu_t cpu = { 0 };
    cpu_t finalCpu = { 0 };

    bus_t bus = {0};
    bus_t busFinal = {0};

    instrSetCpuPtr(&cpu);

    while ((pEntry = readdir(pTestDir)) != NULL)
    {
        bool passedBus = true;

        bool checkBus = false;

        if (pEntry->d_name[0] == '.') continue;
        char fullpath[256] = "./src/instructions/";
        strncat(fullpath, pEntry->d_name, 10);
        //printf("name %s\n", fullpath);
        FILE *pFile = fopen(fullpath, "r");

        fseek(pFile, 0, SEEK_END);
        long length = ftell(pFile);
        fseek(pFile, 0, SEEK_SET);

        char *content = malloc(length+1);
        fread(content, 1, length, pFile);
        content[length] = '\0';

        cJSON *json_array = cJSON_Parse(content);

        cJSON *json_item = NULL;
        cJSON_ArrayForEach(json_item, json_array)
        {
            if (cJSON_IsObject(json_item))
            {
                overrideBus(&bus);
                resetCpu();
                overrideCpu(&cpu);

                int posCounter = 0;

                // Parse and print "initial" object
                cJSON *initial = cJSON_GetObjectItemCaseSensitive(json_item, "initial");
                set_state(initial, &cpu, &bus);

                // Parse and print "final" object
                cJSON *final = cJSON_GetObjectItemCaseSensitive(json_item, "final");
                set_state(final, &finalCpu, &busFinal);

                cJSON *cycles = cJSON_GetObjectItemCaseSensitive(json_item, "cycles");

                cJSON *cycleEntry = NULL;

                cJSON_ArrayForEach(cycleEntry, cycles)
                {
                    executeInstruction(bus.bus[cpu.reg16.pc]);
                    posCounter++;
                }

                if (!memcmp(&cpu, &finalCpu, sizeof(cpu_t)))
                {
                    printf("cpu mismatch in instr file %s, name %d\n", fullpath, posCounter);
                }

                if (checkBus && passedBus)
                {
                    if (!memcmp(&bus, &busFinal, sizeof(bus_t)))
                    {
                        printf("bus mismatches in instr file %s:\n", fullpath);
                        for (unsigned int i = 0; i < GB_BUS_SIZE; i++)
                        {
                            if (bus.bus[i] != busFinal.bus[i])
                            {
                                printf("value 0x%2x != 0x%2x at addr 0x%04x\n", bus.bus[i], busFinal.bus[i], i);
                            }
                        }
                        passedBus = false;
                    }
                }
            }
        }

        cJSON_Delete(json_array);
        fclose(pFile);
        free(content);
    }
}

static void set_state(cJSON *state, cpu_t *pCpu, bus_t *pBus)
{
    memset(pCpu, 0, sizeof(cpu_t));
    memset(pBus, 0, sizeof(bus_t));
    memset(&pBus->map.ioregs.joypad, 0xFF, 1);

    if (cJSON_IsObject(state))
    {
        cJSON *pc = cJSON_GetObjectItemCaseSensitive(state, "pc");
        cJSON *sp = cJSON_GetObjectItemCaseSensitive(state, "sp");
        cJSON *a = cJSON_GetObjectItemCaseSensitive(state, "a");
        cJSON *b = cJSON_GetObjectItemCaseSensitive(state, "b");
        cJSON *c = cJSON_GetObjectItemCaseSensitive(state, "c");
        cJSON *d = cJSON_GetObjectItemCaseSensitive(state, "d");
        cJSON *e = cJSON_GetObjectItemCaseSensitive(state, "e");
        cJSON *f = cJSON_GetObjectItemCaseSensitive(state, "f");
        cJSON *h = cJSON_GetObjectItemCaseSensitive(state, "h");
        cJSON *l = cJSON_GetObjectItemCaseSensitive(state, "l");
        //cJSON *ime = cJSON_GetObjectItemCaseSensitive(state, "ime");

        pCpu->reg16.pc = pc->valueint;
        pCpu->reg16.sp = sp->valueint;
        pCpu->reg8.a = a->valueint;
        pCpu->reg8.b = b->valueint;
        pCpu->reg8.c = c->valueint;
        pCpu->reg8.d = d->valueint;
        pCpu->reg8.e = e->valueint;
        pCpu->reg8.f = f->valueint;
        pCpu->reg8.h = h->valueint;
        pCpu->reg8.l = l->valueint;
        //cpu.ime = ime->valueint;

        // Parse and print "ram" array
        cJSON *ram = cJSON_GetObjectItemCaseSensitive(state, "ram");
        if (cJSON_IsArray(ram))
        {
            cJSON *ram_entry = NULL;
            cJSON_ArrayForEach(ram_entry, ram)
            {
                pBus->bus[cJSON_GetArrayItem(ram_entry, 0)->valueint] = cJSON_GetArrayItem(ram_entry, 1)->valueint;
            }
        }
    }
}