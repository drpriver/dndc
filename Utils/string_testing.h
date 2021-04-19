#ifndef STRING_TESTING_H
#define STRING_TESTING_H
#include "testing.h"
#include "long_string.h"

#define TestExpectSvEquals(lhs, rhs) do{\
    auto _lhs = lhs; \
    auto _rhs = rhs; \
    TEST_stats.executed++;\
    if(!(SV_equals(_lhs, _rhs))) {\
        TEST_stats.failures++; \
        TestReport("Test condition failed");\
        TestReport("%s == %s", #lhs, #rhs); \
        TestReport(#lhs " = '%.*s'", (int)_lhs.length, _lhs.text);\
        TestReport(#rhs " = '%.*s'", (int)_rhs.length, _rhs.text);\
        }\
    }while(0)

#endif
