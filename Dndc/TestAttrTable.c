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

    StringView more_keys[] = {
        SV("a"),
        SV("b"),
        SV("c"),
        SV("d"),
        SV("e"),
        SV("f"),
        SV("g"),
        SV("h"),
        SV("i"),
        SV("j"),
        SV("k"),
        SV("l"),
        SV("m"),
        SV("n"),
        SV("o"),
        SV("p"),
    };
    for(size_t i = 0; i < (sizeof more_keys) / (sizeof more_keys[0]); i++){
        StringView k = more_keys[i];
        if(i & 1){
            err = AttrTable_set(&table, a, k, k);
            TestAssertFalse(err);
            TestAssert(AttrTable_has(table, k));
            StringView* v = NULL;
            err = AttrTable_alloc(&table, a, k, &v);
            TestAssertFalse(err);
            TestAssert(v);
            TestExpectEquals2(SV_equals, *v, k);

        }
        else {
            StringView* v = NULL;
            err = AttrTable_alloc(&table, a, k, &v);
            TestAssert(v);
            TestAssertFalse(err);
            TestAssert(AttrTable_has(table, k));
            *v = k;
            StringView v2 = SV("-1");
            err = AttrTable_get(table, k, &v2);
            TestAssertFalse(err);
            TestExpectEquals2(SV_equals, k, v2);
        }
    }

    {
        TestAssertFalse(AttrTable_has(table, SV("-1")));
        StringView v = SV("-2");
        err = AttrTable_get(table, SV("-1"), &v);
        TestAssert(err);
        TestExpectEquals2(SV_equals, v, SV("-2"));
    }
    for(size_t i = 0; i < 4; i++){
        StringView k = more_keys[i];
        TestAssert(AttrTable_has(table, k));
        int deleted = AttrTable_del(table, k);
        TestAssert(deleted);
        deleted = AttrTable_del(table, k);
        TestAssertFalse(deleted);
        TestAssertFalse(AttrTable_has(table, k));
    }

    StringView even_more_keys[] = {
        SV("A"),
        SV("B"),
        SV("C"),
        SV("D"),
        SV("E"),
        SV("F"),
        SV("G"),
        SV("H"),
        SV("I"),
        SV("J"),
        SV("K"),
        SV("L"),
        SV("M"),
        SV("N"),
        SV("O"),
        SV("P"),
    };
    for(size_t i = 0; i < (sizeof even_more_keys) / (sizeof even_more_keys[0]); i++){
        StringView k = even_more_keys[i];
        if(i & 1){
            err = AttrTable_set(&table, a, k, k);
            TestAssertFalse(err);
            TestAssert(AttrTable_has(table, k));
            StringView* v = NULL;
            err = AttrTable_alloc(&table, a, k, &v);
            TestAssertFalse(err);
            TestAssert(v);
            TestExpectEquals2(SV_equals, *v, k);

        }
        else {
            StringView* v = NULL;
            err = AttrTable_alloc(&table, a, k, &v);
            TestAssert(v);
            TestAssertFalse(err);
            TestAssert(AttrTable_has(table, k));
            *v = k;
            StringView v2 = SV("-1");
            err = AttrTable_get(table, k, &v2);
            TestAssertFalse(err);
            TestExpectEquals2(SV_equals, k, v2);
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
