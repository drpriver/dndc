#ifndef error_handling_h
#define error_handling_h
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
// size_t
#include <stdlib.h>
#include "common_macros.h"
// X macros that is all the error conditions.
// TODO: cleanup these. Only have the ones we actually
// use, or else use them consistently!
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
    /*File IO Errors*/ \
    apply(FILE_ERROR, 9) \
    apply(FILE_NOT_OPENED, 10) \
    /*KWARG*/ \
    apply(MISSING_KWARG, 11) \
    apply(EXCESS_KWARGS, 12) \
    apply(DUPLICATE_KWARG, 13) \
    apply(MISSING_ARG, 14) \
    /*Some low level routine failed*/ \
    apply(OS_ERROR, 15) \
    /*idk man*/ \
    apply(GENERIC_ERROR, 16)\

#ifdef _WIN32
// Windows.h defines NO_ERROR. What a PITA.
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
#undef ERROR_CODES

// This kind of sucks, but given an errorable gets the corresponding c string.
#define get_error_name(err) ({ERROR_NAMES[err.errored];})

#define _Errorable_impl(T) T##__Errorable

// This is for local variables.
#define Errorable(T) struct _Errorable_impl(T)
// This one is for functions. It will make the caller get a warning if
// they ignore the value.
#define Errorable_f(T) warn_unused struct _Errorable_impl(T)
#define Errorable_declare(T) Errorable(T) { T result; ErrorCode errored; }
// Declare some common types
// Errorable(void) is specialized to not have a result field.
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
typedef void* void_ptr;
Errorable_declare(void_ptr);
//
// Assumes there is an Errorable named result in the local scope.
// Sets the errored field to the given value and then returns result.
// Saves a bit of repetition.
//
#define Raise(error_value) ({result.errored = error_value; return result;})

//
// Extracts the inner value from the errorable. Asserts that the error field is
// set to NO_ERROR.
//
#define unwrap(error_holder) ({auto err_ = error_holder;\
                            assert(!err_.errored); \
                            err_.result;})
//
// If the value of "maybe" is an errorable with errored set, Raises that error.
// Otherwise, unwraps the value.
//
#define attempt(maybe) ({   auto const maybe_ = maybe;\
                            if(unlikely(maybe_.errored)) {\
                                result.errored = maybe_.errored;\
                                return result;\
                                }\
                            maybe_.result;})
#endif
