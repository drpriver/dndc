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

static inline
Nonnull(Rarray(RARRAY_T)*)
Rarray_check_size(RARRAY_T)(Nullable(Rarray(RARRAY_T)*) rarray, const Allocator a){
    if(!rarray){
        rarray = Allocator_alloc(a, sizeof(Rarray(RARRAY_T)) + 4 * sizeof(RARRAY_T));
        rarray->count = 0;
        rarray->capacity = 4;
        }
    if(rarray->count == rarray->capacity){
        auto new_array = Allocator_realloc(a, rarray, rarray->capacity*sizeof(RARRAY_T)+sizeof(Rarray(RARRAY_T)), rarray->capacity*2*sizeof(RARRAY_T)+sizeof(Rarray(RARRAY_T)));
        rarray = new_array;
        rarray->capacity *= 2;
        }
    return rarray;
    }


static inline
Nonnull(Rarray(RARRAY_T)*)
Rarray_push(RARRAY_T)(Nullable(Rarray(RARRAY_T)*) rarray, const Allocator a, RARRAY_T item){
    rarray = Rarray_check_size(RARRAY_T)(rarray,a);
    rarray->data[rarray->count++] = item;
    return rarray;
    }

static inline
Nonnull(RARRAY_T*)
Rarray_alloc(RARRAY_T)(Nonnull(Nullable(Rarray(RARRAY_T)*)*) rarray, const Allocator a){
    *rarray = Rarray_check_size(RARRAY_T)(*rarray, a);
    return &(*rarray)->data[(*rarray)->count++];
    }

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
