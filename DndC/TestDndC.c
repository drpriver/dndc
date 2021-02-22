#include "testing.h"
#define NOMAIN
#include "dndc.c"

// TODO: more interesting tests.
TestFunction(TestDndC1);
TestFunction(TestDndC2);
TestFunction(TestDndC3);
TestFunction(TestDndcOutParam);

static inline
void
register_tests(void){
    RegisterTest(TestDndC1);
    RegisterTest(TestDndC2);
    RegisterTest(TestDndC3);
    RegisterTest(TestDndcOutParam);
    }

TestFunction(TestDndC1){
    TESTBEGIN();
    LongString source = LS(
        "::md\n"
        "   * Hello World\n"
        "   * This is amazing!\n"
        "::python\n"
        "  ctx.root.add_child('hello')\n"
        );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_DONT_WRITE
        | DNDC_SOURCE_PATH_IS_DATA_NOT_PATH
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        ;
    auto e = run_the_dndc(flags, source, NULL, LS(""), NULL);
    TestExpectSuccess(e);
    TESTEND();
    }

TestFunction(TestDndC2){
    TESTBEGIN();
    LongString source = LS(
        "::md\n"
        "   * Hello World\n"
        "   * This is amazing!\n"
        "::python\n"
        "  ctx.root.add_child('hello')\n"
        );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_PYTHON_IS_INIT
        | DNDC_DONT_WRITE
        | DNDC_SOURCE_PATH_IS_DATA_NOT_PATH
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        ;
    auto e = run_the_dndc(flags, source, NULL, LS(""), NULL);
    TestExpectSuccess(e);
    TESTEND();
    }
TestFunction(TestDndC3){
    TESTBEGIN();
    LongString source = LS(
        "::asjdiasjdmd\n"
        "   * Hello World\n"
        "   * This is amazing!\n"
        "::python\n"
        "  ctx.root.add_child('hello')\n"
        );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_PYTHON_IS_INIT
        | DNDC_DONT_WRITE
        | DNDC_SOURCE_PATH_IS_DATA_NOT_PATH
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        ;
    auto e = run_the_dndc(flags, source, NULL, LS(""), NULL);
    TestExpectFailure(e);
    TESTEND();
    }
TestFunction(TestDndcOutParam){
    TESTBEGIN();
    LongString source = LS(
        "::md\n"
        "   * Hello World\n"
        "   * This is amazing!\n"
        "::python\n"
        "  ctx.root.add_child('hello')\n"
        );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SOURCE_PATH_IS_DATA_NOT_PATH
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_OUTPUT_PATH_IS_OUT_PARAM
        ;
    LongString outdata = {};
    auto e = run_the_dndc(flags, source, &outdata, LS(""), NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        // A bit brittle of a test, but it shows that the outparam works.
        auto expected = LS(
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "<meta charset=\"UTF-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">\n"
            "<script>\n"
            "const data_blob = {};\n"
            "</script>\n"
            "<title></title>\n"
            "</head>\n"
            "<body>\n"
            "<div>\n"
            "<div>\n"
            "<div>\n"
            "<ul>\n"
            "<li>\n"
            "Hello World\n"
            "</li>\n"
            "<li>\n"
            "This is amazing!\n"
            "</li>\n"
            "</ul>\n"
            "</div>\n"
            "</div>\n"
            "hello\n"
            "</div>\n"
            "</body>\n"
            "</html>\n");
        TestExpectEquals(expected.length, outdata.length);
        TestExpectEquals(LS_equals(expected, outdata), true);
        if(!LS_equals(expected, outdata)){
            HEREPrint(expected.text);
            HEREPrint(outdata.text);
            }
        const_free(outdata.text);
        }
    TESTEND();
    }
