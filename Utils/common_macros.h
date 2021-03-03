#ifndef COMMON_MACROS_H
#define COMMON_MACROS_H

// I've made a half-hearted attempt to get things to work with non-gcc,
// non-clang compilers. Oh well.

#ifndef warn_unused
#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__ ((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused _Check_return
#else
#error "No warn unused analogue"
#endif
#endif

#include <assert.h>
#include <stdbool.h>

#if defined(__GNUC__) || defined(__clang__)
#define force_inline __attribute__((always_inline))
#define never_inline __attribute__((noinline))
#else
#define force_inline
#define never_inline
#endif

#ifndef __cplusplus
#define auto __auto_type
#define and &&
#define or ||
#define not !
#endif

#ifndef __clang__
#define _Nonnull
#define _Nullable
#define _Null_unspecified
#endif

#define Nonnull(x) x _Nonnull
#define Nullable(x) x _Nullable
#define NullUnspec(x) x _Null_unspecified

/*
   Force a compilation error if condition is true, but also produce a result
   (of value 0 and type size_t), so the expression can be used e.g. in a
   structure initializer (or where-ever else comma expressions aren't
   permitted).
*/
#ifndef WINDOWS
// this is weird... on windows this evaluates to 4, while on linux/mac this is 0
#define BUILD_BUG_IF(e) (sizeof(struct { int:-!!(e); }))
#define __must_be_array(a) \
 BUILD_BUG_IF(__builtin_types_compatible_p(typeof(a), typeof(&a[0])))
#else
#define __must_be_array(a) 0
#endif

// Gets the length of an array, while avoiding the problem of pointers.
#define arrlen(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

#if defined(__GNUC__) || defined(__clang__)
    #ifndef WINDOWS
    #define SmallEnum enum __attribute__((__packed__))
        #if !defined(__clang__)
        #define FlagEnum enum
        #else
        #define FlagEnum enum __attribute__((flag_enum))
        #endif
    #else
    // leave SmallEnum undefined as this doesn't work on windows
    #define FlagEnum enum
    #endif
#else
// leave both undefined as things are going to be broken
#endif

#ifdef DEBUG
#define unreachable() ({assert(0);__builtin_unreachable();})
#else
#define unreachable() __builtin_unreachable()
#endif

// TODO: Log an error message?
#define unhandled_error_condition(cond) assert(!(cond))

#if defined(__clang__)
#define nosan \
    __attribute__((no_sanitize("address"))) \
    __attribute__((no_sanitize("nullability"))) \
    __attribute__((no_sanitize("undefined")))
#define nosan_null __attribute__((no_sanitize("nullability")))
#elif defined(__GNUC__)
#define nosan \
    __attribute__((no_sanitize("address"))) \
    __attribute__((no_sanitize("undefined")))
#define nosan_null
#else
#define nosan
#define nosan_null
#endif

#if defined(__GNUC__) || defined(__clang__)
#define unimplemented() ({assert(!"This code not implemented yet.");__builtin_unreachable();})
#else
#define unimplemented() assert(!"This code not implemented yet.")
#endif

// TODO: breakpoints on arm
#define breakpoint()  asm("int $3")

// These all use GNU statement expressions, so I'll have to address those later
// if I want to port to MSVC, but screw that crappy compiler.

#define Max(a, b) ({ 	auto max_temp_a__ = (a);\
			auto max_temp_b__ = (b);\
			max_temp_a__ > max_temp_b__ ? max_temp_a__: max_temp_b__;})

#define Max_literal(a, literal) ({ auto _a = a;\
            auto _b = (typeof(_a)) literal;\
            _a > _b ? _a : _b;})

#define Min(a, b) ({ 	auto min_temp_a__ = (a);\
			auto min_temp_b__ = (b);\
			min_temp_a__ < min_temp_b__ ? min_temp_a__: min_temp_b__;})

#define Min_literal(a, literal) ({ auto _a = a;\
            auto _b = (typeof(_a)) literal;\
            _a < _b ? _a : _b;})


// There's a bug in the c spec that free's prototype is
//
// void free(void*);
//
// instead of
//
// void free(const void*);
//
// This is stupid as hell as it means you can't free pointers that you have
// made const after allocation (for example, a string).
// So, suppress the diagnostics and do it anyway.
#define const_free(ptr) do{\
    PushDiagnostic(); \
    SuppressDiscardQualifiers(); \
    free(ptr);\
    PopDiagnostic(); \
    }while(0)

/*
 * Warning Suppression
 *
 * Pragmas to suppress warnings. Currently only supporting
 * clang and gcc, but using these macros means I can use the
 * compiler-specific pragmas.
 */
#ifdef __clang__
#define SuppressNullabilityComplete() _Pragma("clang diagnostic ignored \"-Wnullability-completeness\"")
#define SuppressUnusedFunction()  _Pragma("clang diagnostic ignored \"-Wunused-function\"")
#define SuppressCastQual()  _Pragma("clang diagnostic ignored \"-Wcast-qual\"")
#define SuppressDiscardQualifiers() _Pragma("clang diagnostic ignored \"-Wincompatible-pointer-types-discards-qualifiers\"")
#define SuppressMissingBraces()  _Pragma("clang diagnostic ignored \"-Wmissing-braces\"")
#define SuppressDoublePromotion() _Pragma("clang diagnostic ignored \"-Wdouble-promotion\"")
#define SuppressCastFunction()
#define SuppressCoveredSwitchDefault() _Pragma("clang diagnostic ignored \"-Wcovered-switch-default\"")
#define PushDiagnostic() _Pragma("clang diagnostic push")
#define PopDiagnostic() _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define SuppressNullabilityComplete()
#define SuppressUnusedFunction()
#define SuppressCastQual() _Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#define SuppressDiscardQualifiers() _Pragma("GCC diagnostic ignored \"-Wdiscarded-qualifiers\"")
#define SuppressDoublePromotion() _Pragma("GCC diagnostic ignored \"-Wdouble-promotion\"")
#define SuppressMissingBraces()  _Pragma("GCC diagnostic ignored \"-Wmissing-braces\"")
#define SuppressCastFunction() _Pragma("GCC diagnostic ignored \"-Wcast-function-type\"")
#define SuppressCoveredSwitchDefault()
#define PushDiagnostic() _Pragma("GCC diagnostic push")
#define PopDiagnostic() _Pragma("GCC diagnostic pop")
#else
#define SuppressNullabilityComplete()
#define SuppressUnusedFunction()
#define SuppressCastQual()
#define SuppressCastFunction()
#define SuppressMissingBraces()
#define SuppressDiscardQualifiers()
#define SuppressDoublePromotion()
#define SuppressCoveredSwitchDefault()
#define PushDiagnostic()
#define PopDiagnostic()
#endif

/*
 * Allocator attributes
 *
 * MALLOC_FUNC: this function is malloc-like and the compiler
 *   is free to assume the pointer returned does not alias any
 *   other pointer. Note that realloc does not meet that criteria
 *   as it can fail and leave the original pointer valid.
 * ALLOCATOR_SIZE(N): the argument at the specified index
 *   (1-based) is the size of the storage that the returned
 *   pointer points to.
 */
#if defined(__GNUC__) || defined(__clang__)
//
// With this defined and apple clang 1200.0.32.29, I was getting incorrect
// __builtin_obj_size values which was causing it to generated ud2
// instructions when _FORTIFY_SOURCE was defined.
//
// #define ALLOCATOR_SIZE(N) __attribute__((alloc_size(N)))
#define ALLOCATOR_SIZE(...)
#define MALLOC_FUNC __attribute__((malloc))
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define ALLOCATOR_SIZE(...)
#define MALLOC_FUNC
#define likely(x) (x)
#define unlikely(x) (x)
#endif


/*
 * printf-like attributes
 */

#if defined(__GNUC__) || defined(__clang__)
#define printf_func(fmt_idx, vararg_idx) __attribute__((__format__ (__printf__, fmt_idx, vararg_idx)))
#else
#define printf_func(...)
#endif

// Always want logging, so include that.
#include "log_print.h"

#endif
