#ifndef RARRAY_H
#define RARRAY_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "allocator.h"
#include "common_macros.h"
#include "error_handling.h"
#endif

//
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

#ifndef RARRAY_T
#error "Must define RARRAY_T"
#endif

#define Rarray(type) RarrayI(type)
#define RarrayI(type) Rarray__##type

typedef struct Rarray(RARRAY_T){
    size_t count;
    size_t capacity;
    RARRAY_T data[];
} Rarray(RARRAY_T);

#define RARRAYIMPL(meth, type) Rarray##_##meth##__##type
#define Rarray_push(type) RARRAYIMPL(push, type)
#define Rarray_check_size(type) RARRAYIMPL(check_size, type)
#define Rarray_alloc(type) RARRAYIMPL(alloc, type)
#define Rarray_remove(type) RARRAYIMPL(remove, type)

//
// Ensures there is enough space for one more element.
// Returns the new rarray. The old pointer is now invalid (even if it happens to be the same)!
//
// Example:
//   Allocator al = get_my_allocator();
//   Rarray(int)* myarray = NULL;
//   myarray = Rarray_check_size(int)(myarray, al);
//   assert(myarray->capacity > 0);
//
static inline
Nonnull(Rarray(RARRAY_T)*)
Rarray_check_size(RARRAY_T)(Nullable(Rarray(RARRAY_T)*) rarray, const Allocator a){
    if(!rarray){
        rarray = Allocator_alloc(a, sizeof(Rarray(RARRAY_T)) + 4 * sizeof(RARRAY_T));
        rarray->count = 0;
        rarray->capacity = 4;
        }
    if(rarray->count == rarray->capacity){
        const size_t old_size = rarray->capacity*sizeof(RARRAY_T)+sizeof(Rarray(RARRAY_T));
        auto new_array = Allocator_realloc(a, rarray, old_size, old_size*2);
        rarray = new_array;
        rarray->capacity *= 2;
        }
    return (Rarray(RARRAY_T)*)rarray;
    }

//
// Pushes one more element onto the end of the rarray.
// Returns the new rarray. The old pointer is now invalid (even if it happens to be the same)!
//
// Example:
//
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
Nonnull(Rarray(RARRAY_T)*)
Rarray_push(RARRAY_T)(Nullable(Rarray(RARRAY_T)*) rarray, const Allocator a, RARRAY_T item){
    rarray = Rarray_check_size(RARRAY_T)(rarray,a);
    rarray->data[rarray->count++] = item;
    return (Rarray(RARRAY_T)*)rarray;
    }

//
// Allocates space for one item in the rarray and returns it.
// The item is uninitialized. The pointer is unstable as any subsequent usage of the
// rarray can invalidate it.
// Takes a pointer to a pointer to rarray and will rewrite it. The previous value
// is invalid, so either only have one pointer to the rarray and have this rewrite it
// or you need to manually update the other pointers.
//
// Example:
//
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
Nonnull(RARRAY_T*)
Rarray_alloc(RARRAY_T)(Nonnull(Nullable(Rarray(RARRAY_T)*)*) rarray, const Allocator a){
    *rarray = Rarray_check_size(RARRAY_T)(*rarray, a);
    return &(*rarray)->data[(*rarray)->count++];
    }

//
// Removes an item by index. All items after it are shifted forward one.
//
// Example:
//
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
Rarray_remove(RARRAY_T)(Nonnull(Rarray(RARRAY_T)*) rarray, size_t i){
    assert(i < rarray->count);
    if(i == rarray->count-1){
        rarray->count--;
        return;
        }
    size_t n_move = rarray->count - i - 1;
    (memmove)(rarray->data+i, rarray->data+i+1, n_move*(sizeof(rarray->data[0])));
    rarray->count--;
    }

#undef RARRAY_T
