#include <stdlib.h>
#include "testing.h"

typedef struct RngState {
    uint64_t state;
    uint64_t inc;
} RngState;

static inline
uint32_t
rng_random32(Nonnull(RngState*) rng) {
    auto oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    uint32_t xorshifted = (uint32_t) ( ((oldstate >> 18u) ^ oldstate) >> 27u);
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }

static inline
void
seed_rng_fixed(Nonnull(RngState*) rng, uint64_t initstate, uint64_t initseq) {
    rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    rng_random32(rng);
    rng->state += initstate;
    rng_random32(rng);
    }

#define DARWIN
#ifdef LINUX
#include <sys/random.h>
#endif
static inline
void
seed_rng(Nonnull(RngState*) rng) {
    _Static_assert(sizeof(unsigned long long) == sizeof(uint64_t), "");
    // spurious warnings on some platforms about unsigned long longs and unsigned longs,
    // so, use the unsigned long long.
    unsigned long long initstate;
    unsigned long long initseq;
#ifdef DARWIN
    arc4random_buf(&initstate, sizeof(initstate));
    arc4random_buf(&initseq, sizeof(initseq));
#elif defined(LINUX)
    // Too lazy to check for error.  Man page says we can't get
    // interrupted by signals for such a small buffer. and we're
    // not requesting nonblocking.
    ssize_t read;
    read = getrandom(&initstate, sizeof(initstate), 0);
    read = getrandom(&initseq, sizeof(initseq), 0);
    (void)read;
#else
    // Idk how to get random numbers on windows.
    // Just use the intel instruction.
    __builtin_ia32_rdseed64_step(&initstate);
    __builtin_ia32_rdseed64_step(&initseq);
#endif
    seed_rng_fixed(rng, initstate, initseq);
    return;
    }

static inline
force_inline
int int_cmp(int* a, int* b){
    int left = *a;
    int right = *b;
    return left < right ? -1 : left > right? 1 : 0;
    }
#define DSORT_CMP(a, b)  (int_cmp(a, b))
#define DSORT_T int
#include "dsort.h"

#define DSORT_CMP(a, b) (StringView_cmp(a, b))
#define DSORT_T StringView
#include "dsort.h"

TestFunction(TestSorts){
    TESTBEGIN();
    int arr[4] = {1, 3, 6, 2};
    int sorted[4] = {1, 2, 3, 6};
    int__array_sort(arr, arrlen(arr));
    TestExpectTrue(memcmp(arr, sorted, sizeof(sorted))==0);
    TESTEND();
    }

TestFunction(TestSortRandoms){
    TESTBEGIN();
    enum {N = 500000};
    int* array = malloc(N*sizeof(*array));
    TestAssert(array);
    RngState rng = {};
    seed_rng(&rng);
    // seed_rng_fixed(&rng, 0x7123, 81728);
    for(int n = 1; n < 0xffff; n++){
    // for(int i = 0; i < 10000; i++){
        // printf("\r%d", i); fflush(stdout);
        printf("\r%d", n); fflush(stdout);
        // int n;
        // do {
            // n = rng_random32(&rng) &0xffff;
        // } while(n > N);
        auto before = rng;
        for(int j = 0; j < n; j++){
            array[j] = rng_random32(&rng);
            }
        DSORT_SLICE_int r = {.data = array, .count=n};
        // int__heap_sort(&r);
        int__array_sort(array, n);
        // qsort(array, N, sizeof(int), (int(*)(const void*, const void*))int_cmp);
        bool is_sorted = int__is_sorted(array, n);
        if(!is_sorted){
            DBGPrint(n);
            // DBGPrint(i);
            DBGPrint(before.inc);
            DBGPrint(before.state);
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
#include "dsort_test_strings.h"
        };
    StringView__array_sort(svs, arrlen(svs));
    TestExpectTrue(StringView__is_sorted(svs, arrlen(svs)));
    TESTEND();
    }

int main(int argc, char** argv){
    RegisterTest(TestSorts);
    RegisterTestFlags(TestSortRandoms, TEST_CASE_FLAGS_SKIP_UNLESS_NAMED);
    RegisterTest(TestStringSorts);
    return test_main(argc, argv);
}

