//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
#ifndef COMMON_MACROS_H
#define COMMON_MACROS_H

// I've made a half-hearted attempt to get things to work with non-gcc,
// non-clang compilers. Oh well.
// This header shouldn't leak into the public interface.

#if 0
#include "Utils/debugging.h"
#endif

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline __attribute__((always_inline))
#else
#define force_inline
#endif
#endif

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

// Gets the length of an array.
#ifndef arrlen
#define arrlen(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef unreachable
#if defined(__GNUC__) || defined(__clang__)
#define unreachable() __builtin_unreachable()
#else
#define unreachable() __assume(0)
#endif
#endif

// This is for TODO error handling. Don't use bare assert for that as asserts
// are for invariants and are left in after development is done, whereas
// these represent defects in the code that needs to be re-written.
#ifndef unhandled_error_condition
#include <assert.h>
#ifdef DEBUGGING_H // debugging.h was included
#define unhandled_error_condition(cond) do {if(cond)bt(); assert(!(cond));}while(0)
#else
#define unhandled_error_condition(cond) do {assert(!(cond));}while(0)
#endif
#endif

#ifndef unimplemented
  #if defined(__GNUC__) || defined(__clang__)
    #ifdef DEBUGGING_H
      #define unimplemented() do{bt(); assert(!"This code not implemented yet.");__builtin_unreachable();} while(0)
    #else
      #define unimplemented() do{assert(!"This code not implemented yet.");__builtin_unreachable();} while(0)
    #endif
  #else
    #ifdef DEBUGGING_H
      #define unimplemented() do{bt(); assert(!"This code not implemented yet.");}while(0)
    #else
      #define unimplemented() assert(!"This code not implemented yet.")
    #endif
  #endif
#endif

#ifdef __clang__
#define breakpoint() __builtin_debugtrap()
#elif defined(_MSC_VER)
#define breakpoint() __debugbreak()
#elif defined(__GNUC__)
// TODO: breakpoints on arm
#define breakpoint()  asm("int $3")
#endif

//
// Warning Suppression
//
// Pragmas to suppress warnings. Currently only supporting
// clang and gcc, but using these macros means I can use the
// compiler-specific pragmas.
//
#ifdef __clang__

#define SuppressNullabilityComplete()   _Pragma("clang diagnostic ignored \"-Wnullability-completeness\"")
#define SuppressUnusedFunction()        _Pragma("clang diagnostic ignored \"-Wunused-function\"")
#define SuppressDiscardQualifiers()     _Pragma("clang diagnostic ignored \"-Wincompatible-pointer-types-discards-qualifiers\"")
#define SuppressCastQual()              _Pragma("clang diagnostic ignored \"-Wcast-qual\"")
#define SuppressCastFunction()
#define SuppressMissingBraces()         _Pragma("clang diagnostic ignored \"-Wmissing-braces\"")
#define SuppressDoublePromotion()       _Pragma("clang diagnostic ignored \"-Wdouble-promotion\"")
#define SuppressCoveredSwitchDefault()  _Pragma("clang diagnostic ignored \"-Wcovered-switch-default\"")
#define SuppressVisibility()            _Pragma("clang diagnostic ignored \"-Wvisibility\"")
#define SuppressNullableConversion()    _Pragma("clang diagnostic ignored \"-Wnullable-to-nonnull-conversion\"")
#define SuppressShadowing()             _Pragma("clang diagnostic ignored \"-Wshadow\"")
#define SuppressEnumCompare()
#define SuppressStringPlusInt()         _Pragma("clang diagnostic ignored \"-Wstring-plus-int\"")

#define PushDiagnostic()                _Pragma("clang diagnostic push")
#define PopDiagnostic()                 _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)

#define SuppressNullabilityComplete()
#define SuppressUnusedFunction()        _Pragma("GCC diagnostic ignored \"-Wunused-function\"")
#define SuppressDiscardQualifiers()     _Pragma("GCC diagnostic ignored \"-Wdiscarded-qualifiers\"")
#define SuppressCastQual()              _Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#define SuppressCastFunction()          _Pragma("GCC diagnostic ignored \"-Wcast-function-type\"")
#define SuppressMissingBraces()         _Pragma("GCC diagnostic ignored \"-Wmissing-braces\"")
#define SuppressDoublePromotion()       _Pragma("GCC diagnostic ignored \"-Wdouble-promotion\"")
#define SuppressCoveredSwitchDefault()
#define SuppressVisibility()
#define SuppressNullableConversion()
#define SuppressShadowing()             _Pragma("GCC diagnostic ignored \"-Wshadow\"")
#define SuppressEnumCompare()           _Pragma("GCC diagnostic ignored \"-Wenum-compare\"")
#define SuppressStringPlusInt()

#define PushDiagnostic()                _Pragma("GCC diagnostic push")
#define PopDiagnostic()                 _Pragma("GCC diagnostic pop")

#else

#define SuppressNullabilityComplete()
#define SuppressUnusedFunction()
#define SuppressDiscardQualifiers()
#define SuppressCastQual()
#define SuppressCastFunction()
#define SuppressMissingBraces()
#define SuppressDoublePromotion()
#define SuppressCoveredSwitchDefault()
#define SuppressVisibility()
#define SuppressNullableConversion()
#define SuppressShadowing()
#define SuppressEnumCompare()
#define SuppressStringPlusInt()

#define PushDiagnostic()
#define PopDiagnostic()

#endif

//
// Allocator attributes
//
// MALLOC_FUNC: this function is malloc-like and the compiler
//   is free to assume the pointer returned does not alias any
//   other pointer. Note that realloc does not meet that criteria
//   as it can fail and leave the original pointer valid.
// ALLOCATOR_SIZE(N): the argument at the specified index
//   (1-based) is the size of the storage that the returned
//   pointer points to.
//
#if defined(__GNUC__) || defined(__clang__)
#define ALLOCATOR_SIZE(N) __attribute__((alloc_size(N)))
#define MALLOC_FUNC __attribute__((malloc))
#else
#define ALLOCATOR_SIZE(...)
#define MALLOC_FUNC
#endif

//
// Likely-hinting.
// Allows expressing that a condition is likely or unlikely to the optimizer.
// This is of dubious value for most functions (although with those that are
// heavily used and heavily inlined it does add up).
// It is somewhat helpeful for the programmer to indicate some if statement
// is unlikely to be true (probably checking for some unlikely error condition).
//

#if !defined(likely) && !defined(unlikely)
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif


//
// printf-like attributes
//

#ifndef printf_func
#if defined(__GNUC__) || defined(__clang__)
#define printf_func(fmt_idx, vararg_idx) \
    __attribute__((__format__ (__printf__, fmt_idx, vararg_idx)))
#else
#define printf_func(...)
#endif
#endif

// Realloc's signature is silly which makes it hard to
// reimplement in a sane way. So in order to accomodate
// platforms where we need to implement it ourselves
// (aka WASM), we use this compatibility macro.
#ifndef __wasm__
#define sane_realloc(ptr, orig_size, size) realloc(ptr, size)
#else
static void* sane_realloc(void* ptr, size_t orig_size, size_t size);
#endif


// gnu_case_ranges are so much nicer but are non-standard.
// Leave off a colon and don't have a leading `case`.
#ifndef CASE_a_f
#define CASE_a_f 'a': case 'b': case 'c': case 'd': case 'e': case 'f'
#endif

#ifndef CASE_A_F
#define CASE_A_F 'A': case 'B': case 'C': case 'D': case 'E': case 'F'
#endif

#ifndef CASE_0_9
#define CASE_0_9 '0': case '1': case '2': case '3': case '4': case '5': \
    case '6': case '7': case '8': case '9'
#endif

#ifndef CASE_a_z
#define CASE_a_z 'a': case 'b': case 'c': case 'd': case 'e': case 'f': \
    case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': \
    case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't': \
    case 'u': case 'v': case 'w': case 'x': case 'y': case 'z'
#endif

#ifndef CASE_A_Z
#define CASE_A_Z 'A': case 'B': case 'C': case 'D': case 'E': case 'F': \
    case 'G': case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': \
    case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': \
    case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z'
#endif

// utf-16 versions
#ifndef CASE_u16_a_f
#define CASE_u16_a_f case u'a': case u'b': case u'c': case u'd': case u'e': \
    case u'f'
#endif

#ifndef CASE_u16_A_F
#define CASE_u16_A_F case u'A': case u'B': case u'C': case u'D': case u'E': \
    case u'F'
#endif

#ifndef CASE_u16_0_9
#define CASE_u16_0_9 '0': case u'1': case u'2': case u'3': case u'4': \
    case u'5': case u'6': case u'7': case u'8': case u'9'
#endif

#ifndef CASE_u16_a_z
#define CASE_u16_a_z 'a': case u'b': case u'c': case u'd': case u'e': \
    case u'f': case u'g': case u'h': case u'i': case u'j': case u'k': \
    case u'l': case u'm': case u'n': case u'o': case u'p': case u'q': \
    case u'r': case u's': case u't': case u'u': case u'v': case u'w': \
    case u'x': case u'y': case u'z'
#endif

#ifndef CASE_u16_A_Z
#define CASE_u16_A_Z 'A': case u'B': case u'C': case u'D': case u'E': \
    case u'F': case u'G': case u'H': case u'I': case u'J': case u'K': \
    case u'L': case u'M': case u'N': case u'O': case u'P': case u'Q': \
    case u'R': case u'S': case u'T': case u'U': case u'V': case u'W': \
    case u'X': case u'Y': case u'Z'
#endif

#endif
