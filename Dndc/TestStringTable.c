//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#include "compiler_warnings.h"
#include "Utils/testing.h"
#include "string_table.h"
#include "Allocators/mallocator.h"

TestFunction(TestStringTable){
    TESTBEGIN();
    Allocator a = MALLOCATOR;
    StringTable table = {0};
    int err = string_table_set(&table, a, SV("hello"), SV("world"));
    TestAssertFalse(err);
    {
        StringView v;
        int missing = string_table_get(&table, SV("hello"), &v);
        TestAssertFalse(missing);
        TestAssert(v.length);
        TestExpectEquals2(SV_equals, SV("world"), v);
    }
    err = string_table_set(&table, a, SV("hello"), SV("world!"));
    TestAssertFalse(err);
    {
        StringView v;
        int missing = string_table_get(&table, SV("hello"), &v);
        TestAssertFalse(missing);
        TestAssert(v.length);
        TestExpectEquals2(SV_equals, SV("world!"), v);
    }
    string_table_destroy(&table, a);
    TESTEND();
}

int main(int argc, char** argv){
    RegisterTest(TestStringTable);
    return test_main(argc, argv, NULL);
}

#include "Allocators/allocator.c"
