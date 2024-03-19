# Lock-Free Utilities

A Library of the Lock-Free Solutions in C for Kernel Model Development on Windows:

 * `lock-free-ring-buffer` - Low Latency Ring Buffer
 * `lock-free-smart-pointer` - Atomic Smart Pointer

## Watch
[![Watch My Video!](https://img.youtube.com/vi/aYwmopy6cdY/0.jpg)](https://youtu.be/aYwmopy6cdY&list=PLAetEEjGZI7OUBYFoQvI0QcO9GKAvT1xT&index=1)
[![Watch My Video!](https://img.youtube.com/vi/8YvBlo1UEkM/0.jpg)](https://youtu.be/8YvBlo1UEkM&list=PLAetEEjGZI7OUBYFoQvI0QcO9GKAvT1xT&index=1)

## Low-Latency Ring-Buffer

### Setup

Include header:
```c
    #include <NTRINGB.H>
```

Define buffer and ring control structure:
```c
    // Define user type
    typedef struct { ... } FOO;

    // Define size of buffer
    #define FOO_COUNT 8

    // Allocate buffer
    volatile FOO g_buffer[FOO_COUNT];

    // Allocate ring-buffer control structure
    NTRINGB g_ringb;
```

Initialize buffer and ring control structure
```c
void initialize() {
    // Initialize ring-buffer control structure
    ntringb_init(&g_ringb, FOO_COUNT);

    // Initialize buffer with zeroes
    memset(g_foo, 0, sizeof(FOO) * FOO_COUNT);
}
```

### Producer Loop
```c
    void keep_producing() {
        // Ring-buffer stream position
        NTRINGB_POS ring_pos;

        // Local copy of the next value
        FOO local_data;

        // Initialize ring-buffer stream position
        ntringb_pos_init(&g_ringb, &ringb_pos);

        // Loop until (forever)
        for (;;) {
            // Produce next value into local variable
            produce_next_value(&local_data);

            // Begin WRITE transaction - returns position
            pos = ntringb_begin_write(&ringb_pos);

            // Copy local value into buffer at position
            memcpy(&g_buffer[pos], &local_data, sizeof(FOO));

            // Commit WRITE transaction
            ntringb_commit_write(&ringb_pos);
        }
    }
```

### Consumer Loop
```c
    void keep_consuming() {
        // Ring-buffer stream position
        NTRINGB_POS ring_pos;

        // Local copy of the next value
        FOO local_data;

        // Initialize ring-buffer stream position
        ntringb_pos_init(&g_ringb, &ringb_pos);

        // Loop until (forever)
        for (;;) {
            // Begin READ transaction - returns position
            pos = ntringb_begin_read(&ringb_pos);

            // Copy value at position in buffer into local variable
            memcpy(&local_data, &g_buffer[pos], sizeof(FOO));

            // Commit READ transaction
            ntringb_commit_read(&ringb_pos);

            // Consume next value from local variable
            consume_next_value(&local_data);
        }
    }
```

## Lock-Free Atomic Shared Pointer

This requires support of 128-bit CAS, which Windows NT provides.

### Setup

Include header:
```c
    #include <NTARC.H> 
``` 

Define atomic shared pointer:
```c
    // Define user type
    typedef struct { ... } FOO;

    // Allocate atomic shared pointer
    volatile NTARC g_foo = { 0, 0 };
```

Program main:
```c
int main(int argc, char **argv) {
    NTARC null_foo = { 0, 0 };

    run_main_program();

    // Release main reference to atomic shared pointer
    ntarc_atomic_store(&g_foo, &null_foo);
}

```

Create destructor of atomic shared pointer:
```c
void foo_destroy(PVOID p_context, PNTARC p_arc) {
    if (p_arc->p_data == 0) {
        return;
    }

    // Destruct user-data and deallocate memory
    destroy_foo_data(NTARC_PDATA(p_arc, FOO));

    // Free memory of the control block using correct deallocator
    deallocate_memory((PVOID)(p_arc->p_control_block));

    p_arc->p_data = 0;
    p_arc->p_control_block = 0;
}
```

Create constructor for atomic shared pointer:
```c
BOOL foo_new(PNTARC p_arc, params...) {
    PFOO p_foo = NULL;
    PNTARC_CONTROL_BLOCK p_control_block = NULL;

    if (p_arc == NULL) {
        goto bail010;
    }

    // Allocate memory for (our) Foo
    p_foo = create_foo(params...);
    if (p_foo == 0) {
        goto bail020;
    }

    // Allocate memory for control block using correct allocator
    p_control_block = (PNTARC_CONTROL_BLOCK)allocate_memory(sizeof(NTARC_CONTROL_BLOCK));
    if (p_control_block == 0) {
        goto bail030;
    }

    // Initialize atomic shared pointer and its control block
    ntarc_new((PVOID)p_foo, NULL, &foo_destroy, p_control_block, p_arc);
    
    return TRUE;

bail030:
    destroy_foo_data(p_foo);
bail020:
bail010:
    return FALSE;
}

```

### Store New Object

Set atomic shared pointer to new object:
```c
void make_foo() {
    NTARC foo;

    // Use previously defined constructor of the atomic shared pointer
    if (foo_new(&foo, 1, 2) == FALSE)
    {
        printf("Cannot create Foo!");
        return;
    }

    // Atomically store newly created shared object into shared variable
    ntarc_atomic_store(&g_foo, &foo);

    /* do some work with foo...*/

    // Drop local reference to shared object
    ntarc_drop(&foo);
}
```

### Load Current Object

Get object from atomic shared pointer:
```c
void read_foo() {
    NTARC foo;

    // Load shared object from shared variable into local shared pointer
    ntarc_atomic_load(&g_foo, &foo);

    /* do some work with foo...*/

    // Drop local reference to shared object
    ntarc_drop(&foo);
}
```
