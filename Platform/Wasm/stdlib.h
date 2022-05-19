//
// Copyright © 2021-2022, David Priver
//
#ifndef STDLIB_H
#define STDLIB_H
#include "allstd.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#pragma clang diagnostic push
#pragma clang diagnostic ignore "-Wnullability-completeness"
#endif
#define alloca __builtin_alloca
typedef int comparator(const void*, const void*);

typedef struct UntypedSlice UntypedSlice;
struct UntypedSlice {
    void* data;
    size_t count;
};

static inline void array_sort(void*, size_t, comparator*, size_t);
static inline void array_sort_insertion(void*, size_t, comparator*, size_t);
static inline void array_sort_d(void*, size_t, comparator*,  size_t, size_t);
static inline void heap_sort(UntypedSlice*, comparator*, size_t);
static inline size_t get_pivot(UntypedSlice*, comparator*, size_t);
static inline void short_sort(void*, size_t, comparator*,  size_t);
static inline void memswap(void*, void*, size_t);
static inline bool is_sorted(void*, size_t, comparator*, size_t);

static inline
void
array_sort_insertion(void* data, size_t n_items, comparator* cmp, size_t item_size){
    void* temp = alloca(item_size);
    for(size_t i = 1; i < n_items; i++){
        for(size_t j = i; (j > 0) && cmp(data+j*item_size, data + (j-1)*item_size) < 0; j--){
            void* a = data + (j-1)*item_size;
            void* b = data + j*item_size;
            memcpy(temp, a, item_size);
            memcpy(a, b, item_size);
            memcpy(b, temp, item_size);
            }
        }
    }

static inline
void
array_sort(void* data, size_t n_items, comparator* cmp, size_t item_size){
    if(n_items*item_size <= 256 || n_items < 4){
        array_sort_insertion(data, n_items, cmp, item_size);
        return;
        }
    array_sort_d(data, n_items, cmp, item_size, n_items);
    }

static inline
void
memswap(void* a, void* b, size_t memsize){
    if(a == b)
        return;
    void* temp = alloca(memsize);
    memcpy(temp, a, memsize);
    memcpy(a, b, memsize);
    memcpy(b, temp, memsize);
    }

static inline
void
array_sort_d(void*  data, size_t n_items, comparator* cmp, size_t item_size, size_t depth){
    const size_t short_sort_better = 1024/item_size > 32? 1024/item_size : 32;
    UntypedSlice r = {.data=data, .count = n_items};
    void* pivot = alloca(item_size);
    while(r.count > short_sort_better){
        if(!depth){
            // something is fucked up here
            // my heap sort is broken is my conclusion
            heap_sort(&r, cmp, item_size);
            return;
            }
        depth *= 2u;
        depth /= 3u;
        // depth = depth >= UINT64_MAX / 2 ? (depth / 3) * 2: (depth * 2) / 3;
        size_t pivot_index = get_pivot(&r, cmp, item_size);
        memcpy(pivot, r.data + pivot_index * item_size, item_size);
        memswap(r.data + pivot_index*item_size, r.data + (r.count-1)*item_size, item_size);
        size_t less_I = (size_t)-1;
        size_t greater_I = r.count - 1;
        for(;;){
            while(cmp(r.data + (++less_I)*item_size, pivot) < 0)
                ;
            for(;;){
                if(greater_I == less_I)
                    goto breakouter;
                if(cmp(pivot, r.data+ (--greater_I)*item_size) >= 0)
                    break;
                }
            if(less_I == greater_I)
                break;
            memswap(r.data+less_I*item_size, r.data+greater_I*item_size, item_size);
            }
        breakouter:;
        memswap(r.data + (r.count-1)*item_size, r.data+less_I*item_size, item_size);
        UntypedSlice left = {r.data, .count = less_I};
        UntypedSlice right = {r.data+(less_I+1)*item_size, .count = r.count - less_I - 1};
        if(right.count > left.count)
            memswap(&left, &right, sizeof(left));
        array_sort_d(right.data, right.count, cmp, item_size, depth);
        r = left;
        }
    short_sort(r.data, r.count, cmp, item_size);
    }

static inline
void
short_sort(void* data, size_t n_items, comparator* cmp, size_t item_size){
    switch(n_items){
        case 0: case 1: return;
        case 2:{
            if(cmp(data+item_size, data) < 0)
                memswap(data+item_size, data, item_size);
            }return;
        case 3:{
            if(cmp(data + 2*item_size, data) < 0){
                if(cmp(data, data+item_size) < 0) {
                    memswap(data, data+item_size, item_size);
                    memswap(data, data+2*item_size, item_size);
                    }
                else {
                    memswap(data, data+2*item_size, item_size);
                    if(cmp(data + item_size, data) < 0)
                        memswap(data, data+item_size, item_size);
                    }
                }
            else {
                if(cmp(data+item_size, data) < 0) {
                    memswap(data, data+item_size, item_size);
                    }
                else {
                    if(cmp(data + 2*item_size, data+item_size) < 0)
                        memswap(data+item_size, data+2*item_size, item_size);
                    }
                }
            }return;
        case 4:{
            if(cmp(data+item_size, data) < 0)
                memswap(data, data+item_size, item_size);
            if(cmp(data+item_size*3, data+item_size*2) < 0)
                memswap(data+item_size*2, data+item_size*3, item_size);
            if(cmp(data+item_size*2, data) < 0)
                memswap(data, data+item_size*2, item_size);
            if(cmp(data+item_size*3, data+item_size) < 0)
                memswap(data+item_size, data+item_size*3, item_size);
            if(cmp(data+item_size*2, data+item_size) < 0)
                memswap(data+item_size, data+item_size*2, item_size);
            }return;
        default:{
            // sort the last 5 elements
            void* last5 = data + item_size * (n_items-5);
            // 1. Sort first two pairs
            if (cmp(last5+1*item_size, last5) < 0)
                memswap(last5+0*item_size, last5+1*item_size, item_size);
            if (cmp(last5+3*item_size, last5+2*item_size) < 0)
                memswap(last5+2*item_size, last5+3*item_size, item_size);
            // 2. Arrange first two pairs by the largest element
            if (cmp(last5+3*item_size, last5+1*item_size) < 0) {
                memswap(last5, last5+2*item_size, item_size);
                memswap(last5+1*item_size, last5+3*item_size, item_size);
                }
            // 3. Insert 4 into [0, 1, 3]
            if (cmp(last5+4*item_size, last5+1*item_size) < 0) {
                memswap(last5+3*item_size, last5+4*item_size, item_size);
                memswap(last5+1*item_size, last5+3*item_size, item_size);
                if (cmp(last5+1*item_size, last5) < 0) {
                    memswap(last5+0*item_size, last5+1*item_size, item_size);
                    }
                }
            else if (cmp(last5+4*item_size, last5+3*item_size) < 0) {
                memswap(last5+3*item_size, last5+4*item_size, item_size);
                }
            // 4. Insert 2 into [0, 1, 3, 4] (note: we already know the last is greater)
            if (cmp(last5+2*item_size, last5+1*item_size) < 0){
                memswap(last5+1*item_size, last5+2*item_size, item_size);
                if (cmp(last5+1*item_size, last5) < 0) {
                    memswap(last5, last5+1*item_size, item_size);
                    }
                }
            else if (cmp(last5+3*item_size, last5+2*item_size) < 0) {
                memswap(last5+2*item_size, last5+3*item_size, item_size);
                }
            // 7 comparisons, 0-9 swaps
            if(n_items == 5)
                return;
            }break;
        }

    // The last 5 elements of the range are sorted.
    // Proceed with expanding the sorted portion downward.
    void* temp = alloca(item_size);
    for(size_t i = n_items - 6; ;i--){
        size_t j = i + 1;
        memcpy(temp, data+i*item_size, item_size);
        if(cmp(data+j*item_size, temp) < 0){
            do {
                memcpy(data+(j-1)*item_size, data+j*item_size, item_size);
                j++;
                } while(j < n_items && (cmp(data + j*item_size, temp) < 0));
            memcpy(data+(j-1)*item_size, temp, item_size);
            }
        if(i == 0)
            break;
        }
    }
static inline
size_t
get_pivot(UntypedSlice* r, comparator* cmp, size_t item_size){
    void* data = r->data;
    size_t mid = r->count / 2;
    if(r->count < 512){
        if(r->count >= 32){
            // median of 0, mid, r.count - 1
            size_t a = 0;
            size_t b = mid;
            size_t c = r->count - 1;
            if (cmp(data+c*item_size, data+a*item_size) < 0) { // c < a
                if (cmp(data+a*item_size, data+b*item_size) < 0) { // c < a < b
                    memswap(data+a*item_size, data+b*item_size, item_size);
                    memswap(data+a*item_size, data+c*item_size, item_size);
                    }
                else { // c < a, b <= a
                    memswap(data+a*item_size, data+c*item_size, item_size);
                    if (cmp(data+b*item_size, data+a*item_size) < 0)
                        memswap(data+a*item_size, data+b*item_size, item_size);
                    }
                }
            else { // a <= c
                if (cmp(data+b*item_size, data+a*item_size) < 0) { // b < a <= c
                    memswap(data+a*item_size, data+b*item_size, item_size);
                    }
                else { // a <= c, a <= b
                    if (cmp(data+c*item_size, data+b*item_size) < 0)
                        memswap(data+b*item_size, data+c*item_size, item_size);
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
    if (cmp(data+c*item_size, data+a*item_size) < 0)
        memswap(data+a*item_size, data+c*item_size, item_size);
    if (cmp(data+d*item_size, data+b*item_size) < 0)
        memswap(data+b*item_size, data+d*item_size, item_size);
    if (cmp(data+d*item_size, data+c*item_size) < 0) {
        memswap(data+c*item_size, data+d*item_size, item_size);
        memswap(data+a*item_size, data+b*item_size, item_size);
        }
    if (cmp(data+e*item_size, data+b*item_size) < 0)
        memswap(data+b*item_size, data+e*item_size, item_size);
    if (cmp(data+e*item_size, data+c*item_size) < 0) {
        memswap(data+c*item_size, data+e*item_size, item_size);
        if (cmp(data+c*item_size, data+a*item_size) < 0)
            memswap(data+a*item_size, data+c*item_size, item_size);
        }
    else {
        if (cmp(data+c*item_size, data+b*item_size) < 0)
            memswap(data+b*item_size, data+c*item_size, item_size);
        }

    return mid;
    }

static inline
void
sift_down(UntypedSlice* r, size_t parent, size_t end, comparator* cmp, size_t item_size){
    void* data = r->data;
    for(;;){
        size_t child = (parent+1) * 2;
        if(child >= end){
            if(child == end && (cmp(data+parent*item_size, data+(--child)*item_size) < 0)){
                memswap(data + parent*item_size, data+child*item_size, item_size);
                }
            break;
            }
        size_t left_child = child - 1;
        if (cmp(data+child*item_size, data+left_child*item_size) < 0)
            child = left_child;
        if (cmp(data+parent*item_size, data+child*item_size) >= 0)
            break;
        memswap(data+parent*item_size, data+child*item_size, item_size);
        parent = child;
        }
    }

static inline
bool
is_heap(UntypedSlice* r, comparator* cmp, size_t item_size){
    size_t parent = 0;
    for(size_t child = 1; child < r->count; child++){
        if(cmp(r->data+parent*item_size, r->data+child*item_size) < 0)
            return false;
        parent += !(child & 1llu);
        }
    return true;
    }

static inline
void
build_heap(UntypedSlice* r, comparator* cmp, size_t item_size){
    // DBGPrint("building heap");
    size_t n = r->count;
    for(size_t i = n / 2; i-- > 0; ){
        sift_down(r, i, n, cmp, item_size);
        }
    assert(is_heap(r, cmp, item_size));
    }

static inline
void
percolate(UntypedSlice* r, size_t parent, size_t end, comparator* cmp, size_t item_size){
    void* data = r->data;
    const size_t root = parent;
    for(;;){
        size_t child = (parent+1)*2;
        if(child >= end){
            if(child == end){
                --child;
                memswap(data+parent*item_size, data+child*item_size, item_size);
                parent = child;
                }
            break;
            }
        size_t left_child = child - 1;
        if(cmp(data+child*item_size, data+left_child*item_size) < 0)
            child = left_child;
        memswap(data+parent*item_size, data+child*item_size, item_size);
        parent = child;
        }
    for(size_t child = parent; child > root; child = parent){
        parent = (child - 1) / 2;
        if(cmp(data+parent*item_size, data+child*item_size) >= 0)
            break;
        memswap(data+parent*item_size, data+child*item_size, item_size);
        }
    }

static inline
void
heap_sort(UntypedSlice* r, comparator* cmp, size_t item_size){
    if(r->count < 2)
        return;
    build_heap(r, cmp, item_size);
    assert(is_heap(r, cmp, item_size));
    void* data = r->data;
    for(size_t i = r->count - 1; i > 0; --i){
        memswap(data, data+i*item_size, item_size);
        percolate(r, 0, i, cmp, item_size);
        }
    assert(is_sorted(r->data, r->count, cmp, item_size));
    }

static inline
bool
is_sorted(void* data, size_t n_items, comparator* cmp , size_t item_size){
    void* before = data;
    for(size_t i = 0; i < n_items; i++){
        if(cmp(data+i*item_size, before) < 0){
            return false;
            }
        before = data+i*item_size;
        }
    return true;
    }

void
qsort(void* base, size_t nel, size_t width, int(*compar)(const void*, const void*)){
    array_sort(base, nel, compar, width);
    }

#ifdef __clang__
#pragma clang diagnostic pop
#pragma clang assume_nonnull end
#endif

#endif
