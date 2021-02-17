#ifndef error_handling_h
#define error_handling_h
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
// size_t
#include <stdlib.h>
#include "common_macros.h"
#define ERROR_CODES(apply) \
    apply(NO_ERROR, 0) \
    /*Messages*/ \
    apply(QUIT, 1) \
    apply(BAD_EXIT, 2) \
    /*Container errors*/ \
    apply(NOT_FOUND, 3) \
    apply(OUT_OF_SPACE, 4) \
    /*Parser Errors*/ \
    apply(PARSE_ERROR, 5) \
    apply(INVALID_SYMBOL, 6) \
    apply(UNEXPECTED_END, 7) \
    apply(INFIX_MISSING, 8) \
    /*Buffer Errors*/ \
    apply(UNDERFLOWED_BUFFER, 9) \
    apply(OVERFLOWED_BUFFER, 10) \
    apply(WOULD_OVERFLOW, 11) \
    /* decoding error */ \
    apply(ENCODING_ERROR, 12)\
    apply(DECODING_ERROR, 13)\
    /*Math Errors*/ \
    apply(INVALID_VALUE, 14) \
    apply(OVERFLOWED_VALUE, 15) \
    /*Dice Errors?*/ \
    apply(EXCESSIVE_DICE, 16) \
    /*File IO Errors*/ \
    apply(FILE_ERROR, 17) \
    apply(FILE_NOT_OPENED, 18) \
    apply(FILE_NOT_FOUND, 19) \
    apply(FILE_CREATE_FAILED, 20) \
    apply(NO_BYTES_WRITTEN, 21) \
    apply(NO_BYTES_READ, 22) \
    apply(INVALID_FILE_TYPE, 23) \
    apply(ALREADY_EXISTS, 24) \
    /*Pointer Errors*/ \
    apply(UNEXPECTED_NULL, 25) \
    apply(ALLOC_FAILURE, 26) \
    apply(DEALLOC_FAILURE, 27) \
    /*Initialization Errors*/ \
    apply(ALREADY_INIT, 28) \
    apply(FAILED_INIT, 29) \
    /*SDL Errors*/ \
    apply(SDL_ERROR, 30) \
    /*Combat Error?*/ \
    apply(UNREACHABLE, 31) \
    /*KWARG*/ \
    apply(BAD_KWARG, 32) \
    apply(MISSING_KWARG, 33) \
    apply(EXCESS_KWARGS, 34) \
    apply(DUPLICATE_KWARG, 35) \
    apply(MISSING_ARG, 36) \
    apply(EARLY_END, 37) \
    /*Some low level routine failed*/ \
    apply(OS_ERROR, 38) \
    /*idk man*/ \
    apply(GENERIC_ERROR, 39)\

#ifdef WINDOWS
#undef NO_ERROR
typedef uint8_t ErrorCode;
enum {
#define X(x, v) x = v,
    ERROR_CODES(X)
#undef X
    };
#else
typedef SmallEnum ErrorCode {
#define X(x, v) x = v,
    ERROR_CODES(X)
#undef X
    } ErrorCode;
#endif

#define X(x, v) [x] = #x,
static const char* const ERROR_NAMES[] = {
    ERROR_CODES(X)
    };
#undef X

#define get_error_name(err) ({ERROR_NAMES[err.errored];})

#define _Errorable_impl(T) T##__Errorable
#define Errorable(T) struct _Errorable_impl(T)
#define Errorable_f(T) warn_unused struct _Errorable_impl(T)
#define Errorable_declare(T) Errorable(T) { T result; ErrorCode errored; }
//Declare some common types
typedef void* void_ptr;
struct _Errorable_impl(void) {
    ErrorCode errored;
    };
Errorable_declare(bool);
Errorable_declare(int);
Errorable_declare(char);
Errorable_declare(short);
Errorable_declare(long);
Errorable_declare(size_t);
Errorable_declare(uint8_t);
Errorable_declare(uint16_t);
Errorable_declare(uint32_t);
Errorable_declare(uint64_t);
Errorable_declare(int8_t);
Errorable_declare(int16_t);
Errorable_declare(int32_t);
Errorable_declare(int64_t);
Errorable_declare(float);
Errorable_declare(double);
Errorable_declare(void_ptr);
#define Raise(error_value) ({result.errored = error_value; return result;})
#define unwrap(error_holder) ({auto err_ = error_holder;\
                            assert(!err_.errored); \
                            err_.result;})
#define attempt(maybe) ({   auto const maybe_ = maybe;\
                            if(unlikely(maybe_.errored)) {\
                                result.errored = maybe_.errored;\
                                return result;\
                                }\
                            maybe_.result;})
#endif
