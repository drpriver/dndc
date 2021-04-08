#ifndef LONG_STRING_TYPE_H
#define LONG_STRING_TYPE_H
// for uint16_t
#include <stdint.h>
// for size_t
#include <stddef.h>

#ifndef __clang__

#ifndef _Nonnull
#define _Nonnull
#endif

#ifndef _Nullable
#define _Nullable
#endif

#ifndef _Null_unspecified
#define _Null_unspecified
#endif

#endif

// LongStrings and StringViews are very similar.
// A LongString is basically a StringView with a guaranteed nul-terminator.
// It is unspecified if a StringView has a nul-terminator.
typedef struct LongString {
    size_t length; // excludes the terminating NUL
    const char* _Null_unspecified text; // utf-8 encoded text
} LongString;

typedef struct StringView {
    size_t length;
    const char*_Null_unspecified text; // utf-8 encoded text
} StringView;

typedef struct StringViewUtf16 {
    size_t length; // in code units
    const uint16_t*_Null_unspecified text; // utf-16 encoded code points
} StringViewUtf16;

#endif
