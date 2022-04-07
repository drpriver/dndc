#include <stdint.h>
#include "long_string.h"
#include "testing.h"
#include "MStringBuilder.h"
#include "Allocators/recording_allocator.h"
#include "msb_format.h"

TestFunction(TestMStringBuilder1){
    TESTBEGIN();
    Allocator a = new_recorded_mallocator();
    MStringBuilder sb = {.allocator=a};
    MSB_FORMAT(&sb, "hello there", " = ", 5, "\n");
    StringView s = msb_borrow_sv(&sb);
    TestExpectEquals2(SV_equals, s, SV("hello there = 5\n"));
    msb_destroy(&sb);
    shallow_free_recorded_mallocator(a);
    TESTEND();
}
TestFunction(TestMStringBuilder2){
    TESTBEGIN();
    Allocator a = new_recorded_mallocator();
    MStringBuilder sb = {.allocator=a};
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
        for(size_t i = 0; i < arrlen(test_cases); i++){
            msb_reset(&sb);
            struct test_case test = test_cases[i];
            msb_write_int32(&sb, test.integer);
            StringView s = msb_borrow_sv(&sb);
            TestExpectEquals2(SV_equals,s, test.sv);
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
        for(size_t i = 0; i < arrlen(test_cases); i++){
            msb_reset(&sb);
            msb_write_literal(&sb, "foo");
            struct test_case test = test_cases[i];
            msb_write_int_space_padded(&sb, test.integer, 8);
            StringView s = msb_borrow_sv(&sb);
            TestExpectEquals2(SV_equals, s, test.sv);
        }
    }
    msb_destroy(&sb);
    shallow_free_recorded_mallocator(a);
    TESTEND();
}
TestFunction(TestMStringBuilder3){
    TESTBEGIN();
    Allocator a = new_recorded_mallocator();
    MStringBuilder sb = {.allocator=a};
    MSB_FORMAT(&sb, "I have ", 2, " apples!");
    {
        StringView s = msb_borrow_sv(&sb);
        TestExpectEquals2(SV_equals, s, SV("I have 2 apples!"));
    }
    MSB_FORMAT(&sb, "\nYou owe me ", 97u, " apples!");
    {
        StringView s = msb_borrow_sv(&sb);
        TestExpectEquals2(SV_equals, s, SV("I have 2 apples!\nYou owe me 97 apples!"));
    }
    msb_destroy(&sb);

    {
        MStringBuilder x = {.allocator = a};
        TestExpectEquals(msb_peek(&x), 0);
        msb_erase(&x, 1);
        TestExpectEquals(msb_peek(&x), 0);
        msb_write_char(&x, 'a');
        msb_erase(&x, 5);
        TestExpectEquals(msb_peek(&x), 0);
    }
    shallow_free_recorded_mallocator(a);
    TESTEND();
}

int
main(int argc, char** argv){
    RegisterTest(TestMStringBuilder1);
    RegisterTest(TestMStringBuilder2);
    RegisterTest(TestMStringBuilder3);
    return test_main(argc, argv);
}
#include "Allocators/allocator.c"
