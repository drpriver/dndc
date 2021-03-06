#include "testing.h"
#define BACKSLASH_IS_A_PATH_SEP
#include "path_util.h"

struct TestCase {
    StringView input;
    StringView expected;
};
TestFunction(TestBaseName){
    TESTBEGIN();
    struct TestCase cases[] = {
        {SV("/foo/bar/baz"), SV("baz")},
        {SV(""), SV("")},
        {SV("foo"), SV("foo")},
        {SV("/"), SV("")},
        {SV("/foo/bar/"), SV("")},
        {SV("//bar/"), SV("")},

        {SV("\\foo\\bar\\baz"), SV("baz")},
        {SV(""), SV("")},
        {SV("foo"), SV("foo")},
        {SV("\\"), SV("")},
        {SV("\\foo\\bar\\"), SV("")},
        {SV("\\\\bar\\"), SV("")},
    };
    for(int i = 0; i < arrlen(cases); i++){
        auto expected = cases[i].expected;
        auto result = path_basename(cases[i].input);
        auto equality = SV_equals(expected, result);
        TestExpectTrue(equality);
        if(!equality){
            HERE("result: '%.*s'", (int)result.length, result.text);
            HERE("expected: '%.*s'", (int)expected.length, expected.text);
            }
        }
    TESTEND();
    }

TestFunction(TestDirName){
    TESTBEGIN();
    struct TestCase cases[] = {
        {SV("/foo/bar/baz"), SV("/foo/bar")},
        {SV(""), SV("")},
        {SV("foo"), SV("")},
        {SV("/"), SV("/")},
        {SV("/foo/bar/"), SV("/foo/bar")},
        {SV("//bar/"), SV("//bar")},

        {SV("\\foo\\bar\\baz"), SV("\\foo\\bar")},
        {SV(""), SV("")},
        {SV("foo"), SV("")},
        {SV("\\"), SV("\\")},
        {SV("\\foo\\bar\\"), SV("\\foo\\bar")},
        {SV("\\\\bar\\"), SV("\\\\bar")},
    };
    for(int i = 0; i < arrlen(cases); i++){
        auto expected = cases[i].expected;
        auto result = path_dirname(cases[i].input);
        auto equality = SV_equals(expected, result);
        TestExpectTrue(equality);
        if(!equality){
            HERE("result: '%.*s'", (int)result.length, result.text);
            HERE("expected: '%.*s'", (int)expected.length, expected.text);
            }
        }
    TESTEND();
    }

TestFunction(TestStripExtension){
    TESTBEGIN();
    struct TestCase cases[] = {
        {SV("/foo/bar/baz"), SV("/foo/bar/baz")},
        {SV(""), SV("")},
        {SV("foo"), SV("foo")},
        {SV("/"), SV("/")},
        {SV("/foo/bar/"), SV("/foo/bar/")},
        {SV("//bar/"), SV("//bar/")},
        {SV("/bar/foo.txt"), SV("/bar/foo")},
        {SV("/bar/foo."), SV("/bar/foo")},
        {SV("/bar/foo.txt.mx"), SV("/bar/foo.txt")},
        {SV("/bar/foo.txt.mx"), SV("/bar/foo.txt")},
        {SV("lmao.txt"), SV("lmao")},

        {SV("\\foo\\bar\\baz"), SV("\\foo\\bar\\baz")},
        {SV(""), SV("")},
        {SV("foo"), SV("foo")},
        {SV("\\"), SV("\\")},
        {SV("\\foo\\bar\\"), SV("\\foo\\bar\\")},
        {SV("\\\\bar\\"), SV("\\\\bar\\")},
        {SV("\\bar\\foo.txt"), SV("\\bar\\foo")},
        {SV("\\bar\\foo."), SV("\\bar\\foo")},
        {SV("\\bar\\foo.txt.mx"), SV("\\bar\\foo.txt")},
        {SV("\\bar\\foo.txt.mx"), SV("\\bar\\foo.txt")},
        {SV("lmao.txt"), SV("lmao")},
    };
    for(int i = 0; i < arrlen(cases); i++){
        auto expected = cases[i].expected;
        auto result = path_strip_extension(cases[i].input);
        auto equality = SV_equals(expected, result);
        TestExpectTrue(equality);
        if(!equality){
            HERE("result: '%.*s'", (int)result.length, result.text);
            HERE("expected: '%.*s'", (int)expected.length, expected.text);
            }
        }
    TESTEND();
    }

void register_tests(void){
    RegisterTest(TestBaseName);
    RegisterTest(TestDirName);
    RegisterTest(TestStripExtension);
    }

#include "allocator.c"
