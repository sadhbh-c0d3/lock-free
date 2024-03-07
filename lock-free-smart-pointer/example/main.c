// Example is for Windows Console, however NTARC targets KMDF development
// NTARC does not use any CRT functions, and only interlocked operations
// that are defined in winnt.h
#define MEAN_AND_LEAN
#include <Windows.h>
#include <stdio.h>

// Header only
#include "ntarc.h"

/// <summary>
/// Global volatile variable holding an ARC to user-type FOO.
/// 
/// Since this variable is global, access from multiple threads need to be
/// synchronized. We will use Atomic Load/Store operations of (ntarc) ARC.
/// 
/// Atomic Load/Store guarantees that replacement of pointer happens
/// together with control block holding reference count (this is lock-free 
/// C implmementation of atomic shared_ptr).
/// </summary>
volatile NTARC g_foo = { 0, 0 };

typedef struct tagFOO {
    int x;
    int y;
} FOO, * PFOO;

/// <summary>
/// An example destructor of ARC to user-type FOO
/// </summary>
/// <param name="p_context">A user-specified context stored at contruction time (unused here)</param>
/// <param name="p_arc">A pointer to variable storing ARC to user-type FOO that is to be destroyed</param>
void foo_destroy(PVOID p_context, PNTARC p_arc)
{
    if (p_arc->p_data == 0) {
        return;
    }

    free((PVOID)(p_arc->p_data));
    free((PVOID)(p_arc->p_control_block));

    p_arc->p_data = 0;
    p_arc->p_control_block = 0;
}

/// <summary>
/// An example constructor of ARC to user-type FOO
/// </summary>
/// <param name="p_arc">A pointer to ARC variable to be initialized</param>
/// <param name="x">A parameter to user-type contructor</param>
/// <param name="y">A parameter to user-type contructor</param>
/// <returns>TRUE if all went well, FALSE if memory allocation failed</returns>
BOOL foo_new(PNTARC p_arc, int x, int y)
{
    PFOO p_foo = NULL;
    PNTARC_CONTROL_BLOCK p_control_block = NULL;

    if (p_arc == NULL) {
        goto bail010;
    }

    // Allocate memory for (our) Foo
    p_foo = (PFOO)malloc(sizeof(FOO));
    if (p_foo == 0) {
        goto bail020;
    }

    // Allocate memory for (ntarc) Control Block
    p_control_block = (PNTARC_CONTROL_BLOCK)malloc(sizeof(NTARC_CONTROL_BLOCK));
    if (p_control_block == 0) {
        goto bail030;
    }

    // Init (our) Foo
    memset((PVOID)p_foo, 0, sizeof(FOO));
    p_foo->x = x;
    p_foo->y = y;

    // Init (ntarc) ARC
    ntarc_new((PVOID)p_foo, NULL, &foo_destroy, p_control_block, p_arc);
    
    return TRUE;

bail030:
    free((PVOID)p_foo);
bail020:
bail010:
    return FALSE;
}

/// <summary>
/// Theoretical thread no.1
/// 
/// This thread uses Atomic Store to set new value of the global ARC.
/// Note that if another thread holds reference to old value, that reference
/// will remain valid until that other thread drops it.
/// </summary>
void foo_thread1() {
    NTARC foo;

    if (foo_new(&foo, 1, 2) == FALSE)
    {
        printf("Cannot create Foo!");
        return;
    }

    ntarc_atomic_store(&g_foo, &foo);

    /* do some work with foo...*/

    ntarc_drop(&foo);
}

/// <summary>
/// Theoretical thread no.2
/// 
/// This thread uses Atomic Load to get last known value of global ARC.
/// Note that if another thread changes value in ARC, this thread will
/// not be affected by that change, and will own the reference to the
/// value that it received from ARC at the time of Load. Ath the end 
/// this thread must drop its own reference so that in case it was the
/// last reference to the object, it will be destroyed. Destruction of
/// the object does not need to be synchronized, because if this thread
/// holds last reference, then no other thread knows the object, and thus
/// no other thread can access it.
/// </summary>
void foo_thread2() {
    NTARC foo;
    ntarc_atomic_load(&g_foo, &foo);

    /* do some work with foo...*/
    PFOO p_foo = NTARC_PDATA((&foo), FOO);

    printf("Foo: %d, %d", p_foo->x, p_foo->y);

    ntarc_drop(&foo);
}

int main(int argc, char** argv)
{
    NTARC null_foo = { 0, 0 };

    // Here we just run code of the two hypothetical threads sequentially
    // for development and debugging purposes.
    foo_thread1();
    foo_thread2();

    // At the end we must reset global reference to null, so that
    // the object if any will be destroyed.
    // Note that we use Atomic Store and not Drop!
    ntarc_atomic_store(&g_foo, &null_foo);
    return 0;
}