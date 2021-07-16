#ifndef ALLOCATOR_H
#define ALLOCATOR_H
// size_t
#include <stddef.h>
// malloc, free
#include <stdlib.h>
// memcpy, memset
#include <string.h>
#include "common_macros.h"

enum AllocatorType {
    ALLOCATOR_UNSET = 0,
    ALLOCATOR_MALLOC = 1,
    ALLOCATOR_LINEAR = 2,
    ALLOCATOR_RECORDED = 3,
    ALLOCATOR_ARENA = 4,
};

typedef struct ArenaAllocator {
    Nullable(struct Arena*) arena;
    Nullable(struct BigAllocation*) big_allocations;
} ArenaAllocator;


typedef struct Allocator {
    enum AllocatorType type;
    // 4 bytes of padding
    Nonnull(void*) _data;
} Allocator;


MALLOC_FUNC
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_alloc(Allocator allocator, size_t size);

MALLOC_FUNC
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_zalloc(Allocator allocator, size_t size);

static inline
// force_inline
warn_unused
Nonnull(void*)
Allocator_realloc(Allocator allocator, Nullable(void*) data, size_t orig_size, size_t size);

static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_dupe(Allocator allocator, Nonnull(const void*) data, size_t size);

static inline
// force_inline
void
Allocator_free(Allocator allocator, Nullable(const void*) data, size_t size);

static inline
void
Allocator_free_all(const Allocator a);

MALLOC_FUNC
static inline
warn_unused
Nonnull(char*)
Allocator_strndup(Allocator allocator, Nonnull(const char*)str, size_t length);

#endif
