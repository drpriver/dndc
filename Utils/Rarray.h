//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#ifndef RARRAY_H
#define RARRAY_H
#include <stddef.h> // size_t
#include <string.h> // memmove
#include <assert.h> // assert
#include "Allocators/allocator.h" // Allocator functions



#if defined(__GNUC__) || defined(__clang__)
#define ra_memmove __builtin_memmove
#define ra_memcpy __builtin_memcpy
#else
#define ra_memmove memmove
#define ra_memcpy memcpy
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

//
// Rarrays
// -------
// Rarrays are dynamically resizable arrays where the length and capacity are
// stored inline with the data. They can only be referred via pointer as they
// are dynamically sized (data is stored continguously after the
// length/capacity).
//
// This is useful for dynamically sized arrays that are usually empty (as you
// can just store a pointer and have NULL be the same as a length 0 array) and
// when you need to access anything about the array you need all of it (so
// paying the cost of a pointer indirection to look up the size doesn't matter
// as you need to read the data anyway.
//
// We use this to slim down the ast nodes as they have some fields which are
// dynamically sized, but usually empty (attributes, classes).
//


#define Rarray(type) RarrayI(type)
#define RarrayI(type) Rarray__##type
#define RARRAYIMPL(meth, type) Rarray##_##meth##__##type
#define Rarray_push(type) RARRAYIMPL(push, type)
#define Rarray_check_size(type) RARRAYIMPL(check_size, type)
#define Rarray_clone(type) RARRAYIMPL(clone, type)
#define Rarray_alloc(type) RARRAYIMPL(alloc, type)
#define Rarray_remove(type) RARRAYIMPL(remove, type)
#define Rarray_sizeof(type) RARRAYIMPL(sizeof, type)

//
// RARRAY_FOR_EACH
// ---------------
// Convenience macro for correctly iterating over an rarray, handling the
// case where it is NULL. Note that `rarray` should be an RARRAY* and that
// `rarray` is evaluated multiple times.
//
#define RARRAY_FOR_EACH(type, iter, rarray) \
for(type *iter=((rarray)?(rarray)->data:NULL), \
      *iter##end__=((rarray)?(rarray)->data+(rarray)->count:NULL); \
    iter!=iter##end__;\
    ++iter)

#endif

#ifndef RARRAY_T
#error "Must define RARRAY_T"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#endif

typedef struct Rarray(RARRAY_T){
    size_t count;
    size_t capacity;
    RARRAY_T data[];
} Rarray(RARRAY_T);

#define RARRAY Rarray(RARRAY_T)

//
// Rarray_check_size
// -----------------
// Ensures there is enough space for one more element.
// Returns the new rarray. The old pointer is now invalid (even if it happens
// to be the same)!
//
// Example:
// --------
//   Allocator al = get_my_allocator();
//   Rarray(int)* myarray = NULL;
//   myarray = Rarray_check_size(int)(myarray, al);
//   assert(myarray->capacity > 0);
//
static inline
warn_unused
int
Rarray_check_size(RARRAY_T)(RARRAY*_Nullable*_Nonnull ra, Allocator a){
    RARRAY*rarray = *ra;
    if(!rarray){
        enum {INITIAL_CAPACITY=4};
        enum {INITIAL_SIZE=INITIAL_CAPACITY*sizeof(RARRAY_T)+sizeof(RARRAY)};
        rarray = Allocator_alloc(a, INITIAL_SIZE);
        if(unlikely(!rarray))
            return 1;
        rarray->count = 0;
        rarray->capacity = INITIAL_CAPACITY;
        *ra = rarray;
    }
    else if(rarray->count == rarray->capacity){
        size_t datasize = rarray->capacity*sizeof(RARRAY_T);
        size_t old_size = datasize + sizeof(RARRAY);
        size_t new_size = datasize*2 + sizeof(RARRAY);
        void* new_array = Allocator_realloc(a, rarray, old_size, new_size);
        if(unlikely(!new_array))
            return 1;
        rarray = new_array;
        rarray->capacity *= 2;
        *ra = rarray;
    }
    return 0;
}

static inline
warn_unused
int
Rarray_clone(RARRAY_T)(RARRAY*_Nullable rarray, Allocator a, RARRAY*_Nullable*_Nonnull result){
    if(!rarray) {
        *result = NULL;
        return 0;
    }
    size_t count = rarray->count;
    if(!count) {
        *result = NULL;
        return 0;
    }

    RARRAY* new_rarray = Allocator_alloc(a, sizeof(RARRAY)+sizeof(RARRAY_T)*count);
    if(unlikely(!new_rarray)){
        return 1;
    }
    new_rarray->count = count;
    new_rarray->capacity = count;
    ra_memcpy(new_rarray->data, rarray->data, sizeof(RARRAY_T)*count);
    *result = new_rarray;
    return 0;
}

//
// Rarray_push
// -----------
// Pushes one more element onto the end of the rarray.
// Returns the new rarray. The old pointer is now invalid (even if it happens
// to be the same)!
//
// Example:
// --------
//   Allocator al = get_my_allocator();
//   Rarray(int)* myarray = NULL;
//   myarray = Rarray_push(int)(myarray, al, 1);
//   myarray = Rarray_push(int)(myarray, al, 2);
//   myarray = Rarray_push(int)(myarray, al, 3);
//   assert(myarray->count == 3);
//   assert(myarray->data[0] == 1);
//   assert(myarray->data[1] == 2);
//   assert(myarray->data[2] == 3);
//
static inline
warn_unused
int
Rarray_push(RARRAY_T)(RARRAY*_Nullable*_Nonnull ra, Allocator a, RARRAY_T item){
    int err = Rarray_check_size(RARRAY_T)(ra, a);
    if(unlikely(err)) return err;
    RARRAY* rarray = *ra;
    rarray->data[rarray->count++] = item;
    return 0;
}

//
// Rarray_alloc
// ------------
// Allocates space for one item in the rarray and returns it.
// The item is uninitialized. The pointer is unstable as any subsequent usage
// of the rarray can invalidate it.
// Takes a pointer to a pointer to rarray and will rewrite it. The previous
// value is invalid, so either only have one pointer to the rarray and have
// this rewrite it or you need to manually update the other pointers.
//
// Example:
// --------
//   Allocator al = get_my_allocator();
//   Rarray(int)* myarray = NULL;
//   // Use a block to scope the allocation as the returned pointer is unstable
//   {
//       int* a = Rarray_alloc(int)(&myarray, al);
//       *a = 3;
//   }
//   {
//       int* b = Rarray_alloc(int)(&myarray, al);
//       *b = 4;
//   }
//   assert(myarray->count == 2);
//   assert(myarray->data[0] == 3);
//   assert(myarray->data[1] == 4);
//
static inline
warn_unused
int
Rarray_alloc(RARRAY_T)(RARRAY*_Nullable*_Nonnull ra, Allocator a, RARRAY_T*_Nullable*_Nonnull result){
    int err = Rarray_check_size(RARRAY_T)(ra, a);
    if(unlikely(err)) return err;
    RARRAY* rarray = *ra;
    *result = &rarray->data[rarray->count++];
    return 0;
}

//
// Rarray_remove
// -------------
// Removes an item by index. All items after it are shifted forward one.
//
// Example:
// --------
//   Allocator al = get_my_allocator();
//   Rarray(int)* myarray = NULL;
//   myarray = Rarray_push(int)(myarray, al, 1);
//   myarray = Rarray_push(int)(myarray, al, 2);
//   myarray = Rarray_push(int)(myarray, al, 3);
//   Rarray_remove(int)(myarray, 1);
//   assert(myarray->count == 2);
//   assert(myarray->data[0] == 1);
//   assert(myarray->data[1] == 3);
//
static inline
void
Rarray_remove(RARRAY_T)(RARRAY* rarray, size_t i){
    assert(rarray);
    assert(i < rarray->count);
    if(i == rarray->count-1){
        rarray->count--;
        return;
    }
    size_t n_move = rarray->count - i - 1;
    ra_memmove(rarray->data+i, rarray->data+i+1, n_move*(sizeof(RARRAY_T)));
    rarray->count--;
}

static inline
size_t
Rarray_sizeof(RARRAY_T)(RARRAY*_Nullable rarray){
    if(!rarray) return 0;
    return sizeof(*rarray) + sizeof(*rarray->data)*rarray->capacity;
}

#undef RARRAY
#undef RARRAY_T

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
