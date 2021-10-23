#include "str_util.h"
#include "testing.h"
TestFunction(TestStrip){
    TESTBEGIN();
    struct TestCase{
        StringView prestrip;
        StringView poststrip;
    } testvalues[] = {
        {SV(" as d as  "), SV("as d as")},
        {SV("hello"), SV("hello")},
        {SV(""), SV("")},
        {SV("   \r\t\t\n"), SV("")},
        {SV("   \r\t\t\nyo"), SV("yo")},
        {SV("   \r\t\t\nyo   \t\t"), SV("yo")},
        {SV("yo   \t\t"), SV("yo")},
        };
    for(int i = 0; i < arrlen(testvalues); i++){
        struct TestCase* testval = &testvalues[i];
        StringView pre = testval->prestrip;
        StringView stripped = stripped_view(pre.text, pre.length);
        TestExpectEquals2(SV_equals,stripped, testval->poststrip);
        }
    TESTEND();
    }

TestFunction(TestCmp){
    TESTBEGIN();
    StringView strings[] = {
        SV("ajfk"),
        SV(""),
        SV("a"),
        SV("asfoo"),
        SV("asd"),
        SV("b"),
        SV("A"),
        SV("asdas"),
        };
    StringView expected[] = {
        SV(""),
        SV("A"),
        SV("a"),
        SV("ajfk"),
        SV("asd"),
        SV("asdas"),
        SV("asfoo"),
        SV("b"),
        };
    qsort(strings, arrlen(strings), sizeof(strings[0]), StringView_cmp);
    TestExpectEquals(memcmp(strings, expected, sizeof(strings)), 0);
    TESTEND();
    }

int main(int argc, char** argv){
    RegisterTest(TestStrip);
    RegisterTest(TestCmp);
    return test_main(argc, argv);
    }
