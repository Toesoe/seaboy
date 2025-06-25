/**
 * @file cart.c
 * @author Toesoe
 * @brief seaboy cartridge emulation
 * @version 0.1
 * @date 2023-06-13
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <fcntl.h> // For open
#include <stdio.h>
#include <sys/mman.h> // For mmap, munmap
#include <sys/stat.h> // For fstat
#include <unistd.h>   // For close

#include "mem.h"

/**
 * map a romfile into memory
 */
void loadRom(const char *fn)
{
    struct stat sb;

    int         fd = open(fn, O_RDONLY);

    if (fd == -1)
    {
        printf("cannot open file %s\n", fn);
        close(fd);
        return;
    }

    if (fstat(fd, &sb) == -1)
    {
        printf("cannot retrieve filesize\n");
        close(fd);
        return;
    }

    uint8_t *pActiveRom = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (pActiveRom == MAP_FAILED)
    {
        printf("cannot map rom in memory\n");
        close(fd);
        return;
    }

    close(fd);

    mapRomIntoMem(&pActiveRom, sb.st_size);
}