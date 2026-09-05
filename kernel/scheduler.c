#include "vga.h"
#include "../include/types.h"


/* ============================================================
 * PIT
 * ============================================================ */

#define PIT_COMMAND     0x43
#define PIT_CHANNEL0    0x40

#define PIT_FREQUENCY   1193180
#define TIMER_FREQUENCY 100

/* ============================================================
 * PIC
 * ============================================================ */

#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

/* ============================================================
 * IDT
 * ============================================================ */

#define IDT_ENTRIES 256

typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  always_zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_pointer_t;

static idt_entry_t idt[IDT_ENTRIES];
static idt_pointer_t idt_pointer;

extern void irq0_stub(void);

/* ============================================================
 * Process information
 * ============================================================ */

typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_TERMINATED
} process_state_t;

typedef struct {
    uint32_t pid;
    process_state_t state;
    uint32_t esp;
    void (*entry_point)(void);
} pcb_t;

extern pcb_t *process_get_table(void);
extern uint32_t process_get_count(void);

/* ============================================================
 * Thread information
 * ============================================================ */

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef struct {
    uint32_t tid;
    thread_state_t state;
    uint32_t esp;
    void (*entry_point)(void *);
    void *arg;
} thread_t;

extern thread_t *thread_get_table(void);
extern uint32_t thread_get_count(void);

/* ============================================================
 * Port I/O
 * ============================================================ */

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ __volatile__(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

/* ============================================================
 * IDT
 * ============================================================ */

static void idt_set_gate(uint8_t number, uint32_t handler)
{
    idt[number].base_low = (uint16_t)(handler & 0xFFFF);
    idt[number].selector = 0x08;
    idt[number].always_zero = 0;
    idt[number].flags = 0x8E;
    idt[number].base_high =
        (uint16_t)((handler >> 16) & 0xFFFF);
}

static void idt_init(void)
{
    uint32_t i;

    for (i = 0; i < IDT_ENTRIES; i++) {
        idt[i].base_low = 0;
        idt[i].selector = 0;
        idt[i].always_zero = 0;
        idt[i].flags = 0;
        idt[i].base_high = 0;
    }

    idt_set_gate(0x20, (uint32_t)irq0_stub);

    idt_pointer.limit = (uint16_t)(sizeof(idt) - 1);
    idt_pointer.base = (uint32_t)&idt;

    __asm__ __volatile__(
        "lidtl (%0)"
        :
        : "r"(&idt_pointer)
    );
}

/* ============================================================
 * PIC initialization
 * ============================================================ */

static void pic_init(void)
{
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    /* Enable only IRQ0 on the master PIC */
    outb(PIC1_DATA, 0xFE);

    /* Disable all IRQs on the slave PIC */
    outb(PIC2_DATA, 0xFF);
}

/* ============================================================
 * PIT initialization
 * ============================================================ */

void pit_init(void)
{
    uint16_t divisor;

    divisor = PIT_FREQUENCY / TIMER_FREQUENCY;

    outb(PIT_COMMAND, 0x36);

    outb(PIT_CHANNEL0,
         (uint8_t)(divisor & 0xFF));

    outb(PIT_CHANNEL0,
         (uint8_t)((divisor >> 8) & 0xFF));
}

/* ============================================================
 * Scheduler state
 * ============================================================ */

typedef enum {
    SCHEDULE_PROCESS,
    SCHEDULE_THREAD
} schedule_type_t;

static schedule_type_t current_type = SCHEDULE_PROCESS;

static uint32_t current_pid = 0;
static uint32_t current_tid = 0;

static uint32_t tick_count = 0;

/*
 * The first timer interrupt happens while the kernel/shell
 * is still running.
 *
 * Therefore we must NOT save the kernel stack as process 0's
 * stack. Process 0 already has a prepared stack.
 */
static bool first_schedule = true;

/* ============================================================
 * Scheduler interrupt
 * ============================================================ */

uint32_t scheduler_irq(uint32_t current_esp)
{
    pcb_t *process_table;
    thread_t *thread_table;

    uint32_t process_count;
    uint32_t thread_count;

    tick_count++;

    process_table = process_get_table();
    process_count = process_get_count();

    thread_table = thread_get_table();
    thread_count = thread_get_count();

    /* ========================================================
     * FIRST SCHEDULE
     * ======================================================== */

    if (first_schedule) {

        first_schedule = false;

        /*
         * Start with process 0.
         */
        if (process_count > 0) {

            current_type = SCHEDULE_PROCESS;
            current_pid = 0;

            process_table[0].state =
                PROCESS_RUNNING;

            return process_table[0].esp;
        }

        /*
         * If there are no processes, start with thread 0.
         */
        if (thread_count > 0) {

            current_type = SCHEDULE_THREAD;
            current_tid = 0;

            thread_table[0].state =
                THREAD_RUNNING;

            vga_putchar('T');

            return thread_table[0].esp;
        }

        return current_esp;
    }

    /* ========================================================
     * SAVE CURRENT CONTEXT
     * ======================================================== */

    if (current_type == SCHEDULE_PROCESS) {

        if (process_count > 0) {

            /*
             * Save the stack pointer created by pushad.
             */
            process_table[current_pid].esp =
                current_esp;

            /*
             * Mark the process ready again.
             */
            if (process_table[current_pid].state ==
                PROCESS_RUNNING) {

                process_table[current_pid].state =
                    PROCESS_READY;
            }
        }

    } else {

        if (thread_count > 0) {

            /*
             * Save the current thread's stack.
             */
            thread_table[current_tid].esp =
                current_esp;

            /*
             * Mark it ready again.
             */
            if (thread_table[current_tid].state ==
                THREAD_RUNNING) {

                thread_table[current_tid].state =
                    THREAD_READY;
            }
        }
    }

    /* ========================================================
     * PROCESS SCHEDULING
     *
     * Order:
     *
     *   Process 0
     *       ↓
     *   Process 1
     *       ↓
     *   Thread 0
     *       ↓
     *   Thread 1
     *       ↓
     *   Process 0
     *       ↓
     *      ...
     * ======================================================== */

    if (current_type == SCHEDULE_PROCESS) {

        /*
         * First try the next process.
         */
        if (current_pid + 1 < process_count) {

            current_pid++;

            process_table[current_pid].state =
                PROCESS_RUNNING;

            return process_table[current_pid].esp;
        }

        /*
         * No more processes.
         * Move to the first thread.
         */
        if (thread_count > 0) {

            current_type = SCHEDULE_THREAD;
            current_tid = 0;

            thread_table[0].state =
                THREAD_RUNNING;

            vga_putchar('T');

            return thread_table[0].esp;
        }

        /*
         * No threads.
         * Return to process 0.
         */
        if (process_count > 0) {

            current_pid = 0;

            process_table[0].state =
                PROCESS_RUNNING;

            return process_table[0].esp;
        }
    }

    /* ========================================================
     * THREAD SCHEDULING
     *
     * Thread 0 → Thread 1 → Process 0
     * ======================================================== */

    else {

        /*
         * Move to the next thread.
         */
        if (current_tid + 1 < thread_count) {

            current_tid++;

            thread_table[current_tid].state =
                THREAD_RUNNING;

            return thread_table[current_tid].esp;
        }

        /*
         * No more threads.
         * Return to process 0.
         */
        if (process_count > 0) {

            current_type = SCHEDULE_PROCESS;
            current_pid = 0;

            process_table[0].state =
                PROCESS_RUNNING;

            return process_table[0].esp;
        }

        /*
         * If there are no processes, restart at thread 0.
         */
        if (thread_count > 0) {

            current_tid = 0;

            thread_table[0].state =
                THREAD_RUNNING;


            return thread_table[0].esp;
        }
    }

    return current_esp;
}

/* ============================================================
 * Scheduler initialization
 * ============================================================ */

void scheduler_init(void)
{
    current_type = SCHEDULE_PROCESS;

    current_pid = 0;
    current_tid = 0;

    tick_count = 0;

    first_schedule = true;

    idt_init();
    pic_init();
    pit_init();

    /*
     * Enable hardware interrupts.
     */
    __asm__ __volatile__("sti");
}

/* ============================================================
 * Information functions
 * ============================================================ */

uint32_t scheduler_current_pid(void)
{
    return current_pid;
}

uint32_t scheduler_tick_count(void)
{
    return tick_count;
}
uint32_t scheduler_current_tid(void)
{
    return current_tid;
}