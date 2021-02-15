#ifndef MARRAY_H
#define MARRAY_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "allocator.h"
#include "common_macros.h"
#include "error_handling.h"

static inline
size_t
resize_to_some_weird_number(uint64_t x){
/**
 * If given a power of two number, gives that number roughly * 1.5
 * Any other number will give the next largest power of 2.
 * This leads to a growth rate of sort of sqrt(2)
 */
    _Static_assert(sizeof(uint64_t) == sizeof(unsigned long long), "fuu");
    if(x < 4)
        return 4;
    if(x == 4)
        return 8;
    if(x <= 8)
        return 16;
    // grow by factor of approx sqrt(2)
    // I have no idea if this is ideal, but it has a nice elegance to it
    auto cnt = __builtin_popcountll(x);
    uint64_t result;
    if(cnt == 1){
        result =  x | (x >> 1);
        }
    else {
        // count leading zeros for the unaware
        auto clz = __builtin_clzll(x);
        result = 1ull << (64 - clz);
        }
    return result;
    }




// Level of indirection is necessary for it to work properly
// with macros.
#define MARRAYIMPL(meth, type) Marray##_##meth##__##type
#define Marray(type) MarrayI(type)
#define MarrayI(type) Marray__##type
#define Marray_push(type) MARRAYIMPL(push, type)
#define Marray_pop(type) MARRAYIMPL(pop, type)
#define Marray_cleanup(type) MARRAYIMPL(cleanup, type)
#define Marray_find(type) MARRAYIMPL(find, type)
#define Marray_reserve(type) MARRAYIMPL(reserve, type)
#define Marray_clear(type) MARRAYIMPL(clear, type)
#define Marray_peek(type) MARRAYIMPL(peek, type)
#define Marray_extend(type) MARRAYIMPL(extend, type)
#define Marray_insert(type) MARRAYIMPL(insert, type)
#define Marray_remove(type) MARRAYIMPL(remove, type)
#define Marray_from(type) MARRAYIMPL(from, type)
#define Marray_alloc(type) MARRAYIMPL(alloc, type)
#define Marray_alloc_index(type) MARRAYIMPL(alloc_index, type)
#define Marray_ensure(type) MARRAYIMPL(ensure, type)
#endif

#ifndef MARRAY_T
#error "Must define MARRAY_T"
#endif

#ifndef MARRAY_IMPL_ONLY
typedef struct Marray(MARRAY_T) {
    size_t capacity;
    size_t count;
    NullUnspec(MARRAY_T*) data;
} Marray(MARRAY_T);

static inline
void
Marray_ensure(MARRAY_T)(Nonnull(Marray(MARRAY_T)*), Nonnull(const Allocator*), size_t);
static inline void Marray_push(MARRAY_T)(Nonnull(Marray(MARRAY_T)*), Nonnull(const Allocator*), MARRAY_T);
static inline warn_unused Nonnull(MARRAY_T*) Marray_alloc(MARRAY_T)(Nonnull(Marray(MARRAY_T)*), Nonnull(const Allocator*));
static inline warn_unused size_t Marray_alloc_index(MARRAY_T)(Nonnull(Marray(MARRAY_T)*), Nonnull(const Allocator*));
static inline void Marray_insert(MARRAY_T)(Nonnull(Marray(MARRAY_T)*),Nonnull(const Allocator*), size_t, MARRAY_T);
static inline void Marray_remove(MARRAY_T)(Nonnull(Marray(MARRAY_T)*), size_t);
static inline
void Marray_extend(MARRAY_T)(Nonnull(Marray(MARRAY_T)*), Nonnull(const Allocator*), Nonnull(const MARRAY_T*) , size_t);
static inline
warn_unused Marray(MARRAY_T) Marray_from(MARRAY_T)(Nonnull(const Allocator*), Nonnull(const MARRAY_T*), size_t);
static inline MARRAY_T
Marray_pop(MARRAY_T)(Nonnull(Marray(MARRAY_T)*));
static inline warn_unused MARRAY_T Marray_peek(MARRAY_T)(Nonnull(Marray(MARRAY_T)*));
static inline void Marray_reserve(MARRAY_T)(Nonnull(Marray(MARRAY_T)*), Nonnull(const Allocator*), size_t);
static inline void Marray_cleanup(MARRAY_T)(Nonnull(Marray(MARRAY_T)*), Nonnull(const Allocator*));
static inline force_inline void Marray_clear(MARRAY_T)(Nonnull(Marray(MARRAY_T)*));
static inline Errorable_f(size_t) Marray_find(MARRAY_T)(Nonnull(Marray(MARRAY_T)*), MARRAY_T);
#endif

#ifndef MARRAY_DECL_ONLY
PushDiagnostic()
SuppressUnusedFunction()

static inline void
Marray_ensure(MARRAY_T)(Nonnull(Marray(MARRAY_T)*)marray, Nonnull(const Allocator*)a, size_t n_additional){
    size_t required_capacity = marray->count + n_additional;
    if(marray->capacity >= required_capacity)
        return;
    size_t new_capacity;
    if(required_capacity < 8)
        new_capacity = 8;
    else {
        new_capacity = resize_to_some_weird_number(marray->capacity);
        while(new_capacity < required_capacity) {
            new_capacity = resize_to_some_weird_number(new_capacity);
            }
        }
    marray->data = Allocator_realloc(a, marray->data, marray->capacity*sizeof(MARRAY_T), new_capacity*sizeof(MARRAY_T));
    assert(marray->data);
    marray->capacity = new_capacity;
    }

static inline
void
Marray_push(MARRAY_T)(Nonnull(Marray(MARRAY_T)*) marray, Nonnull(const Allocator*)a, MARRAY_T value){
    Marray_ensure(MARRAY_T)(marray, a, 1);
    marray->data[marray->count] = value;
    marray->count++;
    }

static inline
Nonnull(MARRAY_T*)
Marray_alloc(MARRAY_T)(Nonnull(Marray(MARRAY_T)*) marray, Nonnull(const Allocator*)a){
    Marray_ensure(MARRAY_T)(marray, a, 1);
    MARRAY_T* result = &marray->data[marray->count];
    marray->count++;
    return result;
    }
static inline
size_t
Marray_alloc_index(MARRAY_T)(Nonnull(Marray(MARRAY_T)*) marray, Nonnull(const Allocator*)a){
    Marray_ensure(MARRAY_T)(marray, a, 1);
    return marray->count++;
    }

static inline
void
Marray_insert(MARRAY_T)(Nonnull(Marray(MARRAY_T)*)marray, Nonnull(const Allocator*)a, size_t index, MARRAY_T value){
    assert(index < marray->count+1);
    if(index == marray->count){
        Marray_push(MARRAY_T)(marray, a, value);
        return;
        }
    Marray_ensure(MARRAY_T)(marray, a, 1);
    size_t n_move = marray->count - index;
    memmove(marray->data+index+1, marray->data+index, n_move*sizeof(marray->data[0]));
    marray->data[index] = value;
    marray->count++;
    }

static inline
void
Marray_remove(MARRAY_T)(Nonnull(Marray(MARRAY_T)*)marray, size_t index){
    assert(index < marray->count);
    if(index == marray->count-1){
        marray->count--;
        return;
        }
    size_t n_move = marray->count - index - 1;
    memmove(marray->data+index, marray->data+index+1, n_move*(sizeof(marray->data[0])));
    marray->count--;
    return;
    }

static inline
void
Marray_extend(MARRAY_T)(Nonnull(Marray(MARRAY_T)*) marray, Nonnull(const Allocator*) a, Nonnull(const MARRAY_T*) values, size_t n_values){
    Marray_ensure(MARRAY_T)(marray, a, n_values);
    memcpy(marray->data+marray->count, values, n_values*(sizeof(marray->data[0])));
    marray->count+=n_values;
    return;
    }

static inline
Marray(MARRAY_T)
Marray_from(MARRAY_T)(Nonnull(const Allocator*)a, Nonnull(const MARRAY_T*) values, size_t n_values){
    Marray(MARRAY_T) result = {};
    Marray_extend(MARRAY_T)(&result, a, values, n_values);
    return result;
    }

static inline
MARRAY_T
Marray_pop(MARRAY_T)(Nonnull(Marray(MARRAY_T)*) marray){
    assert(marray->count);
    marray->count--;
    return marray->data[marray->count];
    }

static inline
MARRAY_T
Marray_peek(MARRAY_T)(Nonnull(Marray(MARRAY_T)*) marray){
    assert(marray->count);
    return marray->data[marray->count-1];
    }

static inline
void
Marray_reserve(MARRAY_T)(Nonnull(Marray(MARRAY_T)*) marray, Nonnull(const Allocator*)a, size_t n){
    if (n <= marray->capacity)
        return;
    size_t old_size = marray->capacity * sizeof(MARRAY_T);
    size_t new_size = n * sizeof(MARRAY_T);
    marray->data = Allocator_realloc(a, marray->data, old_size, new_size);
    marray->capacity = n;
    assert(marray->data);
    return;
    }

static inline
void
Marray_cleanup(MARRAY_T)(Nonnull(Marray(MARRAY_T)*) marray, Nonnull(const Allocator*)a){
    Allocator_free(a, marray->data, marray->capacity*sizeof(MARRAY_T));
    marray->data = 0;
    marray->count = 0;
    marray->capacity = 0;
    return;
    }

static inline
force_inline
void
Marray_clear(MARRAY_T)(Nonnull(Marray(MARRAY_T)*) marray){
    marray->count = 0;
    return;
    }

static inline
Errorable_f(size_t)
Marray_find(MARRAY_T)(Nonnull(Marray(MARRAY_T)*) marray, MARRAY_T needle){
    Errorable(size_t) result = {};
    for (size_t i = 0; i < marray->count; i++){
        if(memcmp(&marray->data[i], &needle, sizeof(MARRAY_T))==0){
            result.result = i;
            return result;
            }
        }
    Raise(NOT_FOUND);
    }
PopDiagnostic()
#endif

#ifdef MARRAY_IMPL_ONLY
#undef MARRAY_IMPL_ONLY
#endif

#ifdef MARRAY_DECL_ONLY
#undef MARRAY_DECL_ONLY
#endif

#undef MARRAY_T
