//
// Copyright © 2021-2022, David Priver
//
#ifndef ALLOCATOR_C
#define ALLOCATOR_C
#include <stddef.h>
// abort, free, malloc, calloc
#include <stdlib.h>
#include <assert.h>

#ifdef __APPLE__
#include <malloc/malloc.h>
#endif

#include "allocator.h"
#include "mallocator.h"
#include "arena_allocator.h"

#ifdef USE_RECORDED_ALLOCATOR
#include "recording_allocator.h"
#endif

#ifdef USE_TESTING_ALLOCATOR
#include "testing_allocator.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef unreachable
#if defined(__GNUC__) || defined(__clang__)
#define unreachable() __builtin_unreachable()
#else
#define unreachable() __assume(0)
#endif
#endif

#ifndef sane_realloc
// Realloc's signature is silly which makes it hard to
// reimplement in a sane way. So in order to accomodate
// platforms where we need to implement it ourselves
// (aka WASM), we use this compatibility macro.
#ifndef WASM
#define sane_realloc(ptr, orig_size, size) realloc(ptr, size)
#else
static void*_Nullable sane_realloc(void* ptr, size_t orig_size, size_t size);
#endif
#endif

#ifdef DEBUGGING_H
#define bad() bt(), abort()
#else
#define bad() abort()
#endif


static inline
int
Allocator_supports_free_all(Allocator a){
    switch(a.type){
        case ALLOCATOR_UNSET:
            bad();
        case ALLOCATOR_MALLOC:
        case ALLOCATOR_NULL:
            return 0;
        case ALLOCATOR_ARENA:
            return 1;
#ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            return 1;
#endif
#ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            return 1;
#endif
    }
    bad();
}

static inline
void
Allocator_free_all(Allocator a){
    switch(a.type){
        case ALLOCATOR_UNSET:
            bad();
            return;
        case ALLOCATOR_MALLOC:
            bad();
            return;
        case ALLOCATOR_ARENA:
            ArenaAllocator_free_all(a._data);
            return;
        case ALLOCATOR_NULL:
            bad();
            return;
#ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            recording_free_all(a._data);
            return;
#endif
#ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            testing_free_all(a._data);
            return;
#endif
    }
    bad();
}

MALLOC_FUNC
static inline
warn_unused
// force_inline
void*_Nullable
Allocator_alloc(Allocator a, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            bad();
            break;
        case ALLOCATOR_MALLOC:
            return malloc(size);
        case ALLOCATOR_ARENA:
            return ArenaAllocator_alloc(a._data, size);
        case ALLOCATOR_NULL:
            return NULL;
#ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            return recording_alloc(a._data, size);
#endif
#ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            return testing_alloc(a._data, size);
#endif
    }
    bad();
    unreachable();
}

MALLOC_FUNC
static inline
warn_unused
// force_inline
void*_Nullable
Allocator_zalloc(Allocator a, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            bad();
            break;
        case ALLOCATOR_MALLOC:
            return calloc(1, size);
        case ALLOCATOR_ARENA:
            return ArenaAllocator_zalloc(a._data, size);
        case ALLOCATOR_NULL:
            return NULL;
#ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            return recording_zalloc(a._data, size);
#endif
#ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            return testing_zalloc(a._data, size);
#endif
    }
    bad();
    unreachable();
}

static inline
// force_inline
warn_unused
void*_Nullable
Allocator_realloc(Allocator a, void*_Nullable data, size_t orig_size, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            bad();
            break;
        case ALLOCATOR_MALLOC:
            return sane_realloc(data, orig_size, size);
        case ALLOCATOR_ARENA:
            return ArenaAllocator_realloc(a._data, data, orig_size, size);
        case ALLOCATOR_NULL:
            return NULL;
#ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            return recording_realloc(a._data, data, orig_size, size);
#endif
#ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            return testing_realloc(a._data, data, orig_size, size);
#endif
    }
    bad();
    unreachable();
}

static inline
// force_inline
void
Allocator_free(Allocator a, const void*_Nullable data, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            bad();
            return;
        case ALLOCATOR_MALLOC:
            const_free(data);
            return;
        case ALLOCATOR_ARENA:
            ArenaAllocator_free(a._data, data, size);
            return;
        case ALLOCATOR_NULL:
            return;
#ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            recording_free(a._data, data, size);
            return;
#endif
#ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            testing_free(a._data, data, size);
            return;
#endif
    }
    bad();
}

static inline
// force_inline
size_t
Allocator_good_size(Allocator a, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            bad();
            return size;
        case ALLOCATOR_MALLOC:
            #ifdef __APPLE__
                return malloc_good_size(size);
            #else
                return size;
            #endif
        case ALLOCATOR_ARENA:
            return ArenaAllocator_round_size_up(size);
        case ALLOCATOR_NULL:
            return size;
#ifdef USE_RECORDED_ALLOCATOR
        case ALLOCATOR_RECORDED:
            #ifdef __APPLE__
                return malloc_good_size(size);
            #else
                return size;
            #endif
#endif
#ifdef USE_TESTING_ALLOCATOR
        case ALLOCATOR_TESTING:
            #ifdef __APPLE__
                return malloc_good_size(size);
            #else
                return size;
            #endif
#endif
    }
    bad();
}

static inline
warn_unused
// force_inline
void*_Nullable
Allocator_dupe(Allocator allocator, const void* data, size_t size){
    void* result = Allocator_alloc(allocator, size);
    if(!result) return NULL;
    if(size)
        memcpy(result, data, size);
    return result;
}

MALLOC_FUNC
static inline
warn_unused
char*_Nullable
Allocator_strndup(Allocator allocator, const char* str, size_t length){
    char* result = Allocator_alloc(allocator, length+1);
    if(!result) return NULL;
    if(length)
        memcpy(result, str, length);
    result[length] = '\0';
    return result;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
