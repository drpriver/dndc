//
// Copyright © 2021-2022, David Priver
//
#include "Utils/testing.h"
#include "string_table.h"
#include "Allocators/mallocator.h"

TestFunction(TestStringTable){
    TESTBEGIN();
    StringTable table = {.allocator = get_mallocator()};
    string_table_set(&table, SV("hello"), SV("world"));
    {
        const StringView* v = string_table_get(&table, SV("hello"));
        TestAssert(v);
        TestExpectEquals2(SV_equals, SV("world"), *v);
    }
    string_table_set(&table, SV("hello"), SV("world!"));
    {
        const StringView* v = string_table_get(&table, SV("hello"));
        TestAssert(v);
        TestExpectEquals2(SV_equals, SV("world!"), *v);
    }
    string_table_destroy(&table);
    TESTEND();
}

int main(int argc, char** argv){
    RegisterTest(TestStringTable);
    return test_main(argc, argv);
}

#include "Allocators/allocator.c"
