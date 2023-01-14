//
// Copyright © 2021-2023, David Priver
//
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#include "testing.h"
#include "long_string.h"
#include "base64.h"
#include "Allocators/testing_allocator.h"

TestFunction(TestBase64){
    TESTBEGIN();
    Allocator a = THE_TESTING_ALLOCATOR;
    MStringBuilder sb = {.allocator=a};
    {
        StringView text = SV("any carnal pleasur");
        msb_write_b64(&sb, text.text, text.length);
        StringView gt = SV("YW55IGNhcm5hbCBwbGVhc3Vy");
        StringView encoded = msb_borrow_sv(&sb);
        TestExpectEquals(gt.length, encoded.length);
        TestExpectEquals2(SV_equals, gt, encoded);
        char decoded[sizeof("any carnal pleasur")] = {0};
        Base64Error e = base64_decode(decoded, sizeof(decoded), (const uint8_t*)encoded.text, encoded.length);
        TestExpectEquals((int)e, BASE64_NO_ERROR);
        TestExpectEquals2(SV_equals, cstr_to_SV(decoded), text);
        msb_reset(&sb);
    }
    {
        StringView text = SV("any carnal pleasure.");
        msb_write_b64(&sb, text.text, text.length);
        StringView gt = SV("YW55IGNhcm5hbCBwbGVhc3VyZS4");
        StringView encoded = msb_borrow_sv(&sb);
        TestExpectEquals(encoded.length, gt.length);
        TestExpectEquals2(SV_equals, encoded, gt);
        char decoded[sizeof("any carnal pleasure.")] = {0};
        Base64Error e = base64_decode(decoded, sizeof(decoded), (const uint8_t*)encoded.text, encoded.length);
        TestExpectEquals((int)e, BASE64_NO_ERROR);
        TestExpectEquals2(SV_equals, cstr_to_SV(decoded), text);
        msb_reset(&sb);
    }
    {
        uint64_t data[] = {0x7812231, 0xdeadbeef, 0xcafebabe, 0x1337f00d, 0x888a6a96};
        msb_write_b64(&sb, data, sizeof(data));
        StringView encoded = msb_borrow_sv(&sb);
        uint64_t decoded[arrlen(data)] = {0};
        Base64Error e = base64_decode(decoded, sizeof(decoded), (const uint8_t*)encoded.text, encoded.length);
        TestExpectEquals((int)e, BASE64_NO_ERROR);
        TestExpectEquals(memcmp(decoded, data, sizeof(data)), 0);
        for(size_t i = 0; i < arrlen(data); i++){
            TestExpectEquals(data[i], decoded[i]);
        }
        uint8_t shortbuff[sizeof(decoded)-1] = {0};
        Base64Error e2 = base64_decode(shortbuff, sizeof(shortbuff), (const uint8_t*)encoded.text, encoded.length);
        TestExpectNotEquals((int)e2, BASE64_NO_ERROR);
    }
    msb_destroy(&sb);
    TESTEND();
}

TestFunction(TestBase64_2){
    TESTBEGIN();
    Allocator a = THE_TESTING_ALLOCATOR;
    {
        StringView data = SV("YW55IGNhcm5hbCBwbGVhc3Vy");
        size_t size = base64_decode_size(data.length);
        void* buff = Allocator_alloc(a, size);

        Base64Error e = base64_decode(buff, size, (const unsigned char*)data.text, data.length);
        TestExpectEquals((int)e, BASE64_NO_ERROR);
        Allocator_free(a, buff, size);
    }
    {
        uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11};
        MStringBuilder sb = {.allocator=a};
        msb_write_b64(&sb,  data, sizeof(data));
        StringView encoded = msb_borrow_sv(&sb);
        StringView expected = SV("AQIDBAUGBwgJCw");
        TestExpectEquals2(SV_equals,encoded, expected);
        uint8_t decoded[arrlen(data)] = {0};
        Base64Error e = base64_decode(decoded, sizeof(decoded), (const uint8_t*)encoded.text, encoded.length);
        TestExpectEquals((int)e, BASE64_NO_ERROR);
        TestExpectEquals(memcmp(decoded, data, sizeof(data)), 0);
        msb_destroy(&sb);
    }
    TESTEND();
}

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTest(TestBase64);
    RegisterTest(TestBase64_2);
    int ret = test_main(argc, argv, NULL);
    testing_assert_all_freed();
    return ret;
}
#include "Allocators/allocator.c"
