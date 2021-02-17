#ifndef TESTING_H
#define TESTING_H
#include <string.h>
#include <stdio.h>
#include "common_macros.h"
struct test_stats {
    int failures;
    int executed;
    int assert_failures;
};

typedef struct test_stats (*_test_func)(void);
typedef Nonnull(_test_func) test_func;
static test_func test_funcs[1000];
int test_funcs_count;
static inline void RegisterTest(test_func);
// Users should register their test by defining this function.
static inline void register_tests(void);

#define TestReport(fmt, ...) fprintf(stderr, gray_coloring "%-16.16s %5d: " reset_coloring fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define TestExpect(cond) ({\
        TEST_stats.executed++;\
        if (! (cond)){\
            TEST_stats.failures++;\
            TestReport("Test condition failed");\
            TestReport("%s", #cond);\
            }\
        })

#define TestExpectEquals(left, right)({\
        TEST_stats.executed++;\
        if (!(left == right)) {\
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s == %s", #left, #right); \
            HEREPrint(left);\
            HEREPrint(right);\
            }\
        })

#define TestExpectNotEquals(left, right)({\
        TEST_stats.executed++;\
        if (!(left != right)) {\
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s != %s", #left, #right); \
            HEREPrint(left);\
            HEREPrint(right);\
            }\
        })

#define TestExpectTrue(cond) ({\
        TEST_stats.executed++;\
        if (! (cond)){ \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s", #cond);\
            }\
        })

#define TestExpectFalse(cond) ({\
        TEST_stats.executed++;\
        if ((cond)){ \
            TEST_stats.failures++; \
            TestReport("Test condition failed (expected falsey)");\
            HEREPrint(cond);\
            }\
        })

#define TestExpectSuccess(cond) ({\
        TEST_stats.executed++;\
        if ((cond.errored)){ \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s = %s", #cond, get_error_name(cond));\
            }\
        })

#define TestExpectFailure(cond) ({\
        TEST_stats.executed++;\
        if ((!cond.errored)){ \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s = %s", #cond, get_error_name(cond));\
            }\
        })

// Unlike the TestExpect* family of macros, TestAssert* macros immediately end execution of
// the test function.
#define TestAssert(cond) ({\
        TEST_stats.executed++;\
        if (! (cond)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s", #cond); \
            return TEST_stats;\
            }\
        })

#define TestAssertEquals(left, right) ({\
        TEST_stats.executed++;\
        if (! (left==right)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s == %s", #left, #right); \
            HEREPrint(left);\
            HEREPrint(right); \
            return TEST_stats;\
            }\
        })

#define TestAssertSuccess(cond) ({\
        TEST_stats.executed++;\
        if ((cond.errored)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s = %s", #cond, get_error_name(cond)); \
            return TEST_stats;\
            }\
        })

#define TestAssertFailure(cond) ({\
        TEST_stats.executed++;\
        if ((!cond.errored)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s = %s", #cond, get_error_name(cond)); \
            return TEST_stats;\
            }\
        })\

#define EndTest(reason) ({\
        TestReport("Test ended early"); \
        TestReport("Reason: %s", reason); \
        TEST_stats.assert_failures++;\
        return TEST_stats;\
        })

#define TestFunction(name) static struct test_stats name(void)
#define TESTBEGIN() struct test_stats TEST_stats = {}; {
#define TESTEND() } return TEST_stats

static
inline
void RegisterTest(test_func func){
    assert(test_funcs_count < arrlen(test_funcs));
    test_funcs[test_funcs_count++] = func;
    }

static
struct test_stats
test_main(void){
    struct test_stats result = {};
    register_tests();
    for (int i = 0; i < test_funcs_count; i++){
        auto func = test_funcs[i];
        assert(func);
        auto func_result = func();
        result.failures += func_result.failures;
        result.executed += func_result.executed;
        result.assert_failures += func_result.assert_failures;
        }
    return result;
    }
#endif

#ifndef SUPPRESS_TEST_MAIN
int main(void){
    auto result = test_main();
    const char* filename = strrchr(__BASE_FILE__, '/') ? strrchr(__BASE_FILE__, '/') + 1 : __BASE_FILE__;
    #define TestMainReport(fmt, ...) fprintf(stderr, gray_coloring "%s: " reset_coloring fmt "\n", filename, ##__VA_ARGS__)
    TestMainReport("%s%d" reset_coloring " test function%s executed", blue_coloring, test_funcs_count, test_funcs_count==1?"":"s");
    TestMainReport("%s%d" reset_coloring " test%s executed", blue_coloring, result.executed, result.executed==1?"":"s");
    TestMainReport("%s%d" reset_coloring " test function%s aborted early", result.assert_failures?red_coloring:green_coloring, result.assert_failures, result.assert_failures==1?"":"s");
    TestMainReport("%s%d" reset_coloring " test%s failed", result.failures?red_coloring:green_coloring, result.failures, result.failures==1?"":"s");
    return Max(result.failures, result.assert_failures);
    }
#include "terminal_logger.c"
#endif
