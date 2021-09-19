#include "testing.h"
#include "Allocators/recording_allocator.h"
#define RARRAY_T int
#include "Rarray.h"

TestFunction(TestCheckSizeExample){
    TESTBEGIN();
    Allocator al = new_recorded_mallocator();
    Rarray(int)* myarray = NULL;
    myarray = Rarray_check_size(int)(myarray, al);
    TestAssert(myarray);
    TestAssertEquals(myarray->capacity > 0, 1);
    Allocator_free(al, myarray, sizeof(*myarray)+sizeof(*myarray->data)*myarray->capacity);
    shallow_free_recorded_mallocator(al);
    TESTEND();
    }

TestFunction(TestPushExample){
    TESTBEGIN();
    Allocator al = new_recorded_mallocator();
    Rarray(int)* myarray = NULL;
    myarray = Rarray_push(int)(myarray, al, 1);
    TestAssert(myarray);
    myarray = Rarray_push(int)(myarray, al, 2);
    TestAssert(myarray);
    myarray = Rarray_push(int)(myarray, al, 3);
    TestAssert(myarray);
    TestAssertEquals(myarray->count, 3);
    TestAssertEquals(myarray->data[0], 1);
    TestAssertEquals(myarray->data[1], 2);
    TestAssertEquals(myarray->data[2], 3);
    Allocator_free(al, myarray, sizeof(*myarray)+sizeof(*myarray->data)*myarray->capacity);
    shallow_free_recorded_mallocator(al);
    TESTEND();
    }

TestFunction(TestAllocExample){
    TESTBEGIN();
    Allocator al = new_recorded_mallocator();
    Rarray(int)* myarray = NULL;
    // Use a block to scope the allocation as the returned pointer is unstable
    {
        int* a = Rarray_alloc(int)(&myarray, al);
        TestAssert(myarray);
        TestAssert(a);
        *a = 3;
    }
    {
        int* b = Rarray_alloc(int)(&myarray, al);
        TestAssert(myarray);
        TestAssert(b);
        *b = 4;
    }
    TestAssertEquals(myarray->count, 2);
    TestAssertEquals(myarray->data[0], 3);
    TestAssertEquals(myarray->data[1], 4);
    shallow_free_recorded_mallocator(al);
    TESTEND();
    }

TestFunction(TestRemoveExample){
    TESTBEGIN();
    Allocator al = new_recorded_mallocator();
    Rarray(int)* myarray = NULL;
    myarray = Rarray_push(int)(myarray, al, 1);
    TestAssert(myarray);
    myarray = Rarray_push(int)(myarray, al, 2);
    TestAssert(myarray);
    myarray = Rarray_push(int)(myarray, al, 3);
    TestAssert(myarray);
    Rarray_remove(int)(myarray, 1);
    TestAssertEquals(myarray->count, 2);
    TestAssertEquals(myarray->data[0], 1);
    TestAssertEquals(myarray->data[1], 3);
    shallow_free_recorded_mallocator(al);
    TESTEND();
    }


int main(int argc, char** argv){
    RegisterTest(TestCheckSizeExample);
    RegisterTest(TestPushExample);
    RegisterTest(TestAllocExample);
    RegisterTest(TestRemoveExample);
    return test_main(argc, argv);
}

#include "Allocators/allocator.c"
