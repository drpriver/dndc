#ifndef ERRORABLE_LONG_STRING_H
#define ERRORABLE_LONG_STRING_H
#include "long_string.h"

typedef struct StringResult StringResult;
struct StringResult {
    LongString result;
    int errored;
};

typedef struct StringViewResult StringViewResult;
struct StringViewResult {
    StringView result;
    int errored;
};


#endif
