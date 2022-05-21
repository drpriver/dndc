//
// Copyright © 2021-2022, David Priver
//
#include <stdlib.h>
#include <stdint.h>
#include "testing.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static inline
force_inline
int int_cmp(int* a, int* b){
    int left = *a;
    int right = *b;
    return left < right ? -1 : left > right? 1 : 0;
}

static inline
force_inline
int
long_cmp(const long*a_, const long*b_){
    long a = *a_;
    long b = *b_;
    if(a < b)
        return -1;
    if(a > b)
        return 1;
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#define DSORT_CMP(a, b)  (int_cmp(a, b))
#define DSORT_T int
#include "dsort.h"

#define DSORT_CMP(a, b) (StringView_cmp(a, b))
#define DSORT_T StringView
#include "dsort.h"

#define DSORT_T long
#define DSORT_CMP long_cmp
#include "dsort.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct RngState {
    uint64_t state;
    uint64_t inc;
} RngState;

static inline
uint32_t
rng_random32(RngState* rng) {
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    uint32_t xorshifted = (uint32_t) ( ((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

static inline
void
seed_rng_fixed(RngState* rng, uint64_t initstate, uint64_t initseq) {
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    rng_random32(rng);
    rng->state += initstate;
    rng_random32(rng);
}


TestFunction(TestSorts){
    TESTBEGIN();
    int arr[4] = {1, 3, 6, 2};
    int sorted[4] = {1, 2, 3, 6};
    TestExpectFalse(int__is_sorted(arr, 4));
    TestExpectTrue(int__is_sorted(sorted, 4));
    int__array_sort(arr, arrlen(arr));
    TestExpectTrue(memcmp(arr, sorted, sizeof(sorted))==0);
    TestExpectTrue(int__is_sorted(arr, 4));
    TESTEND();
}

TestFunction(TestSortRandoms){
    TESTBEGIN();
    enum {N = 500000};
    int* array = malloc(N*sizeof(*array));
    TestAssert(array);
    RngState rng = {0};
    seed_rng_fixed(&rng, 0x7123, 81728);
    for(int n = 1; n < 0xffff; n++){
        printf("\r%d", n); fflush(stdout);
        RngState before = rng;
        for(int j = 0; j < n; j++){
            array[j] = rng_random32(&rng);
        }
        int__array_sort(array, n);
        bool is_sorted = int__is_sorted(array, n);
        if(!is_sorted){
            TestPrintValue("n", n);
            TestPrintValue("before.inc", before.inc);
            TestPrintValue("before.state", before.state);
        }
        TestExpectTrue(is_sorted);
    }
    putchar('\n');
    free(array);
    TESTEND();
}

TestFunction(TestStringSorts){
    TESTBEGIN();
    StringView svs[] = {
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "dsort_test_strings.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
    };
    StringView__array_sort(svs, arrlen(svs));
    TestExpectTrue(StringView__is_sorted(svs, arrlen(svs)));
    TESTEND();
}

TestFunction(TestExample){
    TESTBEGIN();
    long longs[10] = {9, 1, 2, 4, 3, 8, 5, 6, 7};
    TestExpectFalse(long__is_sorted(longs, 10));
    long__array_sort(longs, 10);
    TestExpectTrue(long__is_sorted(longs, 10));
    TESTEND();
}

int main(int argc, char** argv){
    RegisterTest(TestSorts);
    RegisterTestFlags(TestSortRandoms, TEST_CASE_FLAGS_SKIP_UNLESS_NAMED);
    RegisterTest(TestStringSorts);
    RegisterTest(TestExample);
    return test_main(argc, argv);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
