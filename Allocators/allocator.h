#ifndef ALLOCATOR_H
#define ALLOCATOR_H
// malloc, free
#include <stdlib.h>
// memcpy, memset
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include "common_macros.h"

typedef Nonnull(void*) (* alloc_func)(Nullable(void*), size_t);
typedef Nonnull(void*) (* realloc_func)(Nullable(void*), Nullable(void*), size_t, size_t);
typedef void (*free_func)(Nullable(void*), Nullable(const void*), size_t);
typedef void (*free_all_func)(Nonnull(void*));

/*
 * It's generally easier to just use the Allocator_* functions instead
 * of invoking these function pointers directly.
 */
typedef struct Allocator {
    Nonnull(void*) _allocator_data;
    const Nonnull(alloc_func) alloc;
    const Nonnull(alloc_func) zalloc;
    // Not all allocators support realloc
    const Nonnull(realloc_func) realloc;
    const Nonnull(free_func) free;
    // If this function is null, indicates the allocator does not support freeing everything
    const Nullable(free_all_func) free_all;
} Allocator;


MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_alloc(Nonnull(const Allocator*) allocator, size_t size);

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_zalloc(Nonnull(const Allocator*) allocator, size_t size);

ALLOCATOR_SIZE(4)
static inline
warn_unused
Nonnull(void*)
Allocator_fallback_realloc(Nonnull(const Allocator*) allocator, Nullable(void*) data, size_t orig_size, size_t size);

ALLOCATOR_SIZE(4)
static inline
// force_inline
warn_unused
Nonnull(void*)
Allocator_realloc(Nonnull(const Allocator*) allocator, Nullable(void*) data, size_t orig_size, size_t size);

static inline
// force_inline
void
Allocator_free(Nonnull(const Allocator*) allocator, Nullable(const void*) data, size_t size);

static inline
void
Allocator_free_all(Nonnull(const Allocator*) allocator){
    unhandled_error_condition(!allocator->free_all);
    allocator->free_all(allocator->_allocator_data);
    }


// MALLOC_FUNC
ALLOCATOR_SIZE(3)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_dupe(Nonnull(const Allocator*) allocator, Nonnull(const void*) data, size_t size);


MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_alloc(Nonnull(const Allocator*) allocator, size_t size){
    return allocator->alloc(allocator->_allocator_data, size);
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_zalloc(Nonnull(const Allocator*) allocator, size_t size){
    return allocator->zalloc(allocator->_allocator_data, size);
    }

ALLOCATOR_SIZE(4)
static inline
// force_inline
warn_unused
Nonnull(void*)
Allocator_realloc(Nonnull(const Allocator*) allocator, Nullable(void*) data, size_t orig_size, size_t size){
    return allocator->realloc(allocator->_allocator_data, data, orig_size, size);
    }

static inline
// force_inline
void
Allocator_free(Nonnull(const Allocator*) allocator, Nullable(const void*) data, size_t size){
    allocator->free(allocator->_allocator_data, data, size);
    }

// Can't use malloc func here as we could be copying a struct
// with pointers.
// Should I make a data version of this?
ALLOCATOR_SIZE(3)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_dupe(Nonnull(const Allocator*) allocator, Nonnull(const void*) data, size_t size){
    void* result = Allocator_alloc(allocator, size);
    memcpy(result, data, size);
    return result;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(3)
static inline
warn_unused
Nonnull(char*)
Allocator_strndup(Nonnull(const Allocator*)allocator, Nonnull(const char*)str, size_t length){
    char* result = Allocator_alloc(allocator, length+1);
    assert(result);
    if(likely(length))
        memcpy(result, str, length);
    result[length] = '\0';
    return result;
    }

#endif
