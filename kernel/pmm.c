/* =============================================================================
 * SENG21213-OS :: Stage 3 Physical Memory Manager
 *
 * The bootloader stores the BIOS E820 map at 0x5000.  This module turns the
 * usable ranges into a bitmap containing one bit for each 4 KB frame.
 * QEMU is started with 32 MB of RAM, so this allocator manages that range.
 * Physical addresses are also valid virtual addresses (identity mapping).
 * ============================================================================*/

#include "pmm.h"

#define E820_COUNT_ADDRESS 0x5000U
#define E820_MAP_ADDRESS   0x5004U
#define E820_MAX_ENTRIES   32U
#define E820_USABLE        1U

#define PMM_MAX_MEMORY     (32U * 1024U * 1024U)
#define PMM_MAX_FRAMES     (PMM_MAX_MEMORY / PMM_FRAME_SIZE)
#define PMM_BITMAP_BYTES   (PMM_MAX_FRAMES / 8U)
#define LOW_MEMORY_LIMIT   0x00100000U

typedef struct {
    uint32_t base_low;
    uint32_t base_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
    uint32_t attributes;
} __attribute__((packed)) e820_entry_t;

static uint8_t frame_bitmap[PMM_BITMAP_BYTES];
static uint32_t total_frames;
static uint32_t free_frames;
static uint32_t e820_entry_count;

static bool bitmap_is_used(uint32_t frame)
{
    return (frame_bitmap[frame / 8U] & (1U << (frame % 8U))) != 0;
}

static void bitmap_mark_used(uint32_t frame)
{
    if (frame >= total_frames || bitmap_is_used(frame)) {
        return;
    }

    frame_bitmap[frame / 8U] |= (uint8_t)(1U << (frame % 8U));
    free_frames--;
}

static void bitmap_mark_free(uint32_t frame)
{
    if (frame >= total_frames || !bitmap_is_used(frame)) {
        return;
    }

    frame_bitmap[frame / 8U] &= (uint8_t)~(1U << (frame % 8U));
    free_frames++;
}

static bool frame_is_e820_usable(uint32_t frame)
{
    volatile e820_entry_t *map =
        (volatile e820_entry_t *)E820_MAP_ADDRESS;
    uint32_t address = frame * PMM_FRAME_SIZE;

    for (uint32_t i = 0; i < e820_entry_count; i++) {
        uint32_t start;
        uint32_t end;

        if (map[i].type != E820_USABLE || map[i].base_high != 0) {
            continue;
        }

        start = map[i].base_low;
        if (start >= PMM_MAX_MEMORY) {
            continue;
        }

        if (map[i].length_high != 0 ||
            map[i].length_low > PMM_MAX_MEMORY - start) {
            end = PMM_MAX_MEMORY;
        } else {
            end = start + map[i].length_low;
        }

        if (address >= start && address + PMM_FRAME_SIZE <= end) {
            return true;
        }
    }

    return false;
}

void pmm_init(void)
{
    volatile uint32_t *bios_count =
        (volatile uint32_t *)E820_COUNT_ADDRESS;
    volatile e820_entry_t *map =
        (volatile e820_entry_t *)E820_MAP_ADDRESS;
    uint32_t managed_limit = 0;

    e820_entry_count = *bios_count;
    if (e820_entry_count > E820_MAX_ENTRIES) {
        e820_entry_count = E820_MAX_ENTRIES;
    }

    /* Begin with every frame reserved. */
    for (uint32_t i = 0; i < PMM_BITMAP_BYTES; i++) {
        frame_bitmap[i] = 0xFF;
    }

    /* Find the highest usable address that this 32 MB build can manage. */
    for (uint32_t i = 0; i < e820_entry_count; i++) {
        uint32_t end;

        if (map[i].type != E820_USABLE || map[i].base_high != 0 ||
            map[i].base_low >= PMM_MAX_MEMORY) {
            continue;
        }

        if (map[i].length_high != 0 ||
            map[i].length_low > PMM_MAX_MEMORY - map[i].base_low) {
            end = PMM_MAX_MEMORY;
        } else {
            end = map[i].base_low + map[i].length_low;
        }

        if (end > managed_limit) {
            managed_limit = end;
        }
    }

    managed_limit &= ~(PMM_FRAME_SIZE - 1U);
    total_frames = managed_limit / PMM_FRAME_SIZE;
    free_frames = 0;

    /* Clear bits belonging to complete frames in usable E820 ranges. */
    for (uint32_t i = 0; i < e820_entry_count; i++) {
        uint32_t start;
        uint32_t end;

        if (map[i].type != E820_USABLE || map[i].base_high != 0 ||
            map[i].base_low >= managed_limit) {
            continue;
        }

        start = (map[i].base_low + PMM_FRAME_SIZE - 1U) &
                ~(PMM_FRAME_SIZE - 1U);

        if (map[i].length_high != 0 ||
            map[i].length_low > managed_limit - map[i].base_low) {
            end = managed_limit;
        } else {
            end = (map[i].base_low + map[i].length_low) &
                  ~(PMM_FRAME_SIZE - 1U);
        }

        for (uint32_t address = start;
             address < end;
             address += PMM_FRAME_SIZE) {
            bitmap_mark_free(address / PMM_FRAME_SIZE);
        }
    }

    /* Never allocate the first MiB: it contains BIOS data, the bootloader,
     * kernel image, kernel stacks and the VGA framebuffer. */
    for (uint32_t frame = 0;
         frame < LOW_MEMORY_LIMIT / PMM_FRAME_SIZE && frame < total_frames;
         frame++) {
        bitmap_mark_used(frame);
    }
}

void *pmm_alloc_frame(void)
{
    uint32_t flags;

    __asm__ __volatile__("pushfl; popl %0; cli" : "=r"(flags) :: "memory");

    for (uint32_t frame = LOW_MEMORY_LIMIT / PMM_FRAME_SIZE;
         frame < total_frames;
         frame++) {
        if (!bitmap_is_used(frame)) {
            bitmap_mark_used(frame);
            __asm__ __volatile__("pushl %0; popfl" :: "r"(flags) : "memory", "cc");
            return (void *)(frame * PMM_FRAME_SIZE);
        }
    }

    __asm__ __volatile__("pushl %0; popfl" :: "r"(flags) : "memory", "cc");
    return NULL;
}

void pmm_free_frame(void *address)
{
    uint32_t raw = (uint32_t)address;
    uint32_t frame;
    uint32_t flags;

    if (raw < LOW_MEMORY_LIMIT || raw >= total_frames * PMM_FRAME_SIZE ||
        (raw & (PMM_FRAME_SIZE - 1U)) != 0) {
        return;
    }

    frame = raw / PMM_FRAME_SIZE;
    if (!frame_is_e820_usable(frame)) {
        return;
    }

    __asm__ __volatile__("pushfl; popl %0; cli" : "=r"(flags) :: "memory");
    bitmap_mark_free(frame);
    __asm__ __volatile__("pushl %0; popfl" :: "r"(flags) : "memory", "cc");
}

uint32_t pmm_get_total_frames(void)
{
    return total_frames;
}

uint32_t pmm_get_used_frames(void)
{
    return total_frames - free_frames;
}

uint32_t pmm_get_free_frames(void)
{
    return free_frames;
}

uint32_t pmm_get_e820_entry_count(void)
{
    return e820_entry_count;
}
