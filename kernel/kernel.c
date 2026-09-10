/* =============================================================================
 * SENG21213-OS :: Main Kernel
 * File   : kernel/kernel.c
 *
 * Stage 4:
 *   - 1 MB RAM disk backed by physical frames
 *   - Superblock, bitmaps, inodes and flat directory
 *   - POSIX-inspired file API
 *   - File-system shell commands
 * =============================================================================*/

#include "vga.h"
#include "keyboard.h"
#include "thread.h"
#include "../include/types.h"
#include "mutex.h"
#include "semaphore.h"
#include "pmm.h"
#include "ramdisk.h"
#include "fs.h"

/* ---------------------------------------------------------------------------
 * Process and scheduler functions
 * --------------------------------------------------------------------------*/
extern int create_process(void (*entry_fn)(void));
extern void scheduler_init(void);

/* ---------------------------------------------------------------------------
 * Shell command declarations
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_meminfo(void);
static void cmd_ls(void);
static void cmd_touch(const char *args);
static void cmd_cat(const char *args);
static void cmd_write(const char *args);
static void cmd_rm(const char *args);

static bool filesystem_ready;

/* ---------------------------------------------------------------------------
 * String utilities
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }

    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n)
{
    while (n-- && *a && (*a == *b)) {
        a++;
        b++;
    }

    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s)
{
    size_t n = 0;

    while (s[n]) {
        n++;
    }

    return n;
}

static const char *k_ltrim(const char *s)
{
    while (*s == ' ') {
        s++;
    }

    return s;
}

/* ---------------------------------------------------------------------------
 * Splash screen
 * --------------------------------------------------------------------------*/
static void print_splash(void)
{
    vga_clear(VGA_BLACK);

    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color(
        "  SENG21213-OS  |  Computer Architecture & Operating Systems",
        VGA_YELLOW,
        VGA_BLACK
    );

    vga_set_cursor(2, 2);
    vga_puts_color(
        "  Stage 4: RAM Disk File System",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_set_cursor(3, 2);
    vga_puts_color(
        "  Faculty of Engineering - Department of Software Engineering",
        VGA_LIGHT_GREY,
        VGA_BLACK
    );

    vga_set_cursor(4, 2);
    vga_puts_color(
        "  Inodes, directories and POSIX-inspired file operations",
        VGA_LIGHT_GREEN,
        VGA_BLACK
    );

    vga_set_cursor(5, 2);
    vga_puts_color(
        "  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
        VGA_DARK_GREY,
        VGA_BLACK
    );

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    vga_puts(
        "  Welcome! This kernel was compiled from source and booted entirely\n"
    );

    vga_puts(
        "  from bare metal. There is no Linux or Windows underneath - only\n"
    );

    vga_puts(
        "  the code you and your team write.\n"
    );

    vga_puts("\n");

    vga_puts("  Stage 4 components:\n");

    vga_puts_color(
        "    [OK] ",
        VGA_LIGHT_GREEN,
        VGA_BLACK
    );

    vga_puts(
        "1 MB frame-backed RAM disk\n"
    );

    vga_puts_color(
        "    [OK] ",
        VGA_LIGHT_GREEN,
        VGA_BLACK
    );

    vga_puts(
        "Superblock and allocation bitmaps\n"
    );

    vga_puts_color(
        "    [OK] ",
        VGA_LIGHT_GREEN,
        VGA_BLACK
    );

    vga_puts(
        "Inodes and flat directory\n"
    );

    vga_puts_color(
        "    [OK] ",
        VGA_LIGHT_GREEN,
        VGA_BLACK
    );

    vga_puts(
        "open/read/write/close/unlink API\n"
    );

    vga_puts_color(
        "    [OK] ",
        VGA_LIGHT_GREEN,
        VGA_BLACK
    );

    vga_puts(
        "ls/touch/cat/write/rm commands\n"
    );

    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void)
{
    vga_puts_color(
        "\n  SENG21213-OS Shell Commands\n",
        VGA_YELLOW,
        VGA_BLACK
    );

    vga_puts(
        "  ---------------------------------------------\n"
    );

    vga_puts(
        "  help    - Show this help message\n"
    );

    vga_puts(
        "  clear   - Clear the screen\n"
    );

    vga_puts(
        "  about   - About this OS and course\n"
    );

    vga_puts(
        "  echo    - Echo text to screen\n"
    );

    vga_puts(
        "  mem     - Memory map\n"
    );

    vga_puts(
        "  meminfo - Physical frame allocator statistics\n"
    );

    vga_puts("  ls      - List files\n");
    vga_puts("  touch   - Create a file: touch <name>\n");
    vga_puts("  cat     - Read a file: cat <name>\n");
    vga_puts("  write   - Write a file: write <name> <text>\n");
    vga_puts("  rm      - Delete a file: rm <name>\n");

    vga_puts("\n");
}

static void cmd_clear(void)
{
    vga_clear(VGA_BLACK);
}

static void cmd_about(void)
{
    vga_puts_color(
        "\n  About SENG21213-OS\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(
        "  ---------------------------------------------\n"
    );

    vga_puts(
        "  Architecture : x86 (i686), 32-bit Protected Mode\n"
    );

    vga_puts(
        "  Bootloader   : Custom MBR (NASM)\n"
    );

    vga_puts(
        "  Kernel       : Freestanding C (GCC, no libc)\n"
    );

    vga_puts(
        "  VM Target    : QEMU (qemu-system-i386)\n"
    );

    vga_puts(
        "  Course       : SENG 21213 - Sem 2\n"
    );

    vga_puts(
        "  Stage        : Stage 4 - RAM Disk File System\n\n"
    );
}

static void cmd_echo(const char *args)
{
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void)
{
    vga_puts_color(
        "\n  Memory Map\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(
        "  ---------------------------------------------\n"
    );

    vga_puts(
        "  0x00000000 - 0x000FFFFF  : First 1 MB\n"
    );

    vga_puts(
        "  0x00100000 - 0x00EFFFFF  : Extended memory\n"
    );

    vga_puts(
        "  0x00F00000 - 0x00FFFFFF  : BIOS / ROM area\n"
    );

    vga_puts(
        "  0xB8000    - 0xBFFFF     : VGA frame buffer\n\n"
    );
}

static void cmd_meminfo(void)
{
    uint32_t total = pmm_get_total_frames();
    uint32_t used = pmm_get_used_frames();
    uint32_t free = pmm_get_free_frames();

    vga_puts_color(
        "\n  Physical Memory Information\n",
        VGA_LIGHT_CYAN,
        VGA_BLACK
    );

    vga_puts(
        "  ---------------------------------------------\n"
    );

    vga_printf("  BIOS E820 entries : %u\n", pmm_get_e820_entry_count());
    vga_printf("  Frame size        : %u bytes\n", PMM_FRAME_SIZE);
    vga_printf("  Total frames      : %u (%u MiB)\n", total, total / 256U);
    vga_printf("  Used frames       : %u\n", used);
    vga_printf("  Free frames       : %u\n\n", free);
}

static bool parse_filename(const char *args, char *name, const char **remaining)
{
    uint32_t length = 0;

    args = k_ltrim(args);

    while (args[length] != '\0' && args[length] != ' ') {
        if (length >= FS_MAX_NAME) {
            return false;
        }
        name[length] = args[length];
        length++;
    }

    if (length == 0) {
        return false;
    }

    name[length] = '\0';
    *remaining = k_ltrim(args + length);
    return true;
}

static void cmd_ls(void)
{
    static fs_file_info_t files[FS_MAX_FILES];
    uint32_t count;

    if (!filesystem_ready) {
        vga_puts("  File system is unavailable.\n");
        return;
    }

    count = fs_list(files, FS_MAX_FILES);
    vga_puts_color("\n  Files\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ---------------------------------------------\n");

    if (count == 0) {
        vga_puts("  (empty)\n\n");
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        vga_puts("  ");
        vga_puts(files[i].name);
        vga_printf("  %u bytes\n", files[i].size);
    }

    vga_puts("\n");
}

static void cmd_touch(const char *args)
{
    char name[FS_MAX_NAME + 1U];
    const char *remaining;
    int descriptor;

    if (!parse_filename(args, name, &remaining) || *remaining != '\0') {
        vga_puts("  Usage: touch <name>\n");
        return;
    }

    descriptor = fs_open(name, FS_O_WRITE | FS_O_CREATE);
    if (descriptor < 0) {
        vga_puts("  touch: could not create file\n");
        return;
    }

    fs_close(descriptor);
    vga_puts("  File created: ");
    vga_puts(name);
    vga_puts("\n");
}

static void cmd_cat(const char *args)
{
    char name[FS_MAX_NAME + 1U];
    char buffer[128];
    const char *remaining;
    int descriptor;
    int bytes_read;

    if (!parse_filename(args, name, &remaining) || *remaining != '\0') {
        vga_puts("  Usage: cat <name>\n");
        return;
    }

    descriptor = fs_open(name, FS_O_READ);
    if (descriptor < 0) {
        vga_puts("  cat: file not found\n");
        return;
    }

    vga_puts("  ");
    do {
        bytes_read = fs_read(descriptor, buffer, sizeof(buffer));
        for (int i = 0; i < bytes_read; i++) {
            vga_putchar(buffer[i]);
        }
    } while (bytes_read > 0);

    fs_close(descriptor);
    vga_puts("\n");
}

static void cmd_write(const char *args)
{
    char name[FS_MAX_NAME + 1U];
    const char *text;
    int descriptor;
    int written;

    if (!parse_filename(args, name, &text) || *text == '\0') {
        vga_puts("  Usage: write <name> <text>\n");
        return;
    }

    descriptor = fs_open(name, FS_O_WRITE | FS_O_CREATE | FS_O_TRUNC);
    if (descriptor < 0) {
        vga_puts("  write: could not open file\n");
        return;
    }

    written = fs_write(descriptor, text, (uint32_t)k_strlen(text));
    fs_close(descriptor);

    if (written < 0) {
        vga_puts("  write: operation failed\n");
        return;
    }

    vga_printf("  Wrote %u bytes to %s\n", (uint32_t)written, name);
}

static void cmd_rm(const char *args)
{
    char name[FS_MAX_NAME + 1U];
    const char *remaining;

    if (!parse_filename(args, name, &remaining) || *remaining != '\0') {
        vga_puts("  Usage: rm <name>\n");
        return;
    }

    if (fs_unlink(name) != 0) {
        vga_puts("  rm: file not found\n");
        return;
    }

    vga_puts("  File removed: ");
    vga_puts(name);
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell
 * --------------------------------------------------------------------------*/
static char shell_buf[256];
static char prompt[] = "\n  ksh> ";

static void shell_run(void)
{
    vga_puts_color(
        "\n  Kernel Shell ready. Type 'help' for commands.\n",
        VGA_LIGHT_GREEN,
        VGA_BLACK
    );

    while (true) {

        vga_puts_color(
            prompt,
            VGA_LIGHT_GREEN,
            VGA_BLACK
        );

        kb_readline(
            shell_buf,
            sizeof(shell_buf)
        );

        const char *cmd = k_ltrim(shell_buf);

        if (k_strlen(cmd) == 0) {
            continue;
        }

        if (k_strcmp(cmd, "help") == 0) {
            cmd_help();
            continue;
        }

        if (k_strcmp(cmd, "clear") == 0) {
            cmd_clear();
            continue;
        }

        if (k_strcmp(cmd, "about") == 0) {
            cmd_about();
            continue;
        }

        if (k_strcmp(cmd, "mem") == 0) {
            cmd_mem();
            continue;
        }

        if (k_strcmp(cmd, "meminfo") == 0) {
            cmd_meminfo();
            continue;
        }

        if (k_strcmp(cmd, "ls") == 0) {
            cmd_ls();
            continue;
        }

        if (k_strncmp(cmd, "touch ", 6) == 0) {
            cmd_touch(cmd + 6);
            continue;
        }

        if (k_strncmp(cmd, "cat ", 4) == 0) {
            cmd_cat(cmd + 4);
            continue;
        }

        if (k_strncmp(cmd, "write ", 6) == 0) {
            cmd_write(cmd + 6);
            continue;
        }

        if (k_strncmp(cmd, "rm ", 3) == 0) {
            cmd_rm(cmd + 3);
            continue;
        }

        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        vga_puts_color(
            "  Unknown command: ",
            VGA_LIGHT_RED,
            VGA_BLACK
        );

        vga_puts(cmd);

        vga_puts(
            "\n  Type 'help' for a list of commands.\n"
        );
    }
}

/* =============================================================================
 * STAGE 1 - PROCESS TESTS
 * =============================================================================*/

void process_a(void)
{
    while (1) {

        vga_putchar('A');

        for (volatile uint32_t i = 0;
             i < 1000000;
             i++) {
        }
    }
}

void process_b(void)
{
    while (1) {

        vga_putchar('B');

        for (volatile uint32_t i = 0;
             i < 2000000;
             i++) {
        }
    }
}

/* =============================================================================
 * STAGE 2 - MUTEX TEST
 * =============================================================================*/

static mutex_t test_mutex;

void thread_a(void *arg)
{
    (void)arg;

    while (1) {

        mutex_lock(&test_mutex);

        vga_putchar('1');
        vga_putchar('1');
        vga_putchar('1');

        for (volatile uint32_t i = 0;
             i < 1000000;
             i++) {
        }

        mutex_unlock(&test_mutex);

        for (volatile uint32_t i = 0;
             i < 500000;
             i++) {
        }
    }
}

void thread_b(void *arg)
{
    (void)arg;

    while (1) {

        mutex_lock(&test_mutex);

        vga_putchar('2');
        vga_putchar('2');
        vga_putchar('2');

        for (volatile uint32_t i = 0;
             i < 1000000;
             i++) {
        }

        mutex_unlock(&test_mutex);

        for (volatile uint32_t i = 0;
             i < 500000;
             i++) {
        }
    }
}

/* =============================================================================
 * STAGE 2 - PRODUCER / CONSUMER
 * =============================================================================*/

#define BUFFER_SIZE 4

static int buffer[BUFFER_SIZE];

static int buffer_in = 0;
static int buffer_out = 0;

static semaphore_t empty_slots;
static semaphore_t full_slots;

static mutex_t buffer_mutex;

void producer(void *arg)
{
    int item = 1;

    (void)arg;

    while (1) {

        /*
         * Wait until an empty buffer slot is available.
         */
        sem_wait(&empty_slots);

        /*
         * Protect the shared buffer.
         */
        mutex_lock(&buffer_mutex);

        buffer[buffer_in] = item;

        vga_puts("[P");
        vga_putchar('0' + item);
        vga_puts("]");

        buffer_in =
            (buffer_in + 1) % BUFFER_SIZE;

        item++;

        if (item > 9) {
            item = 1;
        }

        mutex_unlock(&buffer_mutex);

        /*
         * Tell the consumer that an item is available.
         */
        sem_signal(&full_slots);

        for (volatile uint32_t i = 0;
             i < 500000;
             i++) {
        }
    }
}

void consumer(void *arg)
{
    int item;

    (void)arg;

    while (1) {

        /*
         * Wait until at least one item is available.
         */
        sem_wait(&full_slots);

        /*
         * Protect the shared buffer.
         */
        mutex_lock(&buffer_mutex);

        item = buffer[buffer_out];

        vga_puts("[C");
        vga_putchar('0' + item);
        vga_puts("]");

        buffer_out =
            (buffer_out + 1) % BUFFER_SIZE;

        mutex_unlock(&buffer_mutex);

        /*
         * Tell the producer that a buffer slot is free.
         */
        sem_signal(&empty_slots);

        for (volatile uint32_t i = 0;
             i < 800000;
             i++) {
        }
    }
}

/* =============================================================================
 * STAGE 2 - RACE CONDITION TEST
 * =============================================================================*/

static volatile int myglobal = 0;

static mutex_t global_mutex;

/*
 * Test WITHOUT mutex.
 *
 * Two threads modify the same global variable.
 */
void race_thread_a(void *arg)
{
    int i;

    (void)arg;

    for (i = 0; i < 10000; i++) {
        myglobal++;
    }
}

void race_thread_b(void *arg)
{
    int i;

    (void)arg;

    for (i = 0; i < 10000; i++) {
        myglobal++;
    }
}

/*
 * Test WITH mutex.
 *
 * Only one thread can modify myglobal at a time.
 */
void safe_thread_a(void *arg)
{
    int i;

    (void)arg;

    for (i = 0; i < 10000; i++) {

        mutex_lock(&global_mutex);

        myglobal++;

        mutex_unlock(&global_mutex);
    }
}

void safe_thread_b(void *arg)
{
    int i;

    (void)arg;

    for (i = 0; i < 10000; i++) {

        mutex_lock(&global_mutex);

        myglobal++;

        mutex_unlock(&global_mutex);
    }
}

/* =============================================================================
 * KERNEL MAIN
 * =============================================================================*/

void kernel_main(void)
{
    /* Initialise hardware */
    vga_init();
    kb_init();

    /* Build the Stage 3 frame bitmap from the bootloader's BIOS E820 map. */
    pmm_init();

    /* Allocate the 1 MB RAM disk and format the Stage 4 file system. */
    filesystem_ready = fs_init();

    /* Display startup information */
    print_splash();

    /* Earlier-stage implementations remain in the kernel. Their noisy demo
     * tasks are not auto-started, leaving the Stage 4 shell usable. */

    /* -----------------------------------------------------------------------
     * Start scheduler
     * ----------------------------------------------------------------------*/

    scheduler_init();

    /*
     * The shell remains available as the kernel's main execution context.
     */
    shell_run();

    __asm__ __volatile__("hlt");
}
