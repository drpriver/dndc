#ifndef ALLOCATOR_C
#define ALLOCATOR_C
#include "allocator.h"
#include "linear_allocator.h"
#include "mallocator.h"
#include "recording_allocator.h"

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
    unreachable();
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
    unreachable();
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
            return sane_realloc(data, orig_size, size);
        case ALLOCATOR_RECORDED:
            return recording_realloc(a._data, data, orig_size, size);
    PushDiagnostic();
    SuppressCoveredSwitchDefault();
        default:
            abort();
            break;
    PopDiagnostic();
        }
    unreachable();
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
#endif
