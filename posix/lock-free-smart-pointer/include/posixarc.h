#ifndef POSIXARC_H
#define POSIXARC_H

//! POSIX version of NTARC (Lock-free Atomic Shared Pointer)
//! Faithful minimal translation with proper _Atomic handling

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// 128-bit aligned structure
typedef struct __attribute__((aligned(16))) tagPOSIXARC {
    uintptr_t p_control_block;
    uintptr_t p_data;
} posixarc;

typedef posixarc* posixarc_ptr;

// 128-bit type for atomic operations
typedef __int128 posixarc_128;

// Destructor
typedef void (*pfn_posixarc_destroy)(void* p_context, posixarc_ptr p_arc);

// Control Block
typedef struct tagPOSIXARC_CONTROL_BLOCK {
    _Atomic long reference_count;
    void* p_destroy_context;
    pfn_posixarc_destroy pfn_destroy;
} posixarc_control_block;

typedef posixarc_control_block* posixarc_control_block_ptr;

// Macros
#define POSIXARC_PDATA(p_chunk, USERTYPE) ((USERTYPE*)(p_chunk->p_data))
#define POSIXARC_PCONTROL_BLOCK(p_chunk) ((posixarc_control_block_ptr)(p_chunk->p_control_block))
#define POSIXARC_PREFCOUNT(p_chunk) (&(POSIXARC_PCONTROL_BLOCK(p_chunk)->reference_count))
#define POSIXARC_PDESTROY_CONTEXT(p_chunk) (&(POSIXARC_PCONTROL_BLOCK(p_chunk)->p_destroy_context))
#define POSIXARC_PFNDESTROY(p_chunk) (&(POSIXARC_PCONTROL_BLOCK(p_chunk)->pfn_destroy))

static inline bool POSIXARC_COMPARE_EXCHANGE(_Atomic posixarc* p_store, posixarc new_value, posixarc old_value)
{
    posixarc_128 expected = ((posixarc_128)old_value.p_control_block << 64) | old_value.p_data;
    posixarc_128 desired  = ((posixarc_128)new_value.p_control_block  << 64) | new_value.p_data;

    return atomic_compare_exchange_weak_explicit(
        (_Atomic posixarc_128*)p_store,
        &expected,
        desired,
        memory_order_acq_rel,
        memory_order_relaxed);
}

// --- Initialization ---

static inline void posixarc_new_with_control_block(void* p_data, posixarc_control_block_ptr p_control_block, posixarc_ptr p_result)
{
    p_result->p_control_block = (uintptr_t)p_control_block;
    p_result->p_data = (uintptr_t)p_data;
}

static inline void posixarc_control_block_new(long reference_count, void* p_destroy_context,
                                              pfn_posixarc_destroy pfn_destroy,
                                              posixarc_control_block_ptr p_result)
{
    atomic_init(&p_result->reference_count, reference_count);
    p_result->p_destroy_context = p_destroy_context;
    p_result->pfn_destroy = pfn_destroy;
}

static inline void posixarc_new(void* p_data, void* p_destroy_context, pfn_posixarc_destroy pfn_destroy,
                                posixarc_control_block_ptr p_control_block, posixarc_ptr p_result)
{
    posixarc_control_block_new(1, p_destroy_context, pfn_destroy, p_control_block);
    posixarc_new_with_control_block(p_data, p_control_block, p_result);
}

// --- Clone / Drop ---

static inline void posixarc_clone(posixarc_ptr p_pointer, posixarc_ptr p_result)
{
    if (0 != p_pointer->p_data) {
        atomic_fetch_add_explicit(POSIXARC_PREFCOUNT(p_pointer), 1, memory_order_relaxed);
    }

    p_result->p_control_block = p_pointer->p_control_block;
    p_result->p_data = p_pointer->p_data;
}

static inline long posixarc_drop_reference(posixarc_ptr p_pointer)
{
    if (0 == p_pointer->p_data) {
        return 0;
    }

    return (1 + atomic_fetch_sub_explicit(POSIXARC_PREFCOUNT(p_pointer), 1, memory_order_acq_rel));
}

static inline void posixarc_drop_data(posixarc_ptr p_pointer, long reference_count)
{
    if (1 == reference_count) {
        (*POSIXARC_PFNDESTROY(p_pointer))(*POSIXARC_PDESTROY_CONTEXT(p_pointer), p_pointer);
    }
}

static inline long posixarc_drop(posixarc_ptr p_pointer)
{
    long ref_count = posixarc_drop_reference(p_pointer);
    posixarc_drop_data(p_pointer, ref_count);
    return ref_count;
}

static inline bool posixarc_is_equal(posixarc_ptr p_first, posixarc_ptr p_second)
{
    return (p_first->p_control_block == p_second->p_control_block);
}

// --- Atomic operations ---

static inline void posixarc_exchange(_Atomic posixarc* p_store, posixarc_ptr p_pointer, posixarc_ptr p_result)
{
    posixarc old_chunk = {0};

    do {
        posixarc_128 current = atomic_load_explicit((_Atomic posixarc_128*)p_store, memory_order_relaxed);
        old_chunk.p_control_block = (uintptr_t)(current >> 64);
        old_chunk.p_data          = (uintptr_t)current;
    } while (!POSIXARC_COMPARE_EXCHANGE(p_store, *p_pointer, old_chunk));

    p_result->p_control_block = old_chunk.p_control_block;
    p_result->p_data = old_chunk.p_data;
}

static inline void posixarc_atomic_begin(_Atomic posixarc* p_store, posixarc_ptr p_old)
{
    posixarc special_chunk = {1, 0};
    posixarc old_chunk = {0};

    do {
        posixarc_128 current = atomic_load_explicit((_Atomic posixarc_128*)p_store, memory_order_relaxed);
        old_chunk.p_control_block = (uintptr_t)(current >> 64);
        old_chunk.p_data          = (uintptr_t)current;
    } while (posixarc_is_equal(&old_chunk, &special_chunk) ||
             !POSIXARC_COMPARE_EXCHANGE(p_store, special_chunk, old_chunk));

    p_old->p_control_block = old_chunk.p_control_block;
    p_old->p_data = old_chunk.p_data;
}

static inline bool posixarc_atomic_commit(_Atomic posixarc* p_store, posixarc_ptr p_pointer)
{
    posixarc special_chunk = {1, 0};
    return POSIXARC_COMPARE_EXCHANGE(p_store, *p_pointer, special_chunk);
}

static inline void posixarc_atomic_store(_Atomic posixarc* p_store, posixarc_ptr p_pointer)
{
    posixarc old_chunk = {0};
    posixarc new_chunk = {0};
    long old_reference_count;

    posixarc_clone(p_pointer, &new_chunk);
    posixarc_atomic_begin(p_store, &old_chunk);
    old_reference_count = posixarc_drop_reference(&old_chunk);
    posixarc_atomic_commit(p_store, &new_chunk);
    posixarc_drop_data(&old_chunk, old_reference_count);
}

static inline void posixarc_atomic_load(_Atomic posixarc* p_store, posixarc_ptr p_result)
{
    posixarc old_chunk = {0};
    posixarc_atomic_begin(p_store, &old_chunk);
    posixarc_clone(&old_chunk, p_result);
    posixarc_atomic_commit(p_store, &old_chunk);
}

#endif