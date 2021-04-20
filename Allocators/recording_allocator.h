#ifndef RECORDING_ALLOCATOR_H
#define RECORDING_ALLOCATOR_H
#include "allocator.h"

PushDiagnostic();
SuppressUnusedFunction();

/*
 * An Allocator wrapper allocator that tracks all allocations
 * and frees done through it so as to provide a free_all function
 * for allocators that don't normally support it.
 *
 * This is currently super unoptimized, but that's ok.
 *
 * Additionally, allows querying if the memory is valid.
 */


typedef struct RecordingAllocator {
    // We need a dynamic array to record all of the allocations.
    // We specialize it to be SOA
    void*_Nullable*_Nonnull allocations;
    Nonnull(size_t*) allocation_sizes;
    size_t count;
    size_t capacity;
} RecordingAllocator;

static inline
void
recording_ensure_capacity(Nonnull(RecordingAllocator*) r){
    if(r->count < r->capacity)
        return;
    if(!r->capacity){
        enum {INITIAL_CAPACITY=32};
        r->capacity = INITIAL_CAPACITY;
        r->allocations = malloc(INITIAL_CAPACITY*sizeof(*r->allocations));
        r->allocation_sizes = malloc(INITIAL_CAPACITY*sizeof(*r->allocation_sizes));
        return;
        }
    size_t old_cap = r->capacity;
    size_t new_cap = old_cap * 2;
    r->allocations = sane_realloc(r->allocations, old_cap * sizeof(*r->allocations), new_cap*sizeof(*r->allocations));
    r->allocation_sizes = sane_realloc(r->allocation_sizes, old_cap*sizeof(*r->allocation_sizes), new_cap*sizeof(*r->allocation_sizes));
    r->capacity = new_cap;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
recording_alloc(Nonnull(RecordingAllocator*) r, size_t size){
    void* result = malloc(size);
    if(!result)
        return result;
    recording_ensure_capacity(r);
    size_t index = r->count++;
    r->allocations[index] = result;
    r->allocation_sizes[index] = size;
    return result;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
recording_zalloc(Nonnull(RecordingAllocator*) r, size_t size){
    void* result = calloc(1, size);
    if(!result)
        return result;
    recording_ensure_capacity(r);
    size_t index = r->count++;
    r->allocations[index] = result;
    r->allocation_sizes[index] = size;
    return result;
    }

static
void
recording_free(Nonnull(RecordingAllocator*)r, Nullable(const void*) data, size_t size){
    if(!data)
        return;
    // inefficient, but whatever
    for(size_t i = 0; i < r->count; i++){
        if(data == r->allocations[i]){
            unhandled_error_condition(size != r->allocation_sizes[i]);
            const_free(data);
            // TODO: compact?
            r->allocations[i] = NULL;
            r->allocation_sizes[i] = 0;
            return;
            }
        }
    ERROR("Freeing a pointer not recorded in this allocator. Double free?");
    assert(0);
    }

// The money function, the reason we did this in the first
// place.
static
void
recording_free_all(Nonnull(RecordingAllocator*)r){
    for(size_t i = 0; i < r->count; i++){
        if(!r->allocations[i])
            continue;
        free(r->allocations[i]);
        }
    r->count = 0;
    }

static
void
recording_cleanup(Nonnull(RecordingAllocator*)r){
    free(r->allocation_sizes);
    free(r->allocations);
    memset(r, 0, sizeof(*r));
    return;
    }

static
void
recording_merge(Nonnull(RecordingAllocator*) restrict dst, Nonnull(const RecordingAllocator*) restrict src){
    for(size_t i = 0; i < src->count; i++){
        size_t size = src->allocation_sizes[i];
        if(!size)
            continue;
        recording_ensure_capacity(dst);
        void* pointer = src->allocations[i];
        size_t index = dst->count++;
        dst->allocations[index] = pointer;
        dst->allocation_sizes[index] = size;
        }
    }

static inline
Nonnull(void*)
ALLOCATOR_SIZE(4)
recording_realloc(Nonnull(RecordingAllocator*)r, Nullable(void*)data, size_t orig_size, size_t new_size){
    for(size_t i = 0; i < r->count; i++){
        if(data == r->allocations[i]){
            unhandled_error_condition(orig_size != r->allocation_sizes[i]);
            // TODO: compact?
            r->allocations[i] = NULL;
            r->allocation_sizes[i] = 0;
            break;
            }
        }
    void* result = sane_realloc(data, orig_size, new_size);
    recording_ensure_capacity(r);
    size_t index = r->count++;
    r->allocations[index] = result;
    r->allocation_sizes[index] = new_size;
    return result;
    }

static inline
void
shallow_free_recorded_mallocator(const Allocator a){
    RecordingAllocator* r = a._data;
    recording_cleanup(r);
    const_free(r);
    }

static inline
void
merge_recorded_mallocators_and_destroy_src(const Allocator dst, const Allocator src){
    recording_merge(dst._data, src._data);
    // shallow_free_recorded_mallocator(src);
    }

static
Allocator
new_recorded_mallocator(void){
    RecordingAllocator* ra = calloc(1, sizeof(*ra));
    return (Allocator){
        ._data = ra,
        .type = ALLOCATOR_RECORDED,
        };
    }

PopDiagnostic();

#endif
