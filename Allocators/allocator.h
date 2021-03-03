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
#include "linear_allocator_forward.h"
#include "recording_allocator_forward.h"

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
    switch(a.type){
        case ALLOCATOR_UNSET:
            abort();
            break;
        case ALLOCATOR_MALLOC:
            abort();
            break;
        case ALLOCATOR_LINEAR:
            linear_reset(a._data);
            break;
        case ALLOCATOR_RECORDED:
            recording_free_all(a._data);
            break;
    PushDiagnostic();
    SuppressCoveredSwitchDefault();
        default:
            abort();
            break;
    PopDiagnostic();
        }
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_alloc(const Allocator a, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            abort();
            break;
        case ALLOCATOR_LINEAR:
            return linear_alloc(a._data, size);
        case ALLOCATOR_MALLOC:
            return malloc(size);
        case ALLOCATOR_RECORDED:
            return recording_alloc(a._data, size);
    PushDiagnostic();
    SuppressCoveredSwitchDefault();
        default:
            abort();
            break;
    PopDiagnostic();
        }
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_zalloc(const Allocator a, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            abort();
            break;
        case ALLOCATOR_LINEAR:
            return linear_zalloc(a._data, size);
        case ALLOCATOR_MALLOC:
            return calloc(1, size);
        case ALLOCATOR_RECORDED:
            return recording_zalloc(a._data, size);
    PushDiagnostic();
    SuppressCoveredSwitchDefault();
        default:
            abort();
            break;
    PopDiagnostic();
        }
    }

ALLOCATOR_SIZE(4)
static inline
// force_inline
warn_unused
Nonnull(void*)
Allocator_realloc(const Allocator a, Nullable(void*) data, size_t orig_size, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            abort();
            break;
        case ALLOCATOR_LINEAR:
            return linear_realloc(a._data, data, orig_size, size);
        case ALLOCATOR_MALLOC:
            return realloc(data, size);
        case ALLOCATOR_RECORDED:
            return recording_realloc(a._data, data, orig_size, size);
    PushDiagnostic();
    SuppressCoveredSwitchDefault();
        default:
            abort();
            break;
    PopDiagnostic();
        }
    }

static inline
// force_inline
void
Allocator_free(const Allocator a, Nullable(const void*) data, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            abort();
            break;
        case ALLOCATOR_LINEAR:
            linear_free(a._data, data, size);
            break;
        case ALLOCATOR_MALLOC:
            const_free(data);
            break;
        case ALLOCATOR_RECORDED:
            recording_free(a._data, data, size);
            break;
    PushDiagnostic();
    SuppressCoveredSwitchDefault();
        default:
            abort();
            break;
    PopDiagnostic();
        }
    }

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
    unhandled_error_condition(!result);
    if(likely(length))
        memcpy(result, str, length);
    result[length] = '\0';
    return result;
    }
// FIXME:
#include "linear_allocator.h"
#include "recording_allocator.h"
#include "mallocator.h"
#endif
