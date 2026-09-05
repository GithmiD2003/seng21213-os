#include "thread.h"

extern void thread_start_trampoline(void);

static thread_t thread_table[MAX_THREADS];
static uint8_t thread_stacks[MAX_THREADS][THREAD_STACK_SIZE];
static uint32_t thread_count = 0;


/*
 * Called by thread_start_trampoline().
 */
void thread_start(thread_t *thread)
{
    if (thread == NULL || thread->entry_point == NULL) {
        while (1) {
            __asm__ __volatile__("cli");
            __asm__ __volatile__("hlt");
        }
    }

    thread->entry_point(thread->arg);

    thread->state = THREAD_TERMINATED;

    while (1) {
        __asm__ __volatile__("cli");
        __asm__ __volatile__("hlt");
    }
}


int thread_create(void (*entry_point)(void *), void *arg)
{
    if (thread_count >= MAX_THREADS || entry_point == NULL) {
        return -1;
    }

    uint32_t tid = thread_count;

    uint32_t *stack =
        (uint32_t *)&thread_stacks[tid][THREAD_STACK_SIZE];

    thread_table[tid].tid = tid;
    thread_table[tid].state = THREAD_READY;
    thread_table[tid].esp = 0;
    thread_table[tid].entry_point = entry_point;
    thread_table[tid].arg = arg;


    /*
     * IMPORTANT:
     *
     * irq0_stub does:
     *
     *     popad
     *     iretd
     *
     * Therefore the stack MUST be:
     *
     *   EDI
     *   ESI
     *   EBP
     *   ESP
     *   EBX
     *   EDX
     *   ECX
     *   EAX
     *   EIP
     *   CS
     *   EFLAGS
     *
     * Because we build the stack backwards, we PUSH
     * these values in the reverse order.
     */


    /* IRET frame -- pushed first */

    *--stack = 0x202;       /* EFLAGS */
    *--stack = 0x08;        /* CS */
    *--stack = (uint32_t)thread_start_trampoline; /* EIP */


    /*
     * POPAD frame.
     *
     * Push in reverse order so final memory layout
     * starts with EDI.
     */

    *--stack = (uint32_t)&thread_table[tid]; /* EAX */
    *--stack = 0;                             /* ECX */
    *--stack = 0;                             /* EDX */
    *--stack = 0;                             /* EBX */
    *--stack = 0;                             /* ESP - ignored */
    *--stack = 0;                             /* EBP */
    *--stack = 0;                             /* ESI */
    *--stack = 0;                             /* EDI */


    /*
     * Save the initial stack pointer.
     */
    thread_table[tid].esp = (uint32_t)stack;

    thread_count++;

    return (int)tid;
}


thread_t *thread_get_table(void)
{
    return thread_table;
}


uint32_t thread_get_count(void)
{
    return thread_count;
}