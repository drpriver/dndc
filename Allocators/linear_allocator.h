#ifndef LINEAR_ALLOCATOR_H
#define LINEAR_ALLOCATOR_H
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "common_macros.h"
#include "allocator.h"
typedef struct LinearAllocator {
    NullUnspec(void*)  _data;
    size_t _capacity; // if over _capacity, we start mallocing
    size_t _cursor;
    size_t high_water;
    NullUnspec(const char*) name; // for logging purposes
} LinearAllocator;

/// name is strdup'd
static inline warn_unused
LinearAllocator
new_linear_storage(size_t size, Nullable(const char*) name){
    // malloc has to return a pointer suitably aligned for any object,
    // so we don't have to do any alignment fixup
    void* _data = malloc(size);
    unhandled_error_condition(!_data);
    return (LinearAllocator){
        ._data = _data,
        ._capacity=size,
        ._cursor=0,
        .high_water=0,
        .name = name?strdup(name):name,
        };
    }

static inline
void
linear_reset(Nonnull(LinearAllocator*) s){
    s->_cursor = 0;
    }

static
void
destroy_linear_storage(Nonnull(LinearAllocator*) s){
    free(s->_data);
    const_free(s->name);
    s->name = NULL;
    s->_data = NULL;
    s->_capacity = 0;
    s->_cursor = 0;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
linear_aligned_alloc(Nonnull(LinearAllocator*) restrict s, size_t size, size_t alignment){
    uintptr_t val = (uintptr_t)s->_data;
    val += s->_cursor;
    // alignment is always a power of 2
    size_t align_mod = val & (alignment - 1);
    if(align_mod){
        s->_cursor += alignment - align_mod;
        }
    if(s->_cursor + size > s->_capacity){
        // fall back to malloc
        ERROR("Exceeded temporary storage capacity for '%s'! Wanted an additional %zu bytes, but only %zu left.\n", s->name?:"(unnamed)", size, s->_capacity - s->_cursor);
        s->high_water = s->_cursor + size;
        // leak
        void* result =  malloc(size);
        unhandled_error_condition(!result);
        return result;
        }
    void* result = ((char*)s->_data) + s->_cursor;
    s->_cursor += size;
    if(s->_cursor > s->high_water){
        s->high_water = s->_cursor;
        }
    return result;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
linear_alloc(Nonnull(LinearAllocator*) restrict s, size_t size){
    enum {GENERIC_ALIGNMENT = 8}; // lmao, but this allows for u64s on 32 bit platforms
    _Static_assert(sizeof(void*) <= GENERIC_ALIGNMENT, "");
    return linear_aligned_alloc(s, size, GENERIC_ALIGNMENT);
    }


MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
linear_aligned_zalloc(Nonnull(LinearAllocator*) restrict s, size_t size, size_t alignment){
    void* result = linear_aligned_alloc(s, size, alignment);
    memset(result, 0, size);
    return result;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static
warn_unused
Nonnull(void*)
linear_zalloc(Nonnull(LinearAllocator*) restrict s, size_t size){
    return linear_aligned_zalloc(s, size, _Alignof(void*));
    }

static
void
linear_free(Nonnull(LinearAllocator*)la, Nullable(const void*) data, size_t size){
    if(!data)
        return;
    assert(size);
    if(la->_cursor + (const char*)la->_data == (const char*)data + size){
        la->_cursor -= size;
        }
    }

static inline
Nonnull(void*)
ALLOCATOR_SIZE(4)
linear_realloc(Nonnull(LinearAllocator*)la, Nullable(void*)data, size_t orig_size, size_t new_size){
    // only support growing
    assert(new_size > orig_size);
    if(!data){
        return linear_alloc(la, new_size);
        }
    assert(new_size);
    // check if we can extend in place.
    if(la->_cursor + (char*)la->_data == (char*)data+orig_size){
        la->_cursor += new_size - orig_size;
        if(la->_cursor > la->high_water){
            la->high_water = la->_cursor;
            }
        return (void*)data; // cast to shut up nullability
        }
    // just do a memcpy
    void* result = linear_alloc(la, new_size);
    memcpy(result, data, orig_size);
    return result;
    }

static const AllocatorVtable LinearAllocatorVtable = {
    .alloc = (alloc_func)linear_alloc,
    .zalloc = (alloc_func)linear_zalloc,
    .realloc = (realloc_func)linear_realloc,
    .free = (free_func)linear_free,
    .free_all = (free_all_func)linear_reset,
    .cleanup = (cleanup_func)destroy_linear_storage,
    };

static inline
Allocator
allocator_from_la(Nonnull(LinearAllocator*)la){
    return (Allocator){
        ._data = la,
        ._vtable = &LinearAllocatorVtable,
        };
    }


#endif
