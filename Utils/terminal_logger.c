#include <stdio.h>
#include <stdarg.h>
#include "common_macros.h"
#include "term_util.h"
static const char*_Nonnull const log_strings[] = {
    "[HERE ]",
    "[ERROR]",
    "[WARN ]",
    "[INFO ]",
    "[DEBUG]",
    };

extern
void vlogfunc(int log_level, const char*_Nonnull file, const char*_Nonnull func, int line, const char*_Nonnull fmt, va_list args){
    if(log_level > LOG_LEVEL)
        return;
    const char* log_text;
    if(log_level > arrlen(log_strings))
        log_text = "[ ??? ]";
    else
        log_text = log_strings[log_level];
    // TODO: figure out how to do this in a single fprintf call.
    static int is_tty;
    if(!is_tty){
        if(isatty(STDERR_FILENO)){
            is_tty = 2;
            }
        else
            is_tty = 1;
        }
    bool print_colors = is_tty == 2;
    fprintf(stderr, "%s %s:%s:%d: ", log_text, file, func, line);
    if(print_colors){
        fprintf(stderr, blue_coloring);
        }
    vfprintf(stderr, fmt, args);
    if(print_colors){
        fprintf(stderr, reset_coloring "\n");
        }
    else
        fputc('\n', stderr);
    }

printf_func(5, 6)
extern
void logfunc(int log_level, const char*_Nonnull file, const char*_Nonnull func, int line, const char*_Nonnull fmt, ...){
    if(log_level > LOG_LEVEL)
        return;
    va_list args;
    va_start(args, fmt);
    vlogfunc(log_level, file, func, line, fmt, args);
    va_end(args);
    }
