#include "mutex.h"
#include "thread.h"

extern uint32_t scheduler_current_tid(void);

void mutex_init(mutex_t *mutex)
{
    uint32_t i;

    if (mutex == NULL) {
        return;
    }

    mutex->locked = 0;
    mutex->wait_head = 0;
    mutex->wait_tail = 0;
    mutex->wait_count = 0;

    for (i = 0; i < MUTEX_MAX_WAITERS; i++) {
        mutex->wait_queue[i] = 0;
    }
}

void mutex_lock(mutex_t *mutex)
{
    thread_t *threads;
    uint32_t tid;

    if (mutex == NULL) {
        return;
    }

    threads = thread_get_table();
    tid = scheduler_current_tid();

    while (1) {

        /*
         * Disable interrupts while checking and changing
         * the mutex state.
         */
        __asm__ __volatile__("cli");

        if (mutex->locked == 0) {
            mutex->locked = 1;

            __asm__ __volatile__("sti");
            return;
        }

        /*
         * Mutex is already locked.
         * Put the current thread into the wait queue.
         */
        if (mutex->wait_count < MUTEX_MAX_WAITERS) {

            mutex->wait_queue[mutex->wait_tail] = tid;

            mutex->wait_tail =
                (mutex->wait_tail + 1) % MUTEX_MAX_WAITERS;

            mutex->wait_count++;

            threads[tid].state = THREAD_BLOCKED;

            __asm__ __volatile__("sti");

            /*
             * Sleep until the scheduler wakes this thread.
             */
            while (threads[tid].state == THREAD_BLOCKED) {
                __asm__ __volatile__("hlt");
            }

            /*
             * The thread was woken.
             * Try to acquire the mutex again.
             */
        } else {

            /*
             * Queue should never become full with MAX_THREADS = 8,
             * but fall back to sleeping if it does.
             */
            __asm__ __volatile__("sti");

            while (mutex->locked) {
                __asm__ __volatile__("hlt");
            }
        }
    }
}

void mutex_unlock(mutex_t *mutex)
{
    thread_t *threads;
    uint32_t tid;

    if (mutex == NULL) {
        return;
    }

    threads = thread_get_table();

    __asm__ __volatile__("cli");

    if (mutex->locked == 0) {
        __asm__ __volatile__("sti");
        return;
    }

    mutex->locked = 0;

    /*
     * Wake one waiting thread.
     */
    if (mutex->wait_count > 0) {

        tid = mutex->wait_queue[mutex->wait_head];

        mutex->wait_head =
            (mutex->wait_head + 1) % MUTEX_MAX_WAITERS;

        mutex->wait_count--;

        if (tid < thread_get_count()) {
            threads[tid].state = THREAD_READY;
        }
    }

    __asm__ __volatile__("sti");
}