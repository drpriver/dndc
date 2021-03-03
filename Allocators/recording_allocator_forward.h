#ifndef RECORDING_ALLOCATOR_FORWARD_H
#define RECORDING_ALLOCATOR_FORWARD_H
#include "common_macros.h"
typedef struct RecordingAllocator RecordingAllocator;

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
recording_zalloc(Nonnull(RecordingAllocator*) restrict s, size_t size);

static
void
recording_free(Nonnull(RecordingAllocator*)la, Nullable(const void*) data, size_t size);

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
recording_alloc(Nonnull(RecordingAllocator*) restrict s, size_t size);

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
recording_zalloc(Nonnull(RecordingAllocator*) restrict s, size_t size);

static inline
void
recording_free_all(Nonnull(RecordingAllocator*) s);

static inline
Nonnull(void*)
ALLOCATOR_SIZE(4)
recording_realloc(Nonnull(RecordingAllocator*)la, Nullable(void*)data, size_t orig_size, size_t new_size);
#endif
