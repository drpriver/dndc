#ifndef LONG_STRING_TYPE_H
#define LONG_STRING_TYPE_H
// for size_t
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// LongStrings and StringViews are very similar.
// A LongString is basically a StringView with a guaranteed nul-terminator.
// It is unspecified if a StringView has a nul-terminator.
typedef struct LongString {
    size_t length; // excludes the terminating NUL
    const char* text; // utf-8 encoded text
} LongString;

typedef struct StringView {
    size_t length;
    const char* text; // utf-8 encoded text
} StringView;

// Avoiding including <stdint.h> in public header.
_Static_assert(sizeof(unsigned short) == 2, "unsigned short is not uint16_t");
typedef struct StringViewUtf16 {
    size_t length; // in code units
    const unsigned short* text; // utf-16 encoded code points
} StringViewUtf16;

#ifdef __cplusplus
}
#endif

#endif
