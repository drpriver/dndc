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
    const Nullable(realloc_func) realloc;
    const Nonnull(free_func) free;
    // If this function is null, indicates the allocator does not support freeing everything
    const Nullable(free_all_func) free_all;
    } Allocator;

static inline
force_inline
warn_unused
bool
Allocator_supports_realloc(Nonnull(const Allocator*) a){
    return !!(a->realloc);
    }

static inline
force_inline
warn_unused
bool
Allocator_supports_free_all(Nonnull(const Allocator*) a){
    return !!(a->free_all);
    }

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
Nonnull(void*)
allocator_wrapped_malloc(Nullable(void*) _unused, size_t size){
    (void)_unused;
    void* result = malloc(size);
    assert(result);
    return result;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
Nonnull(void*)
allocator_wrapped_zalloc(Nullable(void*) _unused, size_t size){
    (void)_unused;
    void* result = calloc(1, size);
    assert(result);
    return result;
    }

ALLOCATOR_SIZE(4)
static inline
Nonnull(void*)
allocator_wrapped_realloc(Nullable(void*) _unused, Nullable(void*) data, size_t orig_size, size_t new_size){
    (void)_unused;
    (void)orig_size;
    void* result = realloc(data, new_size);
    assert(result);
    return result;
    }

static inline
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

static const Allocator MallocAllocator = {
    ._allocator_data = NULL,
    .alloc           = allocator_wrapped_malloc,
    .zalloc          = allocator_wrapped_zalloc,
    .realloc         = allocator_wrapped_realloc,
    .free            = allocator_wrapped_free,
    };

static inline
force_inline
Nonnull(const Allocator*)
get_mallocator(void){
    return &MallocAllocator;
    }

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
    // TODO: allocators are allowed to not have realloc but
    // should they just return NULL if they don't support it?
    if(!allocator->realloc)
        return Allocator_fallback_realloc(allocator, data, orig_size, size);
    assert(allocator->realloc);
    return allocator->realloc(allocator->_allocator_data, data, orig_size, size);
    }

ALLOCATOR_SIZE(4)
static inline
warn_unused
Nonnull(void*)
Allocator_fallback_realloc(Nonnull(const Allocator*) allocator, Nullable(void*) data, size_t orig_size, size_t size){
    auto new_data = Allocator_alloc(allocator, size);
    assert(size >= orig_size);
    memcpy(new_data, data, orig_size);
    Allocator_free(allocator, data, orig_size);
    return new_data;
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

MALLOC_FUNC
printf_func(2, 3)
static inline warn_unused
Nonnull(char*)
mprintf(Nonnull(const Allocator*) a, Nonnull(const char*) restrict fmt, ...){
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);
    auto _msg_size = vsnprintf(NULL, 0, fmt, args)+1;
    va_end(args);
    auto buff = Allocator_alloc(a, _msg_size);
    vsprintf(buff, fmt, args2);
    va_end(args2);
    return buff;
    }

MALLOC_FUNC
static inline warn_unused
Nonnull(char*)
vmprintf(Nonnull(const Allocator*) a, Nonnull(const char*) restrict fmt, va_list args){
    va_list args2;
    va_copy(args2, args);
    auto _msg_size = vsnprintf(NULL, 0, fmt, args)+1;
    auto buff = Allocator_alloc(a, _msg_size);
    vsprintf(buff, fmt, args2);
    va_end(args2);
    return buff;
    }

#endif
