// 
// NTARC is header-only C library that can be used in Windows Kernel Mode
// 
// NOTE
// ====
//   NTARC is Lock-Free implementation of Atomic Shared Pointer
//
// LICENSE
// =======
//   The MIT License(MIT) Copyright(c) 2024, Sadhbh C0d3
//
//   Permission is hereby granted, free of charge, to any person obtaining a copy of
//   this software and associated documentation files(the "Software"), to deal in
//   the Software without restriction, including without limitation the rights to
//   use, copy, modify, merge, publish, distribute, sublicense, and /or sell copies
//   of the Software, and to permit persons to whom the Software is furnished to do
//   so, subject to the following conditions :
//
//   The above copyright notice and this permission notice shall be included in all
//   copies or substantial portions of the Software.
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//   SOFTWARE.


#include <winnt.h>

/// <summary>
/// ARC data structure.
/// 
/// Must be homogenous with LONG64[2] !!!
/// </summary>
typedef struct tagNTARC {
    LONG64 p_control_block;
    LONG64 p_data;

} NTARC, *PNTARC;

/// <summary>
/// Destructor function that user needs to supply
/// </summary>
typedef  void (*PFN_NTARC_DESTROY)(PVOID, PNTARC);

/// <summary>
/// ARC Control Block data structure.
/// 
/// Contains reference count, and user-defined destructor
/// with user-supplied context.
/// </summary>
typedef struct tagNTARC_CONTROL_BLOCK {
    volatile LONG reference_count;
    PVOID p_destroy_context;
    PFN_NTARC_DESTROY pfn_destroy;

} NTARC_CONTROL_BLOCK, *PNTARC_CONTROL_BLOCK;

// Special macros for extraction of user-defined data and ARC control block
#define NTARC_PDATA(p_chunk, USERTYPE) ((USERTYPE*)(p_chunk->p_data))
#define NTARC_PCONTROL_BLOCK(p_chunk) ((PNTARC_CONTROL_BLOCK)(p_chunk->p_control_block))
#define NTARC_PREFCOUNT(p_chunk) (&(NTARC_PCONTROL_BLOCK(p_chunk)->reference_count))
#define NTARC_PDESTROY_CONTEXT(p_chunk) (&(NTARC_PCONTROL_BLOCK(p_chunk)->p_destroy_context))
#define NTARC_PFNDESTROY(p_chunk) (&(NTARC_PCONTROL_BLOCK(p_chunk)->pfn_destroy))
#define NTARC_LOW(chunk) (chunk.p_control_block)
#define NTARC_HIGH(chunk) (chunk.p_data)
#define NTARC_COMPARE_EXCHANGE(p_store, new_value, old_value) (InterlockedCompareExchange128((volatile LONG64*)p_store, \
    NTARC_HIGH(new_value), NTARC_LOW(new_value) , \
    (LONG64*)&(old_value)))

/// <summary>
/// Initialize ARC w/ Control Block
/// </summary>
/// <param name="p_data">A pointer to user-defined data</param>
/// <param name="p_control_block">A pointer to ARC Control Block</param>
/// <param name="p_result">A pointer to variable receiving the ARC</param>
void ntarc_new_with_control_block(PVOID p_data, PNTARC_CONTROL_BLOCK p_control_block, PNTARC p_result) {
    p_result->p_control_block = (LONG64)p_control_block;
    p_result->p_data = (LONG64)p_data;
}

/// <summary>
/// Initialize ARC Control Block
/// </summary>
/// <param name="reference_count">Initial reference count</param>
/// <param name="p_destroy_context">A pointer to user-specified Destructor Context</param>
/// <param name="pfn_destroy">A pointer to user-defined Destructor Function</param>
/// <param name="p_result">A pointer to variable receiving the ARC Control Block</param>
void ntarc_control_block_new(LONG reference_count, PVOID p_destroy_context, PFN_NTARC_DESTROY pfn_destroy,
    PNTARC_CONTROL_BLOCK p_result) {
    p_result->reference_count = reference_count;
    p_result->p_destroy_context = p_destroy_context;
    p_result->pfn_destroy = pfn_destroy;
}

/// <summary>
/// Initialize ARC
/// </summary>
/// <param name="p_data">A pointer to user-defined data</param>
/// <param name="p_destroy_context">A pointer to user-specified Destructor Context</param>
/// <param name="pfn_destroy">A pointer to user-defined Destructor Function</param>
/// <param name="p_control_block">A pointer to uninitialized user-allocated ARC Control Block</param>
/// <param name="p_result">A pointer to variable receiving the ARC</param>
void ntarc_new(PVOID p_data, PVOID p_destroy_context, PFN_NTARC_DESTROY pfn_destroy,
    PNTARC_CONTROL_BLOCK p_control_block, PNTARC p_result) {
    ntarc_control_block_new(1, p_destroy_context, pfn_destroy, p_control_block);
    ntarc_new_with_control_block(p_data, p_control_block, p_result);
}

/// <summary>
/// Clone ARC
/// </summary>
/// <param name="p_pointer">A pointer to variable holding an ARC to be cloned</param>
/// <param name="p_result">A pointer to variable receiving the cloned ARC</param>
void ntarc_clone(PNTARC p_pointer, PNTARC p_result) {

    if (0 != p_pointer->p_data) {      
        InterlockedIncrement(NTARC_PREFCOUNT(p_pointer));
    }

    p_result->p_control_block = p_pointer->p_control_block;
    p_result->p_data = p_pointer->p_data;
}

LONG ntarc_drop_reference(PNTARC p_pointer) {
    if (0 == p_pointer->p_data) {
        return 0;
    }

    return (1 + InterlockedDecrement(NTARC_PREFCOUNT(p_pointer)));
}

void ntarc_drop_data(PNTARC p_pointer, LONG reference_count) {
    if (1 == reference_count) {
        (*NTARC_PFNDESTROY(p_pointer))(NTARC_PDESTROY_CONTEXT(p_pointer), p_pointer);
    }
}

/// <summary>
/// Drop ARC
/// </summary>
/// <param name="p_pointer">A pointer to variable holding ARC to be dropped</param>
/// <returns>Reference count before drop happened</returns>
LONG ntarc_drop(PNTARC p_pointer) {
    LONG ref_count;

    ref_count = ntarc_drop_reference(p_pointer);
    ntarc_drop_data(p_pointer, ref_count);

    return ref_count;
}

/// <summary>
/// Tell if two ARCs are the same, i.e. pointing to same data
/// </summary>
BOOLEAN ntarc_is_equal(PNTARC p_first, PNTARC p_second) {
    return (p_first->p_control_block == p_second->p_control_block);
}

void ntarc_exchange(NTARC volatile* p_store, PNTARC p_pointer, PNTARC p_result) {
    NTARC old_chunk;

    do {
        old_chunk.p_control_block = p_store->p_control_block;
        old_chunk.p_data = p_store->p_data;

    } while (FALSE == NTARC_COMPARE_EXCHANGE(p_store, (*p_pointer), old_chunk));
    
    p_result->p_control_block = old_chunk.p_control_block;
    p_result->p_data = old_chunk.p_data;
}

void ntarc_atomic_begin(NTARC volatile* p_store, PNTARC p_old) {
    NTARC special_chunk = {1,0};
    NTARC old_chunk;
    do {
        old_chunk.p_control_block = p_store->p_control_block;
        old_chunk.p_data = p_store->p_data;
        
    } while (ntarc_is_equal(&old_chunk, &special_chunk) 
        || (FALSE == NTARC_COMPARE_EXCHANGE(p_store, special_chunk, old_chunk)));
    
    p_old->p_control_block = old_chunk.p_control_block;
    p_old->p_data = old_chunk.p_data;
}

BOOLEAN ntarc_atomic_commit(NTARC volatile *p_store, PNTARC p_pointer) {
    NTARC special_chunk = {1,0};
    return NTARC_COMPARE_EXCHANGE(p_store, (*p_pointer), special_chunk);
}

/// <summary>
/// Atomic Store
/// </summary>
/// <param name="p_store">A pointer to global variable holding shared ARC</param>
/// <param name="p_pointer">A pointer to variable holding ARC to be stored</param>
void ntarc_atomic_store(NTARC volatile *p_store, PNTARC p_pointer) {
    NTARC old_chunk;
    NTARC new_chunk;
    LONG old_reference_count;
    ntarc_clone(p_pointer, &new_chunk);
    ntarc_atomic_begin(p_store, &old_chunk);
    old_reference_count = ntarc_drop_reference(&old_chunk);
    ntarc_atomic_commit(p_store, &new_chunk);
    ntarc_drop_data(&old_chunk, old_reference_count);
}

/// <summary>
/// Atomic Load
/// </summary>
/// <param name="p_store">A pointer to global variable holding shared ARC</param>
/// <param name="p_pointer">A pointer to variable receiving clone of the shared ARC</param>
void ntarc_atomic_load(NTARC volatile *p_store, PNTARC p_result) {
    NTARC old_chunk;
    ntarc_atomic_begin(p_store, &old_chunk);
    ntarc_clone(&old_chunk, p_result);
    ntarc_atomic_commit(p_store, &old_chunk);
}

