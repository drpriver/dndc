//
// Copyright © 2021-2022, David Priver
//
#ifndef LOG_PRINT_H
#define LOG_PRINT_H
#include "long_string.h"

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline __attribute__((always_inline))
#else
#define force_inline
#endif
#endif

#ifndef printf_func

#if defined(__GNUC__) || defined(__clang__)
#define printf_func(fmt_idx, vararg_idx) __attribute__((__format__ (__printf__, fmt_idx, vararg_idx)))
#else
#define printf_func(...)
#endif

#endif

// Escape codes for colored text.
// I should really be checking if stdout/stderr is interactive
// but meh.
#if 0
#define gray_coloring ""
#define blue_coloring ""
#define reset_coloring ""
#define green_coloring ""
#define red_coloring ""
#else
#define gray_coloring "\033[97m"
#define blue_coloring "\033[94m"
#define reset_coloring "\033[39;49m"
#define green_coloring "\033[92m"
#define red_coloring "\033[91m"
#endif

// Turning macros into a string requires a level of indirection.
#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

printf_func(5, 6)
static
void
logfunc(int log_level, const char* file, const char* func, int line, const char* fmt, ...);
// The log levels.
#define LOG_LEVEL_HERE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

#ifndef LOG_LEVEL
#define LOG_LEVEL 5
#endif

#if LOG_LEVEL > 0
#define ERROR(fmt, ...) logfunc(LOG_LEVEL_ERROR, __FILE__, __func__, __LINE__, "" fmt, ##__VA_ARGS__)
#else
#define ERROR(...)
#endif

#if LOG_LEVEL > 1
#define WARN(fmt, ...) logfunc(LOG_LEVEL_WARN, __FILE__, __func__, __LINE__, "" fmt, ##__VA_ARGS__)
#else
#define WARN(...)
#endif

#if LOG_LEVEL > 2
#define INFO(fmt, ...) logfunc(LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, "" fmt, ##__VA_ARGS__)
#else
#define INFO(...)
#endif

#if LOG_LEVEL > 3
#define DBG(fmt, ...) logfunc(LOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, "" fmt, ##__VA_ARGS__)
#else
#define DBG(...)
#endif

#define HERE(fmt, ...) logfunc(LOG_LEVEL_HERE, __FILE__, __func__, __LINE__, "" fmt, ##__VA_ARGS__)

// Conveniently allows logging a variable.
// Usage:
//   SomeDir/SomeFile.c
//   ------------------
//   #include "log_print.h"
//   int main(){
//       int foo = 3;
//       DBGPrint(foo);
//       return 0;
//   }
//
// Will result in:
//   [DEBUG] SomeDir/SomeFile.c:main:4: foo = 3
//
#define DBGPrint(x) DBGPrintIMPL(LOG_LEVEL_DEBUG, x)
#define HEREPrint(x) DBGPrintIMPL(LOG_LEVEL_HERE, x)
#define INFOPrint(x) DBGPrintIMPL(LOG_LEVEL_INFO, x)

#define LOGFUNCS(apply) \
    apply(bool, _Bool, "%d", x) \
    apply(char, char, "'%c'", x) \
    apply(uchar, unsigned char, "%u", x) \
    apply(schar, signed char, "%d", x) \
    apply(float, float, "%f", (double)x) \
    apply(double, double, "%f", x) \
    apply(short, short, "%d", x) \
    apply(ushort, unsigned short, "%u", x) \
    apply(int, int, "%d", x) \
    apply(uint, unsigned int, "%d", x) \
    apply(long, long, "%ld", x) \
    apply(ulong, unsigned long, "%lu", x) \
    apply(llong, long long, "%lld", x) \
    apply(ullong, unsigned long long, "%llu", x) \
    apply(pchar, char*, "\"%s\"", x) \
    apply(cpchar, const char*, "\"%s\"", x) \
    apply(cpvoid, const void*, "%p", x) \
    apply(pvoid, void*, "%p", x) \
    apply(LongString, LongString, "\"%s\"", x.text) \
    apply(StringView, StringView, "\"%.*s\"", (int)x.length, x.text) \


#define LOGFUNC(name, type, fmt, ...) \
    static inline force_inline void log_##name(int log_level, const char* file, const char* func, int line, const char* expr, type x){ \
        logfunc(log_level, file, func, line, "%s = " fmt, expr, ##__VA_ARGS__); \
        }
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
LOGFUNCS(LOGFUNC);
#pragma clang diagnostic pop;
#else
LOGFUNCS(LOGFUNC);
#endif
#undef LOGFUNC

#define LOGFUNC(name, type, ...) type: log_##name,
#define DBGPrintIMPL(loglevel, x) \
_Generic(x, \
        LOGFUNCS(LOGFUNC) \
        struct{}: 0)(loglevel, __FILE__, __func__, __LINE__, #x, x)

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
