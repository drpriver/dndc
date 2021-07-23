#ifndef ALLOCATOR_H
#define ALLOCATOR_H
// size_t
#include <stddef.h>
// free
#include <stdlib.h>

#ifndef warn_unused

#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused _Check_return
#else
#define warn_unused
#endif

#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

#ifndef MALLOC_FUNC
#if defined(__GNUC__) || defined(__clang__)
#define MALLOC_FUNC __attribute__((malloc))
#else
#define MALLOC_FUNC
#endif
#endif

#ifndef unhandled_error_condition
#define unhandled_error_condition(cond) assert(!(cond))
#endif

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline __attribute__((always_inline))
#else
#define force_inline
#endif
#endif

#ifndef const_free
#ifdef __clang__
#define const_free(ptr) do{\
    _Pragma("clang diagnostic push");\
    _Pragma("clang diagnostic ignored \"-Wcast-qual\"");\
    free((void*)ptr); \
    _Pragma("clang diagnostic pop");\
    }while(0)
#elif defined(__GNUC__)
#define const_free(ptr) do{\
    _Pragma("GCC diagnostic push");\
    _Pragma("GCC diagnostic ignored \"-Wdiscarded-qualifiers\"");\
    free((void*)ptr); \
    _Pragma("GCC diagnostic pop");\
    }while(0)
#else
    #define const_free(ptr) free((void*)ptr)
#endif
#endif


enum AllocatorType {
    ALLOCATOR_UNSET = 0,
    ALLOCATOR_MALLOC = 1,
    ALLOCATOR_LINEAR = 2,
    ALLOCATOR_RECORDED = 3,
    ALLOCATOR_ARENA = 4,
};

typedef struct ArenaAllocator {
    struct Arena*_Nullable arena;
    struct BigAllocation*_Nullable big_allocations;
} ArenaAllocator;


typedef struct Allocator {
    enum AllocatorType type;
    // 4 bytes of padding
    void* _data;
} Allocator;


MALLOC_FUNC
static inline
warn_unused
void*
Allocator_alloc(Allocator allocator, size_t size);

MALLOC_FUNC
static inline
warn_unused
void*
Allocator_zalloc(Allocator allocator, size_t size);

static inline
warn_unused
void*
Allocator_realloc(Allocator allocator, void*_Nullable data, size_t orig_size, size_t size);

static inline
warn_unused
void*
Allocator_dupe(Allocator allocator, const void* data, size_t size);

static inline
void
Allocator_free(Allocator allocator, const void*_Nullable data, size_t size);

static inline
void
Allocator_free_all(const Allocator a);

MALLOC_FUNC
static inline
warn_unused
char*
Allocator_strndup(Allocator allocator, const char* str, size_t length);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
