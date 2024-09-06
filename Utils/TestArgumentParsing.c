//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#include "compiler_warnings.h"
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#include <string.h>
#include "testing.h"
#include "argument_parsing.h"

#define MARRAY_T short
#include "Marray.h"
#include "Allocators/testing_allocator.h"
#include "str_util.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

// make an argparser ready to parse again.
// ArgParsers are usually single shot and we don't
// want this to be a public API.
static inline
void
clear_parser(ArgParser* parser){
    for(size_t i = 0; i < parser->positional.count; i++){
        ArgToParse* arg = &parser->positional.args[i];
        arg->num_parsed = 0;
        arg->visited = 0;
    }
    for(size_t i = 0; i < parser->keyword.count; i++){
        ArgToParse* arg = &parser->keyword.args[i];
        arg->num_parsed = 0;
        arg->visited = 0;
    }
    memset(&parser->failed, 0, sizeof(parser->failed));
}

TestFunction(TestArgumentParsing1){
    TESTBEGIN();
    struct holder {
        int64_t x;
        uint64_t y;
        int64_t z[3];
        LongString a;
        _Bool flag;
        int an_int;
    } h = {0};
    ArgToParse kw_args[] = {
        {
            .name = SV("--x"),
            .min_num=0, .max_num=1,
            .dest = ARGDEST(&h.x),
            .help="",
        },
        {
            .name = SV("--y"),
            .min_num=0, .max_num=1,
            .dest = ARGDEST(&h.y),
            .help="",
        },
        {
            .name = SV("--z"),
            .altname1 = SV("--w"),
            .min_num=0, .max_num=3,
            .dest = ARGDEST(h.z),
            .help="",
        },
        {
            .name = SV("--a"),
            .min_num=0, .max_num=1,
            .dest = ARGDEST(&h.a),
            .help="",
        },
        {
            .name = SV("--f"),
            .min_num=0,
            .max_num=1,
            .dest = ARGDEST(&h.flag),
            .help=""
        },
        {
            .name = SV("--i"),
            .min_num=0, .max_num=1,
            .dest = ARGDEST(&h.an_int),
            .help="",
        },
    };
    ArgParser parser = {
        .description  = "foo",
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
    };
    {
        const char* argv[] = {
            "--x", "1", "--y", "0x00f02", "--z", "3", "4", "5", "--a", "hello",
        };
        Args args= {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&parser, &args, ARGPARSE_FLAGS_NONE);
        TestAssert(!e);
        TestExpectEquals(h.x, 1);
        TestExpectEquals(h.y, 0x00f02u);
        TestAssertEquals(kw_args[2].num_parsed, 3);
        TestExpectEquals(h.z[0], 3);
        TestExpectEquals(h.z[1], 4);
        TestExpectEquals(h.z[2], 5);
        TestExpectEquals(h.flag, 0);
        TestExpectEquals(h.an_int, 0);
        TestAssertEquals(kw_args[3].num_parsed, 1);
        TestExpectEquals(h.a.length, sizeof("hello")-1);
        TestExpectEquals(memcmp(h.a.text, "hello", h.a.length), 0);
        clear_parser(&parser);
        memset(&h, 0, sizeof(h));
    }
    {
        const char* argv[] = {"--x", "--j", "--y", "2"};
        Args args= {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&parser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectTrue(e != ARGPARSE_NO_ERROR);
        clear_parser(&parser);
        memset(&h, 0, sizeof(h));
    }
    {
        // is this allow by C standard?
        const char* argv[1] = {NULL};
        Args args= {0, argv};
        enum ArgParseError e = parse_args(&parser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, ARGPARSE_NO_ERROR);
        clear_parser(&parser);
        memset(&h, 0, sizeof(h));
    }
    {
        const char* argv[] = {"--f"};
        Args args= {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&parser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, ARGPARSE_NO_ERROR);
        TestExpectTrue(h.flag);
        clear_parser(&parser);
        memset(&h, 0, sizeof(h));
    }
    {
        const char* argv[] = {"--f", "--f"};
        Args args= {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&parser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, ARGPARSE_DUPLICATE_KWARG);
        TestExpectTrue(h.flag);
        clear_parser(&parser);
        memset(&h, 0, sizeof(h));
    }
    TESTEND();
}

TestFunction(TestArgumentParsing2){
    TESTBEGIN();
    LongString f = {0};
    ArgToParse kw_args[] = {
        {
            .name = SV("--f"),
            .min_num=0,
            .max_num=1,
            .dest = ARGDEST(&f),
            .help=""
        },
    };
    ArgParser parser = {
        .description = "foo",
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
    };
    {
        const char* argv[] = {"--f", "lol"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&parser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, ARGPARSE_NO_ERROR);
        clear_parser(&parser);
        memset(&f, 0, sizeof(f));
    }
    {
        const char* argv[] = {"--f", "-g", "lol"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&parser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, ARGPARSE_UNKNOWN_KWARG);
        clear_parser(&parser);
        memset(&f, 0, sizeof(f));
    }
    TESTEND();
}

TestFunction(TestArgumentParsing3){
    TESTBEGIN();
    const char*argv[] = {"3.0", "-1e12"};
    float foo = -1.f;
    double bar = 0.2;
    ArgToParse pos_args[] = {
        [0] = {
            .name = SV("foo"),
            .min_num = 1,
            .max_num = 1,
            .dest = ARGDEST(&foo),
        },
        [1] = {
            .name = SV("bar"),
            .min_num = 1,
            .max_num = 1,
            .dest = ARGDEST(&bar),
        },
    };
    ArgToParse kwargs[1] = {0};
    ArgParser argparser = {
        .name = "barzle",
        .description = "A flim flam.",
        .positional.args = pos_args,
        .positional.count = arrlen(pos_args),
        .keyword.args = kwargs,
        .keyword.count = 0,
    };
    Args args = {arrlen(argv), argv};
    enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
    TestExpectEquals((int)e, 0);
    TestExpectEquals(foo, 3.0f);
    TestExpectEquals(bar, -1e12);
    TESTEND();
}

TestFunction(TestArgumentParsing4){
    TESTBEGIN();
    const char* a[2] = {0};
    const char* b[1] = {0};
    ArgToParse pos_args[] = {
        [0] = {
            .name = SV("a"),
            .min_num = 1,
            .max_num = arrlen(a),
            .dest = ARGDEST(&a[0]),
        },
    };
    ArgToParse kw_args[] = {
        {
            .name = SV("-b"),
            .min_num = 0,
            .max_num = arrlen(b),
            .dest = ARGDEST(&b[0]),
        },
    };
    ArgParser argparser = {
        .name = "lmao",
        .description = "lol",
        .positional.args = pos_args,
        .positional.count = arrlen(pos_args),
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
    };
    {
        const char* argv[] = {"a1", "-b", "b1", "c"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, ARGPARSE_EXCESS_ARGS);
        clear_parser(&argparser);
        memset(a, 0, sizeof(a));
        memset(b, 0, sizeof(b));
    }
    {
        const char* argv[] = {"a1", "a2", "-b", "b1"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, 0);
        clear_parser(&argparser);
        memset(a, 0, sizeof(a));
        memset(b, 0, sizeof(b));
    }
    {
        const char* argv[] = {"-b", "b1", "a1", "a2"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, 0);
        clear_parser(&argparser);
        memset(a, 0, sizeof(a));
        memset(b, 0, sizeof(b));
    }
    TESTEND();
}
typedef struct Point Point;
struct Point {
    int x, y;
};
static
int
point_parse(void* _Null_unspecified ud, const char*s, size_t length, void* dest){
    (void)ud;
    SplitPair split = stripped_split(s, length, ',');
    if(!split.tail.length) return 1;
    IntResult x_e = parse_int(split.head.text, split.head.length);
    if(x_e.errored) return x_e.errored;
    IntResult y_e = parse_int(split.tail.text, split.tail.length);
    if(y_e.errored) return y_e.errored;
    Point* p = dest;
    *p = (Point){x_e.result, y_e.result};
    return 0;
}

static
void
point_print(void*vp){
    Point*p = vp;
    printf(" = %d,%d", p->x, p->y);
}

TestFunction(TestParseUserDefined){
    TESTBEGIN();
    struct Point p;
    ArgParseUserDefinedType point_def = {
        .converter = point_parse,
        .type_name = LS("point"),
        .type_size = sizeof(Point),
        .default_printer = point_print,
    };
    ArgToParse pos_args[] = {
        [0] = {
            .name = SV("point"),
            .min_num = 1,
            .max_num = 1,
            .dest = ArgUserDest(&p, &point_def),
        },
    };
    ArgParser argparser = {
        .name = "point printer",
        .description = "prints points",
        .positional.args = pos_args,
        .positional.count = arrlen(pos_args),
    };
    {
        const char* argv[] = {"asd"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, ARGPARSE_CONVERSION_ERROR);
        clear_parser(&argparser);
        memset(&p, 0, sizeof(p));
    }
    {
        const char* argv[] = {"-1,3"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, 0);
        TestExpectEquals(p.x, -1);
        TestExpectEquals(p.y, 3);
        clear_parser(&argparser);
        memset(&p, 0, sizeof(p));
    }
    {
        const char* argv[] = {"4,6"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, 0);
        TestExpectEquals(p.x, 4);
        TestExpectEquals(p.y, 6);
        clear_parser(&argparser);
        memset(&p, 0, sizeof(p));
    }
    TESTEND();
}

TestFunction(TestParseEnum){
    TESTBEGIN();
    enum FooBar{
        NOFOOBAR=0,
        FOO = 1,
        BAR = 2,
    };
    enum FooBar fb = NOFOOBAR;
    StringView names[] = {
        [NOFOOBAR] = SV("no-foo-bar"),
        [FOO] = SV("foo"),
        [BAR] = SV("bar"),
    };
    ArgParseEnumType enum_def = {
        .enum_size = sizeof(enum FooBar),
        .enum_count = arrlen(names),
        .enum_names = names,
    };
    ArgToParse pos_args[] = {
        [0] = {
            .name = SV("foobar"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgEnumDest(&fb, &enum_def),
        },
    };
    ArgParser argparser = {
        .name = "foo-barrer",
        .description = "fooes the barr",
        .positional.args = pos_args,
        .positional.count = arrlen(pos_args),
    };
    {
        const char* argv[] = {"asd"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, ARGPARSE_CONVERSION_ERROR);
        clear_parser(&argparser);
    }
    {
        fb = FOO;
        const char* argv[] = {"no-foo-bar"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, 0);
        TestExpectEquals((int)fb, NOFOOBAR);
        clear_parser(&argparser);
    }
    {
        fb = NOFOOBAR;
        const char* argv[] = {"foo"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, 0);
        TestExpectEquals((int)fb, FOO);
        clear_parser(&argparser);
    }
    TESTEND();
}
TestFunction(TestParseHex){
    TESTBEGIN();

    #define HexTest(hexval) do{\
        char argstring[] = #hexval;\
        Uint64Result e = parse_hex(argstring, sizeof(argstring)-1);\
        TestExpectTrue(! e.errored);\
        if( e.errored) {\
            TestExpectEquals(e.result, hexval);\
        }\
    }while(0)

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

    #define FailHexTest(hexstr, error_code) do{\
        char argstring[] = hexstr;\
        Uint64Result e = parse_hex(argstring, sizeof(argstring)-1);\
        TestExpectEquals((int)e.errored, error_code);\
    }while(0)

    FailHexTest("0 xff",                PARSENUMBER_INVALID_CHARACTER);
    FailHexTest("0xff ",                PARSENUMBER_INVALID_CHARACTER);
    FailHexTest("0xff 0x",              PARSENUMBER_INVALID_CHARACTER);
    FailHexTest("0x",                   PARSENUMBER_UNEXPECTED_END);
    FailHexTest("0X",                   PARSENUMBER_UNEXPECTED_END);
    FailHexTest("0X-",                  PARSENUMBER_INVALID_CHARACTER);
    FailHexTest("X",                    PARSENUMBER_UNEXPECTED_END);
    FailHexTest("a",                    PARSENUMBER_UNEXPECTED_END);
    FailHexTest("1",                    PARSENUMBER_UNEXPECTED_END);
    FailHexTest("?",                    PARSENUMBER_UNEXPECTED_END);
    FailHexTest("0xfff??ff",            PARSENUMBER_INVALID_CHARACTER);
    FailHexTest("0xffffffffffffffffff", PARSENUMBER_OVERFLOWED_VALUE);

    #undef FailHexTest
    TESTEND();
}

TestFunction(TestIntegerParsing){
    TESTBEGIN();
    {
        char digits[6] = {'1', '3', '4', '5', '6', '2'};
        IntResult e = parse_int(digits, 6);
        TestAssertSuccess(e);
        int val = e.result;
        TestExpectEquals(val, 134562);
    }

    {
        Int64Result e2 = parse_int64("9223372036854775807", sizeof("9223372036854775807")-1);
        TestAssertSuccess(e2);
        int64_t val2 = e2.result;
        TestExpectEquals(val2, 9223372036854775807);
    }
    {
        Int64Result e2 = parse_int64("-9223372036854775808", sizeof("-9223372036854775808")-1);
        TestAssertSuccess(e2);
        int64_t val2 = e2.result;
        // Bizarrely, C source code can't properly represent
        // INT64_MIN! So, use the macro!
        TestExpectEquals(val2, INT64_MIN);
    }
    {
        Int64Result e2 = parse_int64("-9223372036854775809", sizeof("-9223372036854775809")-1);
        TestAssertFailure(e2);
    }
    {
        Int64Result e2 = parse_int64("9223372036854775808", sizeof("9223372036854775808")-1);
        TestAssertFailure(e2);
    }
    {
        Int64Result e2 = parse_int64("9223372036854775809", sizeof("9223372036854775809")-1);
        TestAssertFailure(e2);
    }

    #define TESTINT(N) do { \
        Int64Result e = parse_int64(#N, sizeof(#N)-3); \
        TestAssertSuccess(e); \
        int64_t val = e.result; \
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
        Uint64Result e = parse_uint64(#N, sizeof(#N)-4); \
        TestAssertSuccess(e); \
        uint64_t val = e.result; \
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
        Uint64Result e = parse_uint64("88446744073709551615", sizeof("88446744073709551615")-1);
        TestAssertFailure(e);
    }

    #undef TESTUINT

    #define TESTINT(N) do { \
        IntResult e = parse_int(#N, sizeof(#N)-1); \
        TestAssertSuccess(e); \
        int val = e.result; \
        TestExpectEquals(val, N); \
    }while(0)

    TESTINT(3);
    TESTINT(0);
    TESTINT(-1);
    TESTINT(9999);
    TESTINT(2147483647);  // INTMAX
    TESTINT(-2147483647); // INTMIN+1
    TESTINT(-1298);
    TESTINT(31928);
    TESTINT(1128312123);

    #undef TESTINT

    TESTEND();
}

TestFunction(TestHumanIntegers){
    TESTBEGIN();
    {
        char digits[6] = {'1', '3', '4', '5', '6', '2'};
        Uint64Result e = parse_unsigned_human(digits, 6);
        TestAssertSuccess(e);
        uint64_t val = e.result;
        TestExpectEquals(val, 134562);
    }
    {
        char digits[6] = {'#', '3', '4', '5', '6', '2'};
        Uint64Result e = parse_unsigned_human(digits, 6);
        TestAssertSuccess(e);
        uint64_t val = e.result;
        TestExpectEquals(val, 0x34562);
    }
    {
        char digits[6] = {'0', 'b', '1', '1', '0', '1'};
        Uint64Result e = parse_unsigned_human(digits, 6);
        TestAssertSuccess(e);
        uint64_t val = e.result;
        TestExpectEquals(val, 0xd);
    }
    TESTEND();
}

TestFunction(TestBitFlags){
    TESTBEGIN();
    uint64_t flags = 0;
    ArgToParse kw_args[] = {
        {
            .name = SV("--foo"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, 1),
        },
        {
            .name = SV("--bar"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, 2),
        },
        {
            .name = SV("--dango"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, 4),
        },
    };
    ArgParser argparser = {
        .name = "bitter",
        .description = "bits",
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
    };
    {
        flags = 0;
        clear_parser(&argparser);
        const char* argv[] = {"--foo"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, 0);
        TestExpectEquals(flags, 1);
    }
    {
        flags = 0;
        clear_parser(&argparser);
        const char* argv[] = {"--dango"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, 0);
        TestExpectEquals(flags, 4);
    }
    {
        flags = 0;
        clear_parser(&argparser);
        const char* argv[] = {"--bar", "--dango"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, 0);
        TestExpectEquals(flags, 6);
    }
    TESTEND();
}

struct ShortContext {
    Marray(short)* marray;
    Allocator a;
};

static
int
append_short(void* dest, const void* arg){
    struct ShortContext* ctx = dest;
    Marray(short)* marray = ctx->marray;
    int value = *(const int*)arg;
    _Static_assert(sizeof(short) == sizeof(int16_t),"");
    if(value < INT16_MIN)
        return 1;
    if(value > INT16_MAX)
        return 1;
    return Marray_push(short)(marray, ctx->a, value);
}
TestFunction(TestAppender){
    TESTBEGIN();
    Marray(short) shorts = {0};
    struct ShortContext ctx = {
        .marray = &shorts,
        .a = THE_TESTING_ALLOCATOR,
    };
    ArgToParse pos_args[] = {
        [0] = {
            .name = SV("shorts"),
            .min_num = 2,
            .max_num = 1<<16,
            .dest = {
                .pointer = &ctx,
                .type = ARG_INT, // parse as int, handle overfow in append func.
            },
            .append_proc = append_short,
        },
    };
    ArgParser argparser = {
        .name = "short shorter",
        .description = "shorts shorts",
        .positional.args = pos_args,
        .positional.count = arrlen(pos_args),
    };
    {
        const char* argv[] = {"asd"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, ARGPARSE_CONVERSION_ERROR);
        TestExpectEquals(shorts.count, 0);
        Marray_cleanup(short)(&shorts, ctx.a);
        clear_parser(&argparser);
    }
    {
        const char* argv[] = {"-1"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, ARGPARSE_INSUFFICIENT_ARGS);
        TestAssertEquals(shorts.count, 1);
        TestExpectEquals(shorts.data[0], -1);
        Marray_cleanup(short)(&shorts, ctx.a);
        clear_parser(&argparser);
    }
    {
        const char* argv[] = {"4", "6", "8", "10", "12"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, 0);
        TestAssertEquals(shorts.count, 5);
        TestExpectEquals(shorts.data[0], 4);
        TestExpectEquals(shorts.data[1], 6);
        TestExpectEquals(shorts.data[2], 8);
        TestExpectEquals(shorts.data[3], 10);
        TestExpectEquals(shorts.data[4], 12);
        Marray_cleanup(short)(&shorts, ctx.a);
        clear_parser(&argparser);
    }
    {
        const char* argv[] = {"262144"};
        Args args = {arrlen(argv), argv};
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        TestExpectEquals((int)e, ARGPARSE_CONVERSION_ERROR);
        TestExpectEquals(shorts.count, 0);
        Marray_cleanup(short)(&shorts, ctx.a);
        clear_parser(&argparser);
    }
    TESTEND();
}

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTest(TestArgumentParsing1);
    RegisterTest(TestArgumentParsing2);
    RegisterTest(TestArgumentParsing3);
    RegisterTest(TestArgumentParsing4);
    RegisterTest(TestParseHex);
    RegisterTest(TestIntegerParsing);
    RegisterTest(TestHumanIntegers);
    RegisterTest(TestParseUserDefined);
    RegisterTest(TestParseEnum);
    RegisterTest(TestBitFlags);
    RegisterTest(TestAppender);
    int ret = test_main(argc, argv, NULL);
    testing_assert_all_freed();
    return ret;
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "Allocators/allocator.c"

