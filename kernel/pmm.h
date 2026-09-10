#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

#define PMM_FRAME_SIZE 4096U

void pmm_init(void);
void *pmm_alloc_frame(void);
void pmm_free_frame(void *address);

uint32_t pmm_get_total_frames(void);
uint32_t pmm_get_used_frames(void);
uint32_t pmm_get_free_frames(void);
uint32_t pmm_get_e820_entry_count(void);

#endif /* PMM_H */
