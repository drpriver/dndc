#ifndef TESTING_H
#define TESTING_H
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#ifdef _WIN32
// for chdir
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif
#include "long_string.h"

#ifndef arrlen
#define arrlen(arr) (sizeof(arr)/sizeof(arr[0]))
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#define _Nonnull
#define _Nullable
#endif


// BUG:
// We use __auto_type in the testing macros, so this will only compile
// with gcc and clang.
// Maybe one day C will standardize auto.
//
// We also use statement expressions. In theory capturing lambdas
// could replace that if those are added to C23.

// Internal use color definitions. They will be set to escape codes if
// stderr is detected to be interactive.
static const char* _test_color_gray  = "";
static const char* _test_color_reset = "";
#if 0
// Currently these are unused.
static const char* _test_color_blue  = ""
static const char* _test_color_green = ""
static const char* _test_color_red   = ""
#endif

static FILE*_Null_unspecified TestOutFiles[9] = {0};
static size_t TestOutFileCount = 0;

static void
TestRegisterOutFile(FILE* fp){
    if(TestOutFileCount >= arrlen(TestOutFiles))
        return;
    TestOutFiles[TestOutFileCount++] = fp;
}


#ifdef __GNUC__
__attribute__((__format__(__printf__, 1, 2)))
#endif
static
void
TestPrintf(const char* fmt, ...){
    va_list arg;
    char buff[10000];
    for(size_t i = 0; i < TestOutFileCount; i++){
        va_start(arg, fmt);
        FILE* fp = TestOutFiles[i];
        if(fp == stderr || fp == stdout)
            vfprintf(fp, fmt, arg);
        else {
            // janky control code stripper.
            // FIXME:
            // This is too late to strip control codes
            // since the test output could have control codes and thus
            // diffing outputs would be wrong.
            // Oh well.
            int printed = vsnprintf(buff, sizeof buff, fmt, arg);
            if(printed > (int)sizeof buff) printed = sizeof buff;
            char* p = buff;
            char* begin = p;
            char* end = p + printed;
            while(p != end){
                if(*p == '\033'){
                    if(p != begin){
                        fwrite(begin, p - begin, 1, fp);
                    }
                    while(p != end && *(p++) != 'm')
                        ;
                    begin = p;
                    continue;
                }
                p++;
            }
            if(begin != end)
                fwrite(begin, end - begin, 1, fp);
        }
        va_end(arg);
    }
}

#ifndef TestPrintValue

#define TestPrintFuncs(apply) \
    apply(bool, _Bool, "%s", x?"true":"false")\
    apply(char, char, "%c", x)\
    apply(uchar, unsigned char, "%u", x) \
    apply(schar, signed char, "%d", x) \
    apply(float, float, "%f", (double)x) \
    apply(double, double, "%f", x) \
    apply(short, short, "%d", x) \
    apply(ushort, unsigned short, "%u", x) \
    apply(int, int, "%d", x)\
    apply(uint, unsigned int, "%u", x) \
    apply(long, long, "%ld", x) \
    apply(ulong, unsigned long, "%lu", x) \
    apply(llong, long long, "%lld", x) \
    apply(ullong, unsigned long long, "%llu", x) \
    apply(cstr, char*, "\"%s\"", x) \
    apply(ccstr, const char*, "\"%s\"", x) \
    apply(pvoid, void*, "%p", x) \
    apply(pcvoid, const void*, "%p", x) \
    apply(LongString, LongString, "\"%s\"", x.text) \
    apply(StringView, StringView, "\"%.*s\"", (int)x.length, x.text)
#define TestPrintFunc(suffix, type, unused, ...) type: TestPrintImpl_##suffix,

#define TestPrintValue(str, val) \
    _Generic(val, \
    TestPrintFuncs(TestPrintFunc) \
    struct{int foo;}: 0)(__FILE__, __func__, __LINE__, str, val)

#define TestPrintImpl_(suffix, type, fmt, ...) \
    static inline __attribute__((always_inline)) void \
    TestPrintImpl_##suffix(const char* file, const char* func, int line, const char* str, type x){ \
        TestPrintf("%s%s:%s:%d%s %s = " fmt "\n",\
                _test_color_gray, file, func, line, _test_color_reset, str, __VA_ARGS__); \
        }
TestPrintFuncs(TestPrintImpl_)

#endif



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
#define TESTBEGIN() struct TestStats TEST_stats = {0}; {
#define TESTEND() } return TEST_stats

//
// If this macro is defined, suppresses generating a test_main function. You will
// be responsible for calling run_the_tests yourself and reporting the results.

// #define SUPPRESS_TEST_MAIN


//
// Internal use struct to keep track of the number of tests executed, failed,
// etc.
struct TestStats {
    unsigned long long funcs_executed;
    unsigned long long failures;
    unsigned long long executed;
    unsigned long long assert_failures;
};

// The type of a test function.
typedef struct TestStats (TestFunc)(void);

enum TestCaseFlags {
    // No flags
    TEST_CASE_FLAGS_NONE = 0x0,
    // Skip this test unless specifically named on the command line.
    // This is useful for slow or exhaustive tests that don't need to be run
    // on every change, but you want to keep them compiling and in tree.
    TEST_CASE_FLAGS_SKIP_UNLESS_NAMED = 0x1,
};

// Internal use.
typedef struct TestCase {
    StringView test_name;
    TestFunc* test_func;
    enum TestCaseFlags flags;
} TestCase;

//
// Register a test for execution.
// Use this before calling test_main
//
// static inline void RegisterTest(TestFunc*);
//
// Example use:
//
// int main(int argc, char**argv){
//     RegisterTest(TestFooIsTwo);
//     RegisterTest(TestBarIsNotBaz);
//     return test_main(argc, argv);
// }

// Implemented as a macro to capture the name of the test
#define RegisterTest(tf) register_test(SV(#tf), tf, TEST_CASE_FLAGS_NONE)
// Ditto, but allows specifying flags.
#define RegisterTestFlags(tf, flags) register_test(SV(#tf), tf, flags)
static inline
void
register_test(StringView test_name, TestFunc* func, enum TestCaseFlags flags);

// Internal use, use the RegisterTest function to register a test.
// This array is where registered tests are located.
// A single test program can not directly register more than 1000 tests.
static StringView test_names[1000];
static TestCase test_funcs[1000];
// How many were registered. Internal use.
static size_t test_funcs_count;

static inline
void
register_test(StringView test_name, TestFunc* func, enum TestCaseFlags flags){
    assert(test_funcs_count < arrlen(test_funcs));
    test_names[test_funcs_count] = test_name;
    test_funcs[test_funcs_count++] = (TestCase){
        .test_name = test_name,
        .test_func=func,
        .flags=flags,
    };
}


//
// Internal use macro to report test results within a failed condition.
// You can use it if you want in your test functions if you need more reporting.
// It's an fprintf wrapper (appends a newline though).
//
#define TestReport(fmt, ...) \
    TestPrintf("%s%s %s %d: %s" fmt "\n",\
        _test_color_gray, __FILE__, __func__, __LINE__, \
        _test_color_reset, ##__VA_ARGS__);

//
// These macros are for expressing the test conditions.
// They only work within a test function.
//
// Each one "returns" the condition being tested. For example,
// TestExpectEquals returns lhs == rhs.
// This means you can use these as expressions to do conditional
// printing or extra testing or whatever.
//

//
// Expects lhs == rhs, using the == operator
//
#define TestExpectEquals(lhs, rhs) ({\
        __auto_type _lhs = lhs; \
        typeof(lhs) _rhs = rhs; \
        TEST_stats.executed++;\
        int equal__ = 1; \
        if (!(_lhs == _rhs)) {\
            equal__ = 0; \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s == %s", #lhs, #rhs); \
            TestPrintValue(#lhs, _lhs);\
            TestPrintValue(#rhs, _rhs);\
            }\
        equal__; \
        })
//
// Expects lhs == rhs, using the passed in binary function instead of == operator
//
#define TestExpectEquals2(func, lhs, rhs) ({\
        __auto_type _lhs = lhs; \
        __auto_type _rhs = rhs; \
        TEST_stats.executed++;\
        int equal__ = 1; \
        if (!(func(_lhs, _rhs))) {\
            equal__ = 0; \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("!%s(%s, %s)", #func, #lhs, #rhs); \
            TestPrintValue(#lhs, _lhs);\
            TestPrintValue(#rhs, _rhs);\
        }\
        equal__;\
    })

//
// Expects lhs != rhs, using the != operator
//
#define TestExpectNotEquals(lhs, rhs) ({\
        __auto_type _lhs = lhs; \
        typeof(lhs) _rhs = rhs; \
        TEST_stats.executed++;\
        int neq = 1; \
        if (!(_lhs != _rhs)) {\
            neq = 0; \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s != %s", #lhs, #rhs); \
            TestPrintValue(#lhs, _lhs);\
            TestPrintValue(#rhs, _rhs);\
        }\
        neq ;\
    })

#define TestExpectNotEqual2(func, lhs, rhs) ({\
        __auto_type _lhs = lhs; \
        __auto_type _rhs = rhs; \
        TEST_stats.executed++;\
        int notequal__ = 1; \
        if (func(_lhs, _rhs)) {\
            notequal__ = 0; \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s(%s, %s)", #func, #lhs, #rhs); \
            TestPrintValue(#lhs, _lhs);\
            TestPrintValue(#rhs, _rhs);\
            }\
        notequal__; \
        })

//
// Expects the condition is truthy (for the usual C definition of truth).
//
#define TestExpectTrue(cond) ({\
        TEST_stats.executed++;\
        _Bool cond_ = !!(cond); \
        if (! (cond_)){ \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s", #cond);\
        }\
        cond_; \
    })

//
// Expects the condition is falsey (for the usual C definition of truth).
//
#define TestExpectFalse(cond) ({\
        _Bool cond_ = !!(cond); \
        TEST_stats.executed++;\
        if (cond_){ \
            TEST_stats.failures++; \
            TestReport("Test condition failed (expected falsey)");\
            TestPrintValue(#cond, cond);\
        }\
        !cond_; \
    })

//
// For an errorable (struct with .errored field), expects .errored is 0
//
#define TestExpectSuccess(cond) ({\
        TEST_stats.executed++;\
        _Bool succeeded = 1; \
        if ((cond).errored){ \
            succeeded = 0; \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s = %d", #cond, (cond).errored);\
        }\
        succeeded; \
    })

//
// For an errorable (struct with .errored field), expects .errored is not 0
//
#define TestExpectFailure(cond) ({\
        TEST_stats.executed++;\
        _Bool did_fail = 1; \
        if (!(cond).errored){ \
            did_fail = 0; \
            TEST_stats.failures++; \
            TestReport("Test condition failed");\
            TestReport("%s = %d", #cond, (cond).errored);\
            }\
        did_fail; \
        })

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

// Ditto, but for falsey
#define TestAssertFalse(cond) do{\
        TEST_stats.executed++;\
        if ((cond)){ \
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
        __auto_type _lhs = lhs; \
        typeof(lhs) _rhs = rhs; \
        TEST_stats.executed++;\
        if (! (_lhs==_rhs)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s == %s", #lhs, #rhs); \
            TestPrintValue(#lhs, _lhs);\
            TestPrintValue(#rhs, _rhs); \
            return TEST_stats;\
        }\
    }while(0)

//
// For an errorable (struct with .errored field), asserts .errored is 0
//
#define TestAssertSuccess(cond) do{\
        TEST_stats.executed++;\
        if ((cond).errored){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s = %d", #cond, (cond).errored); \
            return TEST_stats;\
        }\
    }while(0)

//
// For an errorable (struct with .errored field), asserts .errored is not 0
//
#define TestAssertFailure(cond) do{\
        TEST_stats.executed++;\
        if ((!cond.errored)){ \
            TEST_stats.failures++; \
            TEST_stats.assert_failures++; \
            TestReport("Test condition failed");\
            TestReport("%s prematurely ended", __func__);\
            TestReport("%s = %d", #cond, (cond).errored); \
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

//
// The actual test runner.
// You can call this in your own test_main or other function.
// Otherwise, don't call this directly.
//
// which_tests is a pointer to an array of indexes into the
// registered tests table.
// test_count is the length of that array.
// As a special case, 0 means to run all the tests.
static
struct TestStats
run_the_tests(size_t*_Nullable which_tests, int test_count){
    struct TestStats result = {};
    if(test_count){
        for(int i = 0; i < test_count; i++){
            TestFunc* func = test_funcs[which_tests[i]].test_func;
            assert(func);
            struct TestStats func_result = func();
            result.funcs_executed++;
            result.failures += func_result.failures;
            result.executed += func_result.executed;
            result.assert_failures += func_result.assert_failures;
        }
    }
    else {
        for(size_t i = 0; i < test_funcs_count; i++){
            if(test_funcs[i].flags & TEST_CASE_FLAGS_SKIP_UNLESS_NAMED)
                continue;
            TestFunc* func = test_funcs[i].test_func;
            assert(func);
            struct TestStats func_result = func();
            result.funcs_executed++;
            result.failures += func_result.failures;
            result.executed += func_result.executed;
            result.assert_failures += func_result.assert_failures;
        }
    }
    return result;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

//
// The default test_main implementation if you don't suppress it.
// Executes run_the_tests and pretty prints the results to the terminal.
// NOTE: you will need to register a file pointer for TestPrintf if you
// don't use this function.
//
#ifndef SUPPRESS_TEST_MAIN
#include "argument_parsing.h"
#include "term_util.h"
static
int
test_main(int argc, char*_Nonnull *_Nonnull argv){
    if(argc < 1){
        fprintf(stderr, "Somehow this program was called without an argv.\n");
        return 1;
    }
    const char* filename = argv[0];
    _Bool no_colors = 0;
    _Bool force_colors = 0;
    _Bool run_all = 0;
    LongString directory = {0};
    size_t tests_to_run[arrlen(test_funcs)] = {0};
    struct ArgParseEnumType targets = {
        .enum_size = sizeof(size_t),
        .enum_count = test_funcs_count,
        .enum_names = test_names,
    };
    LongString outfile = {0};
    LongString extrafiles[8] = {0};
    _Bool append = 0;
    _Bool print_pid = 0;
    _Bool should_wait = 0;
    enum {TEE_INDEX=6, TARGET_INDEX=3};
    ArgToParse kw_args[] = {
        {
            .name = SV("-C"),
            .altname1 = SV("--change-directory"),
            .dest = ARGDEST(&directory),
            .help = "Directory to change the working directory to before "
                    "executing tests.",
        },
        {
            .name = SV("--no-colors"),
            .dest = ARGDEST(&no_colors),
            .help = "Dont use ANSI escape codes to print colors in reporting.",
        },
        {
            .name = SV("--force-colors"),
            .dest = ARGDEST(&force_colors),
            .help = "Always use ANSI escape codes to print colors in reporting, "
                    "even if output is not a tty.",
        },
        [TARGET_INDEX] = {
            .name = SV("-t"),
            .altname1 = SV("--target"),
            .max_num = test_funcs_count,
            .dest = ArgEnumDest(tests_to_run, &targets),
            .help = "If given, only run the named test function. If not given, "
                    "all tests will be run. Specify by name or by number.",
        },
        {
            .name = SV("--all"),
            .dest = ARGDEST(&run_all),
            .help = "Run all tests, including those which are disabled by default.",
        },
        {
            .name = SV("-o"),
            .altname1 = SV("--outfile"),
            .dest = ARGDEST(&outfile),
            .help = "Where to print test results outputs to. If not given, defaults to "
                    "stderr. Implies --no-colors. If you want to output to stderr and "
                    "also to file, use --tee instead.",
        },
        [TEE_INDEX] = {
            .name = SV("--tee"),
            .max_num = arrlen(extrafiles),
            .dest = ARGDEST(extrafiles),
            .help = "In addition to the primary output (either stderr or the file "
                    "given to --outfile), also print results to this file.",
        },
        {
            .name = SV("--append"),
            .dest = ARGDEST(&append),
            .help = "Open the files indicated by --outfile or --tee in append mode.",
        },
        {
            .name = SV("-p"),
            .altname1 = SV("--print-pid"),
            .help = "Print the pid of this process",
            .dest = ARGDEST(&print_pid),
        },
        {
            .name = SV("-w"),
            .altname1 = SV("--wait"),
            .help = "Do a getchar() before running the tests to give time to attach or whatever",
            .dest = ARGDEST(&should_wait),
        },
    };
    enum {HELP=0, LIST=1};
    ArgToParse early_args[] = {
        [HELP] = {
            .name = SV("-h"),
            .altname1 = SV("--help"),
            .help = "Print this help and exit.",
        },
        [LIST] = {
            .name = SV("-l"),
            .altname1 = SV("--list"),
            .help = "List the names of the test functions and exit.",
        },
    };
    ArgParser argparser = {
        .name = argc?argv[0]:"(Unnamed program)",
        .description = "A test runner.",
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
        .early_out.args = early_args,
        .early_out.count = arrlen(early_args),
        .styling.plain = !isatty(fileno(stdout)),
    };
    Args args = argc?(Args){argc-1, (const char*const*)argv+1}: (Args){0, 0};
    switch(check_for_early_out_args(&argparser, &args)){
        case HELP:{
            int columns = get_terminal_size().columns;
            if(columns > 80)
                columns = 80;
            print_argparse_help(&argparser, columns);
            return 1;
        }
        case LIST:
            for(size_t i = 0; i < test_funcs_count; i++){
                fprintf(stdout, "%s\t", test_funcs[i].test_name.text);
                if(test_funcs[i].flags & TEST_CASE_FLAGS_SKIP_UNLESS_NAMED){
                    fprintf(stdout, "Will-Skip");
                }
                fputc('\n', stdout);
            }
            return 1;
        default:
            break;
        }
    enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
    if(e){
        print_argparse_error(&argparser, e);
        fprintf(stderr, "Use --help to see usage.\n");
        return (int)e;
    }
    // Register primary output file
    if(outfile.length){
        no_colors = true;
        FILE* fp = fopen(outfile.text, append?"ab":"wb");
        if(!fp){
            fprintf(stderr, "Unable to open '%s': %s\n", outfile.text, strerror(errno));
            return 1;
        }
        TestRegisterOutFile(fp);
    }
    else {
        TestRegisterOutFile(stderr);
    }
    // Register extras
    for(int i = 0; i < kw_args[TEE_INDEX].num_parsed; i++){
        FILE* fp = fopen(extrafiles[i].text, append?"ab":"wb");
        if(!fp){
            fprintf(stderr, "Unable to open '%s': %s\n", extrafiles[i].text, strerror(errno));
            return 1;
        }
        TestRegisterOutFile(fp);
    }
    if(directory.length){
        int changed = chdir(directory.text);
        if(changed != 0){
            fprintf(stderr, "Failed to change directory to '%s': %s.\n",
                    directory.text, strerror(errno));
            return changed;
        }
    }
    if(run_all){
        for(size_t i = 0; i < arrlen(tests_to_run); i++)
            tests_to_run[i] = i;
    }

    filename = strrchr(filename, '/')? strrchr(filename, '/')+1 : filename;
    _Bool use_colors = force_colors || (!no_colors && isatty(fileno(stderr)));
    const char* gray  = use_colors? "\033[97m"    : "";
    const char* blue  = use_colors? "\033[94m"    : "";
    const char* green = use_colors? "\033[92m"    : "";
    const char* red   = use_colors? "\033[91m"    : "";
    const char* reset = use_colors? "\033[39;49m" : "";
    _test_color_gray = gray;
    _test_color_reset = reset;
#if 0
    _test_color_blue = blue;
    _test_color_green = green;
    _test_color_red = red;
#endif

    size_t num_to_run = run_all? arrlen(tests_to_run) : kw_args[TARGET_INDEX].num_parsed;

    assert(SV_equals(kw_args[TARGET_INDEX].name, SV("-t")));

    if(print_pid){
        #ifdef _WIN32
        fprintf(stderr, "pid: %d\n", (int)GetCurrentProcessId());
        #else
        fprintf(stderr, "pid: %d\n", getpid());
        #endif
    }
    if(should_wait){
        getchar();
    }
    struct TestStats result = run_the_tests(tests_to_run, num_to_run);

    const char* text = result.funcs_executed == 1?
        "test function executed"
        : "test functions executed";
    TestPrintf("%s%s: %s%llu%s %s\n",
            gray, filename,
            blue, (unsigned long long)result.funcs_executed,
            reset, text);

    text = result.executed == 1? "test executed" : "tests executed";
    TestPrintf("%s%s: %s%llu%s %s\n",
            gray, filename,
            blue, (unsigned long long)result.executed,
            reset, text);

    text = result.assert_failures == 1?
        "test function aborted early"
        : "test functions aborted early";
    const char* color = result.assert_failures?red:green;
    TestPrintf("%s%s: %s%llu%s %s\n",
            gray, filename,
            color, (unsigned long long)result.assert_failures,
            reset, text);

    color = result.failures?red:green;
    text = result.failures == 1? "test failed" : "tests failed";
    TestPrintf("%s%s: %s%llu%s %s\n",
            gray, filename,
            color, (unsigned long long)result.failures,
            reset, text);
    for(size_t i = 0 ; i < TestOutFileCount; i++){
        if(TestOutFiles[i] != stderr)
            fclose(TestOutFiles[i]);
    }
    return result.failures + result.assert_failures == 0? 0 : 1;
}

#endif

#endif
