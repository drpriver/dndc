#define DNDC_EXAMPLE
#include <stdio.h>
#include "dndc.h"
#include "dndc_long_string.h"
#include "testing.h"
#define NO_DNDC_AST_API
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
static TestFunc TestFileCache;
static TestFunc TestExpand;
static TestFunc TestMd;
static TestFunc TestUtf16Syntax;

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
    RegisterTest(TestFileCache);
    RegisterTest(TestExpand);
    RegisterTest(TestMd);
    RegisterTest(TestUtf16Syntax);
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
    int e = run_the_dndc(flags, SV(""), source, SV(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestExpectFalse(output.text);
    TestExpectFalse(e);
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
    int e = run_the_dndc(flags, SV(""), source, SV(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestExpectFalse(output.text);
    TestExpectFalse(e);
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
    int e = run_the_dndc(flags, SV(""), source, SV(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestExpectFalse(output.text);
    TestExpectTrue(e);
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
            "<img src=\"SomeImg.png\" width=\"600\" height=\"800\" alt=\"&quot;Hello World!&quot;\">\n"
            "</div>\n"
            );
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        | DNDC_FRAGMENT_ONLY
        ;
    LongString output = {0};
    int e = run_the_dndc(flags, SV(""), source, SV(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestExpectFalse(e);
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
    int e = run_the_dndc(flags, SV(""), source, SV(""), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestExpectFalse(e);
    if(!e){
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
    int e = run_the_dndc(flags, SV(""), source, SV(""), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestExpectFalse(e);
    if(!e){
        // A bit brittle of a test, but it shows that the outparam works.
        LongString expected = LS(
            "<style>\n"
            "p { color: blue;}\n"
            "</style>\n"
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
    int e = run_the_dndc(flags, SV(""), source, SV(""), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestExpectFalse(e);
    if(!e){
        // A bit brittle of a test, but it shows that the outparam works.
        LongString expected = LS(
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "<meta charset=\"UTF-8\">\n"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">\n"
            "</head>\n"
            "<body>\n"
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
    int e = run_the_dndc(flags, SV(""), source, SV(""), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestExpectFalse(e);
    if(!e){
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
    e = run_the_dndc(flags, SV(""), source, SV(""), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestExpectFalse(e);
    if(!e){
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
    int e = run_the_dndc(flags, SV(""), source, SV(""), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    if(!TestExpectFalse(e)){
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
            int e2 = run_the_dndc(flags|DNDC_DONT_WRITE, SV(""), LS_to_SV(outdata),  SV(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
            TestExpectFalse(output.text);
            TestExpectFalse(e2);
        }
        dndc_free_string(outdata);
    }
    TESTEND();
}
TestFunction(TestFormatKV){
    TESTBEGIN();
    StringView source = SV(
        "::kv.a.b.c.d.e.f\n"
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
    int e = run_the_dndc(flags, SV(""), source, SV(""), &outdata, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestExpectFalse(e);
    if(!e){
        // A bit brittle of a test, but it shows that the outparam works.
        LongString expected = LS(
            "::kv .a .b .c .d .e .f\n"
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
            int e2 = run_the_dndc(flags|DNDC_DONT_WRITE, SV(""),  LS_to_SV(outdata), SV(""), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
            TestExpectFalse(output.text);
            TestExpectFalse(e2);
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
        int e = run_the_dndc(flags, SV("TestCases"), LS_to_SV(data.result), LS_to_SV(cases[i].name), &output, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
        TestExpectFalse(output.text);
        if(cases[i].error){
            TestExpectTrue(e);
        }
        else {
            TestExpectFalse(e);
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
        LS("Documentation/OVERVIEW.dnd"),
        LS("Documentation/REFERENCE.dnd"),
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
    DndcWorkerThread* worker = dndc_worker_thread_create();
    for(size_t i = 0; i < arrlen(examples); i++){
        LongString output = {};
        Allocator allocator = get_mallocator();
        TextFileResult data = read_file(examples[i].text, allocator);
        if(data.errored){
            TestPrintValue("Unable to open: examples[i]", examples[i]);
        }
        TestAssertSuccess(data);
        {
            int e = run_the_dndc(flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, dndc_stderr_log_func, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
            TestExpectFalse(output.text);
            if(!TestExpectFalse(e)){
                TestPrintValue("Example failed:", examples[i]);
                TestPrintValue("Base dir:", base_dirs[i]);
            }
        }
        {
            int e = run_the_dndc(flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, dndc_stderr_log_func, NULL, NULL, NULL, NULL, NULL, (WorkerThread*)worker, LS(""));
            TestExpectFalse(output.text);
            if(!TestExpectFalse(e)){
                TestPrintValue("Example failed:", examples[i]);
                TestPrintValue("Base dir:", base_dirs[i]);
            }
        }
        Allocator_free(allocator, data.result.text, data.result.length+1);
    }
    dndc_worker_thread_destroy(worker);
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
        int e = run_the_dndc(flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, dndc_stderr_log_func, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
        TestExpectFalse(output.text);
        if(!TestExpectTrue(e)){
            TestPrintValue("source file", examples[i]);
        }
        Allocator_free(allocator, data.result.text, data.result.length+1);
    }
    StringView inline_examples[] = {
        SV(""
           "::script\n"
           "  foo.js\n"
        ),
        SV(""
           "::js\n"
           "  console.log(Args)\n"
        ),
    };
    for(size_t i = 0; i < arrlen(inline_examples); i++){
        LongString output = {};
        StringView data = inline_examples[i];
        int e = run_the_dndc(flags, base_dirs[i], data, SV("(string input"), &output, NULL, NULL, dndc_stderr_log_func, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
        TestExpectFalse(output.text);
        if(!TestExpectTrue(e)){
            TestPrintValue("source file", examples[i]);
        }
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
        int e = run_the_dndc(flags, SV(""), testcases[i].source, SV(""), &output, NULL, NULL, dndc_stderr_log_func, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
        TestAssertFalse(e);
        if(!TestExpectEquals2(SV_equals, sv_slice(LS_to_SV(output), 172, testcases[i].result.length), testcases[i].result)){
            TestPrintValue("output", output);
        }
        dndc_free_string(output);
    }
    TESTEND();
}

struct TestToken {
    int type, line, col;
    StringViewUtf16 msg;
};

struct test_utf16_data {
    struct TestStats* ts;
    size_t idx;
};

static const struct TestToken tokens[] = {
    [ 0] = {DNDC_SYNTAX_HEADER,              0,  0, SV16("This is a node")},
    [ 1] = {DNDC_SYNTAX_DOUBLE_COLON,        0, 14, SV16("::")},
    [ 2] = {DNDC_SYNTAX_NODE_TYPE,           0, 16, SV16("md")},
    [ 3] = {DNDC_SYNTAX_ATTRIBUTE,           0, 19, SV16("@hello")},
    [ 4] = {DNDC_SYNTAX_DIRECTIVE,           0, 26, SV16("#id")},
    [ 5] = {DNDC_SYNTAX_ATTRIBUTE_ARGUMENT,  0, 30, SV16("hi")},
    [ 6] = {DNDC_SYNTAX_CLASS,               0, 34, SV16(".a")},
    [ 7] = {DNDC_SYNTAX_CLASS,               0, 37, SV16(".b")},
    [ 8] = {DNDC_SYNTAX_HEADER,              1,  2, SV16("This is a table")},
    [ 9] = {DNDC_SYNTAX_DOUBLE_COLON,        1, 17, SV16("::")},
    [10] = {DNDC_SYNTAX_NODE_TYPE,           1, 19, SV16("table")},
    [11] = {DNDC_SYNTAX_ATTRIBUTE,           1, 25, SV16("@ok")},
    [12] = {DNDC_SYNTAX_ATTRIBUTE_ARGUMENT,  1, 29, SV16("((1))")},
    [13] = {DNDC_SYNTAX_DOUBLE_COLON,        4,  0, SV16("::")},
    [14] = {DNDC_SYNTAX_NODE_TYPE,           4,  2, SV16("import")},
    [15] = {DNDC_SYNTAX_DOUBLE_COLON,        6,  0, SV16("::")},
    [16] = {DNDC_SYNTAX_NODE_TYPE,           6,  2, SV16("css")},
    [17] = {DNDC_SYNTAX_DIRECTIVE,           6,  6, SV16("#import")},
    [18] = {DNDC_SYNTAX_DOUBLE_COLON,        8,  0, SV16("::")},
    [19] = {DNDC_SYNTAX_NODE_TYPE,           8,  2, SV16("raw")},
    [20] = {DNDC_SYNTAX_RAW_STRING,          9,  2, SV16("<div>Spooky!</div>")},
    [21] = {DNDC_SYNTAX_DOUBLE_COLON,       10,  0, SV16("::")},
    [22] = {DNDC_SYNTAX_NODE_TYPE,          10,  2, SV16("js")},
    [23] = {DNDC_SYNTAX_JS_KEYWORD,         11,  2, SV16("for")},
    [24] = {DNDC_SYNTAX_JS_VAR,             11,  6, SV16("let")},
    [25] = {DNDC_SYNTAX_JS_IDENTIFIER,      11, 10, SV16("n")},
    [26] = {DNDC_SYNTAX_JS_IDENTIFIER,      11, 12, SV16("of")},
    [27] = {DNDC_SYNTAX_JS_BUILTIN,         11, 15, SV16("ctx")},
    [28] = {DNDC_SYNTAX_JS_IDENTIFIER,      11, 19, SV16("select_nodes")},
    [29] = {DNDC_SYNTAX_JS_BRACE,           11, 32, SV16("{")},
    [30] = {DNDC_SYNTAX_JS_IDENTIFIER,      11, 33, SV16("type")},
    [31] = {DNDC_SYNTAX_JS_BUILTIN,         11, 38, SV16("NodeType")},
    [32] = {DNDC_SYNTAX_JS_NODETYPE,        11, 47, SV16("DIV")},
    [33] = {DNDC_SYNTAX_JS_IDENTIFIER,      11, 52, SV16("classes")},
    [34] = {DNDC_SYNTAX_JS_STRING,          11, 61, SV16("'hi'")},
    [35] = {DNDC_SYNTAX_JS_BRACE,           11, 65, SV16("}")},
    [36] = {DNDC_SYNTAX_JS_BRACE,           11, 68, SV16("{")},
    [37] = {DNDC_SYNTAX_JS_BUILTIN,         12,  4, SV16("console")},
    [38] = {DNDC_SYNTAX_JS_IDENTIFIER,      12, 12, SV16("log")},
    [39] = {DNDC_SYNTAX_JS_STRING,          12, 16, SV16("`hi ${n}\\n`")},
    [40] = {DNDC_SYNTAX_JS_KEYWORD,         13,  4, SV16("if")},
    [41] = {DNDC_SYNTAX_JS_STRING,          13,  7, SV16("'foo bar'")},
    [42] = {DNDC_SYNTAX_JS_IDENTIFIER,      13, 17, SV16("matches")},
    [43] = {DNDC_SYNTAX_JS_REGEX,           13, 25, SV16("/foo\\sbar/g")},
    [44] = {DNDC_SYNTAX_JS_IDENTIFIER,      13, 38, SV16("length")},
    [45] = {DNDC_SYNTAX_JS_KEYWORD,         14,  4, SV16("for")},
    [46] = {DNDC_SYNTAX_JS_VAR,             14,  8, SV16("let")},
    [47] = {DNDC_SYNTAX_JS_IDENTIFIER,      14, 12, SV16("i")},
    [48] = {DNDC_SYNTAX_JS_NUMBER,          14, 16, SV16("0")},
    [49] = {DNDC_SYNTAX_JS_IDENTIFIER,      14, 19, SV16("i")},
    [50] = {DNDC_SYNTAX_JS_NUMBER,          14, 23, SV16("10")},
    [51] = {DNDC_SYNTAX_JS_IDENTIFIER,      14, 27, SV16("i")},
    [52] = {DNDC_SYNTAX_JS_BUILTIN,         14, 32, SV16("console")},
    [53] = {DNDC_SYNTAX_JS_IDENTIFIER,      14, 40, SV16("log")},
    [54] = {DNDC_SYNTAX_JS_IDENTIFIER,      14, 44, SV16("i")},
    [55] = {DNDC_SYNTAX_JS_VAR,             15,  4, SV16("let")},
    [56] = {DNDC_SYNTAX_JS_IDENTIFIER,      15,  8, SV16("x")},
    [57] = {DNDC_SYNTAX_JS_NUMBER,          15, 13, SV16("1")},
    [58] = {DNDC_SYNTAX_JS_VAR,             16,  4, SV16("let")},
    [59] = {DNDC_SYNTAX_JS_IDENTIFIER,      16,  8, SV16("m")},
    [60] = {DNDC_SYNTAX_JS_KEYWORD,         16, 12, SV16("new")},
    [61] = {DNDC_SYNTAX_JS_IDENTIFIER,      16, 16, SV16("Map")},
    [62] = {DNDC_SYNTAX_JS_COMMENT,         18,  4, SV16("/* This is a ")},
    [63] = {DNDC_SYNTAX_JS_COMMENT,         19,  5, SV16("* block")},
    [64] = {DNDC_SYNTAX_JS_COMMENT,         20,  5, SV16("* comment */")},
    [65] = {DNDC_SYNTAX_JS_COMMENT,         21,  4, SV16("// this is a line comment")},
    [66] = {DNDC_SYNTAX_JS_BRACE,           22,  2, SV16("}")},
    [67] = {DNDC_SYNTAX_JS_KEYWORD,         23,  2, SV16("for")},
    [68] = {DNDC_SYNTAX_JS_VAR,             23,  6, SV16("let")},
    [69] = {DNDC_SYNTAX_JS_IDENTIFIER,      23, 10, SV16("i")},
    [70] = {DNDC_SYNTAX_JS_NUMBER,          23, 14, SV16("0")},
    [71] = {DNDC_SYNTAX_JS_IDENTIFIER,      23, 17, SV16("i")},
    [72] = {DNDC_SYNTAX_JS_NUMBER,          23, 21, SV16("10")},
    [73] = {DNDC_SYNTAX_JS_IDENTIFIER,      23, 27, SV16("i")},
    [74] = {DNDC_SYNTAX_JS_BRACE,           23, 29, SV16("{")},
    [75] = {DNDC_SYNTAX_JS_VAR,             24,  5, SV16("const")},
    [76] = {DNDC_SYNTAX_JS_IDENTIFIER,      24, 11, SV16("x")},
    [77] = {DNDC_SYNTAX_JS_BRACE,           24, 15, SV16("{")},
    [78] = {DNDC_SYNTAX_JS_IDENTIFIER,      24, 16, SV16("a")},
    [79] = {DNDC_SYNTAX_JS_NUMBER,          24, 18, SV16("1")},
    [80] = {DNDC_SYNTAX_JS_STRING,          24, 21, SV16("'b'")},
    [81] = {DNDC_SYNTAX_JS_NUMBER,          24, 26, SV16("1")},
    [82] = {DNDC_SYNTAX_JS_NUMBER,          24, 28, SV16("2")},
    [83] = {DNDC_SYNTAX_JS_NUMBER,          24, 30, SV16("3")},
    [84] = {DNDC_SYNTAX_JS_BRACE,           24, 32, SV16("}")},
    [85] = {DNDC_SYNTAX_JS_IDENTIFIER,      25,  7, SV16("x")},
    [86] = {DNDC_SYNTAX_JS_STRING,          25,  9, SV16("'a'")},
    [87] = {DNDC_SYNTAX_JS_BRACE,           26,  2, SV16("}")},
};

void
test_syntax_func(void* ud_, int type, int line, int col, const unsigned short* begin, size_t length){
    struct test_utf16_data* ud = ud_;
    struct TestStats* ts = ud->ts;
    struct TestStats TEST_stats = *ts;
    if(ud->idx >= arrlen(tokens)){
        ts->failures++;
        return;
    }
    struct TestToken token = tokens[ud->idx++];
    int fail1 = !TestExpectEquals(token.type, type);
    int fail2 = !TestExpectEquals(token.line, line);
    int fail3 = !TestExpectEquals(token.col, col);
    int fail4 = !TestExpectEquals(token.msg.length, length);
    StringViewUtf16 msg = {.text=begin, .length=length};
    int equals = SV_utf16_equals(token.msg, msg);
    int fail5 = !TestExpectTrue(equals);
    if(fail1 || fail2 || fail3 || fail4 || fail5){
        TestPrintf("Failed for token: %d:%d\n", token.line, token.col);
        fprintf(stderr, "[%zu] = {%d,%d,%d, SV16(\"", ud->idx++, type, line, col);
        for(size_t i = 0; i < length; i++){
            fputc((char)begin[i], stderr);
        }
        fprintf(stderr, "\")},\n");
        fprintf(stderr, "vs {%d,%d,%d, SV16(\"", token.type, token.line, token.col);
        for(size_t i = 0; i < token.msg.length; i++){
            fputc((char)token.msg.text[i], stderr);
        }
        fprintf(stderr, "\")},\n");
    }
    *ts = TEST_stats;
#if 1
    // ts->failures++;
#endif
}


TestFunction(TestUtf16Syntax){
    TESTBEGIN();

    StringViewUtf16 source = SV16(
            "This is a node::md @hello #id(hi) .a .b\n"
            "  This is a table::table @ok(((1)))\n"
            "  a | b\n"
            "  1 | 2\n"
            "::import\n"
            "  somefile.dnd\n"
            "::css #import\n"
            "  somecss.css\n"
            "::raw\n"
            "  <div>Spooky!</div>\n"
            "::js\n"
            "  for(let n of ctx.select_nodes({type:NodeType.DIV, classes:['hi'})){\n"
            "    console.log(`hi ${n}\\n`);\n"
            "    if('foo bar'.matches(/foo\\sbar/g).length);\n"
            "    for(let i = 0; i < 10; i++) console.log(i);\n"
            "    let x = [1];\n"
            "    let m = new Map();\n"
            "\n"
            "    /* This is a \n"
            "     * block\n"
            "     * comment */\n"
            "    // this is a line comment\n"
            "  }\n"
            "  for(let i = 0; i < 10; --i){\n"
            "     const x = {a:1, 'b':[1,2,3]};\n"
            "     ++x['a'];\n"
            "  }\n"
            );
    struct test_utf16_data ud = {&TEST_stats, 0};
    dndc_analyze_syntax_utf16(source, test_syntax_func, &ud);
    TestExpectEquals(ud.idx, arrlen(tokens));
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
            if(TestExpectTrue(node->attributes)){
                TestExpectEquals(node->attributes->count, 1);
                RARRAY_FOR_EACH(Attribute, attr, node->attributes){
                    TestExpectEquals2(SV_equals, attr->key, SV("1"));
                    TestExpectEquals2(SV_equals, attr->value, SV("1"));
                }
            }
            if(TestExpectTrue(node->classes)){
                TestExpectEquals(node->classes->count, 1);
                RARRAY_FOR_EACH(StringView, cls, node->classes){
                    TestExpectEquals2(SV_equals, *cls, SV("hello"));
                }
            }
        }
        if(node->type == NODE_KEYVALUE){
            TestExpectEquals(node_children_count(node), 1);
        }
        if(node->type == NODE_STRING && NodeHandle_eq(node->parent, ctx->root_handle)){
            TestExpectEquals2(SV_equals, node->header, SV("hi"));
        }
    }
    TestExpectEquals(n_md, 4);

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
            "    m.id = 'hello';\n"
            "    m.id = 'world';\n"
            "    m.noid = true;\n"
            "    m.noinline = true;\n"
            "    if(m.noinline){\n"
            "        m.attributes.set('1', '1');\n"
            "        m.attributes.set('1', '1');\n"
            "        m.attributes.get('2');\n"
            "        m.classes.append('hello');\n"
            "    }\n"
            "    if(!m.has_class('hello')) m.err('oh noes');\n"
            "    m.clone();\n"
            "  }\n"
            "  const kv = ctx.root.make_child(NodeType.KEYVALUE);\n"
            "  kv.set('a', 'b');\n"
            "  kv.set('a', 'b');\n"
            "  if(kv.get('a') != 'b') kv.err();\n"
            "  ctx.root.add_child(ctx.make_string('hi'));\n"
            "  ctx.root.insert_child(0, ctx.make_string('hello'));\n"
            "  ctx.root.replace_child(ctx.root.children[0], ctx.make_string('hi'));\n"
            "  const d = ctx.root.make_child(NodeType.DIV, {header:""+ctx.root+ctx+ctx.base+ctx.nodes});\n"
            "  try {\n"
            "    ctx.root.err('lmao');\n"
            "  }catch(e){"
            "  }\n"
            "");
    uint64_t flags = 0
        | DNDC_DONT_WRITE;
    DndcLongString output;
    int e = run_the_dndc(flags, SV(""),input, SV(""), &output, NULL, NULL, dndc_stderr_log_func, NULL, NULL, NULL, post_js_ast_func, &TEST_stats, NULL, LS(""));
    TestAssertFalse(e);
    TESTEND();
}

TestFunction(TestFileCache){
    TESTBEGIN();
    RecordingAllocator* ra = calloc(1, sizeof(*ra));
    Allocator allocator = {
        ._data = ra,
        .type = ALLOCATOR_RECORDED,
    };
    FileCache cache = {
        .allocator = allocator,
        .scratch = allocator,
    };
    StringView input = SV(
            "::img\n"
            "  Makefile\n"
            );
    uint64_t flags = DNDC_DONT_WRITE;
    DndcLongString output;
    int e = run_the_dndc(flags, SV(""), input, SV(""), &output, &cache, NULL, dndc_stderr_log_func, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    FileCache_clear(&cache);
    for(size_t i = 0; i < ra->count; i++){
        TestExpectEquals((void*)ra->allocations[i], NULL);
        TestExpectEquals(ra->allocation_sizes[i], 0);
    }
    TestAssertFalse(e);
    TESTEND();
}

TestFunction(TestExpand){
    TESTBEGIN();
    StringView input = SV(
            "Hello::md #id(3)\n"
            "  world\n"
            "* It is good to see you\n"
            "* This is a document\n"
            "::js\n"
            "  let div = ctx.root.make_child(NodeType.DIV)\n"
            "  div.id = 'div';\n"
            "::script #noinline\n"
            "  somescript.js\n"
            );
    LongString expected = LS(
            "Hello::md #id(3)\n"
            "  world\n"
            "\n"
            "* It is good to see you\n"
            "* This is a document\n"
            "::script #noinline\n"
            "  somescript.js\n"
            "::div #id(div)\n"
            );
    LongString output;
    uint64_t flags = DNDC_OUTPUT_EXPANDED_DND;
    int e = run_the_dndc(flags, SV(""), input, SV(""), &output, NULL, NULL, dndc_stderr_log_func, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestAssertFalse(e);
    TestExpectEquals2(LS_equals, expected, output);
    dndc_free_string(output);
    TESTEND();
}

TestFunction(TestMd){
    TESTBEGIN();
    StringView input = SV(
            "Go to campaign:\n"
            "* Hello\n"
            "* World\n"
            );
    LongString expected = LS(
            "<p>\n"
            "Go to campaign:\n"
            "</p>\n"
            "<ul>\n"
            "<li>\n"
            "Hello\n"
            "</li>\n"
            "<li>\n"
            "World\n"
            "</li>\n"
            "</ul>\n"
            );
    LongString output;
    uint64_t flags = DNDC_FRAGMENT_ONLY;
    int e = run_the_dndc(flags, SV(""), input, SV(""), &output, NULL, NULL, dndc_stderr_log_func, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestAssertFalse(e);
    TestExpectEquals2(LS_equals, expected, output);
    dndc_free_string(output);

    input = SV(
            "::js\n"
            "  let s = 'Go to campaign:\\n* Hello\\n* World\\n';\n"
            "  node.parent.parse(s)\n"
            // This was for debugging the order of the nodes
            // "  function ltree(n, pref){\n"
            // "       console.log(pref, n); \n"
            // "       for(let child of n.children) \n"
            // "           ltree(child, pref+ '  '); \n"
            // "  }\n"
            // " ltree(node.parent);\n"
            );
    e = run_the_dndc(flags, SV(""), input, SV(""), &output, NULL, NULL, dndc_stderr_log_func, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
    TestAssertFalse(e);
    TestExpectEquals2(LS_equals, expected, output);
    dndc_free_string(output);
    TESTEND();
}

