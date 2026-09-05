#include "../include/types.h"

/* ============================================================
 * PIT (Programmable Interval Timer)
 * ============================================================ */

#define PIT_COMMAND     0x43
#define PIT_CHANNEL0    0x40

#define PIT_FREQUENCY   1193180
#define TIMER_FREQUENCY 100

/* ============================================================
 * PIC (Programmable Interrupt Controller)
 * ============================================================ */

#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

#define PIC_EOI         0x20

/* ============================================================
 * IDT
 * ============================================================ */

#define IDT_ENTRIES     256

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

/* IRQ0 handler from boot/switch.asm */
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
 * IDT functions
 * ============================================================ */

static void idt_set_gate(
    uint8_t number,
    uint32_t handler
)
{
    idt[number].base_low =
        (uint16_t)(handler & 0xFFFF);

    idt[number].selector = 0x08;
    idt[number].always_zero = 0;

    /* Present + ring 0 + 32-bit interrupt gate */
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

    idt_pointer.limit =
        (uint16_t)(sizeof(idt) - 1);

    idt_pointer.base =
        (uint32_t)&idt;

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
    /*
     * Start PIC initialization sequence.
     */
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    /*
     * Remap IRQs:
     *
     * Master PIC: IRQ0-7  -> INT 20h-27h
     * Slave PIC : IRQ8-15 -> INT 28h-2Fh
     */
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    /*
     * Tell the PICs how they are connected.
     */
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    /*
     * 8086/88 mode.
     */
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    /*
     * Enable only IRQ0 on the master PIC.
     * Mask all other hardware interrupts.
     */
    outb(PIC1_DATA, 0xFE);

    /* Mask all slave PIC interrupts. */
    outb(PIC2_DATA, 0xFF);
}

/* ============================================================
 * PIT initialization
 * ============================================================ */

void pit_init(void)
{
    uint16_t divisor;

    divisor = PIT_FREQUENCY / TIMER_FREQUENCY;

    /*
     * Channel 0
     * Low byte + high byte
     * Mode 3 (square wave)
     */
    outb(PIT_COMMAND, 0x36);

    outb(
        PIT_CHANNEL0,
        (uint8_t)(divisor & 0xFF)
    );

    outb(
        PIT_CHANNEL0,
        (uint8_t)((divisor >> 8) & 0xFF)
    );
}

/* ============================================================
 * Scheduler state
 * ============================================================ */

static uint32_t current_pid = 0;
static uint32_t tick_count = 0;

/*
 * Called by irq0_stub().
 *
 * current_esp points to the register frame created by PUSHAD.
 *
 * Returns the ESP of the next process.
 */
uint32_t scheduler_irq(uint32_t current_esp)
{
    pcb_t *table;
    uint32_t count;
    uint32_t next_pid;

    tick_count++;

    table = process_get_table();
    count = process_get_count();

    /*
     * No processes to schedule.
     */
    if (count == 0) {
        return current_esp;
    }

    /*
     * Save the current process context.
     */
    table[current_pid].esp = current_esp;

    /*
     * Mark the current process READY.
     */
    if (table[current_pid].state == PROCESS_RUNNING) {
        table[current_pid].state = PROCESS_READY;
    }

    /*
     * Find the next process using Round-Robin.
     */
    next_pid = current_pid;

    do {
        next_pid++;

        if (next_pid >= count) {
            next_pid = 0;
        }

        /*
         * Stop when we find a process that can run.
         */
        if (table[next_pid].state != PROCESS_TERMINATED) {
            break;
        }

    } while (next_pid != current_pid);

    current_pid = next_pid;
    table[current_pid].state = PROCESS_RUNNING;

    /*
     * Return the next process's saved stack.
     */
    return table[current_pid].esp;
}

/* ============================================================
 * Scheduler initialization
 * ============================================================ */

void scheduler_init(void)
{
    current_pid = 0;
    tick_count = 0;

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