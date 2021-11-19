#include "dndc_api_def.h"
#include "dndc.h"
#include "dndc_long_string.h"
#include "testing.h"
#include "dndc.c"

static TestFunc TestDndc1;
static TestFunc TestDndc2;
static TestFunc TestDndc3;
static TestFunc TestDndcOutParam;
static TestFunc TestDndcTableMultiline;
static TestFunc TestFormatTable;
static TestFunc TestFormatList;
static TestFunc TestFormatKV;
static TestFunc TestCrashesFixed;
static TestFunc TestExamplesWork;
static TestFunc TestUntrusted;
static TestFunc TestSpecialChars;

int main(int argc, char** argv){
    RegisterTest(TestDndc1);
    RegisterTest(TestDndc2);
    RegisterTest(TestDndc3);
    RegisterTest(TestDndcOutParam);
    RegisterTest(TestDndcTableMultiline);
    RegisterTest(TestFormatTable);
    RegisterTest(TestFormatList);
    RegisterTest(TestFormatKV);
    RegisterTest(TestCrashesFixed);
    RegisterTest(TestExamplesWork);
    RegisterTest(TestUntrusted);
    RegisterTest(TestSpecialChars);
    int ret = test_main(argc, argv);
    return ret;
}

TestFunction(TestDndc1){
    TESTBEGIN();
    LongString source = LS(
        "::md\n"
        "   * Hello World\n"
        "   * This is amazing!\n"
        "::js\n"
        "  ctx.root.add_child('hello');\n"
        );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_DONT_WRITE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        ;
    LongString output = {};
    auto e = run_the_dndc(flags, LS(""), source, LS(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectFalse(output.text);
    TestExpectSuccess(e);
    TESTEND();
    }

TestFunction(TestDndc2){
    TESTBEGIN();
    LongString source = LS(
        "::md\n"
        "   * Hello World\n"
        "   * This is amazing!\n"
        "::js\n"
        "  ctx.root.add_child('hello')\n"
        );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_DONT_WRITE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        ;
    LongString output = {};
    auto e = run_the_dndc(flags, LS(""), source, LS(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectFalse(output.text);
    TestExpectSuccess(e);
    TESTEND();
    }
TestFunction(TestDndc3){
    TESTBEGIN();
    LongString source = LS(
        "::asjdiasjdmd\n"
        "   * Hello World\n"
        "   * This is amazing!\n"
        "::js\n"
        "  ctx.root.add_child('hello')\n"
        );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_DONT_WRITE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        ;
    LongString output = {};
    auto e = run_the_dndc(flags, LS(""), source, LS(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectFalse(output.text);
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
        "::js\n"
        "  ctx.root.add_child('hello')\n"
        );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        ;
    LongString outdata = {};
    auto e = run_the_dndc(flags, LS(""), source, LS("hello.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        // A bit brittle of a test, but it shows that the outparam works.
        auto expected = LS(
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "<meta charset=\"UTF-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">\n"
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
        TestExpectEquals2(LS_equals, expected, outdata);
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
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        ;
    LongString outdata = {};
    auto e = run_the_dndc(flags, LS(""), source, LS("this.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        // A bit brittle of a test, but it shows that the outparam works.
        auto expected = LS(
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "<meta charset=\"UTF-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">\n"
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
        TestExpectEquals2(LS_equals, expected, outdata);
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
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_REFORMAT_ONLY
        ;
    LongString outdata = {};
    auto e = run_the_dndc(flags, LS(""), source, LS("this.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
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
        TestExpectEquals2(LS_equals, expected, outdata);
        const_free(outdata.text);
        }
    source = LS(
            "::table\n"
            "  a\n"
            "  b\n"
            );
    outdata = (LongString){};
    e = run_the_dndc(flags, LS(""), source, LS("test.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        auto expected = LS(
                "::table\n"
                "  a\n"
                "  b\n");
        TestExpectEquals(expected.length, outdata.length);
        TestExpectEquals2(LS_equals, expected, outdata);
        const_free(outdata.text);
        }
    TESTEND();
    }
TestFunction(TestFormatList){
    TESTBEGIN();
    LongString source = LS(
        "Hello\n"
        "1. 1\n"
        "2. 2\n"
        "3. 3\n"
        "4. 4\n"
        "5. 5\n"
        "6. 1\n"
        "7. 2\n"
        "8. 3\n"
        "9. 4\n"
        "10. 5\n"
        "11. 5\n"
        "12. 5\n"
        );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_REFORMAT_ONLY
        ;
    LongString outdata = {};
    auto e = run_the_dndc(flags, LS(""), source, LS("test.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        // A bit brittle of a test, but it shows that the outparam works.
        auto expected = LS(
            "Hello\n"
            "\n"
            "1. 1\n"
            "2. 2\n"
            "3. 3\n"
            "4. 4\n"
            "5. 5\n"
            "6. 1\n"
            "7. 2\n"
            "8. 3\n"
            "9. 4\n"
            "10. 5\n"
            "11. 5\n"
            "12. 5\n"
            );
        TestExpectEquals(expected.length, outdata.length);
        TestExpectEquals2(LS_equals, expected, outdata);
        {
            // check it parses after format
            LongString output = {};
            auto e2 = run_the_dndc(flags|DNDC_DONT_WRITE, LS(""), outdata, LS("test.html"), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            TestExpectFalse(output.text);
            TestExpectSuccess(e2);
        }
        const_free(outdata.text);
        }
    TESTEND();
    }
TestFunction(TestFormatKV){
    TESTBEGIN();
    LongString source = LS(
        "::kv\n"
        "  AC: 13\n"
        "  Attacks: +3 claws (2)\n"
        "    +5 bite\n"
        "  HP: 22\n"
        "  Ref: +3\n"
        "  Fort: +4\n"
        "  Will: +5\n"
        );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_REFORMAT_ONLY
        ;
    LongString outdata = {};
    auto e = run_the_dndc(flags, LS(""), source, LS("test.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        // A bit brittle of a test, but it shows that the outparam works.
        auto expected = LS(
            "::kv\n"
            "  AC:      13\n"
            "  Attacks: +3 claws (2) +5 bite\n"
            "  HP:      22\n"
            "  Ref:     +3\n"
            "  Fort:    +4\n"
            "  Will:    +5\n"
            );
        TestExpectEquals(expected.length, outdata.length);
        TestExpectEquals2(LS_equals, expected, outdata);
        {
            // check it parses after format
            LongString output = {};
            auto e2 = run_the_dndc(flags|DNDC_DONT_WRITE, LS(""),  outdata, LS("test.html"), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            TestExpectFalse(output.text);
            TestExpectSuccess(e2);
        }
        const_free(outdata.text);
        }
    TESTEND();
    }

TestFunction(TestCrashesFixed){
    TESTBEGIN();
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SOURCE_IS_PATH_NOT_DATA
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_DONT_WRITE
        ;
    struct {
        LongString name;
        bool error; // if we expect an error
        } cases[] = {
            {.name=LS("case1.dnd"), .error=false},
            {.name=LS("case2.dnd"), .error=true},
            };
    for(size_t i = 0; i < arrlen(cases); i++){
        LongString output = {};
        auto e = run_the_dndc(flags, LS("TestCases"), cases[i].name, LS("test.html"), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        TestExpectFalse(output.text);
        if(cases[i].error){
            TestExpectFailure(e);
            }
        else {
            TestExpectSuccess(e);
            }
        }
    TESTEND();
    }
TestFunction(TestExamplesWork){
    TESTBEGIN();
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SOURCE_IS_PATH_NOT_DATA
        | DNDC_SUPPRESS_WARNINGS
        // | DNDC_DONT_PRINT_ERRORS
        | DNDC_DONT_WRITE
        ;
    LongString examples[] = {
        LS("calendar.dnd"),
        LS("krugs-basement.dnd"),
        LS("mechanics.dnd"),
        LS("characters.dnd"),
        };
    LongString base_dirs[] = {
        LS("Examples/Calendar"),
        LS("Examples/KrugsBasement"),
        LS("Examples/Rules"),
        LS("Examples/Rules"),
        };
    _Static_assert(arrlen(base_dirs) == arrlen(examples), "");
    for(size_t i = 0; i < arrlen(examples); i++){
        LongString output = {};
        auto e = run_the_dndc(flags, base_dirs[i], examples[i], LS("test.html"), &output, NULL, NULL, dndc_stderr_error_func, NULL, NULL, NULL, NULL, NULL, NULL);
        TestExpectFalse(output.text);
        TestExpectSuccess(e);
        }
    TESTEND();
    }
TestFunction(TestUntrusted){
    TESTBEGIN();
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SOURCE_IS_PATH_NOT_DATA
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_INPUT_IS_UNTRUSTED
        | DNDC_DONT_WRITE
        ;
    LongString examples[] = {
        LS("calendar.dnd"),
        LS("krugs-basement.dnd"),
        LS("mechanics.dnd"),
        LS("characters.dnd"),
        // These aren't great tests.
        LS("untrusted-imports.dnd"),
        LS("untrusted-js.dnd"),
        LS("untrusted-css.dnd"),
        };
    LongString base_dirs[] = {
        LS("Examples/Calendar"),
        LS("Examples/KrugsBasement"),
        LS("Examples/Rules"),
        LS("Examples/Rules"),
        LS("TestCases"),
        LS("TestCases"),
        LS("TestCases"),
        };
    _Static_assert(arrlen(base_dirs) == arrlen(examples), "");
    for(size_t i = 0; i < arrlen(examples); i++){
        LongString output = {};
        auto e = run_the_dndc(flags, base_dirs[i], examples[i], LS("test.html"), &output, NULL, NULL, dndc_stderr_error_func, NULL, NULL, NULL, NULL, NULL, NULL);
        TestExpectFalse(output.text);
        TestExpectFailure(e);
        }
    TESTEND();
    }

static inline
StringView
sv_slice(StringView src, size_t begin, size_t length){
    assert(begin < src.length);
    assert(length <= src.length -begin);
    return (StringView){
        .text = src.text+begin,
        .length = length,
        };
    }
TestFunction(TestSpecialChars){
    TESTBEGIN();
    struct test_case {
        LongString source;
        StringView result;
    };
    uint64_t flags = 0
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_SUPPRESS_WARNINGS;
    struct test_case testcases[] = {
        {LS("---"), SV("&mdash;")},
        {LS("--"), SV("&ndash;")},
        {LS("--- ---"), SV("&mdash; &mdash;")},
        // The ones with all the numbers are long enough to trigger simd code
        {LS("1234567890123456 --- 1234567890123456"), SV("1234567890123456 &mdash; 1234567890123456")},
        {LS("1234567890123456 -- 1234567890123456"), SV("1234567890123456 &ndash; 1234567890123456")},
        {LS("1234567890123456 <p>hi!</p> 1234567890123456"), SV("1234567890123456 &lt;p&gt;hi!&lt;/p&gt; 1234567890123456")},
        {LS("<p>hi!</p> 1234567890123456"), SV("&lt;p&gt;hi!&lt;/p&gt; 1234567890123456")},
        {LS("<p>hi!</p>"), SV("&lt;p&gt;hi!&lt;/p&gt;")},
        {LS("1234567890123456 <i>hi!</i> 1234567890123456"), SV("1234567890123456 <i>hi!</i> 1234567890123456")},
        {LS("<i>hi!</i>"), SV("<i>hi!</i>")},
        {LS("1234567890123456 <b>hi!</b> 1234567890123456"), SV("1234567890123456 <b>hi!</b> 1234567890123456")},
        {LS("1234567890123456 <br>hi! 1234567890123456"), SV("1234567890123456 <br>hi! 1234567890123456")},
        {LS("1234567890123456 <s>hi!</s> 1234567890123456"), SV("1234567890123456 <s>hi!</s> 1234567890123456")},
        {LS("1234567890123456 [hi!] 1234567890123456"), SV("1234567890123456 <a href=\"hi\">hi!</a> 1234567890123456")},
        {LS("1234567890123456 \r 1234567890123456"), SV("1234567890123456   1234567890123456")},
        {LS("hi \r\n"), SV("hi")},
        };
    for(size_t i = 0; i < arrlen(testcases); i++){
        LongString output = {};
        auto e = run_the_dndc(flags, LS(""), testcases[i].source, LS("test.html"), &output, NULL, NULL, dndc_stderr_error_func, NULL, NULL, NULL, NULL, NULL, NULL);
        TestAssertSuccess(e);
        if(!TestExpectEquals2(SV_equals, sv_slice(LS_to_SV(output), 198, testcases[i].result.length), testcases[i].result)){
            TestPrintValue("output", output);
            }
        dndc_free_string(output);
        }
    TESTEND();
    }
