#ifndef LOG_PRINT_H
#define LOG_PRINT_H

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

printf_func(5, 6)
extern
void logfunc(int log_level, const char*_Nonnull file, const char*_Nonnull func, int line, const char*_Nonnull fmt, ...);
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
// This is convenient for debugging and logging some stats, like high water
// marks on allocators.
#define DBGPrint(x) DBGPrintIMPL(LOG_LEVEL_DEBUG, x, DBGC)
#define HEREPrint(x) DBGPrintIMPL(LOG_LEVEL_HERE, x, DBGC)
#define INFOPrint(x) DBGPrintIMPL(LOG_LEVEL_INFO, x, DBGC)
#define DBGPrintIMPL(loglevel, x, dbgc) logfunc(loglevel, __FILE__, __func__, __LINE__, DBGF(x), #x, dbgc(x))

// Turns a type into the appropriate format string.
// DBGE ->  "Debug Expansion" I guess? not great name.
#define DBGE(fmt) "%s = " fmt
// DBGF -> "Debug Format"
#define DBGF(x) _Generic(x,\
        _Bool              : DBGE("%d"), \
        char               : DBGE("'%c'"), \
        unsigned char      : DBGE("%d"), \
        signed char        : DBGE("%d"), \
        float              : DBGE("%f"), \
        double             : DBGE("%f"), \
        short              : DBGE("%d"), \
        unsigned short     : DBGE("%u"), \
        int                : DBGE("%d"),\
        long               : DBGE("%ld"),\
        long long          : DBGE("%lld"),\
        unsigned           : DBGE("%u"),\
        unsigned long      : DBGE("%lu"),\
        unsigned long long : DBGE("%llu"),\
        char*              : DBGE("\"%s\""), \
        const char*        : DBGE("\"%s\""),\
        void*              : DBGE("%p"),\
        const void*        : DBGE("%p"),\
        default            : 0)
// FIXME: This below explanation doesn't seem to be true
//        when I test it out. Why do we do this?
// Some types need to be cast to other types for printing
// and for handling in the _Generic switch:
//      floats need to be promoted to double
//      char[] needs to turn into char*
//      Same for const char[].
// DBGC -> "Debug Cast"
#define DBGC(x) (typeof(_Generic(x,\
        float: (double){0}, \
        char*: (char*){0}, \
        const char* : (const char*){0}, \
        default: x)))(x)
// In theory you could redefine DBGF, DBGE, DBGC in order to customize
// the formatting and even print non-builtin datatypes.


#endif
