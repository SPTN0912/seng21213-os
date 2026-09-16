#include "ramdisk.h"

/*
 * Stage 4 - RAM Disk
 * Lecture 12
 *
 * The RAM disk is a fixed 1 MB byte array.
 *
 * Because this array is not explicitly initialized, it is placed
 * in the kernel's .bss section.
 */
static uint8_t ramdisk[RAMDISK_SIZE];

/*
 * Initialize the RAM disk.
 *
 * The kernel does not use a C runtime to automatically clear .bss,
 * so we explicitly clear the RAM disk here.
 */
void ramdisk_init(void)
{
    uint32_t i;

    for (i = 0; i < RAMDISK_SIZE; i++)
    {
        ramdisk[i] = 0;
    }
}

/*
 * Return a pointer to the beginning of a RAM disk block.
 */
uint8_t *ramdisk_get_block(uint32_t block)
{
    if (block >= RAMDISK_BLOCK_COUNT)
    {
        return NULL;
    }

    return &ramdisk[block * RAMDISK_BLOCK_SIZE];
}
