#include "testing.h"
#include "argument_parsing.h"
typedef struct {
    int64_t x;
    uint64_t y;
    int64_t z[3];
    LongString a;
    bool flag;
    int an_int;
    int x_count;
    int y_count;
    int z_count;
    int a_count;
    int f_count;
    int i_count;
} holder;
Errorable_declare(holder);
Errorable_f(holder)
test_parse_args(int argc, const char** argv){
    Errorable(holder) result = {};
    Args args= {argc-1, argv+1};
    ArgToParse kw_args[] = {
        {
            .name = SV("--x"),
            .min_num=0, .max_num=1,
            .dest = ARGDEST(&result.result.x),
            .help="",
        },
        {
            .name = SV("--y"),
            .min_num=0, .max_num=1,
            .dest = ARGDEST(&result.result.y),
            .help="",
        },
        {
            .name = SV("--z"),
            .altname1 = SV("--w"),
            .min_num=0, .max_num=3,
            .dest = ARGDEST(result.result.z),
            .help="",
        },
        {
            .name = SV("--a"),
            .min_num=0, .max_num=1,
            .dest = ARGDEST(&result.result.a),
            .help="",
        },
        {
            .name = SV("--f"),
            .min_num=0,
            .max_num=1,
            .dest = ARGDEST(&result.result.flag),
            .help=""
        },
        {
            .name = SV("--i"),
            .min_num=0, .max_num=1,
            .dest = ARGDEST(&result.result.an_int),
            .help="",
        },
    };
    ArgParser parser = {
        .description  = "foo",
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
        };
    auto e = parse_args(&parser, &args);
    if(e.errored)
        Raise(e.errored);
    // IDK if these are needed, but this is to preserve
    // compatibility with how the parsing used to work
    // and I don't want to rethink the tests yet
    result.result.x_count = kw_args[0].num_parsed;
    result.result.y_count = kw_args[1].num_parsed;
    result.result.z_count = kw_args[2].num_parsed;
    result.result.a_count = kw_args[3].num_parsed;
    result.result.f_count = kw_args[4].num_parsed;
    result.result.i_count = kw_args[5].num_parsed;
    return result;
    }
typedef struct {
    LongString f;
    int f_count;
} holder2;
Errorable_declare(holder2);
Errorable_f(holder2) test_parse_args2(int argc, const char** argv){
    Errorable(holder2) result = {};
    Args args= {argc-1, argv+1};
    ArgToParse kw_args[] = {
        {
            .name = SV("--f"),
            .min_num=0,
            .max_num=1,
            .dest = ARGDEST(&result.result.f),
            .help=""
        },
    };
    ArgParser parser = {
        .description = "foo",
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
        };
    auto e = parse_args(&parser, &args);
    if(e.errored)
        Raise(e.errored);
    result.result.f_count = kw_args[0].num_parsed;
    return result;
    }

TestFunction(TestArgumentParsing1){
    TESTBEGIN();
    const char* argv[] = {
        "bin", "--x", "1", "--y", "0x00f02", "--z", "3", "4", "5", "--a", "hello",
        };
    auto e = test_parse_args(arrlen(argv), argv);
    TestAssert(not e.errored);
    auto const h = unwrap(e);
    TestExpectEquals(h.x, 1);
    TestExpectEquals(h.y, 0x00f02);
    TestAssertEquals(h.z_count, 3);
    TestExpectEquals(h.z[0], 3);
    TestExpectEquals(h.z[1], 4);
    TestExpectEquals(h.z[2], 5);
    TestExpectEquals(h.flag, false);
    TestExpectEquals(h.an_int, 0);
    TestAssertEquals(h.a_count, 1);
    TestExpectEquals(h.a.length, sizeof("hello")-1);
    TestExpectEquals(memcmp(h.a.text, "hello", h.a.length), 0);
    TESTEND();
    }

TestFunction(TestArgumentParsing2){
    TESTBEGIN();
    const char* argv[]  = {"bin", "--x"};
    auto e = test_parse_args(arrlen(argv), argv);
    TestExpectEquals(e.errored, MISSING_ARG);
    TESTEND();
    }
TestFunction(TestArgumentParsing3){
    TESTBEGIN();
    const char* argv[] = {"bin", "--x", "--y", "2"};
    auto e = test_parse_args(arrlen(argv), argv);
    TestExpectEquals(e.errored, MISSING_ARG);
    TESTEND();
    }
TestFunction(TestArgumentParsing4){
    TESTBEGIN();
    const char* argv[] = {"bin", "--x", "--j", "--y", "2"};
    auto e = test_parse_args(arrlen(argv), argv);
    TestExpectTrue(e.errored);
    TESTEND();
    }
TestFunction(TestArgumentParsing5){
    TESTBEGIN();
    const char* argv[] = {"bin"};
    auto e = test_parse_args(arrlen(argv), argv);
    TestExpectSuccess(e);
    TESTEND();
    }

TestFunction(TestArgumentParsing6){
    TESTBEGIN();
    const char* argv[] = {"bin", "--f"};
    auto e = test_parse_args(arrlen(argv), argv);
    TestAssert(not e.errored);
    auto const h = unwrap(e);
    TestExpectEquals(h.flag, true);
    TESTEND();
    }
TestFunction(TestArgumentParsing7){
    TESTBEGIN();
    char argstring[] = "bin --f --f";
    const char* argv[] = {"bin", "--f", "--f"};
    auto e = test_parse_args(arrlen(argv), argv);
    TestExpectEquals(e.errored, DUPLICATE_KWARG);
    TESTEND();
    }
TestFunction(TestArgumentParsing8){
    TESTBEGIN();
    #define HexTest(hexval) ({\
            char argstring[] = #hexval;\
            auto e = parse_hex(argstring, sizeof(argstring)-1);\
            TestExpect(not e.errored);\
            if(not e.errored) {\
                TestExpectEquals(e.result, hexval);\
            }\
            })
    HexTest(0xff);
    HexTest(0xf1b4);
    HexTest(0x88888);
    HexTest(0x1);
    HexTest(0x11223344);
    HexTest(0xffffffffffffffff);
    HexTest(0xFFFF);
    HexTest(0XaaFF);
    HexTest(0XFffF);
    HexTest(0XF0fa);
    #undef HexTest
    #define FailHexTest(hexstr, error_code) ({\
            char argstring[] = hexstr;\
            auto e = parse_hex(argstring, sizeof(argstring)-1);\
            TestExpectEquals(e.errored, error_code);\
            })
    FailHexTest("0 xff", INVALID_SYMBOL);
    FailHexTest("0xff ", INVALID_SYMBOL);
    FailHexTest("0xff 0x", INVALID_SYMBOL);
    FailHexTest("0x", UNEXPECTED_END);
    FailHexTest("0X", UNEXPECTED_END);
    FailHexTest("0X-", INVALID_SYMBOL);
    FailHexTest("X", UNEXPECTED_END);
    FailHexTest("a", UNEXPECTED_END);
    FailHexTest("1", UNEXPECTED_END);
    FailHexTest("?", UNEXPECTED_END);
    FailHexTest("0xfff??ff", INVALID_SYMBOL);
    FailHexTest("0xffffffffffffffffff", OVERFLOWED_VALUE);
    #undef FailHexTest
    TESTEND();
    }

TestFunction(TestArgumentParsing9){
    TESTBEGIN();
    const char* argv[] = {"bin", "--f", "lol"};
    auto e = test_parse_args2(arrlen(argv), argv);
    TestExpectEquals(e.errored, 0);
    TESTEND();
    }

TestFunction(TestArgumentParsing10){
    TESTBEGIN();
    const char* argv[] = {"bin", "--f", "-h", "lol"};
    auto e = test_parse_args2(arrlen(argv), argv);
    TestExpectEquals(e.errored, EXCESS_KWARGS);
    TESTEND();
    }

TestFunction(TestIntegerParsing){
    TESTBEGIN();
    {
    char digits[6] = {'1', '3', '4', '5', '6', '2'};
    auto e = parse_int(digits, 6);
    TestAssertSuccess(e);
    auto val = unwrap(e);
    TestExpectEquals(val, 134562);
    }

    {
    auto e2 = parse_int64("9223372036854775807", sizeof("9223372036854775807")-1);
    TestAssertSuccess(e2);
    auto val2 = unwrap(e2);
    TestExpectEquals(val2, 9223372036854775807);
    }
    {
    auto e2 = parse_int64("-9223372036854775808", sizeof("-9223372036854775808")-1);
    TestAssertSuccess(e2);
    auto val2 = unwrap(e2);
    // Bizarrely, C source code can't properly represent
    // INT64_MIN! So, use the macro!
    TestExpectEquals(val2, INT64_MIN);
    }
    {
    auto e2 = parse_int64("-9223372036854775809", sizeof("-9223372036854775809")-1);
    TestAssertFailure(e2);
    }
    {
    auto e2 = parse_int64("9223372036854775808", sizeof("9223372036854775808")-1);
    TestAssertFailure(e2);
    }
    {
    auto e2 = parse_int64("9223372036854775809", sizeof("9223372036854775809")-1);
    TestAssertFailure(e2);
    }
#define TESTINT(N) do { \
    auto e = parse_int64(#N, sizeof(#N)-3); \
    TestAssertSuccess(e); \
    auto val = unwrap(e); \
    TestExpectEquals(val, N); \
    }while(0)
    TESTINT(128ll);
    TESTINT(0ll);
    TESTINT(-1ll);
    TESTINT(-4ll);
    TESTINT(-1238123821738ll);
    TESTINT(12873812ll);
    TESTINT(21378109127ll);
    TESTINT(-9223372036854775807ll);
#undef TESTINT
#define TESTUINT(N) do { \
    auto e = parse_uint64(#N, sizeof(#N)-4); \
    TestAssertSuccess(e); \
    auto val = unwrap(e); \
    TestExpectEquals(val, N); \
    }while(0)
    TESTUINT(128llu);
    TESTUINT(0llu);
    TESTUINT(1llu);
    TESTUINT(4llu);
    TESTUINT(1238123821738llu);
    TESTUINT(12873812llu);
    TESTUINT(21378109127llu);
    TESTUINT(18446744073709551615llu);
    {
    auto e = parse_uint64("88446744073709551615", sizeof("88446744073709551615")-1);
    TestAssertFailure(e);
    }
#undef TESTUINT

#undef TESTINT
    TESTEND();
    }

TestFunction(TestHumanIntegers){
    TESTBEGIN();
    {
    char digits[6] = {'1', '3', '4', '5', '6', '2'};
    auto e = parse_unsigned_human(digits, 6);
    TestAssertSuccess(e);
    auto val = unwrap(e);
    TestExpectEquals(val, 134562);
    }
    {
    char digits[6] = {'#', '3', '4', '5', '6', '2'};
    auto e = parse_unsigned_human(digits, 6);
    TestAssertSuccess(e);
    auto val = unwrap(e);
    TestExpectEquals(val, 0x34562);
    }
    {
    char digits[6] = {'0', 'b', '1', '1', '0', '1'};
    auto e = parse_unsigned_human(digits, 6);
    TestAssertSuccess(e);
    auto val = unwrap(e);
    TestExpectEquals(val, 0b1101);
    }
    TESTEND();
    }


void register_tests(void){
    RegisterTest(TestArgumentParsing1);
    RegisterTest(TestArgumentParsing2);
    RegisterTest(TestArgumentParsing3);
    RegisterTest(TestArgumentParsing4);
    RegisterTest(TestArgumentParsing5);
    RegisterTest(TestArgumentParsing6);
    RegisterTest(TestArgumentParsing7);
    RegisterTest(TestArgumentParsing8);
    RegisterTest(TestArgumentParsing9);
    RegisterTest(TestArgumentParsing10);
    RegisterTest(TestIntegerParsing);
    RegisterTest(TestHumanIntegers);
    }

