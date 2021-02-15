#ifndef DBG_PRINT_H
#define DBG_PRINT_H
#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
#define USERF(x) 0
#define DBGF(x) USERF(x)?:_Generic(x,\
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
#define DBGC(x) (typeof(_Generic(x,\
        float: (double){0}, \
        char*: (char*){0}, \
        const char* : (const char*){0}, \
        default: x)))(x)
#define push_dbgc() _Pragma("push_macro(\"DBGC\")");
#define pop_dbgc() _Pragma("pop_macro(\"DBGC\")");
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

printf_func(5, 6)
extern
void logfunc(int log_level, const char*_Nonnull file, const char*_Nonnull func, int line, const char*_Nonnull fmt, ...);
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
#define DBGE(fmt) "%s = " fmt
#define DBGPrintIMPL(loglevel, x, dbgc) logfunc(loglevel, __FILE__, __func__, __LINE__, DBGF(x), #x, dbgc(x))
#define DBGPrint(x) DBGPrintIMPL(LOG_LEVEL_DEBUG, x, DBGC)
#define HEREPrint(x) DBGPrintIMPL(LOG_LEVEL_HERE, x, DBGC)
#define INFOPrint(x) DBGPrintIMPL(LOG_LEVEL_INFO, x, DBGC)


#endif
