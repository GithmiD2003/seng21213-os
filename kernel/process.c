#include "../include/types.h"

/* Process states */
typedef enum {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_TERMINATED
} process_state_t;

/* Process Control Block (PCB) */
typedef struct {
    uint32_t pid;
    process_state_t state;
    uint32_t esp;
    void (*entry_point)(void);
} pcb_t;

/* Maximum number of processes */
#define MAX_PROCESSES 8

/* Process table */
static pcb_t process_table[MAX_PROCESSES];

/* 4 KB stack for each process */
static uint8_t process_stacks[MAX_PROCESSES][4096];

/* Number of processes currently created */
static uint32_t process_count = 0;

/*
 * Create a new process.
 *
 * The stack is prepared so that when the scheduler
 * restores it, POPAD + IRETD will start the process
 * at entry_fn().
 */
int create_process(void (*entry_fn)(void))
{
    if (process_count >= MAX_PROCESSES || entry_fn == NULL) {
        return -1;
    }

    uint32_t pid = process_count;

    /* Start at the top of the 4 KB stack. */
    uint32_t *stack =
        (uint32_t *)&process_stacks[pid][4096];

    /*
     * Build the stack in reverse order.
     *
     * IRETD will eventually consume:
     *   EIP
     *   CS
     *   EFLAGS
     *
     * POPAD will consume the 8 values before them.
     */

    /* IRET frame */
    *--stack = 0x202;              /* EFLAGS: interrupts enabled */
    *--stack = 0x08;               /* CS: kernel code segment */
    *--stack = (uint32_t)entry_fn; /* EIP: process entry point */

    /* POPAD frame */
    *--stack = 0;  /* EDI */
    *--stack = 0;  /* ESI */
    *--stack = 0;  /* EBP */
    *--stack = 0;  /* ESP (ignored by POPAD) */
    *--stack = 0;  /* EBX */
    *--stack = 0;  /* EDX */
    *--stack = 0;  /* ECX */
    *--stack = 0;  /* EAX */

    process_table[pid].pid = pid;
    process_table[pid].state = PROCESS_READY;
    process_table[pid].esp = (uint32_t)stack;
    process_table[pid].entry_point = entry_fn;

    process_count++;

    return (int)pid;
}

/*
 * Return the process table.
 * Used by the scheduler and ps command.
 */
pcb_t *process_get_table(void)
{
    return process_table;
}

/*
 * Return the number of created processes.
 */
uint32_t process_get_count(void)
{
    return process_count;
}