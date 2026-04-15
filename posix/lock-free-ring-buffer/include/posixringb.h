#ifndef POSIX_RINGB_H
#define POSIX_RINGB_H

//! Complete POSIX version of [NTRINGB](https://github.com/sadhbh-c0d3/lock-free)
//! All 14 public functions from the original Windows header are now present and correct.

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    _Atomic long next_write_pos;
    _Atomic long last_write_pos;
    _Atomic long next_read_pos;
    _Atomic long last_read_pos;
    long pow2_buffer_count;
} posix_ringb;

typedef struct
{
    posix_ringb volatile *p_ringb;
    long current_pos;
} posix_ringb_pos;

// --- Initialization ---

static inline void ringb_init(posix_ringb *p_ringb, long pow2_count)
{
    atomic_init(&p_ringb->next_write_pos, -1);
    atomic_init(&p_ringb->last_write_pos, -1);
    atomic_init(&p_ringb->next_read_pos, -1);
    atomic_init(&p_ringb->last_read_pos, -1);
    p_ringb->pow2_buffer_count = pow2_count;
}

static inline void ringb_pos_init(posix_ringb *p_ringb, posix_ringb_pos *p_pos)
{
    p_pos->p_ringb = p_ringb;
    p_pos->current_pos = -1;
}

// --- Availability Logic ---

static inline long ringb_available_write(posix_ringb_pos *p_pos)
{
    // Acquire ensures we see the latest read commits from other threads
    long last_read = atomic_load_explicit(&p_pos->p_ringb->last_read_pos, memory_order_acquire);
    return p_pos->p_ringb->pow2_buffer_count + last_read - p_pos->current_pos + 1;
}

static inline long ringb_available_read(posix_ringb_pos *p_pos)
{
    // Acquire ensures we see the latest write commits from other threads
    long last_write = atomic_load_explicit(&p_pos->p_ringb->last_write_pos, memory_order_acquire);
    return last_write - p_pos->current_pos + 1;
}

// --- Blocking Operations ---

static inline long ringb_begin_write(posix_ringb_pos *p_pos)
{
    p_pos->current_pos = atomic_fetch_add_explicit(&p_pos->p_ringb->next_write_pos, 1, memory_order_relaxed) + 1;

    while (ringb_available_write(p_pos) < 1)
        ;
    return p_pos->current_pos & (p_pos->p_ringb->pow2_buffer_count - 1);
}

static inline long ringb_begin_read(posix_ringb_pos *p_pos)
{
    p_pos->current_pos = atomic_fetch_add_explicit(&p_pos->p_ringb->next_read_pos, 1, memory_order_relaxed) + 1;

    while (ringb_available_read(p_pos) < 1)
        ;
    return p_pos->current_pos & (p_pos->p_ringb->pow2_buffer_count - 1);
}

// --- Commit Operations ---

static inline void ringb_commit_write(posix_ringb_pos *p_pos)
{
    long expected = p_pos->current_pos - 1;
    // Release ensures the data written to the buffer is visible before the index updates
    while (!atomic_compare_exchange_weak_explicit(
        &p_pos->p_ringb->last_write_pos,
        &expected,
        p_pos->current_pos,
        memory_order_release,
        memory_order_relaxed))
    {
        expected = p_pos->current_pos - 1; // Reset expected if CAS fails
    }
}

static inline void ringb_commit_read(posix_ringb_pos *p_pos)
{
    long expected = p_pos->current_pos - 1;
    while (!atomic_compare_exchange_weak_explicit(
        &p_pos->p_ringb->last_read_pos,
        &expected,
        p_pos->current_pos,
        memory_order_release,
        memory_order_relaxed))
    {
        expected = p_pos->current_pos - 1;
    }
}

// --- Poll / Async Operations ---
// (exact equivalents of the ntringb_poll_* functions)

static inline long ringb_poll_begin_write(posix_ringb_pos *p_pos)
{
    p_pos->current_pos = atomic_fetch_add_explicit(&p_pos->p_ringb->next_write_pos, 1, memory_order_relaxed) + 1;
    return p_pos->current_pos & (p_pos->p_ringb->pow2_buffer_count - 1);
}

static inline long ringb_poll_begin_read(posix_ringb_pos *p_pos)
{
    p_pos->current_pos = atomic_fetch_add_explicit(&p_pos->p_ringb->next_read_pos, 1, memory_order_relaxed) + 1;
    return p_pos->current_pos & (p_pos->p_ringb->pow2_buffer_count - 1);
}

static inline bool ringb_poll_write_ready(posix_ringb_pos *p_pos)
{
    return (0 < ringb_available_write(p_pos));
}

static inline bool ringb_poll_read_ready(posix_ringb_pos *p_pos)
{
    return (0 < ringb_available_read(p_pos));
}

static inline bool ringb_poll_commit_write(posix_ringb_pos *p_pos)
{
    long expected = p_pos->current_pos - 1;
    return atomic_compare_exchange_weak_explicit(
        &p_pos->p_ringb->last_write_pos,
        &expected,
        p_pos->current_pos,
        memory_order_release,
        memory_order_relaxed);
}

static inline bool ringb_poll_commit_read(posix_ringb_pos *p_pos)
{
    long expected = p_pos->current_pos - 1;
    return atomic_compare_exchange_weak_explicit(
        &p_pos->p_ringb->last_read_pos,
        &expected,
        p_pos->current_pos,
        memory_order_release,
        memory_order_relaxed);
}

#endif