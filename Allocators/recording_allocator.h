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
    struct {
        void*_Nullable*_Nonnull allocations;
        Nonnull(size_t*) allocation_sizes;
        size_t count;
        size_t capacity;
    } recorded;
} RecordingAllocator;

static inline
void
recording_ensure_capacity(Nonnull(RecordingAllocator*) r){
    auto rec = &r->recorded;
    if(rec->count < rec->capacity)
        return;
    if(!rec->capacity){
        enum {INITIAL_CAPACITY=32};
        rec->capacity = INITIAL_CAPACITY;
        rec->allocations = malloc(INITIAL_CAPACITY*sizeof(*rec->allocations));
        rec->allocation_sizes = malloc(INITIAL_CAPACITY*sizeof(*rec->allocation_sizes));
        return;
        }
    auto old_cap = rec->capacity;
    auto new_cap = old_cap * 2;
    rec->allocations = realloc(rec->allocations, new_cap*sizeof(*rec->allocations));
    rec->allocation_sizes = realloc(rec->allocation_sizes, new_cap*sizeof(*rec->allocation_sizes));
    rec->capacity = new_cap;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
recording_alloc(Nonnull(RecordingAllocator*) r, size_t size){
    auto result = malloc(size);
    if(!result)
        return result;
    recording_ensure_capacity(r);
    auto rec = &r->recorded;
    auto index = rec->count++;
    rec->allocations[index] = result;
    rec->allocation_sizes[index] = size;
    return result;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
recording_zalloc(Nonnull(RecordingAllocator*) r, size_t size){
    auto result = calloc(1, size);
    if(!result)
        return result;
    recording_ensure_capacity(r);
    auto rec = &r->recorded;
    auto index = rec->count++;
    rec->allocations[index] = result;
    rec->allocation_sizes[index] = size;
    return result;
    }

static
void
recording_free(Nonnull(RecordingAllocator*)r, Nullable(const void*) data, size_t size){
    if(!data)
        return;
    // inefficient, but whatever
    auto rec = &r->recorded;
    for(size_t i = 0; i < rec->count; i++){
        if(data == rec->allocations[i]){
            unhandled_error_condition(size != rec->allocation_sizes[i]);
            const_free(data);
            // TODO: compact?
            rec->allocations[i] = NULL;
            rec->allocation_sizes[i] = 0;
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
    auto rec = &r->recorded;
    for(size_t i = 0; i < rec->count; i++){
        if(!rec->allocations[i])
            continue;
        free(rec->allocations[i]);
        }
    rec->count = 0;
    }

static
void
recording_cleanup(Nonnull(RecordingAllocator*)r){
    auto rec = &r->recorded;
    // if(!rec->capacity)
        // return;
    free(rec->allocation_sizes);
    free(rec->allocations);
    memset(rec, 0, sizeof(*rec));
    return;
    }

static
void
recording_merge(Nonnull(RecordingAllocator*) restrict dst, Nonnull(const RecordingAllocator*) restrict src){
    auto dst_rec = &dst->recorded;
    auto src_rec = &src->recorded;
    for(size_t i = 0; i < src_rec->count; i++){
        auto size = src_rec->allocation_sizes[i];
        if(!size)
            continue;
        recording_ensure_capacity(dst);
        auto pointer = src_rec->allocations[i];
        auto index = dst_rec->count++;
        dst_rec->allocations[index] = pointer;
        dst_rec->allocation_sizes[index] = size;
        }
    }

static inline
Nonnull(void*)
ALLOCATOR_SIZE(4)
recording_realloc(Nonnull(RecordingAllocator*)r, Nullable(void*)data, size_t orig_size, size_t new_size){
    auto rec = &r->recorded;
    for(size_t i = 0; i < rec->count; i++){
        if(data == rec->allocations[i]){
            unhandled_error_condition(orig_size != rec->allocation_sizes[i]);
            // TODO: compact?
            rec->allocations[i] = NULL;
            rec->allocation_sizes[i] = 0;
            break;
            }
        }
    void* result = realloc(data, new_size);
    recording_ensure_capacity(r);
    rec = &r->recorded;
    auto index = rec->count++;
    rec->allocations[index] = result;
    rec->allocation_sizes[index] = new_size;
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

static AllocatorVtable RecordedAllocatorVtable = {
    .alloc = (alloc_func)recording_alloc,
    .zalloc = (alloc_func)recording_zalloc,
    .realloc = (realloc_func)recording_realloc,
    .free = (free_func)recording_free,
    .free_all = (free_all_func)recording_free_all,
    .cleanup = (cleanup_func)recording_cleanup,
    };

static
Allocator
new_recorded_mallocator(void){
    RecordingAllocator* ra = calloc(1, sizeof(*ra));
    return (Allocator){
        ._data = ra,
        ._vtable = &RecordedAllocatorVtable,
        };
    }

PopDiagnostic();

#endif
