#include "cputest.h"

#include "hw/cpu.h"
#include "hw/mem.h"

#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

void runTests(void)
{
    resetBus();
    bus_t *pBus = pGetBusPtr();
    cpu_t cpu = { 0 };

    DIR *pTestDir = opendir("instructions");

    if (pTestDir == NULL)
    {
        return;
    }

    struct dirent *pEntry;

    while ((pEntry = readdir(pTestDir)) != NULL)
    {
        FILE *pFile = fopen(pEntry->d_name, "r");

        fseek(pFile, 0, SEEK_END);
        long length = ftell(pFile);
        fseek(pFile, 0, SEEK_SET);

        char *content = malloc(length+1);
        fread(content, 1, length, pFile);
        content[length] = '\0';

        cJSON *obj = cJSON_Parse(content);

        (void)obj;
        (void)cpu;
        (void)pBus;

        fclose(pFile);
        free(content);
    }
}