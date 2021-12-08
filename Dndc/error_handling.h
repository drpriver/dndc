#ifndef error_handling_h
#define error_handling_h
#include <stdint.h>
// size_t
#include <stddef.h>

#ifndef warn_unused

#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused _Check_return
#else
#define warn_unused
#endif

#endif

// X macros that is all the error conditions.
#define ERROR_CODES(apply) \
    apply(NO_ERROR, 0) \
    apply(ALLOC_FAILURE, 1) \
    /*Parser Errors*/ \
    apply(PARSE_ERROR, 2) \
    apply(INVALID_SYMBOL, 3) \
    apply(UNEXPECTED_END, 4) \
    /*Buffer Errors*/ \
    apply(WOULD_OVERFLOW, 5) \
    /* decoding error */ \
    apply(ENCODING_ERROR, 6)\
    apply(DECODING_ERROR, 7)\
    /*Math Errors*/ \
    apply(OVERFLOWED_VALUE, 8) \
    /*Some low level routine failed*/ \
    apply(OS_ERROR, 11) \
    /*idk man*/ \
    apply(GENERIC_ERROR, 12)\
    apply(FORMAT_ERROR, 13)\

#ifdef _WIN32
// Windows.h defines NO_ERROR. What a PITA.
#undef NO_ERROR
#endif
enum {
    #define X(x, v) x = v,
    ERROR_CODES(X)
    #undef X
};

#define X(x, v) [x] = #x,
static const char* const ERROR_NAMES[] = {
    ERROR_CODES(X)
};
#undef X
#undef ERROR_CODES

#endif
