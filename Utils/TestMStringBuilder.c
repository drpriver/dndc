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

void register_tests(void){
    RegisterTest(TestMStringBuilder1);
    }
