# Low-Latency Lock-Free Utilities

A Library of the Lock-Free Solutions in C for Kernel Model Development on Windows:

 * `lock-free-ring-buffer` - Low Latency Ring Buffer
 * `lock-free-smart-pointer` - Atomic Smart Pointer


## What is Low Latency?

🌪️ Low latency is nanoseconds! Every CPU cycle matters. You want to fit evreything in Level 3 cache. You want to pin threads to CPU cores.
You want lock-free and wait-free programming, so that your threads run at full throtle without any interruptions!

## Watch
[![Watch My Video!](https://img.youtube.com/vi/KwSWLVUANxY/0.jpg)](https://www.youtube.com/watch?v=KwSWLVUANxY&list=PLAetEEjGZI7OUBYFoQvI0QcO9GKAvT1xT&index=1&t=2613s)
[![Watch My Video!](https://img.youtube.com/vi/aYwmopy6cdY/0.jpg)](https://youtu.be/aYwmopy6cdY&list=PLAetEEjGZI7OUBYFoQvI0QcO9GKAvT1xT&index=1)
[![Watch My Video!](https://img.youtube.com/vi/8YvBlo1UEkM/0.jpg)](https://youtu.be/8YvBlo1UEkM&list=PLAetEEjGZI7OUBYFoQvI0QcO9GKAvT1xT&index=1)
[![Watch My Video!](https://img.youtube.com/vi/WP8h4U2xXM4/0.jpg)](https://youtu.be/WP8h4U2xXM4&list=PLAetEEjGZI7OUBYFoQvI0QcO9GKAvT1xT&index=1)

## Low-Latency Ring-Buffer

*‼️ You are responsible for allocating buffer and accessing its elements by index*

The `NTRINGB_POS` controlls current stream position, which is an index in your buffer.
You are the one who decides what is the buffer and how to access its elements.

The `NTRINGB` controlls read and write transactions. You need one instance of this
data structure shared between producers and consumers.

*‼️ You can have multiple producers and multiple consumers, but not more than buffer size*

While `NTRINGB` supports multiple producers and consumers, the total number of all of them
must not exceed buffer size, and shall be less than half of the buffer size. This is due to
allocation technique, which would start causing problems if total number of producers and consumers
exceeded the limit of buffer size.

*‼️ Bigger buffer is more resistant to jitter, and allows more producers and consumers, while latency is not affected*

There is three latency numbers:
- Time between moment you insert new data on Thread A, to the moment when you pick the data on Thread B
- Time it takes to begin transaction when buffer is available (no wait)
- Time it takes to begin transaction when buffer was not available (this is wake-up time not the whole time)

This is lock-free / wait-free solution, meaning that all operations on `NTRINGB` and `NTRINGB_POS` are never
acquiring any locks from operating system, and are never waiting for operating system. While ring-buffer in
order to be thread-safe must implement wait semantics, the wait happens in busy-spin without any yield to
operatign system, meaning that one CPU core is fully consumed by this wait, which guarantees instant wake-up.

This solution is designed for pipelining, where one CPU core is receiving data in some direct manner from source,
and writes that data into ring-buffer, so that another CPU core can pick that data for further processing. This way
the CPU core that is originally receiving data can uninterruptibly do fast processing and receive next data, while
second CPU core is free to do operating system calls, such as sending data into database or logging. If the second
thread takes too much time to process one item, more consumer thread can be used to increase the bandwidth of 
consumers.

This solution works well for situations when either data keeps coming from the source, or sink keeps sending data.
The essential indicator whether this solution is good for you, is whether you have lots of data that keeps moving,
or it is not suitable if do you have lots of waiting on I/O. Solution might be viable for when there is number of
source channels, or sinks that you need to scan, and in such cases you can use asynchronous polling.

Normally asynchronous programming may sound like opposite to low-latency. However if you use `NTRINGB` polling,
you can have multiple ring-buffers, and if you iterate over them in round-robin fashion polling for new data
you will never give control back to operating system, and you will not be wasting CPU on spinning to do operation
on single ring-buffer. If you have four channels, then without polling you would need to have four threads
that produce, and four threads that consume, and that already consumed all your CPU cores. With polling you
can have just one thread that consumes from multiple ring-buffers, and this way CPU cycles aren't wasted.

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

This is fragment from example, where we demonstrate global `NTARC` variable.

*‼️ Shared data structures shall have their NTARC fields reset to `NTARC{0, 0}` using `ntarc_atomic_store()`*

This means that any shared data structure that has fields of NTARC need to get those fields reset to `NTARC{0,0}`
at the moment that data structure is destroyed.

In case of global variables of type NTARC they need to be reset to `NTARC{0,0}` at the end of `main()` function.

This is important, because `ntarc_atomic_store()` with `NTARC{0,0}` will not only zero those variables, but it
will also release and destroy those shared objects, and this will happen in atomic fashion, so that no thread
will borrow the reference to any of these variables after that time.

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

### Dropped Object Destructor

*‼️ You are responsible for deallocating all memory allocated for both your object and also control block*

Copy the below code, and replace `deallocate_memory()` with your memory deallocation routine, and 
replace `destroy_foo_data()` with destructor of your user type.

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

### New Object Constructor

*‼️ You are responsible for allocating all memory for both your object and also control block*

Copy the below code, and replace `allocate_memory()` with your memory allocation routine, and 
replace `create_foo()` with constructor of your user type.

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

*‼️ NTARC shall be used within the scope of the function*

*‼️ NTARC shall not be stored otherwise than by using `ntarc_atomic_store()`*

If you need to store NTARC for later in any shared data structure, you should always use `ntarc_atomic_store()`.
Treat that as good coding practice. If you always use `ntarc_atomic_store()` you will never run into
undefined behavior of the race-condition. The cost of running `ntarc_atomic_store()` is negligible.

*‼️ NTARC must be dropped at the end of scope*

This is C, and we don't have automatic unwinding. We must always call `ntarc_drop()` on every NTARC
local to the scope.

*‼️ Library is designed in such a fashion that your scope never passes ownership of any NTARC to any other place*

You should have no reasons to call `ntarc_clone()`, as if you are storing NTARC, then you use `ntarc_atomic_store()`,
and when you finish using within current scope you call `ntarc_drop()`.

When calling function that would take `NTARC`, you should pass `PNTARC` as parameter, i.e. you should pass by reference, because
original NTARC lives on the stack in the calling scope, and should not be cloned. Pass `PNTARC` so that the function you are
calling can use `ntarc_atomic_store()` of that NTARC for later.

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

*‼️ NTARC shall be used within the scope of the function*

*‼️ NTARC shall not be loaded otherwise than by using `ntarc_atomic_load()`*

If you need to load NTARC from any shared data structure, you should always use `ntarc_atomic_load()`.
Treat that as good coding practice. If you always use `ntarc_atomic_load()` you will never run into
undefined behavior of the race-condition. The cost of running `ntarc_atomic_load()` is negligible.

*‼️ NTARC must be dropped at the end of scope*

This is C, and we don't have automatic unwinding. We must always call `ntarc_drop()` on every NTARC
local to the scope.

*‼️ Library is designed in such a fashion that your scope never passes ownership of any NTARC to any other place*

You should have no reasons to call `ntarc_clone()`, as if you are loading NTARC, then you use `ntarc_atomic_load()`,
and when you finish using within current scope you call `ntarc_drop()`.

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
