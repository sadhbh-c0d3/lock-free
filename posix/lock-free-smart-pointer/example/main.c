#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "posixarc.h"

typedef struct tagFOO {
    int x;
    int y;
} FOO, *PFOO;

// Global shared ARC (was volatile NTARC g_foo in original)
_Atomic posixarc g_foo;

void foo_destroy(void* p_context, posixarc_ptr p_arc)
{
    (void)p_context;
    if (p_arc->p_data == 0) {
        return;
    }

    free((void*)(p_arc->p_data));
    free((void*)(p_arc->p_control_block));

    p_arc->p_data = 0;
    p_arc->p_control_block = 0;
}

bool foo_new(posixarc_ptr p_arc, int x, int y)
{
    PFOO p_foo = NULL;
    posixarc_control_block_ptr p_control_block = NULL;

    if (p_arc == NULL) goto bail010;

    p_foo = (PFOO)malloc(sizeof(FOO));
    if (p_foo == 0) goto bail020;

    p_control_block = (posixarc_control_block_ptr)malloc(sizeof(posixarc_control_block));
    if (p_control_block == 0) goto bail030;

    memset((void*)p_foo, 0, sizeof(FOO));
    p_foo->x = x;
    p_foo->y = y;

    posixarc_new((void*)p_foo, NULL, foo_destroy, p_control_block, p_arc);
    
    return true;

bail030:
    free((void*)p_foo);
bail020:
bail010:
    return false;
}

void foo_thread1(void)
{
    posixarc foo = {0, 0};

    if (foo_new(&foo, 1, 2) == false)
    {
        printf("Cannot create Foo!\n");
        return;
    }

    posixarc_atomic_store(&g_foo, &foo);

    posixarc_drop(&foo);
}

void foo_thread2(void)
{
    posixarc foo = {0, 0};
    posixarc_atomic_load(&g_foo, &foo);

    PFOO p_foo = POSIXARC_PDATA((&foo), FOO);

    printf("Foo: %d, %d\n", p_foo->x, p_foo->y);

    posixarc_drop(&foo);
}

int main(void)
{
    // Safe zero initialization of the atomic global (matches original {0,0})
    posixarc zero = {0, 0};
    atomic_init(&g_foo, zero);     // Now correct: passing posixarc, not __int128

    foo_thread1();
    foo_thread2();

    posixarc null_foo = {0, 0};
    posixarc_atomic_store(&g_foo, &null_foo);

    printf("Done.\n");
    return 0;
}