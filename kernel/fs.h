#ifndef FS_H
#define FS_H

#include "../include/types.h"

#define FS_MAX_NAME       27U
#define FS_MAX_FILES      64U
#define FS_MAX_FILE_SIZE  4096U

#define FS_O_READ    0x01U
#define FS_O_WRITE   0x02U
#define FS_O_CREATE  0x04U
#define FS_O_TRUNC   0x08U

typedef struct {
    char name[FS_MAX_NAME + 1U];
    uint32_t size;
} fs_file_info_t;

bool fs_init(void);
int fs_open(const char *name, uint32_t flags);
int fs_read(int descriptor, void *buffer, uint32_t count);
int fs_write(int descriptor, const void *buffer, uint32_t count);
int fs_close(int descriptor);
int fs_unlink(const char *name);
uint32_t fs_list(fs_file_info_t *files, uint32_t capacity);

#endif /* FS_H */
