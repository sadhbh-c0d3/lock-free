#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#include "posixringb.h"

typedef struct tagFOO {
    int x;
    int y;
} FOO;

typedef struct tagFOOTHREAD {
    int max_i;
    int max_j;
    int sleep_ms;
} FOOTHREAD;

#define FOO_COUNT 8

FOO g_foo[FOO_COUNT];
posix_ringb g_ringb;

void* foo_producer(void* arg)
{
    FOOTHREAD* p = (FOOTHREAD*)arg;
    posix_ringb_pos ringb_pos;
    long pos;
    FOO current;
    int last_x = 1;
    int i, j;

    ringb_pos_init(&g_ringb, &ringb_pos);

    for (j = 0; j < p->max_j; ++j)
    {
        for (i = 0; i < p->max_i; ++i)
        {
            current.x = last_x;
            current.y = ++last_x;

            pos = ringb_begin_write(&ringb_pos);

            memcpy(&g_foo[pos], &current, sizeof(FOO));

            ringb_commit_write(&ringb_pos);
        }

        printf("Last_X = %d\n", last_x);
        if (p->sleep_ms > 0)
            usleep(p->sleep_ms * 1000);
    }

    return NULL;
}

void* foo_consumer(void* arg)
{
    FOOTHREAD* p = (FOOTHREAD*)arg;
    posix_ringb_pos ringb_pos;
    long pos;
    FOO current;
    int i;

    ringb_pos_init(&g_ringb, &ringb_pos);

    for (i = 0; i < (p->max_i * p->max_j); ++i)
    {
        pos = ringb_begin_read(&ringb_pos);

        memcpy(&current, &g_foo[pos], sizeof(FOO));

        ringb_commit_read(&ringb_pos);

        printf("Received: x = %d, y = %d\n", current.x, current.y);
    }

    return NULL;
}

void main_st(void)
{
    FOOTHREAD foo_thread = { 4, 2, 0 };

    printf("\n=== Running Single-Threaded Example ===\n");

    ringb_init(&g_ringb, FOO_COUNT);
    memset(g_foo, 0, sizeof(FOO) * FOO_COUNT);

    foo_producer(&foo_thread);
    foo_consumer(&foo_thread);
}

void main_mt(int sleep_ms)
{
    FOOTHREAD foo_thread = { 3, 4, sleep_ms };
    pthread_t producer_thread, consumer_thread;

    printf("\n=== Running Multi-Threaded Example with Sleep %d ms ===\n", sleep_ms);

    ringb_init(&g_ringb, FOO_COUNT);
    memset(g_foo, 0, sizeof(FOO) * FOO_COUNT);

    pthread_create(&consumer_thread, NULL, foo_consumer, &foo_thread);
    pthread_create(&producer_thread, NULL, foo_producer, &foo_thread);

    pthread_join(consumer_thread, NULL);
    pthread_join(producer_thread, NULL);
}

int main(void)
{
    // Single threaded version
    main_st();

    // Multi-threaded versions with different sleep delays to simulate contention
    main_mt(1000);
    main_mt(100);
    main_mt(10);
    main_mt(1);

    return 0;
}