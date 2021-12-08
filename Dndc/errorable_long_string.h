#ifndef ERRORABLE_LONG_STRING_H
#define ERRORABLE_LONG_STRING_H
#include "long_string.h"

typedef struct StringResult {
    LongString result;
    int errored;
}StringResult;

typedef struct StringViewResult {
    StringView result;
    int errored;
}StringViewResult;


#endif
