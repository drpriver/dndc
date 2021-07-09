#include "long_string.h"
#include "testing.h"
#include "string_testing.h"
#include "MStringBuilder.h"
#include "mallocator.h"
#include "msb_format.h"
#include "msb_extensions.h"

TestFunction(TestMStringBuilder1){
    TESTBEGIN();
    MStringBuilder sb = {.allocator=get_mallocator()};
    MSB_FORMAT(&sb, "hello there", " = ", 5, "\n");
    auto s = msb_borrow(&sb);
    TestExpectSvEquals(s, SV("hello there = 5\n"));
    msb_destroy(&sb);
    TESTEND();
    }
TestFunction(TestMStringBuilder2){
    TESTBEGIN();
    MStringBuilder sb = {.allocator=get_mallocator()};
    struct test_case {
        int integer;
        StringView sv;
    };
    {
    struct test_case test_cases[] = {
        {9999, SV("9999")},
        {-9999, SV("-9999")},
        {0, SV("0")},
        {1929128, SV("1929128")},
        {-1929128, SV("-1929128")},
        {2147483647, SV("2147483647")},
        {INT32_MIN, SV("-2147483648")}, // INT32_MIN cannot be written as a literal in C
    };
    for(int i = 0; i < arrlen(test_cases); i++){
        msb_reset(&sb);
        auto test = test_cases[i];
        msb_write_int32(&sb, test.integer);
        auto s = msb_borrow(&sb);
        TestExpectSvEquals(s, test.sv);
        }
    }
    {
    struct test_case test_cases[] = {
        {9999,       SV("foo    9999")},
        {-9999,      SV("foo   -9999")},
        {0,          SV("foo       0")},
        {1929128,    SV("foo 1929128")},
        {-1929128,   SV("foo-1929128")},
        {2147483647, SV("foo2147483647")},
        {INT32_MIN,  SV("foo-2147483648")}, // INT32_MIN cannot be written as a literal in C
    };
    for(int i = 0; i < arrlen(test_cases); i++){
        msb_reset(&sb);
        msb_write_literal(&sb, "foo");
        auto test = test_cases[i];
        msb_write_int_space_padded(&sb, test.integer, 8);
        auto s = msb_borrow(&sb);
        TestExpectSvEquals(s, test.sv);
        }
    }
    msb_destroy(&sb);
    TESTEND();
    }
TestFunction(TestMStringBuilder3){
    TESTBEGIN();
    MStringBuilder sb = {.allocator=get_mallocator()};
    MSB_FORMAT(&sb, "I have ", 2, " apples!");
    {
    auto s = msb_borrow(&sb);
    TestExpectSvEquals(s, SV("I have 2 apples!"));
    }
    MSB_FORMAT(&sb, "\nYou owe me ", 97u, " apples!");
    {
    auto s = msb_borrow(&sb);
    TestExpectSvEquals(s, SV("I have 2 apples!\nYou owe me 97 apples!"));
    }
    msb_destroy(&sb);
    TESTEND();
    }
TestFunction(TestKebab){
    TESTBEGIN();
    struct {
        StringView before;
        StringView after;
        } testcases[] = {
            {SV("hello there"), SV("hello-there")},
            {SV("H3l--lo"), SV("h3l-lo")},
            {SV("  hi    "), SV("hi")},
            {SV("1   2   3"), SV("1-2-3")},
            {SV("My wonderful cat, Lucy"), SV("my-wonderful-cat-lucy")},
            {SV("123, North Elm St."), SV("123-north-elm-st")},
            {SV(""), SV("")},
        };
    for(int i = 0; i < arrlen(testcases); i++){
        auto tc = &testcases[i];
        MStringBuilder sb = {.allocator=get_mallocator()};
        msb_write_kebab(&sb, tc->before.text, tc->before.length);
        auto str = msb_borrow(&sb);
        TestExpectSvEquals(str, tc->after);
        msb_destroy(&sb);
        }
    TESTEND();
    }

int main(int argc, char** argv){
    RegisterTest(TestMStringBuilder1);
    RegisterTest(TestMStringBuilder2);
    RegisterTest(TestMStringBuilder3);
    RegisterTest(TestKebab);
    return test_main(argc, argv);
    }
#include "allocator.c"
