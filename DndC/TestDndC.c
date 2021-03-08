#include "testing.h"
#define NOMAIN
#include "dndc.c"

// TODO: more interesting tests.
TestFunction(TestDndC1);
TestFunction(TestDndC2);
TestFunction(TestDndC3);
TestFunction(TestDndcOutParam);
TestFunction(TestDndcTableMultiline);
TestFunction(TestFormatTable);

static inline
void
register_tests(void){
    RegisterTest(TestDndC1);
    RegisterTest(TestDndC2);
    RegisterTest(TestDndC3);
    RegisterTest(TestDndcOutParam);
    RegisterTest(TestDndcTableMultiline);
    RegisterTest(TestFormatTable);
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
    auto e = run_the_dndc(flags, SV(""), source, NULL, LS(""), NULL);
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
    auto e = run_the_dndc(flags, SV(""), source, NULL, LS(""), NULL);
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
    auto e = run_the_dndc(flags, SV(""), source, NULL, LS(""), NULL);
    TestExpectFailure(e);
    TESTEND();
    }
TestFunction(TestDndcOutParam){
    TESTBEGIN();
    LongString source = LS(
        "Hello::title\n"
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
    auto e = run_the_dndc(flags, SV(""), source, &outdata, LS(""), NULL);
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
            "<title>Hello</title>\n"
            "</head>\n"
            "<body>\n"
            "<div>\n"
            "<h1 id=\"hello\">Hello</h1>\n"
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
TestFunction(TestDndcTableMultiline){
    TESTBEGIN();
    LongString source = LS(
        "::table\n"
        "  d8|thing\n"
        "  1|this is a multiline\n"
        "    table\n"
        "  2| This is a singleline cell\n"
        "  3| This\n"
        "     is\n"
        "     another\n"
        "     multiline\n"
        "     table\n"
        );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SOURCE_PATH_IS_DATA_NOT_PATH
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_OUTPUT_PATH_IS_OUT_PARAM
        ;
    LongString outdata = {};
    auto e = run_the_dndc(flags, SV(""), source, &outdata, LS(""), NULL);
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
            "<title>This</title>\n"
            "</head>\n"
            "<body>\n"
            "<div>\n"
            "<div>\n"
            "<table>\n"
            "<thead>\n"
            "<tr>\n"
            "<th>d8\n"
            "</th>\n"
            "<th>thing\n"
            "</th>\n"
            "</tr>\n"
            "</thead>\n"
            "<tbody>\n"
            "<tr>\n"
            "<td>1\n"
            "</td>\n"
            "<td>this is a multiline\n"
            "table\n"
            "</td>\n"
            "</tr>\n"
            "<tr>\n"
            "<td>2\n"
            "</td>\n"
            "<td>This is a singleline cell\n"
            "</td>\n"
            "</tr>\n"
            "<tr>\n"
            "<td>3\n"
            "</td>\n"
            "<td>This\n"
            "is\n"
            "another\n"
            "multiline\n"
            "table\n"
            "</td>\n"
            "</tr>\n"
            "</tbody></table>\n"
            "</div>\n"
            "</div>\n"
            "</body>\n"
            "</html>\n"
            );
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
TestFunction(TestFormatTable){
    TESTBEGIN();
    LongString source = LS(
        "::table\n"
        "  d8|thing\n"
        "  1|this is a multiline\n"
        "    table\n"
        "  2| This is a singleline cell\n"
        "  3| This\n"
        "     is\n"
        "     another\n"
        "     multiline\n"
        "     table\n"
        "  4 | This is a really long text table. As you can see, it is much longer than it really needs to be. But whatever. Long things are long. Long live the long thing! So why not. Be long!\n"
        );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SOURCE_PATH_IS_DATA_NOT_PATH
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_OUTPUT_PATH_IS_OUT_PARAM
        | DNDC_REFORMAT_ONLY
        ;
    LongString outdata = {};
    auto e = run_the_dndc(flags, SV(""), source, &outdata, LS(""), NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        // A bit brittle of a test, but it shows that the outparam works.
        auto expected = LS(
        "::table\n"
        "  d8 | thing\n"
        "  1  | this is a multiline table\n"
        "  2  | This is a singleline cell\n"
        "  3  | This is another multiline table\n"
        "  4  | This is a really long text table. As you can see, it is much longer than\n"
        "       it really needs to be. But whatever. Long things are long. Long live the\n"
        "       long thing! So why not. Be long!\n"
            );
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
