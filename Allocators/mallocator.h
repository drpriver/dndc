#ifndef MALLOCATOR_H
#define MALLOCATOR_H
#include "allocator.h"
MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
Nonnull(void*)
allocator_wrapped_malloc(Nullable(void*) _unused, size_t size){
    (void)_unused;
    void* result = malloc(size);
    assert(result);
    return result;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
Nonnull(void*)
allocator_wrapped_zalloc(Nullable(void*) _unused, size_t size){
    (void)_unused;
    void* result = calloc(1, size);
    assert(result);
    return result;
    }

ALLOCATOR_SIZE(4)
static
Nonnull(void*)
allocator_wrapped_realloc(Nullable(void*) _unused, Nullable(void*) data, size_t orig_size, size_t new_size){
    (void)_unused;
    (void)orig_size;
    void* result = realloc(data, new_size);
    assert(result);
    return result;
    }

static
void
allocator_wrapped_free(Nullable(void*) _unused, Nullable(const void*) data, size_t size){
    (void)_unused;
    (void)size;
    PushDiagnostic();
    SuppressCastQual();
    free((void*)data);
    PopDiagnostic();
    return;
    }

static const AllocatorVtable MallocVtable = {
    .alloc           = allocator_wrapped_malloc,
    .zalloc          = allocator_wrapped_zalloc,
    .realloc         = allocator_wrapped_realloc,
    .free            = allocator_wrapped_free,
    };

static const Allocator MallocAllocator = {
    ._data= NULL,
    ._vtable = &MallocVtable,
    };

static inline
force_inline
Allocator
get_mallocator(void){
    return MallocAllocator;
    }
#endif
