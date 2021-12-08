#include "dndc_api_def.h"
#include "dndc.h"
#include "dndc_long_string.h"
#include "testing.h"
#include "dndc.c"

// These are forward function decls
static TestFunc TestDndc1;
static TestFunc TestDndc2;
static TestFunc TestDndc3;
static TestFunc TestDndcOutParam;
static TestFunc TestDndcFragment;
static TestFunc TestDndcTableMultiline;
static TestFunc TestFormatTable;
static TestFunc TestFormatList;
static TestFunc TestFormatKV;
static TestFunc TestCrashesFixed;
static TestFunc TestExamplesWork;
static TestFunc TestUntrusted;
static TestFunc TestSpecialChars;
static TestFunc TestImgAttributes;
static TestFunc TestJs;

int main(int argc, char** argv){
    RegisterTest(TestDndc1);
    RegisterTest(TestDndc2);
    RegisterTest(TestDndc3);
    RegisterTest(TestDndcOutParam);
    RegisterTest(TestDndcFragment);
    RegisterTest(TestDndcTableMultiline);
    RegisterTest(TestFormatTable);
    RegisterTest(TestFormatList);
    RegisterTest(TestFormatKV);
    RegisterTest(TestCrashesFixed);
    RegisterTest(TestExamplesWork);
    RegisterTest(TestUntrusted);
    RegisterTest(TestSpecialChars);
    RegisterTest(TestImgAttributes);
    RegisterTest(TestJs);
    int ret = test_main(argc, argv);
    return ret;
}

TestFunction(TestDndc1){
    TESTBEGIN();
    StringView source = SV(
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
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    LongString output = {};
    Errorable(void) e = run_the_dndc(flags, SV(""), source, SV(""), SV(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectFalse(output.text);
    TestExpectSuccess(e);
    TESTEND();
}

TestFunction(TestDndc2){
    TESTBEGIN();
    StringView source = SV(
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
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    LongString output = {};
    Errorable(void) e = run_the_dndc(flags, SV(""), source, SV(""), SV(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectFalse(output.text);
    TestExpectSuccess(e);
    TESTEND();
}
TestFunction(TestDndc3){
    TESTBEGIN();
    StringView source = SV(
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
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    LongString output = {};
    Errorable(void) e = run_the_dndc(flags, SV(""), source, SV(""), SV(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectFalse(output.text);
    TestExpectFailure(e);
    TESTEND();
}


TestFunction(TestImgAttributes){
    TESTBEGIN();
    StringView source = SV(
        "::img #noinline\n"
        "   SomeImg.png\n"
        "   width=600\n"
        "   height = 800\n"
        "   alt = \"Hello World!\"\n"
    );
    LongString expected = LS(
            "<div>\n"
            "<div>\n"
            "<img src=\"SomeImg.png\" width=\"600\" height=\"800\" alt=\"&quot;Hello World!&quot;\">\n"
            "</div>\n"
            "</div>\n"
            );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        | DNDC_FRAGMENT_ONLY
        ;
    LongString output = {0};
    Errorable(void) e = run_the_dndc(flags, SV(""), source, SV(""), SV(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    TestExpectEquals2(LS_equals, output, expected);
    dndc_free_string(output);
    TESTEND();
}
TestFunction(TestDndcOutParam){
    TESTBEGIN();
    StringView source = SV(
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
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    LongString outdata = {};
    Errorable(void) e = run_the_dndc(flags, SV(""), source, SV(""), SV("hello.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        // A bit brittle of a test, but it shows that the outparam works.
        LongString expected = LS(
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
        dndc_free_string(outdata);
    }
    TESTEND();
}
TestFunction(TestDndcFragment){
    TESTBEGIN();
    StringView source = SV(
        "Hello::title\n"
        "::md\n"
        "   * Hello World\n"
        "   * This is amazing!\n"
        "::js\n"
        "  ctx.root.add_child('hello')\n"
        "::css\n"
        "  p { color: blue;}\n"
    );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_FRAGMENT_ONLY
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    LongString outdata = {};
    Errorable(void) e = run_the_dndc(flags, SV(""), source, SV(""), SV("hello.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        // A bit brittle of a test, but it shows that the outparam works.
        LongString expected = LS(
            "<style>\n"
            "p { color: blue;}\n"
            "</style>\n"
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
            );
        TestExpectEquals(expected.length, outdata.length);
        TestExpectEquals2(LS_equals, expected, outdata);
        dndc_free_string(outdata);
    }
    TESTEND();
}
TestFunction(TestDndcTableMultiline){
    TESTBEGIN();
    StringView source = SV(
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
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    LongString outdata = {};
    Errorable(void) e = run_the_dndc(flags, SV(""), source, SV(""), SV("this.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        // A bit brittle of a test, but it shows that the outparam works.
        LongString expected = LS(
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
        dndc_free_string(outdata);
    }
    TESTEND();
}
TestFunction(TestFormatTable){
    TESTBEGIN();
    StringView source = SV(
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
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    LongString outdata = {};
    Errorable(void) e = run_the_dndc(flags, SV(""), source, SV(""), SV("this.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        // A bit brittle of a test, but it shows that the outparam works.
        LongString expected = LS(
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
        dndc_free_string(outdata);
    }
    source = SV(
            "::table\n"
            "  a\n"
            "  b\n"
            );
    outdata = (LongString){};
    e = run_the_dndc(flags, SV(""), source, SV(""), SV("test.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        LongString expected = LS(
                "::table\n"
                "  a\n"
                "  b\n");
        TestExpectEquals(expected.length, outdata.length);
        TestExpectEquals2(LS_equals, expected, outdata);
        dndc_free_string(outdata);
    }
    TESTEND();
}
TestFunction(TestFormatList){
    TESTBEGIN();
    StringView source = SV(
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
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    LongString outdata = {};
    Errorable(void) e = run_the_dndc(flags, SV(""), source, SV(""), SV("test.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        // A bit brittle of a test, but it shows that the outparam works.
        LongString expected = LS(
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
            Errorable(void) e2 = run_the_dndc(flags|DNDC_DONT_WRITE, SV(""), LS_to_SV(outdata),  SV(""), SV("test.html"), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            TestExpectFalse(output.text);
            TestExpectSuccess(e2);
        }
        dndc_free_string(outdata);
    }
    TESTEND();
}
TestFunction(TestFormatKV){
    TESTBEGIN();
    StringView source = SV(
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
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    LongString outdata = {};
    Errorable(void) e = run_the_dndc(flags, SV(""), source, SV(""), SV("test.html"), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    TestExpectSuccess(e);
    if(!e.errored){
        // A bit brittle of a test, but it shows that the outparam works.
        LongString expected = LS(
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
            Errorable(void) e2 = run_the_dndc(flags|DNDC_DONT_WRITE, SV(""),  LS_to_SV(outdata), SV(""), SV("test.html"), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            TestExpectFalse(output.text);
            TestExpectSuccess(e2);
        }
        dndc_free_string(outdata);
    }
    TESTEND();
}

TestFunction(TestCrashesFixed){
    TESTBEGIN();
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_DONT_WRITE
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    struct {
        LongString name;
        bool error; // if we expect an error
    } cases[] = {
        {.name=LS("TestCases/case1.dnd"), .error=false},
        {.name=LS("TestCases/case2.dnd"), .error=true},
    };
    for(size_t i = 0; i < arrlen(cases); i++){
        LongString output = {};
        Allocator allocator = get_mallocator();
        TextFileResult data = read_file(cases[i].name.text, allocator);
        TestAssertSuccess(data);
        Errorable(void) e = run_the_dndc(flags, SV("TestCases"), LS_to_SV(data.result), LS_to_SV(cases[i].name), SV("test.html"), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        TestExpectFalse(output.text);
        if(cases[i].error){
            TestExpectFailure(e);
        }
        else {
            TestExpectSuccess(e);
        }
        Allocator_free(allocator, data.result.text, data.result.length+1);
    }
    TESTEND();
}
TestFunction(TestExamplesWork){
    TESTBEGIN();
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        // | DNDC_DONT_PRINT_ERRORS
        | DNDC_DONT_WRITE
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    LongString examples[] = {
        LS("Examples/Calendar/calendar.dnd"),
        LS("Examples/KrugsBasement/krugs-basement.dnd"),
        LS("Examples/Rules/characters.dnd"),
        LS("Examples/Rules/index.dnd"),
        LS("Examples/Rules/mechanics.dnd"),
        LS("Examples/Rules/religion.dnd"),
        LS("Examples/Rules/rules.dnd"),
        LS("Examples/Wiki/Inner/hello.dnd"),
        LS("Examples/Wiki/flat.dnd"),
        LS("Examples/Wiki/index.dnd"),
        LS("Examples/Wiki/lorem.dnd"),
        LS("Examples/Wiki/wiki.dnd"),
        LS("Examples/index.dnd"),
        LS("OVERVIEW.dnd"),
        LS("REFERENCE.dnd"),
        LS("PyGdndc/jsdoc.dnd"),
        LS("PyGdndc/changelog.dnd"),
        LS("PyGdndc/Manual.dnd"),
    };
    StringView base_dirs[] = {
        SV("Examples/Calendar"),
        SV("Examples/KrugsBasement"),
        SV("Examples/Rules"),
        SV("Examples/Rules"),
        SV("Examples/Rules"),
        SV("Examples/Rules"),
        SV("Examples/Rules"),
        SV("Examples/Wiki/Inner"),
        SV("Examples/Wiki"),
        SV("Examples/Wiki"),
        SV("Examples/Wiki"),
        SV("Examples/Wiki"),
        SV("Examples"),
        SV(""),
        SV(""),
        SV("PyGdndc"),
        SV(""),
        SV(""),
    };
    _Static_assert(arrlen(base_dirs) == arrlen(examples), "");
    for(size_t i = 0; i < arrlen(examples); i++){
        LongString output = {};
        Allocator allocator = get_mallocator();
        TextFileResult data = read_file(examples[i].text, allocator);
        if(data.errored){
            TestPrintValue("Unable to open: examples[i]", examples[i]);
        }
        TestAssertSuccess(data);
        Errorable(void) e = run_the_dndc(flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), SV("test.html"), &output, NULL, NULL, dndc_stderr_error_func, NULL, NULL, NULL, NULL, NULL, NULL);
        TestExpectFalse(output.text);
        if(!TestExpectSuccess(e)){
            TestPrintValue("Example failed:", examples[i]);
            TestPrintValue("Base dir:", base_dirs[i]);
        }
        Allocator_free(allocator, data.result.text, data.result.length+1);
    }
    TESTEND();
}
TestFunction(TestUntrusted){
    TESTBEGIN();
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_INPUT_IS_UNTRUSTED
        | DNDC_DONT_WRITE
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    LongString examples[] = {
        LS("Examples/Calendar/calendar.dnd"),
        LS("Examples/KrugsBasement/krugs-basement.dnd"),
        LS("Examples/Rules/mechanics.dnd"),
        LS("Examples/Rules/characters.dnd"),
        // These aren't great tests.
        LS("TestCases/untrusted-imports.dnd"),
        LS("TestCases/untrusted-js.dnd"),
        LS("TestCases/untrusted-css.dnd"),
    };
    StringView base_dirs[] = {
        SV("Examples/Calendar"),
        SV("Examples/KrugsBasement"),
        SV("Examples/Rules"),
        SV("Examples/Rules"),
        SV("TestCases"),
        SV("TestCases"),
        SV("TestCases"),
    };
    _Static_assert(arrlen(base_dirs) == arrlen(examples), "");
    for(size_t i = 0; i < arrlen(examples); i++){
        LongString output = {};
        Allocator allocator = get_mallocator();
        TextFileResult data = read_file(examples[i].text, allocator);
        TestAssertSuccess(data);
        Errorable(void) e = run_the_dndc(flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), SV("test.html"), &output, NULL, NULL, dndc_stderr_error_func, NULL, NULL, NULL, NULL, NULL, NULL);
        TestExpectFalse(output.text);
        if(!TestExpectFailure(e)){
            TestPrintValue("source file", examples[i]);
        }
        Allocator_free(allocator, data.result.text, data.result.length+1);

    }
    TESTEND();
}

TestFunction(TestSpecialChars){
    TESTBEGIN();
    struct test_case {
        StringView source;
        StringView result;
    };
    uint64_t flags = 0
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_SUPPRESS_WARNINGS;
    struct test_case testcases[] = {
        {SV("---"), SV("&mdash;")},
        {SV("--"), SV("&ndash;")},
        {SV("--- ---"), SV("&mdash; &mdash;")},
        // e ones with all the numbers are long enough to trigger simd code
        {SV("1234567890123456 --- 1234567890123456"), SV("1234567890123456 &mdash; 1234567890123456")},
        {SV("1234567890123456 -- 1234567890123456"), SV("1234567890123456 &ndash; 1234567890123456")},
        {SV("1234567890123456 <p>hi!</p> 1234567890123456"), SV("1234567890123456 &lt;p&gt;hi!&lt;/p&gt; 1234567890123456")},
        {SV("<p>hi!</p> 1234567890123456"), SV("&lt;p&gt;hi!&lt;/p&gt; 1234567890123456")},
        {SV("<p>hi!</p>"), SV("&lt;p&gt;hi!&lt;/p&gt;")},
        {SV("1234567890123456 <i>hi!</i> 1234567890123456"), SV("1234567890123456 <i>hi!</i> 1234567890123456")},
        {SV("<i>hi!</i>"), SV("<i>hi!</i>")},
        {SV("1234567890123456 <b>hi!</b> 1234567890123456"), SV("1234567890123456 <b>hi!</b> 1234567890123456")},
        {SV("1234567890123456 <br>hi! 1234567890123456"), SV("1234567890123456 <br>hi! 1234567890123456")},
        {SV("1234567890123456 <s>hi!</s> 1234567890123456"), SV("1234567890123456 <s>hi!</s> 1234567890123456")},
        {SV("1234567890123456 [hi!] 1234567890123456"), SV("1234567890123456 <a href=\"hi\">hi!</a> 1234567890123456")},
        {SV("1234567890123456 \r 1234567890123456"), SV("1234567890123456   1234567890123456")},
        {SV("hi \r\n"), SV("hi")},
    };
    for(size_t i = 0; i < arrlen(testcases); i++){
        LongString output = {};
        Errorable(void) e = run_the_dndc(flags, SV(""), testcases[i].source, SV(""), SV("test.html"), &output, NULL, NULL, dndc_stderr_error_func, NULL, NULL, NULL, NULL, NULL, NULL);
        TestAssertSuccess(e);
        if(!TestExpectEquals2(SV_equals, sv_slice(LS_to_SV(output), 198, testcases[i].result.length), testcases[i].result)){
            TestPrintValue("output", output);
        }
        dndc_free_string(output);
    }
    TESTEND();
}


int 
post_js_ast_func(void* user_data, DndcContext*ctx){
    struct TestStats* ts = user_data;
    struct TestStats TEST_stats = *ts;
    int n_md = 0;
    for(size_t i = 0; i < ctx->nodes.count; i++){
        Node* node = &ctx->nodes.data[i];
        if(node->type == NODE_MD){
            n_md++;
            TestExpectEquals2(endswith, node->header, SV("12345"));
            TestExpectTrue(node->flags & NODEFLAG_NOID);
            TestExpectTrue(node->flags & NODEFLAG_HIDE);
            TestExpectTrue(node->flags & NODEFLAG_NOINLINE);
        }
    }
    TestExpectEquals(n_md, 2);

    *ts = TEST_stats;
    return 0;
}

TestFunction(TestJs){
    TESTBEGIN();
    StringView input = SV(""
            "Hello World::md\n"
            "  This is something special, ain't it\n"
            "::js\n"
            "  const md = ctx.select_nodes({type:NodeType.MD});\n"
            "  for(let m of md){\n"
            "    m.header += '12345';\n"
            "    m.hide = true;\n"
            "    m.noid = true;\n"
            "    m.noinline = true;\n"
            "  }\n"
            "");
    uint64_t flags = 0
        | DNDC_DONT_WRITE;
    DndcLongString output;
    Errorable(void) e = run_the_dndc(flags, SV(""),input, SV(""), SV(""), &output, NULL, NULL, dndc_stderr_error_func, NULL, NULL, NULL, post_js_ast_func, &TEST_stats, NULL);
    TestAssertSuccess(e);
    TESTEND();
}


