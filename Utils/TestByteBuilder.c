#include "testing.h"
#include "ByteBuilder.h"
#include "Allocators/recording_allocator.h"

TestFunction(TestByteBuilder1){
    TESTBEGIN();
    Allocator a = new_recorded_mallocator();
    ByteBuilder bb = {.allocator = a};
    ByteBuilder bb2 = {.allocator = a};
    uint16_t word = 0;
    word |= 'a' << 8;
    word |= 'b';
    bb_write_word(&bb, word);
    bb_write(&bb2, &word, 2);
    ByteBuffer b = bb_borrow(&bb);
    ByteBuffer b2 = bb_borrow(&bb2);
    TestExpectEquals(memcmp(b.buff, &word, sizeof(word)), 0);
    TestExpectEquals(memcmp(b.buff, b2.buff, sizeof(word)), 0);
    bb_destroy(&bb);
    bb_destroy(&bb2);
    shallow_free_recorded_mallocator(a);
    TESTEND();
    }

int
main(int argc, char** argv){
    RegisterTest(TestByteBuilder1);
    // RegisterTest(TestByteBuilder2);
    // RegisterTest(TestByteBuilder3);
    // RegisterTest(TestByteBuilder4);
    // RegisterTest(TestByteBuilder5);
    // RegisterTest(TestByteBuilder6);
    // RegisterTest(TestByteBuilder7);
    // RegisterTest(TestByteBuilder8);
    // RegisterTest(TestByteBuilder9);
    // RegisterTest(TestByteBuilder10);
    return test_main(argc, argv);
}

#include "Allocators/allocator.c"
