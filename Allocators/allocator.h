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
typedef void (*cleanup_func)(Nonnull(void*));

/*
 * It's generally easier to just use the Allocator_* functions instead
 * of invoking these function pointers directly.
 */
typedef struct AllocatorVtable {
    const Nonnull(alloc_func) alloc;
    const Nonnull(alloc_func) zalloc;
    const Nonnull(realloc_func) realloc;
    const Nonnull(free_func) free;
    const Nullable(free_all_func) free_all;
    const Nullable(cleanup_func) cleanup;
} AllocatorVtable;

typedef struct Allocator {
    Nonnull(void*) _data;
    Nonnull(const AllocatorVtable*) _vtable;
} Allocator;


MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_alloc(const Allocator allocator, size_t size);

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_zalloc(const Allocator allocator, size_t size);

ALLOCATOR_SIZE(4)
static inline
// force_inline
warn_unused
Nonnull(void*)
Allocator_realloc(const Allocator allocator, Nullable(void*) data, size_t orig_size, size_t size);

// MALLOC_FUNC
ALLOCATOR_SIZE(3)
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
Allocator_free_all(const Allocator a){
    unhandled_error_condition(!a._vtable->free_all);
    a._vtable->free_all(a._data);
    }

static inline
void
Allocator_cleanup(const Allocator a){
    if(!a._vtable->cleanup)
        return;
    a._vtable->cleanup(a._data);
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_alloc(const Allocator a, size_t size){
    return a._vtable->alloc(a._data, size);
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_zalloc(const Allocator a, size_t size){
    return a._vtable->zalloc(a._data, size);
    }

ALLOCATOR_SIZE(4)
static inline
// force_inline
warn_unused
Nonnull(void*)
Allocator_realloc(const Allocator a, Nullable(void*) data, size_t orig_size, size_t size){
    return a._vtable->realloc(a._data, data, orig_size, size);
    }

static inline
// force_inline
void
Allocator_free(const Allocator a, Nullable(const void*) data, size_t size){
    a._vtable->free(a._data, data, size);
    }

// Can't use malloc func here as we could be copying a struct
// with pointers.
// Should I make a data version of this?
ALLOCATOR_SIZE(3)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_dupe(const Allocator allocator, Nonnull(const void*) data, size_t size){
    void* result = Allocator_alloc(allocator, size);
    memcpy(result, data, size);
    return result;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(3)
static inline
warn_unused
Nonnull(char*)
Allocator_strndup(const Allocator allocator, Nonnull(const char*)str, size_t length){
    char* result = Allocator_alloc(allocator, length+1);
    assert(result);
    if(likely(length))
        memcpy(result, str, length);
    result[length] = '\0';
    return result;
    }

#endif
