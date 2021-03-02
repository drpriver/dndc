#include "testing.h"
#include "MStringBuilder.h"
#include "mallocator.h"

TestFunction(TestMStringBuilder1){
    TESTBEGIN();
    MStringBuilder sb = {.allocator=get_mallocator()};
    msb_sprintf(&sb, "%s = %d\n", "hello there", 5);
    auto s = msb_borrow(&sb);
    TestExpectEquals(memcmp(s.text, "hello there = 5\n", s.length), 0);
    msb_destroy(&sb);
    TESTEND();
    }

void register_tests(void){
    RegisterTest(TestMStringBuilder1);
    }
