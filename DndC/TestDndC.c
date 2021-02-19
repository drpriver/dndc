#include "testing.h"
#define NOMAIN
#include "dndc.c"

// TODO: more interesting tests.
TestFunction(TestDndC1);
TestFunction(TestDndC2);
TestFunction(TestDndC3);

static inline
void
register_tests(void){
    RegisterTest(TestDndC1);
    RegisterTest(TestDndC2);
    RegisterTest(TestDndC3);
    }

TestFunction(TestDndC1){
    TESTBEGIN();
    LongString source = LS(
        "::md\n"
        "   * Hello World\n"
        "   + This is amazing!\n"
        "::python\n"
        "  ctx.root.add_child('hello')\n"
        );
    uint64_t flags = PARSE_FLAGS_NONE
        | PARSE_DONT_WRITE
        | PARSE_SOURCE_PATH_IS_DATA_NOT_PATH
        | PARSE_SUPPRESS_WARNINGS
        | PARSE_DONT_PRINT_ERRORS
        ;
    auto e = run_the_parser(flags, source, LS(""), LS(""));
    TestExpectSuccess(e);
    TESTEND();
    }

TestFunction(TestDndC2){
    TESTBEGIN();
    LongString source = LS(
        "::md\n"
        "   * Hello World\n"
        "   + This is amazing!\n"
        "::python\n"
        "  ctx.root.add_child('hello')\n"
        );
    uint64_t flags = PARSE_FLAGS_NONE
        | PARSE_PYTHON_IS_INIT
        | PARSE_DONT_WRITE
        | PARSE_SOURCE_PATH_IS_DATA_NOT_PATH
        | PARSE_SUPPRESS_WARNINGS
        | PARSE_DONT_PRINT_ERRORS
        ;
    auto e = run_the_parser(flags, source, LS(""), LS(""));
    TestExpectSuccess(e);
    TESTEND();
    }
TestFunction(TestDndC3){
    TESTBEGIN();
    LongString source = LS(
        "::asjdiasjdmd\n"
        "   * Hello World\n"
        "   + This is amazing!\n"
        "::python\n"
        "  ctx.root.add_child('hello')\n"
        );
    uint64_t flags = PARSE_FLAGS_NONE
        | PARSE_PYTHON_IS_INIT
        | PARSE_DONT_WRITE
        | PARSE_SOURCE_PATH_IS_DATA_NOT_PATH
        | PARSE_SUPPRESS_WARNINGS
        | PARSE_DONT_PRINT_ERRORS
        ;
    auto e = run_the_parser(flags, source, LS(""), LS(""));
    TestExpectFailure(e);
    TESTEND();
    }
