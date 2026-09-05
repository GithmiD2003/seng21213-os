#include "semaphore.h"
#include "thread.h"

extern uint32_t scheduler_current_tid(void);

void sem_init(semaphore_t *sem, int value)
{
    uint32_t i;

    if (sem == NULL) {
        return;
    }

    sem->count = value;
    sem->wait_head = 0;
    sem->wait_tail = 0;
    sem->wait_count = 0;

    for (i = 0; i < SEM_MAX_WAITERS; i++) {
        sem->wait_queue[i] = 0;
    }
}

void sem_wait(semaphore_t *sem)
{
    thread_t *threads;
    uint32_t tid;

    if (sem == NULL) {
        return;
    }

    threads = thread_get_table();
    tid = scheduler_current_tid();

    while (1) {

        /*
         * Disable interrupts while checking and modifying
         * the semaphore count.
         */
        __asm__ __volatile__("cli");

        if (sem->count > 0) {
            sem->count--;

            __asm__ __volatile__("sti");
            return;
        }

        /*
         * No resource is available.
         * Put the current thread into the wait queue.
         */
        if (sem->wait_count < SEM_MAX_WAITERS) {

            sem->wait_queue[sem->wait_tail] = tid;

            sem->wait_tail =
                (sem->wait_tail + 1) % SEM_MAX_WAITERS;

            sem->wait_count++;

            threads[tid].state = THREAD_BLOCKED;

            __asm__ __volatile__("sti");

            /*
             * Sleep until sem_signal() wakes this thread.
             */
            while (threads[tid].state == THREAD_BLOCKED) {
                __asm__ __volatile__("hlt");
            }

            /*
             * We were woken.
             * Try sem_wait() again.
             */

        } else {

            /*
             * Safety fallback.
             */
            __asm__ __volatile__("sti");

            while (sem->count == 0) {
                __asm__ __volatile__("hlt");
            }
        }
    }
}

void sem_signal(semaphore_t *sem)
{
    thread_t *threads;
    uint32_t tid;

    if (sem == NULL) {
        return;
    }

    threads = thread_get_table();

    __asm__ __volatile__("cli");

    sem->count++;

    /*
     * Wake one waiting thread.
     */
    if (sem->wait_count > 0) {

        tid = sem->wait_queue[sem->wait_head];

        sem->wait_head =
            (sem->wait_head + 1) % SEM_MAX_WAITERS;

        sem->wait_count--;

        if (tid < thread_get_count()) {
            threads[tid].state = THREAD_READY;
        }
    }

    __asm__ __volatile__("sti");
}