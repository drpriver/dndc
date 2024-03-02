//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#define REPLACE_MALLOCATOR
#define USE_TESTING_ALLOCATOR
#include "testing.h"
#include "Allocators/testing_allocator.h"
#define RARRAY_T int
#include "Rarray.h"

TestFunction(TestCheckSizeExample){
    TESTBEGIN();
    Allocator al = THE_TESTING_ALLOCATOR;
    Rarray(int)* myarray = NULL;
    int err = Rarray_check_size(int)(&myarray, al);
    TestAssertFalse(err);
    TestAssert(myarray);
    TestAssertEquals(myarray->capacity > 0, 1);
    Allocator_free(al, myarray, Rarray_sizeof(int)(myarray));
    TESTEND();
}

TestFunction(TestPushExample){
    TESTBEGIN();
    Allocator al = THE_TESTING_ALLOCATOR;
    Rarray(int)* myarray = NULL;
    int err = Rarray_push(int)(&myarray, al, 1);
    TestAssertFalse(err);
    TestAssert(myarray);
    err = Rarray_push(int)(&myarray, al, 2);
    TestAssertFalse(err);
    TestAssert(myarray);
    err = Rarray_push(int)(&myarray, al, 3);
    TestAssertFalse(err);
    TestAssert(myarray);
    TestAssertEquals(myarray->count, 3);
    TestAssertEquals(myarray->data[0], 1);
    TestAssertEquals(myarray->data[1], 2);
    TestAssertEquals(myarray->data[2], 3);
    Allocator_free(al, myarray, Rarray_sizeof(int)(myarray));
    TESTEND();
}

TestFunction(TestAllocExample){
    TESTBEGIN();
    Allocator al = THE_TESTING_ALLOCATOR;
    Rarray(int)* myarray = NULL;
    // Use a block to scope the allocation as the returned pointer is unstable
    {
        int* a; int err = Rarray_alloc(int)(&myarray, al, &a);
        TestAssertFalse(err);
        TestAssert(myarray);
        TestAssert(a);
        *a = 3;
    }
    {
        int* b; int err = Rarray_alloc(int)(&myarray, al, &b);
        TestAssertFalse(err);
        TestAssert(myarray);
        TestAssert(b);
        *b = 4;
    }
    TestAssertEquals(myarray->count, 2);
    TestAssertEquals(myarray->data[0], 3);
    TestAssertEquals(myarray->data[1], 4);
    Allocator_free(al, myarray, Rarray_sizeof(int)(myarray));
    TESTEND();
}

TestFunction(TestRemoveExample){
    TESTBEGIN();
    Allocator al = MALLOCATOR;
    Rarray(int)* myarray = NULL;
    int err = Rarray_push(int)(&myarray,al, 1);
    TestAssertFalse(err);
    TestAssert(myarray);
    err = Rarray_push(int)(&myarray, al, 2);
    TestAssertFalse(err);
    TestAssert(myarray);
    err = Rarray_push(int)(&myarray, al, 3);
    TestAssertFalse(err);
    TestAssert(myarray);
    err = Rarray_push(int)(&myarray, al, 4);
    TestAssertFalse(err);
    TestAssert(myarray);
    // the 5th one is important to trigger a reallocation.
    err = Rarray_push(int)(&myarray, al, 5);
    TestAssertFalse(err);
    TestAssert(myarray);
    Rarray_remove(int)(myarray, 1);
    TestAssertEquals(myarray->count, 4);
    TestAssertEquals(myarray->data[0], 1);
    TestAssertEquals(myarray->data[1], 3);
    TestAssertEquals(myarray->data[2], 4);
    TestAssertEquals(myarray->data[3], 5);
    Allocator_free(al, myarray, Rarray_sizeof(int)(myarray));
    TESTEND();
}


int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTest(TestCheckSizeExample);
    RegisterTest(TestPushExample);
    RegisterTest(TestAllocExample);
    RegisterTest(TestRemoveExample);
    int ret = test_main(argc, argv, NULL);
    if(!ret) testing_assert_all_freed();
    return ret;
}

#include "Allocators/allocator.c"
