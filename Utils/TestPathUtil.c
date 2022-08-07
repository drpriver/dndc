//
// Copyright © 2021-2022, David Priver
//
#define BACKSLASH_IS_A_PATH_SEP 1
#include "path_util.h"
#include "testing.h"

struct PathTestCase {
    StringView input;
    StringView expected;
};
TestFunction(TestBaseName){
    TESTBEGIN();
    struct PathTestCase cases[] = {
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
    for(size_t i = 0; i < arrlen(cases); i++){
        StringView expected = cases[i].expected;
        StringView result = path_basename(cases[i].input);
        bool equality = SV_equals(expected, result);
        TestExpectTrue(equality);
        if(!equality){
            TestPrintValue("result", result);
            TestPrintValue("expected", expected);
        }
    }
    TESTEND();
}

TestFunction(TestDirName){
    TESTBEGIN();
    struct PathTestCase cases[] = {
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
    for(size_t i = 0; i < arrlen(cases); i++){
        StringView expected = cases[i].expected;
        StringView result = path_dirname(cases[i].input);
        bool equality = SV_equals(expected, result);
        TestExpectTrue(equality);
        if(!equality){
            TestPrintValue("result", result);
            TestPrintValue("expected", expected);
        }
    }
    TESTEND();
}

TestFunction(TestStripExtension){
    TESTBEGIN();
    struct PathTestCase cases[] = {
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
    for(size_t i = 0; i < arrlen(cases); i++){
        StringView expected = cases[i].expected;
        StringView result = path_strip_extension(cases[i].input);
        bool equality = SV_equals(expected, result);
        TestExpectTrue(equality);
        if(!equality){
            TestPrintValue("result", result);
            TestPrintValue("expected", expected);
        }
    }
    TESTEND();
}

int main(int argc, char** argv){
    RegisterTest(TestBaseName);
    RegisterTest(TestDirName);
    RegisterTest(TestStripExtension);
    return test_main(argc, argv, NULL);
}
