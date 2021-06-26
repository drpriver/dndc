#include "testing.h"
#include "long_string.h"
#include "base64.h"
#include "mallocator.h"

TestFunction(TestBase64){
    TESTBEGIN();
    MStringBuilder sb = {.allocator=get_mallocator()};
    {
        StringView text = SV("any carnal pleasur");
        msb_write_b64(&sb, text.text, text.length);
        StringView gt = SV("YW55IGNhcm5hbCBwbGVhc3Vy");
        auto encoded = msb_borrow(&sb);
        TestExpectEquals(gt.length, encoded.length);
        TestExpectEquals2(!strcmp, gt.text, encoded.text);
        char decoded[sizeof("any carnal pleasur")] = {};
        auto e = base64_decode(decoded, sizeof(decoded), (const uint8_t*)encoded.text, encoded.length);
        TestExpectSuccess(e);
        TestExpectEquals2(!strcmp,decoded, text.text);
        msb_reset(&sb);
    }
    {
        StringView text = SV("any carnal pleasure.");
        msb_write_b64(&sb, text.text, text.length);
        StringView gt = SV("YW55IGNhcm5hbCBwbGVhc3VyZS4");
        auto encoded = msb_borrow(&sb);
        TestExpectEquals(encoded.length, gt.length);
        TestExpectEquals2(!strcmp,encoded.text, gt.text);
        char decoded[sizeof("any carnal pleasure.")] = {};
        auto e = base64_decode(decoded, sizeof(decoded), (const uint8_t*)encoded.text, encoded.length);
        TestExpectSuccess(e);
        TestExpectEquals2(!strcmp,decoded, text.text);
        msb_reset(&sb);
    }
    {
        uint64_t data[] = {0x7812231, 0xdeadbeef, 0xcafebabe, 0x1337f00d, 0x888a6a96};
        msb_write_b64(&sb, data, sizeof(data));
        auto encoded = msb_borrow(&sb);
        uint64_t decoded[arrlen(data)] = {};
        auto e = base64_decode(decoded, sizeof(decoded), (const uint8_t*)encoded.text, encoded.length);
        TestExpectSuccess(e);
        TestExpectEquals(memcmp(decoded, data, sizeof(data)), 0);
        for(size_t i = 0; i < arrlen(data); i++){
            TestExpectEquals(data[i], decoded[i]);
            }
        uint8_t shortbuff[sizeof(decoded)-1] = {};
        auto e2 = base64_decode(shortbuff, sizeof(shortbuff), (const uint8_t*)encoded.text, encoded.length);
        TestExpectFailure(e2);
    }
    msb_destroy(&sb);
    TESTEND();
    }

TestFunction(TestBase64_2){
    TESTBEGIN();
    auto a = get_mallocator();
    {
        StringView data = SV("YW55IGNhcm5hbCBwbGVhc3Vy");
        ByteBuilder bb = {.allocator = a};
        auto e = bb_decode_b64(&bb, (const unsigned char*)data.text, data.length);
        TestExpectSuccess(e);
        bb_destroy(&bb);
    }
    {
        uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 11};
        MStringBuilder sb = {.allocator=a};
        msb_write_b64(&sb,  data, sizeof(data));
        StringView encoded = msb_borrow(&sb);
        StringView expected = SV("AQIDBAUGBwgJCw");
        TestExpectEquals2(SV_equals,encoded, expected);
        uint8_t decoded[arrlen(data)] = {};
        auto e = base64_decode(decoded, sizeof(decoded), (const uint8_t*)encoded.text, encoded.length);
        TestExpectSuccess(e);
        TestExpectEquals(memcmp(decoded, data, sizeof(data)), 0);
        msb_destroy(&sb);
    }
    TESTEND();
    }

int main(int argc, char** argv){
    RegisterTest(TestBase64);
    RegisterTest(TestBase64_2);
    return test_main(argc, argv);
    }
#include "allocator.c"
