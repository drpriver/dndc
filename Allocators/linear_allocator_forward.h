#ifndef LINEAR_ALLOCATOR_FORWARD_H
#define LINEAR_ALLOCATOR_FORWARD_H
#include "common_macros.h"
typedef struct LinearAllocator LinearAllocator;

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
linear_zalloc(Nonnull(LinearAllocator*) restrict s, size_t size);

static
void
linear_free(Nonnull(LinearAllocator*)la, Nullable(const void*) data, size_t size);

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
linear_alloc(Nonnull(LinearAllocator*) restrict s, size_t size);

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
linear_zalloc(Nonnull(LinearAllocator*) restrict s, size_t size);

static inline
void
linear_reset(Nonnull(LinearAllocator*) s);

static inline
Nonnull(void*)
ALLOCATOR_SIZE(4)
linear_realloc(Nonnull(LinearAllocator*)la, Nullable(void*)data, size_t orig_size, size_t new_size);
#endif
