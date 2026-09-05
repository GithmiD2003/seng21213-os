#ifndef THREAD_H
#define THREAD_H

#include "../include/types.h"

#define MAX_THREADS 8
#define THREAD_STACK_SIZE 4096

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

int thread_create(void (*entry_point)(void *), void *arg);

thread_t *thread_get_table(void);
uint32_t thread_get_count(void);

#endif
