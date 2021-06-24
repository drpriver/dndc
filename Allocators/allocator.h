#ifndef ALLOCATOR_H
#define ALLOCATOR_H
// malloc, free
#include <stdlib.h>
// memcpy, memset
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "common_macros.h"

enum AllocatorType {
    ALLOCATOR_UNSET = 0,
    ALLOCATOR_MALLOC = 1,
    ALLOCATOR_LINEAR = 2,
    ALLOCATOR_RECORDED = 3,
};


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
Allocator_alloc(const Allocator allocator, size_t size);

MALLOC_FUNC
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_zalloc(const Allocator allocator, size_t size);

static inline
// force_inline
warn_unused
Nonnull(void*)
Allocator_realloc(const Allocator allocator, Nullable(void*) data, size_t orig_size, size_t size);

static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_dupe(const Allocator allocator, Nonnull(const void*) data, size_t size);

static inline
// force_inline
void
Allocator_free(const Allocator allocator, Nullable(const void*) data, size_t size);

static inline
void
Allocator_free_all(const Allocator a);

MALLOC_FUNC
static inline
warn_unused
Nonnull(char*)
Allocator_strndup(const Allocator allocator, Nonnull(const char*)str, size_t length);

#endif
