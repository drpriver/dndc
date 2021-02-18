#ifndef TESTING_H
#define TESTING_H
#include <string.h>
#include <stdio.h>
#include "common_macros.h"

//
// Macros for defining a test function.
// TESTBEGIN must be paired with TESTEND().
// You should use semicolons after them.
//
// Example use:
//
// TestFunction(TestFooIsTwo){
//     TESTBEGIN();
//     TestExpectEquals(foo(), 2);
//     TESTEND();
// }
//

#define TestFunction(name) static struct TestStats name(void)
#define TESTBEGIN() struct TestStats TEST_stats = {}; {
#define TESTEND() } return TEST_stats

//
// If this macro is defined, suppresses generating a main function. You will
// be responsible for calling test_main yourself and reporting the results.
// #define SUPPRESS_TEST_MAIN
//

//
// Internal use struct to keep track of the number of tests executed, failed,
// etc.
struct TestStats {
    int failures;
    int executed;
    int assert_failures;
};

// The type of a test function.
typedef struct TestStats (*_test_func)(void);
typedef Nonnull(_test_func) test_func;

//
// Register a test for execution.
// Use this within your definition of the register_tests function.
//
static inline void RegisterTest(test_func);

//
// Users should register their test by defining this function.
// The test main will call this function to register all of the tests
// int the test array and then execute them one by one.
//
// Example use:
//
// static inline
// void
// register_tests(void){
//     RegisterTest(TestFooIsTwo);
//     RegisterTest(TestBarIsNotBaz);
// }
//
static inline void register_tests(void);



// Internal use, use the RegisterTest function to register a test.
// This array is where registered tests are located.
// A single test program can not directly register more than 1000 tests.
static test_func test_funcs[1000];
// How many were registered. Internal use.
static int test_funcs_count;

// Internal use color definitions. They will be set to escape codes if
// stderr is detected to be interactive.
Nonnull(const char*) _test_color_gray  = "";
Nonnull(const char*) _test_color_reset = "";
#if 0
// Currently these are unused.
Nonnull(const char*) _test_color_blue  = ""
Nonnull(const char*) _test_color_green = ""
Nonnull(const char*) _test_color_red   = ""
#endif
// Internal use macro to report test results within a failed condition.
// You can use it if you want.
#define TestReport(fmt, ...) \
    fprintf(stderr, "%s%-16.16s %5d: %s" fmt "\n",\
        _test_color_gray, __FILE__, __LINE__, \
        _test_color_reset, ##__VA_ARGS__);

//
// These macros are for expressing the test conditions.
// They only work within a test function.
//

//
// Expects the condition is truthy (for the usual C definition of truth).
//
#define TestExpect(cond) do{\
        TEST_stats.executed++;\
        if (! (cond)){\
            TEST_stats.failures++;\
            TestReport("Test condition failed");\
            TestReport("%s", #cond);\
            }\
        }while(0)
//
// Expects lhs == rhs, using the == operator
//
#define TestExpectEquals(lhs, rhs)do{\
        TEST_stats.executed++;\
        if (!(lhs == rhs)) {\
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s == %s", #lhs, #rhs); \
            HEREPrint(lhs);\
            HEREPrint(rhs);\
            }\
        }while(0)

//
// Expects lhs != rhs, using the != operator
//
#define TestExpectNotEquals(lhs, rhs) do{\
        TEST_stats.executed++;\
        if (!(lhs != rhs)) {\
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s != %s", #lhs, #rhs); \
            HEREPrint(lhs);\
            HEREPrint(rhs);\
            }\
        }while(0)

//
// Expects the condition is truthy (for the usual C definition of truth).
// This is identical to TestExpect.
//
#define TestExpectTrue(cond) do{\
        TEST_stats.executed++;\
        if (! (cond)){ \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s", #cond);\
            }\
        }while(0)

//
// Expects the condition is falsey (for the usual C definition of truth).
// This is identical to TestExpect.
//
#define TestExpectFalse(cond) do{\
        TEST_stats.executed++;\
        if ((cond)){ \
            TEST_stats.failures++; \
            TestReport("Test condition failed (expected falsey)");\
            HEREPrint(cond);\
            }\
        }while(0)

//
// For an Errorable, expects .errored is NO_ERROR
//
#define TestExpectSuccess(cond) do{\
        TEST_stats.executed++;\
        if ((cond.errored)){ \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s = %s", #cond, get_error_name(cond));\
            }\
        }while(0)

//
// For an Errorable, expects .errored is not NO_ERROR
//
#define TestExpectFailure(cond) do{\
        TEST_stats.executed++;\
        if ((!cond.errored)){ \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s = %s", #cond, get_error_name(cond));\
            }\
        }while(0)

//
// Unlike the TestExpect* family of macros, TestAssert* macros immediately end
// execution of the test function.
// The program is not halted, just the test function immediately returns.
// You will need to cleanup any state. In the future we might have a
// cleanup section in test functions.
//

//
// Asserts the condition is truthy.
//
#define TestAssert(cond) do{\
        TEST_stats.executed++;\
        if (! (cond)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s", #cond); \
            return TEST_stats;\
            }\
        }while(0)

//
// Asserts lhs is equal to rhs, using ==
//
#define TestAssertEquals(lhs, rhs) do{\
        TEST_stats.executed++;\
        if (! (lhs==rhs)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s == %s", #lhs, #rhs); \
            HEREPrint(lhs);\
            HEREPrint(rhs); \
            return TEST_stats;\
            }\
        }while(0)

//
// For an Errorable, asserts .errored is NO_ERROR
//
#define TestAssertSuccess(cond) do{\
        TEST_stats.executed++;\
        if ((cond.errored)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s = %s", #cond, get_error_name(cond)); \
            return TEST_stats;\
            }\
        }while(0)

//
// For an Errorable, asserts .errored is not NO_ERROR
//
#define TestAssertFailure(cond) do{\
        TEST_stats.executed++;\
        if ((!cond.errored)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s = %s", #cond, get_error_name(cond)); \
            return TEST_stats;\
            }\
        }while(0)

//
// Immediately ends the test function, counting as an early termination of
// the test and reports the message given by reason.
//
#define EndTest(reason) do{\
        TestReport("Test ended early"); \
        TestReport("Reason: %s", reason); \
        TEST_stats.assert_failures++;\
        return TEST_stats;\
        }while(0)

// Implementation of RegisterTest
static
inline
void RegisterTest(test_func func){
    assert(test_funcs_count < arrlen(test_funcs));
    test_funcs[test_funcs_count++] = func;
    }

//
// The actual test runner.
// You can call this in your own main or other program if you have
// suppressed the provided main (perhaps you need more setup).
// Otherwise, don't call this directly.
//
static
struct TestStats
test_main(void){
    struct TestStats result = {};
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

//
// The default main implementation if you don't suppress it.
// Executes test_main and pretty prints the results to the terminal.
//
#ifndef SUPPRESS_TEST_MAIN
#include "term_util.h"
PushDiagnostic()
SuppressNullabilityComplete()
int main(int argc, char**argv){
PopDiagnostic();
    if(argc < 1){
        fprintf(stderr, "Somehow this program was called without an argv.\n");
        return 1;
        }
    const char* filename = argv[0];

    filename = strrchr(filename, '/')? strrchr(filename, '/')+1 : filename;
    bool interactive = isatty(fileno(stderr));
    const char* gray  = interactive? "\033[97m"    : "";
    const char* blue  = interactive? "\033[94m"    : "";
    const char* green = interactive? "\033[92m"    : "";
    const char* red   = interactive? "\033[91m"    : "";
    const char* reset = interactive? "\033[39;49m" : "";
    _test_color_gray = gray;
    _test_color_reset = reset;
#if 0
    _test_color_blue = blue;
    _test_color_green = green;
    _test_color_red = red;
#endif

    auto result = test_main();

    const char* text = test_funcs_count == 1? "test function executed" : "test functions executed";
    fprintf(stderr, "%s%s: %s%d%s %s\n",
            gray, filename,
            blue, test_funcs_count,
            reset, text);

    text = result.executed == 1? "test executed" : "tests executed";
    fprintf(stderr, "%s%s: %s%d%s %s\n",
            gray, filename,
            blue, result.executed,
            reset, text);

    text = result.assert_failures == 1? "test function aborted early" : "test functions aborted early";
    const char* color = result.assert_failures?red:green;
    fprintf(stderr, "%s%s: %s%d%s %s\n",
            gray, filename,
            color, result.assert_failures,
            reset, text);

    color = result.failures?red:green;
    text = result.failures == 1? "test failed" : "tests failed";
    fprintf(stderr, "%s%s: %s%d%s %s\n",
            gray, filename,
            color, result.failures,
            reset, text);

    return result.failures + result.assert_failures == 0? 0 : 1;
    }

#include "terminal_logger.c"
#endif
