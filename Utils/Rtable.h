//
// Copyright © 2022, David Priver
//
#ifndef RTABLE_H
#define RTABLE_H
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include "Allocators/allocator.h"
#include "Utils/hash_func.h"

typedef struct RtableIndex RtableIndex;
struct RtableIndex {
    uint32_t idx;
    uint32_t hash;
};
#endif


#if !defined(likely) && !defined(unlikely)
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif

#ifndef warn_unused
#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif
#endif

// Features
// --------
//
// Preserves insertion order.
// Supports deletion.


typedef struct RTABLE RTABLE;
struct RTABLE {
    size_t count;
    size_t capacity;
    char data[];
};


#ifdef RT_VALUE_TYPE
#ifndef RT_HASH_FUNC
#define RT_HASH_FUNC(k) hash_alignany(k, sizeof(*k))
#endif

#ifndef RT_EQ
#define RT_EQ(k1, k2) ((*k1) == (*k2))
#endif
#endif

#ifndef RT_THRESH
#define RT_THRESH 8
#endif


#define RT_ITEM_SIZE (sizeof(RTABLE_K)+sizeof(RTABLE_V))

static inline
size_t
RT_sizeof(RTABLE*_Nullable rt){
    if(!rt) return 0;
    if(rt->capacity > RT_THRESH){
    }
    else {
    }
}

static inline
warn_unused
int
RT_check_size(RTABLE*_Nullable*_Nonnull rt, Allocator a){
    RTABLE* rtable = *rt;
    if(!rtable){
        if(RT_THRESH > 0){
            enum {INITIAL_CAPACITY=RT_THRESH>4?4:RT_THRESH};
            enum {INITIAL_SIZE=INITIAL_CAPACITY*RT_ITEM_SIZE + sizeof(RTABLE)};
            rtable = Allocator_alloc(a, INITIAL_SIZE);
            if(unlikely(!rtable))
                return 1;
            rtable->capacity = INITIAL_CAPACITY;
            rtable->count = 0;
            *rt = rtable;
        }
        else {
            enum {INITIAL_CAPACITY=4};
            enum {INITIAL_SIZE=INITIAL_CAPACITY*RT_ITEM_SIZE+INITIAL_CAPACITY*sizeof(RtableIndex)+sizeof(RTABLE)};
            rtable = Allocator_alloc(a, INITIAL_SIZE);
            if(unlikely(!rtable))
                return 1;
            rtable->capacity = INITIAL_CAPACITY;
            rtable->count = 0;
            *rt = rtable;
        }
    }
    else if(rtable->capacity > RT_THRESH){ // is already a hash table
        if(rtable->count*2 >= rtable->capacity){
        }
    }
    else if (rtable->count == rtable->capacity

}
