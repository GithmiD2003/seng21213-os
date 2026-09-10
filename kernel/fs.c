/* =============================================================================
 * SENG21213-OS :: Stage 4 RAM Disk File System
 *
 * Layout (512-byte blocks):
 *   0       superblock
 *   1       inode bitmap
 *   2       data-block bitmap
 *   3..7    64 inodes
 *   8..11   flat directory (64 entries)
 *   12..    file data
 * ============================================================================*/

#include "fs.h"
#include "ramdisk.h"

#define FS_MAGIC               0x53454E47U /* "SENG" */
#define FS_INODE_BITMAP_BLOCK  1U
#define FS_BLOCK_BITMAP_BLOCK  2U
#define FS_INODE_TABLE_START   3U
#define FS_INODE_TABLE_BLOCKS  5U
#define FS_DIRECTORY_START     8U
#define FS_DIRECTORY_BLOCKS    4U
#define FS_DATA_START          12U
#define FS_DIRECT_BLOCKS       8U
#define FS_MAX_OPEN_FILES      16U

typedef struct {
    uint32_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t inode_bitmap_block;
    uint32_t block_bitmap_block;
    uint32_t inode_table_start;
    uint32_t directory_start;
    uint32_t data_start;
    uint32_t free_blocks;
    uint32_t free_inodes;
} superblock_t;

typedef struct {
    uint32_t used;
    uint32_t size;
    uint32_t direct[FS_DIRECT_BLOCKS];
} inode_t;

typedef struct {
    char name[FS_MAX_NAME + 1U];
    uint32_t inode_number;
} directory_entry_t;

typedef struct {
    bool used;
    uint32_t inode_number;
    uint32_t offset;
    uint32_t flags;
} open_file_t;

_Static_assert(sizeof(inode_t) == 40U, "inode layout must remain 40 bytes");
_Static_assert(sizeof(directory_entry_t) == 32U,
               "directory entry layout must remain 32 bytes");

static superblock_t *superblock;
static uint8_t *inode_bitmap;
static uint8_t *block_bitmap;
static inode_t *inode_table;
static directory_entry_t *directory;
static open_file_t open_files[FS_MAX_OPEN_FILES];
static bool filesystem_ready;

static void clear_bytes(void *memory, uint32_t count)
{
    uint8_t *bytes = (uint8_t *)memory;

    for (uint32_t i = 0; i < count; i++) {
        bytes[i] = 0;
    }
}

static uint32_t string_length(const char *text)
{
    uint32_t length = 0;

    while (text != NULL && text[length] != '\0') {
        length++;
    }

    return length;
}

static bool string_equal(const char *left, const char *right)
{
    uint32_t i = 0;

    if (left == NULL || right == NULL) {
        return false;
    }

    while (left[i] != '\0' && left[i] == right[i]) {
        i++;
    }

    return left[i] == right[i];
}

static void copy_name(char *destination, const char *source)
{
    uint32_t i = 0;

    while (i < FS_MAX_NAME && source[i] != '\0') {
        destination[i] = source[i];
        i++;
    }

    destination[i] = '\0';
    while (i < FS_MAX_NAME + 1U) {
        destination[i++] = '\0';
    }
}

static bool bitmap_test(const uint8_t *bitmap, uint32_t index)
{
    return (bitmap[index / 8U] & (1U << (index % 8U))) != 0;
}

static void bitmap_set(uint8_t *bitmap, uint32_t index)
{
    bitmap[index / 8U] |= (uint8_t)(1U << (index % 8U));
}

static void bitmap_clear(uint8_t *bitmap, uint32_t index)
{
    bitmap[index / 8U] &= (uint8_t)~(1U << (index % 8U));
}

static int find_directory_entry(const char *name)
{
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (directory[i].name[0] != '\0' &&
            string_equal(directory[i].name, name)) {
            return (int)i;
        }
    }

    return -1;
}

static int allocate_inode(void)
{
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (!bitmap_test(inode_bitmap, i)) {
            bitmap_set(inode_bitmap, i);
            clear_bytes(&inode_table[i], sizeof(inode_t));
            inode_table[i].used = 1;
            superblock->free_inodes--;
            return (int)i;
        }
    }

    return -1;
}

static int allocate_block(void)
{
    for (uint32_t block = FS_DATA_START;
         block < RAMDISK_BLOCK_COUNT;
         block++) {
        if (!bitmap_test(block_bitmap, block)) {
            bitmap_set(block_bitmap, block);
            clear_bytes(ramdisk_get_block(block), RAMDISK_BLOCK_SIZE);
            superblock->free_blocks--;
            return (int)block;
        }
    }

    return -1;
}

static void release_inode_blocks(inode_t *inode)
{
    for (uint32_t i = 0; i < FS_DIRECT_BLOCKS; i++) {
        uint32_t block = inode->direct[i];

        if (block >= FS_DATA_START && block < RAMDISK_BLOCK_COUNT &&
            bitmap_test(block_bitmap, block)) {
            bitmap_clear(block_bitmap, block);
            clear_bytes(ramdisk_get_block(block), RAMDISK_BLOCK_SIZE);
            superblock->free_blocks++;
        }

        inode->direct[i] = 0;
    }

    inode->size = 0;
}

static bool valid_name(const char *name)
{
    uint32_t length = string_length(name);

    if (length == 0 || length > FS_MAX_NAME) {
        return false;
    }

    for (uint32_t i = 0; i < length; i++) {
        if (name[i] == ' ' || name[i] == '/' || name[i] == '\\') {
            return false;
        }
    }

    return true;
}

bool fs_init(void)
{
    if (!ramdisk_init()) {
        filesystem_ready = false;
        return false;
    }

    superblock = (superblock_t *)ramdisk_get_block(0);
    inode_bitmap = (uint8_t *)ramdisk_get_block(FS_INODE_BITMAP_BLOCK);
    block_bitmap = (uint8_t *)ramdisk_get_block(FS_BLOCK_BITMAP_BLOCK);
    inode_table = (inode_t *)ramdisk_get_block(FS_INODE_TABLE_START);
    directory = (directory_entry_t *)ramdisk_get_block(FS_DIRECTORY_START);

    clear_bytes(superblock, RAMDISK_BLOCK_SIZE);
    clear_bytes(inode_bitmap, RAMDISK_BLOCK_SIZE);
    clear_bytes(block_bitmap, RAMDISK_BLOCK_SIZE);
    clear_bytes(inode_table, FS_INODE_TABLE_BLOCKS * RAMDISK_BLOCK_SIZE);
    clear_bytes(directory, FS_DIRECTORY_BLOCKS * RAMDISK_BLOCK_SIZE);
    clear_bytes(open_files, sizeof(open_files));

    superblock->magic = FS_MAGIC;
    superblock->block_size = RAMDISK_BLOCK_SIZE;
    superblock->total_blocks = RAMDISK_BLOCK_COUNT;
    superblock->total_inodes = FS_MAX_FILES;
    superblock->inode_bitmap_block = FS_INODE_BITMAP_BLOCK;
    superblock->block_bitmap_block = FS_BLOCK_BITMAP_BLOCK;
    superblock->inode_table_start = FS_INODE_TABLE_START;
    superblock->directory_start = FS_DIRECTORY_START;
    superblock->data_start = FS_DATA_START;
    superblock->free_blocks = RAMDISK_BLOCK_COUNT - FS_DATA_START;
    superblock->free_inodes = FS_MAX_FILES;

    for (uint32_t block = 0; block < FS_DATA_START; block++) {
        bitmap_set(block_bitmap, block);
    }

    filesystem_ready = true;
    return true;
}

int fs_open(const char *name, uint32_t flags)
{
    int directory_index;
    int inode_number;
    int descriptor = -1;

    if (!filesystem_ready || !valid_name(name)) {
        return -1;
    }

    directory_index = find_directory_entry(name);

    if (directory_index < 0) {
        int free_directory_index = -1;

        if ((flags & FS_O_CREATE) == 0) {
            return -1;
        }

        for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
            if (directory[i].name[0] == '\0') {
                free_directory_index = (int)i;
                break;
            }
        }

        if (free_directory_index < 0) {
            return -1;
        }

        inode_number = allocate_inode();
        if (inode_number < 0) {
            return -1;
        }

        directory_index = free_directory_index;
        copy_name(directory[directory_index].name, name);
        directory[directory_index].inode_number = (uint32_t)inode_number;
    } else {
        inode_number = (int)directory[directory_index].inode_number;
    }

    for (uint32_t i = 0; i < FS_MAX_OPEN_FILES; i++) {
        if (!open_files[i].used) {
            descriptor = (int)i;
            break;
        }
    }

    if (descriptor < 0) {
        return -1;
    }

    if ((flags & FS_O_TRUNC) != 0) {
        release_inode_blocks(&inode_table[inode_number]);
    }

    open_files[descriptor].used = true;
    open_files[descriptor].inode_number = (uint32_t)inode_number;
    open_files[descriptor].offset = 0;
    open_files[descriptor].flags = flags;

    return descriptor;
}

int fs_read(int descriptor, void *buffer, uint32_t count)
{
    open_file_t *file;
    inode_t *inode;
    uint8_t *output = (uint8_t *)buffer;
    uint32_t bytes_read = 0;

    if (descriptor < 0 || descriptor >= (int)FS_MAX_OPEN_FILES ||
        buffer == NULL || !open_files[descriptor].used ||
        (open_files[descriptor].flags & FS_O_READ) == 0) {
        return -1;
    }

    file = &open_files[descriptor];
    inode = &inode_table[file->inode_number];

    while (bytes_read < count && file->offset < inode->size) {
        uint32_t direct_index = file->offset / RAMDISK_BLOCK_SIZE;
        uint32_t block_offset = file->offset % RAMDISK_BLOCK_SIZE;
        uint8_t *block;

        if (direct_index >= FS_DIRECT_BLOCKS || inode->direct[direct_index] == 0) {
            break;
        }

        block = (uint8_t *)ramdisk_get_block(inode->direct[direct_index]);
        output[bytes_read++] = block[block_offset];
        file->offset++;
    }

    return (int)bytes_read;
}

int fs_write(int descriptor, const void *buffer, uint32_t count)
{
    open_file_t *file;
    inode_t *inode;
    const uint8_t *input = (const uint8_t *)buffer;
    uint32_t bytes_written = 0;

    if (descriptor < 0 || descriptor >= (int)FS_MAX_OPEN_FILES ||
        buffer == NULL || !open_files[descriptor].used ||
        (open_files[descriptor].flags & FS_O_WRITE) == 0) {
        return -1;
    }

    file = &open_files[descriptor];
    inode = &inode_table[file->inode_number];

    while (bytes_written < count && file->offset < FS_MAX_FILE_SIZE) {
        uint32_t direct_index = file->offset / RAMDISK_BLOCK_SIZE;
        uint32_t block_offset = file->offset % RAMDISK_BLOCK_SIZE;
        uint8_t *block;

        if (inode->direct[direct_index] == 0) {
            int new_block = allocate_block();

            if (new_block < 0) {
                break;
            }

            inode->direct[direct_index] = (uint32_t)new_block;
        }

        block = (uint8_t *)ramdisk_get_block(inode->direct[direct_index]);
        block[block_offset] = input[bytes_written++];
        file->offset++;

        if (file->offset > inode->size) {
            inode->size = file->offset;
        }
    }

    return (int)bytes_written;
}

int fs_close(int descriptor)
{
    if (descriptor < 0 || descriptor >= (int)FS_MAX_OPEN_FILES ||
        !open_files[descriptor].used) {
        return -1;
    }

    clear_bytes(&open_files[descriptor], sizeof(open_file_t));
    return 0;
}

int fs_unlink(const char *name)
{
    int directory_index;
    uint32_t inode_number;

    if (!filesystem_ready || !valid_name(name)) {
        return -1;
    }

    directory_index = find_directory_entry(name);
    if (directory_index < 0) {
        return -1;
    }

    inode_number = directory[directory_index].inode_number;

    for (uint32_t i = 0; i < FS_MAX_OPEN_FILES; i++) {
        if (open_files[i].used && open_files[i].inode_number == inode_number) {
            clear_bytes(&open_files[i], sizeof(open_file_t));
        }
    }

    release_inode_blocks(&inode_table[inode_number]);
    clear_bytes(&inode_table[inode_number], sizeof(inode_t));
    bitmap_clear(inode_bitmap, inode_number);
    superblock->free_inodes++;
    clear_bytes(&directory[directory_index], sizeof(directory_entry_t));

    return 0;
}

uint32_t fs_list(fs_file_info_t *files, uint32_t capacity)
{
    uint32_t count = 0;

    if (!filesystem_ready || files == NULL) {
        return 0;
    }

    for (uint32_t i = 0; i < FS_MAX_FILES && count < capacity; i++) {
        if (directory[i].name[0] == '\0') {
            continue;
        }

        copy_name(files[count].name, directory[i].name);
        files[count].size = inode_table[directory[i].inode_number].size;
        count++;
    }

    return count;
}
