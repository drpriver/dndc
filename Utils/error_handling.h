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
    /*File IO Errors*/ \
    apply(FILE_ERROR, 16) \
    apply(FILE_NOT_OPENED, 17) \
    apply(FILE_NOT_FOUND, 18) \
    apply(FILE_CREATE_FAILED, 19) \
    apply(NO_BYTES_WRITTEN, 20) \
    apply(NO_BYTES_READ, 21) \
    apply(INVALID_FILE_TYPE, 22) \
    apply(ALREADY_EXISTS, 23) \
    /*Pointer Errors*/ \
    apply(UNEXPECTED_NULL, 24) \
    apply(ALLOC_FAILURE, 25) \
    apply(DEALLOC_FAILURE, 26) \
    /*Initialization Errors*/ \
    apply(ALREADY_INIT, 27) \
    apply(FAILED_INIT, 28) \
    /*KWARG*/ \
    apply(BAD_KWARG, 29) \
    apply(MISSING_KWARG, 30) \
    apply(EXCESS_KWARGS, 31) \
    apply(DUPLICATE_KWARG, 32) \
    apply(MISSING_ARG, 33) \
    apply(EARLY_END, 34) \
    /*Some low level routine failed*/ \
    apply(OS_ERROR, 35) \
    /*idk man*/ \
    apply(GENERIC_ERROR, 36)\

#ifdef WINDOWS
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
