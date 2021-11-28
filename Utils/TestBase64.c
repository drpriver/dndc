#include "testing.h"
#include "long_string.h"
#include "base64.h"
#include "Allocators/recording_allocator.h"

TestFunction(TestBase64){
    TESTBEGIN();
    Allocator a = new_recorded_mallocator();
    MStringBuilder sb = {.allocator=a};
    {
        StringView text = SV("any carnal pleasur");
        msb_write_b64(&sb, text.text, text.length);
        StringView gt = SV("YW55IGNhcm5hbCBwbGVhc3Vy");
        StringView encoded = msb_borrow_sv(&sb);
        TestExpectEquals(gt.length, encoded.length);
        TestExpectEquals2(SV_equals, gt, encoded);
        char decoded[sizeof("any carnal pleasur")] = {};
        Base64Error e = base64_decode(decoded, sizeof(decoded), (const uint8_t*)encoded.text, encoded.length);
        TestExpectEquals(e, BASE64_NO_ERROR);
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
        char decoded[sizeof("any carnal pleasure.")] = {};
        Base64Error e = base64_decode(decoded, sizeof(decoded), (const uint8_t*)encoded.text, encoded.length);
        TestExpectEquals(e, BASE64_NO_ERROR);
        TestExpectEquals2(SV_equals, cstr_to_SV(decoded), text);
        msb_reset(&sb);
    }
    {
        uint64_t data[] = {0x7812231, 0xdeadbeef, 0xcafebabe, 0x1337f00d, 0x888a6a96};
        msb_write_b64(&sb, data, sizeof(data));
        StringView encoded = msb_borrow_sv(&sb);
        uint64_t decoded[arrlen(data)] = {};
        Base64Error e = base64_decode(decoded, sizeof(decoded), (const uint8_t*)encoded.text, encoded.length);
        TestExpectEquals(e, BASE64_NO_ERROR);
        TestExpectEquals(memcmp(decoded, data, sizeof(data)), 0);
        for(size_t i = 0; i < arrlen(data); i++){
            TestExpectEquals(data[i], decoded[i]);
            }
        uint8_t shortbuff[sizeof(decoded)-1] = {};
        Base64Error e2 = base64_decode(shortbuff, sizeof(shortbuff), (const uint8_t*)encoded.text, encoded.length);
        TestExpectNotEquals(e2, BASE64_NO_ERROR);
    }
    msb_destroy(&sb);
    shallow_free_recorded_mallocator(a);
    TESTEND();
    }

TestFunction(TestBase64_2){
    TESTBEGIN();
    Allocator a = new_recorded_mallocator();
    {
        StringView data = SV("YW55IGNhcm5hbCBwbGVhc3Vy");
        ByteBuilder bb = {.allocator = a};
        Base64Error e = bb_decode_b64(&bb, (const unsigned char*)data.text, data.length);
        TestExpectEquals(e, BASE64_NO_ERROR);
        bb_destroy(&bb);
    }
    {
        uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11};
        MStringBuilder sb = {.allocator=a};
        msb_write_b64(&sb,  data, sizeof(data));
        StringView encoded = msb_borrow_sv(&sb);
        StringView expected = SV("AQIDBAUGBwgJCw");
        TestExpectEquals2(SV_equals,encoded, expected);
        uint8_t decoded[arrlen(data)] = {};
        Base64Error e = base64_decode(decoded, sizeof(decoded), (const uint8_t*)encoded.text, encoded.length);
        TestExpectEquals(e, BASE64_NO_ERROR);
        TestExpectEquals(memcmp(decoded, data, sizeof(data)), 0);
        msb_destroy(&sb);
    }
    shallow_free_recorded_mallocator(a);
    TESTEND();
    }

int main(int argc, char** argv){
    RegisterTest(TestBase64);
    RegisterTest(TestBase64_2);
    return test_main(argc, argv);
    }
#include "Allocators/allocator.c"
