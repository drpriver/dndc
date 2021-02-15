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
    // Apparently there's no good way to use const members
    // if you want to malloc things
    // Really, _data and _capacity should be const as they are set once
    // when we allocate the arena and then never again, but the language
    // semantics don't allow that apparently.
    NullUnspec(void*)  _data;
    size_t _capacity; // if over _capacity, we start mallocing
    size_t _cursor;
    size_t high_water;
    NullUnspec(const char*) name; // for logging purposes
    } LinearAllocator;
// static asserts to keep in sync with D
_Static_assert(sizeof(LinearAllocator) == 40, "");
_Static_assert(offsetof(LinearAllocator, _data)==0, "");
_Static_assert(offsetof(LinearAllocator,_capacity)==8, "");
_Static_assert(offsetof(LinearAllocator,_cursor)==16, "");
_Static_assert(offsetof(LinearAllocator,high_water)==24, "");
_Static_assert(offsetof(LinearAllocator,name)==32, "");

typedef struct LinearAllocatorCheckpoint{
    // this is a bit dangerous to use, but this auto's you back up to an earlier allocation state.
    // This can be useful for deep callgraphs that need lots of scratch space, but only for some
    // computations or communication.
    size_t _cursor;
    } LinearAllocatorCheckpoint;

static inline warn_unused
LinearAllocatorCheckpoint
get_linear_checkpoint(Nonnull(LinearAllocator*)s){
    return (LinearAllocatorCheckpoint){
        ._cursor=s->_cursor,
        };
    }
static inline
void
reset_to_linear_checkpoint(Nonnull(LinearAllocator*)s, LinearAllocatorCheckpoint checkpoint){
    s->_cursor = checkpoint._cursor;
    }

/// name is strdup'd
static inline warn_unused
LinearAllocator
new_linear_storage(size_t size, Nullable(const char*) name){
    // malloc has to return a pointer suitably aligned for any object,
    // so we don't have to do any alignment fixup
    void* _data = malloc(size);
    assert(_data);
    return (LinearAllocator){
        ._data = _data,
        ._capacity=size,
        ._cursor=0,
        .high_water=0,
        .name = name?strdup(name):name,
        };
    }

/// name is duped by the allocator
static inline warn_unused
LinearAllocator
mnew_linear_storage(Nonnull(const Allocator*)a , size_t size, Nullable(const char*) name){
    // malloc has to return a pointer suitably aligned for any object,
    // so we don't have to do any alignment fixup
    void* _data = Allocator_alloc(a, size);
    assert(_data);
    return (LinearAllocator){
        ._data = _data,
        ._capacity=size,
        ._cursor=0,
        .high_water=0,
        .name = name?Allocator_dupe(a, (const void*)name, strlen(name)+1):name,
        };
    }

static inline
void
linear_reset(Nonnull(LinearAllocator*) s){
    s->_cursor = 0;
    }

static inline
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
static inline
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
        assert(result);
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
static inline
warn_unused
Nonnull(void*)
linear_alloc(Nonnull(LinearAllocator*) restrict s, size_t size){
    enum {GENERIC_ALIGNMENT = 8}; // lmao, but this allows for u64s on 32 bit platforms
    _Static_assert(sizeof(void*) <= GENERIC_ALIGNMENT, "");
    return linear_aligned_alloc(s, size, GENERIC_ALIGNMENT);
    }


MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
Nonnull(void*)
linear_aligned_zalloc(Nonnull(LinearAllocator*) restrict s, size_t size, size_t alignment){
    void* result = linear_aligned_alloc(s, size, alignment);
    memset(result, 0, size);
    return result;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline warn_unused
Nonnull(void*)
linear_zalloc(Nonnull(LinearAllocator*) restrict s, size_t size){
    return linear_aligned_zalloc(s, size, _Alignof(void*));
    }

MALLOC_FUNC
ALLOCATOR_SIZE(3)
static inline warn_unused
Nonnull(void*)
linear_aligned_dupe(Nonnull(LinearAllocator*) restrict s, Nonnull(const void*) restrict src, size_t size, size_t alignment){
    void* result = linear_aligned_alloc(s, size, alignment);
    memcpy(result, src, size);
    return result;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(3)
static inline warn_unused
Nonnull(void*)
linear_dupe(Nonnull(LinearAllocator*) restrict s, Nonnull(const void*) restrict src, size_t size){
    return linear_aligned_dupe(s, src, size, _Alignof(void*));
    }

printf_func(2, 3)
static inline warn_unused
Nonnull(char*)
lprintf(Nonnull(LinearAllocator*) restrict s, Nonnull(const char*) restrict fmt, ...){
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);
    auto _msg_size = vsnprintf(NULL, 0, fmt, args)+1;
    va_end(args);
    auto buff = linear_aligned_alloc(s, _msg_size, 1);
    vsprintf(buff, fmt, args2);
    va_end(args2);
    return buff;
    }

static inline warn_unused
Nonnull(char*)
lvprintf(Nonnull(LinearAllocator*) restrict s, Nonnull(const char*) restrict fmt, va_list args){
    va_list args2;
    va_copy(args2, args);
    auto msg_size = vsnprintf(NULL, 0, fmt, args)+1;
    va_end(args);
    auto buff = linear_aligned_zalloc(s, msg_size, 1);
    vsprintf(buff, fmt, args2);
    va_end(args2);
    return buff;
    }

static inline
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

static inline
Allocator
Allocator_from_linear_allocator(Nonnull(LinearAllocator*)la){
    Allocator allocator = {
        ._allocator_data = la,
        .alloc = (alloc_func)linear_alloc,
        .zalloc = (alloc_func)linear_zalloc,
        .realloc = (realloc_func)linear_realloc,
        .free = (free_func)linear_free,
        .free_all = (free_all_func)linear_reset,
        };
    return allocator;
    }
#endif
