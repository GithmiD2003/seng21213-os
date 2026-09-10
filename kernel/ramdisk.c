/* =============================================================================
 * SENG21213-OS :: Stage 4 RAM Disk
 *
 * The 1 MB disk is backed by 256 frames obtained from the Stage 3 physical
 * memory manager. Each physical frame contains eight 512-byte disk blocks.
 * ============================================================================*/

#include "ramdisk.h"
#include "pmm.h"

#define RAMDISK_FRAME_COUNT (RAMDISK_SIZE / PMM_FRAME_SIZE)
#define BLOCKS_PER_FRAME    (PMM_FRAME_SIZE / RAMDISK_BLOCK_SIZE)

static uint8_t *disk_frames[RAMDISK_FRAME_COUNT];
static bool initialized;

static void copy_bytes(uint8_t *destination, const uint8_t *source, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        destination[i] = source[i];
    }
}

bool ramdisk_init(void)
{
    uint32_t allocated = 0;

    initialized = false;

    for (uint32_t i = 0; i < RAMDISK_FRAME_COUNT; i++) {
        disk_frames[i] = (uint8_t *)pmm_alloc_frame();

        if (disk_frames[i] == NULL) {
            for (uint32_t j = 0; j < allocated; j++) {
                pmm_free_frame(disk_frames[j]);
                disk_frames[j] = NULL;
            }
            return false;
        }

        allocated++;

        for (uint32_t byte = 0; byte < PMM_FRAME_SIZE; byte++) {
            disk_frames[i][byte] = 0;
        }
    }

    initialized = true;
    return true;
}

void *ramdisk_get_block(uint32_t block_number)
{
    uint32_t frame_index;
    uint32_t block_offset;

    if (!initialized || block_number >= RAMDISK_BLOCK_COUNT) {
        return NULL;
    }

    frame_index = block_number / BLOCKS_PER_FRAME;
    block_offset = (block_number % BLOCKS_PER_FRAME) * RAMDISK_BLOCK_SIZE;

    return disk_frames[frame_index] + block_offset;
}

bool ramdisk_read_block(uint32_t block_number, void *buffer)
{
    uint8_t *source = (uint8_t *)ramdisk_get_block(block_number);

    if (source == NULL || buffer == NULL) {
        return false;
    }

    copy_bytes((uint8_t *)buffer, source, RAMDISK_BLOCK_SIZE);
    return true;
}

bool ramdisk_write_block(uint32_t block_number, const void *buffer)
{
    uint8_t *destination = (uint8_t *)ramdisk_get_block(block_number);

    if (destination == NULL || buffer == NULL) {
        return false;
    }

    copy_bytes(destination, (const uint8_t *)buffer, RAMDISK_BLOCK_SIZE);
    return true;
}
