#ifndef DSORT_H
#define DSORT_H
// This header generates a quick sort implementation, specialized
// to a specific data type and comparison operation.
//
// Usage:
//
//   #define DSORT_T long
//   static inline
//   int
//   long_cmp(const long* a_, const long* b_){
//      long a = *a_;
//      long b = *b_;
//      if(a < b)
//          return -1;
//      if(a > b)
//          return 1;
//      return 0;
//   }
//   #define DSORT_CMP long_cmp
//   #include "dsort.h"
//
//   long longs[10] = {9, 1, 2, 4, 3, 8, 5, 6, 7};
//   long__array_sort(longs, 10);
//   assert(long__is_sorted(longs, 10));
//

// for size_t
#include <stddef.h>
#endif
#ifndef DSORT_CMP
#error "Must define a DSORT_CMP macro (DSORT_CMP)"
#endif

#ifndef DSORT_T
#error "Must define a DSORT_T"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif


#define DSORT_SLICE_(X) DSORT_SLICE_##X
#define DSORT_SLICE(x) DSORT_SLICE_(x)
typedef struct DSORT_SLICE(DSORT_T) {
    DSORT_T* data;
    size_t count;
} DSORT_SLICE(DSORT_T);
#define DSORT_SWAP(a, b) do{\
    DSORT_T tmp = a; \
    a = b; \
    b = tmp; \
}while(0)
#define DSORT_IMPL__(X, B) X##__##B
#define DSORT_IMPL_(A, X) DSORT_IMPL__(A,X)
#define DSORT_IMPL(X) DSORT_IMPL_(DSORT_T,X)

static void DSORT_IMPL(array_sort)(DSORT_T*, size_t);
static inline void DSORT_IMPL(array_sort_insertion)(DSORT_T*, size_t);
static inline void DSORT_IMPL(array_sort_d)(DSORT_T*, size_t, size_t);
static inline void DSORT_IMPL(heap_sort)(DSORT_SLICE(DSORT_T)*);
static inline size_t DSORT_IMPL(get_pivot)(DSORT_SLICE(DSORT_T)*);
static inline void DSORT_IMPL(short_sort)(DSORT_T*, size_t);
static inline _Bool DSORT_IMPL(is_sorted)(DSORT_T*, size_t);

static inline
void
DSORT_IMPL(array_sort_insertion)(DSORT_T* data, size_t n_items){
    for(size_t i = 1; i < n_items; i++){
        for(size_t j = i; (j > 0) && DSORT_CMP(data+j, data+(j-1)) < 0; j--){
            DSORT_T* a = data + (j-1);
            DSORT_T* b = data + j;
            DSORT_SWAP(*a, *b);
        }
    }
}
static
void
DSORT_IMPL(array_sort)(DSORT_T* data, size_t n_items){
    if(n_items*sizeof(DSORT_T) <= 256 || n_items < 4){
        DSORT_IMPL(array_sort_insertion)(data, n_items);
        return;
    }
    DSORT_IMPL(array_sort_d)(data, n_items, n_items);
}

static inline
void
DSORT_IMPL(array_sort_d)(DSORT_T* data, size_t n_items, size_t depth){
    enum{short_sort_better = 256/sizeof(DSORT_T) > 32? 256/sizeof(DSORT_T) : 32};
    DSORT_SLICE(DSORT_T) r = {.data=data, .count = n_items};
    DSORT_T pivot;
    while(r.count > short_sort_better){
    // while(r.count > 32){
        if(!depth){
            // assert(0);
            // something is fucked up here
            // my heap sort is broken is my conclusion
            DSORT_IMPL(heap_sort)(&r);
            return;
        }
        depth /= 3u;
        depth *= 2u;
        // depth = depth >= UINT64_MAX / 2 ? (depth / 3) * 2: (depth * 2) / 3;
        size_t pivot_index = DSORT_IMPL(get_pivot)(&r);
        pivot = r.data[pivot_index];
        DSORT_SWAP(r.data[pivot_index], r.data[r.count-1]);
        size_t less_I = (size_t)-1;
        size_t greater_I = r.count - 1;
        for(;;){
            while(DSORT_CMP(r.data + (++less_I), &pivot) < 0)
                ;
            for(;;){
                if(greater_I == less_I)
                    goto breakouter;
                if(!(DSORT_CMP(&pivot, r.data+ (--greater_I)) < 0))
                    break;
            }
            if(less_I == greater_I)
                break;
            DSORT_SWAP(r.data[less_I], r.data[greater_I]);
        }
        breakouter:;
        DSORT_SWAP(r.data[r.count-1], r.data[less_I]);
        DSORT_SLICE(DSORT_T) left = {r.data, .count = less_I};
        DSORT_SLICE(DSORT_T) right = {r.data+(less_I+1), .count = r.count - less_I - 1};
        if(right.count > left.count){
            // swap
            DSORT_SLICE(DSORT_T) temp = left;
            left = right;
            right = temp;
        }
        DSORT_IMPL(array_sort_d)(right.data, right.count, depth);
        r = left;
    }
    DSORT_IMPL(short_sort)(r.data, r.count);
    }

static inline
void
DSORT_IMPL(short_sort)(DSORT_T* data, size_t n_items){
    switch(n_items){
        case 0: case 1: return;
        case 2:{
            if(DSORT_CMP(data+1, data) < 0)
                DSORT_SWAP(data[1], data[0]);
        }return;
        case 3:{
            if(DSORT_CMP(data + 2, data) < 0){
                if(DSORT_CMP(data, data+1) < 0) {
                    DSORT_SWAP(data[0], data[1]);
                    DSORT_SWAP(data[0], data[2]);
                }
                else {
                    DSORT_SWAP(data[0], data[2]);
                    if(DSORT_CMP(data + 1, data) < 0)
                        DSORT_SWAP(data[0], data[1]);
                }
            }
            else {
                if(DSORT_CMP(data+1, data) < 0) {
                    DSORT_SWAP(data[0], data[1]);
                }
                else {
                    if(DSORT_CMP(data + 2, data+1) < 0)
                        DSORT_SWAP(data[1], data[2]);
                }
            }
        }return;
        case 4:{
            if(DSORT_CMP(data+1, data) < 0)
                DSORT_SWAP(data[0], data[1]);
            if(DSORT_CMP(data+3, data+2) < 0)
                DSORT_SWAP(data[2], data[3]);
            if(DSORT_CMP(data+2, data) < 0)
                DSORT_SWAP(data[0], data[2]);
            if(DSORT_CMP(data+3, data+1) < 0)
                DSORT_SWAP(data[1], data[3]);
            if(DSORT_CMP(data+2, data+1) < 0)
                DSORT_SWAP(data[1], data[2]);
        }return;
        default:{
            // sort the last 5 elements
            DSORT_T* last5 = data + (n_items-5);
            // 1. Sort first two pairs
            if (DSORT_CMP(last5+1, last5) < 0)
                DSORT_SWAP(last5[0], last5[1]);
            if (DSORT_CMP(last5+3, last5+2) < 0)
                DSORT_SWAP(last5[2], last5[3]);
            // 2. Arrange first two pairs by the largest element
            if (DSORT_CMP(last5+3, last5+1) < 0) {
                DSORT_SWAP(last5[0], last5[2]);
                DSORT_SWAP(last5[1], last5[3]);
            }
            // 3. Insert 4 into [0, 1, 3]
            if (DSORT_CMP(last5+4, last5+1) < 0) {
                DSORT_SWAP(last5[3], last5[4]);
                DSORT_SWAP(last5[1], last5[3]);
                if (DSORT_CMP(last5+1, last5+0) < 0) {
                    DSORT_SWAP(last5[0], last5[1]);
                }
            }
            else if (DSORT_CMP(last5+4, last5+3) < 0) {
                DSORT_SWAP(last5[3], last5[4]);
            }
            // 4. Insert 2 into [0, 1, 3, 4] (note: we already know the last is greater)
            if (DSORT_CMP(last5+2, last5+1) < 0){
                DSORT_SWAP(last5[1], last5[2]);
                if (DSORT_CMP(last5+1, last5+0) < 0) {
                    DSORT_SWAP(last5[0], last5[1]);
                }
            }
            else if (DSORT_CMP(last5+3, last5+2) < 0) {
                DSORT_SWAP(last5[2], last5[3]);
            }
            // 7 comparisons, 0-9 swaps
            if(n_items == 5)
                return;
        }break;
    }

    // The last 5 elements of the range are sorted.
    // Proceed with expanding the sorted portion downward.
    DSORT_T temp;
    for(size_t i = n_items - 6; ;i--){
        size_t j = i + 1;
        temp = data[i];
        if(DSORT_CMP(data+j, &temp) < 0){
            do {
                data[j-1] = data[j];
                j++;
            } while(j < n_items && (DSORT_CMP(data + j, &temp) < 0));
            data[j-1] = temp;
        }
        if(i == 0)
            break;
    }
}

static inline
size_t
DSORT_IMPL(get_pivot)(DSORT_SLICE(DSORT_T)* r){
    DSORT_T* data = r->data;
    size_t mid = r->count / 2;
    if(r->count < 512){
        if(r->count >= 32){
            // median of 0, mid, r.count - 1
            size_t a = 0;
            size_t b = mid;
            size_t c = r->count - 1;
            if(DSORT_CMP(data+c, data+a) < 0) { // c < a
                if (DSORT_CMP(data+a, data+b) < 0) { // c < a < b
                    DSORT_SWAP(data[a], data[b]);
                    DSORT_SWAP(data[a], data[c]);
                }
                else { // c < a, b <= a
                    DSORT_SWAP(data[a], data[c]);
                    if (DSORT_CMP(data+b, data+a) < 0)
                        DSORT_SWAP(data[a], data[b]);
                }
            }
            else { // a <= c
                if(DSORT_CMP(data+b, data+a) < 0) { // b < a <= c
                    DSORT_SWAP(data[a], data[b]);
                }
                else { // a <= c, a <= b
                    if(DSORT_CMP(data+c, data+b) < 0)
                        DSORT_SWAP(data[b], data[c]);
                }
            }
        }
        return mid;
    }
    const size_t quarter = r->count / 4;
    const size_t a = 0;
    const size_t b = mid - quarter;
    const size_t c = mid;
    const size_t d = mid + quarter;
    const size_t e = r->count - 1;
    if(DSORT_CMP(data+c, data+a) < 0)
        DSORT_SWAP(data[a], data[c]);
    if(DSORT_CMP(data+d, data+b) < 0)
        DSORT_SWAP(data[b], data[d]);
    if(DSORT_CMP(data+d, data+c) < 0) {
        DSORT_SWAP(data[c], data[b]);
        DSORT_SWAP(data[a], data[b]);
    }
    if(DSORT_CMP(data+e, data+b) < 0)
        DSORT_SWAP(data[b], data[e]);
    if(DSORT_CMP(data+e, data+c) < 0) {
        DSORT_SWAP(data[c], data[e]);
        if(DSORT_CMP(data+c, data+a) < 0)
            DSORT_SWAP(data[a], data[c]);
    }
    else {
        if(DSORT_CMP(data+c, data+b) < 0)
            DSORT_SWAP(data[b], data[c]);
    }
    return mid;
}

static inline
void
DSORT_IMPL(sift_down)(DSORT_SLICE(DSORT_T)* r, size_t parent, size_t end){
    DSORT_T* data = r->data;
    for(;;){
        size_t child = (parent+1) * 2;
        if(child >= end){
            if(child == end && (DSORT_CMP(data+parent, data+(--child)) < 0)){
                DSORT_SWAP(data[parent], data[child]);
            }
            break;
        }
        size_t left_child = child - 1;
        if (DSORT_CMP(data+child, data+left_child) < 0)
            child = left_child;
        if (DSORT_CMP(data+parent, data+child) >= 0)
            break;
        DSORT_SWAP(data[parent], data[child]);
        parent = child;
    }
}

static inline
_Bool
DSORT_IMPL(is_heap)(DSORT_SLICE(DSORT_T)* r){
    size_t parent = 0;
    for(size_t child = 1; child < r->count; child++){
        if(DSORT_CMP(r->data+parent, r->data+child) < 0)
            return 0;
        parent += !(child & 1llu);
    }
    return 1;
}

static inline
void
DSORT_IMPL(build_heap)(DSORT_SLICE(DSORT_T)* r){
    // DBGPrint("building heap");
    size_t n = r->count;
    for(size_t i = n / 2; i-- > 0; ){
        DSORT_IMPL(sift_down)(r, i, n);
    }
    // assert(DSORT_IMPL(is_heap)(r));
}

static inline
void
DSORT_IMPL(percolate)(DSORT_SLICE(DSORT_T)* r, size_t parent, size_t end){
    DSORT_T* data = r->data;
    const size_t root = parent;
    for(;;){
        size_t child = (parent+1)*2;
        if(child >= end){
            if(child == end){
                --child;
                DSORT_SWAP(data[parent], data[child]);
                parent = child;
            }
            break;
        }
        size_t left_child = child - 1;
        if(DSORT_CMP(data+child, data+left_child) < 0)
            child = left_child;
        DSORT_SWAP(data[parent], data[child]);
        parent = child;
    }
    for(size_t child = parent; child > root; child = parent){
        parent = (child - 1) / 2;
        if(DSORT_CMP(data+parent, data+child) >= 0)
            break;
        DSORT_SWAP(data[parent], data[child]);
    }
}

static inline
void
DSORT_IMPL(heap_sort)(DSORT_SLICE(DSORT_T)* r){
    if(r->count < 2)
        return;
    DSORT_IMPL(build_heap)(r);
    // assert(DSORT_IMPL(is_heap)(r));
    DSORT_T* data = r->data;
    for(size_t i = r->count - 1; i > 0; --i){
        DSORT_SWAP(data[0], data[i]);
        DSORT_IMPL(percolate)(r, 0, i);
    }
    // assert(DSORT_IMPL(is_sorted)(r->data, r->count));
}

static inline
_Bool
DSORT_IMPL(is_sorted)(DSORT_T* data, size_t n_items){
    DSORT_T* before = data;
    for(size_t i = 0; i < n_items; i++){
        if(DSORT_CMP(data+i, before) < 0){
            return 0;
        }
        before = data+i;
    }
    return 1;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#undef DSORT_T
#undef DSORT_CMP
#undef DSORT_IMPL__
#undef DSORT_IMPL_
#undef DSORT_IMPL
#undef DORT_SWAP
#undef DSORT_SLICE
