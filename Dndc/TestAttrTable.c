//
// Copyright © 2022-2023, David Priver
//
#define REPLACE_MALLOCATOR
#define HEAVY_RECORDING
#define USE_TESTING_ALLOCATOR
#include "Allocators/testing_allocator.h"
#include "Utils/testing.h"

#define ATTRIBUTE_THRESH 2
#include "AttrTable.h"

TestFunction(TestAttrTable){
    TESTBEGIN();
    StringView hello = SV("hello");
    StringView hola = SV("hola");
    StringView bonjour = SV("bonjour");
    AttrTable* table = NULL;
    Allocator a = MALLOCATOR;
    int err = AttrTable_set(&table, a, hello, SV("world"));
    TestAssertFalse(err);
    TestExpectEquals(table->count, 1);
    TestExpectEquals(table->capacity, 2);
    TestExpectEquals(table->tombs, 0);
    TestAssert(table);
    {
        StringView value;
        err  = AttrTable_get(table, hello, &value);
        TestAssertFalse(err);
        TestAssertEquals2(SV_equals, value, SV("world"));
    }
    TestAssert(AttrTable_has(table, hello));
    err = AttrTable_set(&table, a, hello, SV("world!"));
    TestAssertFalse(err);
    {
        StringView value;
        err  = AttrTable_get(table, hello, &value);
        TestAssertFalse(err);
        TestAssertEquals2(SV_equals, value, SV("world!"));
    }
    err = AttrTable_del(table, hello);
    TestAssertEquals(err, 1);
    TestAssertFalse(AttrTable_has(table, hello));
    err = AttrTable_set(&table, a, hola, SV("mundo"));
    TestAssertFalse(err);
    err = AttrTable_set(&table, a, bonjour, SV("monde"));
    TestAssertFalse(err);
    err = AttrTable_set(&table, a, hello, SV("world"));
    TestAssertFalse(err);
    TestExpectEquals(table->capacity, 8);
    TestExpectEquals(table->count, 3);
    for(size_t i = 0; i < table->count; i++){
        StringView2* p = AttrTable_items(table)+i;
        TestExpectNotEquals(p->key.length, 0);
        // remembers insertion order
        switch(i){
            case 0:
                TestExpectTrue(SV_equals(p->key, hola));
                break;
            case 1:
                TestExpectTrue(SV_equals(p->key, bonjour));
                break;
            case 2:
                TestExpectTrue(SV_equals(p->key, hello));
                break;
            default:
                TestAssert(0);
                break;
        }
    }

    AttrTable_cleanup(table, a);
    TESTEND();
}

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTest(TestAttrTable);
    int ret = test_main(argc, argv, NULL);
    testing_assert_all_freed();
    return ret;
}

#include "Allocators/allocator.c"
