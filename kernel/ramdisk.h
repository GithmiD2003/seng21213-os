#ifndef RAMDISK_H
#define RAMDISK_H

#include "../include/types.h"

#define RAMDISK_SIZE        (1024U * 1024U)
#define RAMDISK_BLOCK_SIZE  512U
#define RAMDISK_BLOCK_COUNT (RAMDISK_SIZE / RAMDISK_BLOCK_SIZE)

bool ramdisk_init(void);
void *ramdisk_get_block(uint32_t block_number);
bool ramdisk_read_block(uint32_t block_number, void *buffer);
bool ramdisk_write_block(uint32_t block_number, const void *buffer);

#endif /* RAMDISK_H */
