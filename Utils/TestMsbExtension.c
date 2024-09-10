//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#include "compiler_warnings.h"
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#include "Allocators/testing_allocator.h"
#include "long_string.h"
#include "testing.h"
#include "MStringBuilder.h"
#include "msb_extensions.h"

TestFunction(TestKebab){
    TESTBEGIN();
    struct test_case{
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
        {SV(" "), SV("")},
        {SV("x"), SV("x")},
    };
    for(size_t i = 0; i < arrlen(testcases); i++){
        struct test_case* tc = &testcases[i];
        MStringBuilder sb = {.allocator=MALLOCATOR};
        msb_write_kebab(&sb, tc->before.text, tc->before.length);
        StringView str = msb_borrow_sv(&sb);
        TestExpectEquals2(SV_equals, str, tc->after);
        msb_destroy(&sb);
    }
    TESTEND();
}

TestFunction(TestTitle){
    TESTBEGIN();
    struct test_case{
        StringView before;
        StringView after;
    } testcases[] = {
        {SV(""), SV("")},
        {SV("this is some text."), SV("This Is Some Text.")},
        {SV("  hello world1!"), SV("  Hello World1!")},
    };
    for(size_t i = 0; i < sizeof(testcases)/sizeof(testcases[0]);i++){
        struct test_case* tc = &testcases[i];
        MStringBuilder sb = {.allocator=MALLOCATOR};
        msb_write_title(&sb, tc->before.text, tc->before.length);
        StringView str = msb_borrow_sv(&sb);
        TestExpectEquals2(SV_equals, str, tc->after);
        msb_destroy(&sb);
    }
    TESTEND();
}

TestFunction(TestLower){
    TESTBEGIN();
    struct test_case{
        StringView before;
        StringView after;
    } testcases[] = {
        {SV("hello there"), SV("hello there")},
        {SV("H3l--lo"), SV("h3l--lo")},
        {SV("  hi    "), SV("  hi    ")},
        {SV("1   2   3"), SV("1   2   3")},
        {SV("My wonderful cat, Lucy"), SV("my wonderful cat, lucy")},
        {SV("123, North Elm St."), SV("123, north elm st.")},
        {SV(""), SV("")},
        {SV(" "), SV(" ")},
        {SV("x"), SV("x")},
    };
    for(size_t i = 0; i < arrlen(testcases); i++){
        struct test_case* tc = &testcases[i];
        MStringBuilder sb = {.allocator=MALLOCATOR};
        msb_write_lower(&sb, tc->before.text, tc->before.length);
        StringView str = msb_borrow_sv(&sb);
        TestExpectEquals2(SV_equals, str, tc->after);
        msb_destroy(&sb);
    }
    TESTEND();
}

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTest(TestKebab);
    RegisterTest(TestTitle);
    RegisterTest(TestLower);
    int ret = test_main(argc, argv, NULL);
    testing_assert_all_freed();
    return ret;
}
#include "Allocators/allocator.c"
