#ifndef RAMDISK_H
#define RAMDISK_H

#include "../include/types.h"

/*
 * Stage 4 - RAM Disk
 * Lecture 12
 *
 * 1 MB RAM disk divided into 4 KB blocks.
 */

#define RAMDISK_SIZE   (1024 * 1024)
#define RAMDISK_BLOCK_SIZE 4096
#define RAMDISK_BLOCK_COUNT (RAMDISK_SIZE / RAMDISK_BLOCK_SIZE)

/*
 * Return a pointer to a block in the RAM disk.
 */
uint8_t *ramdisk_get_block(uint32_t block);

/*
 * Initialize and clear the RAM disk.
 */
void ramdisk_init(void);

#endif /* RAMDISK_H */
