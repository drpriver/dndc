#include "testing.h"
#include "MStringBuilder.h"
#include "mallocator.h"

TestFunction(TestMStringBuilder1){
    TESTBEGIN();
    auto a = get_mallocator();
    MStringBuilder sb = {};
    msb_sprintf(&sb, a, "%s = %d\n", "hello there", 5);
    auto s = msb_borrow(&sb, a);
    TestExpectEquals(memcmp(s.text, "hello there = 5\n", s.length), 0);
    msb_destroy(&sb, a);
    TESTEND();
    }

TestFunction(TestMStringBuilder2){
    TESTBEGIN();
    auto a = get_mallocator();
    MStringBuilder sb = {};
    msb_write_cstr(&sb, a, " this is a string with trailing whitespace \t\n\t  \n");
    const char* stripped = " this is a string with trailing whitespace";
    msb_rstrip(&sb);
    auto s = msb_detach(&sb, a);
    auto len = strlen(stripped);
    TestAssertEquals(s.length, len);
    TestExpectEquals(memcmp(s.text, stripped, len), 0);
    Allocator_free(a, s.text, s.length+1);
    const char* other = "This is a string without trailing whitespace";
    msb_write_cstr(&sb, a, other);
    msb_rstrip(&sb);
    len = strlen(other);
    s = msb_detach(&sb, a);
    TestAssertEquals(s.length, len);
    TestExpectEquals(memcmp(s.text, other, len), 0);
    Allocator_free(a, s.text, s.length+1);
    TESTEND();
    }

TestFunction(TestMStringBuilder3){
    TESTBEGIN();
    auto a = get_mallocator();
    MStringBuilder sb = {};
    msb_write_cstr(&sb, a, "hello");
    msb_rjust(&sb, a, ' ', 2);
    {
    auto str = msb_borrow(&sb, a);
    TestExpectEquals(strcmp(str.text, "hello"), 0);
    }
    msb_ljust(&sb, a, ' ', 2);
    {
    auto str = msb_borrow(&sb, a);
    TestExpectEquals(strcmp(str.text, "hello"), 0);
    }
    msb_rjust(&sb, a, ' ', 10);
    {
    auto str = msb_borrow(&sb, a);
    TestExpectEquals(strcmp(str.text, "     hello"), 0);
    }
    msb_ljust(&sb, a, ' ', 15);
    {
    auto str = msb_borrow(&sb, a);
    TestExpectEquals(strcmp(str.text, "     hello     "), 0);
    }
    msb_destroy(&sb, a);
    TESTEND();
    }

void register_tests(void){
    RegisterTest(TestMStringBuilder1);
    RegisterTest(TestMStringBuilder2);
    RegisterTest(TestMStringBuilder3);
    }
