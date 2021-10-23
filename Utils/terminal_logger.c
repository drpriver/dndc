#ifndef TERMINAL_LOGGER_C
#define TERMINAL_LOGGER_C
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include "log_print.h"
#include "term_util.h"

// This file is the implementation of the logfunc needed by log_print.h
//
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nonnull
#define _Nonnull
#endif
#endif

#ifndef printf_func
#if defined(__GNUC__) || defined(__clang__)
#define printf_func(fmt_idx, vararg_idx) __attribute__((__format__ (__printf__, fmt_idx, vararg_idx)))
#else
#define printf_func(...)
#endif
#endif

enum {N_LOG_STRINGS=5};
static const char*_Nonnull const log_strings[N_LOG_STRINGS] = {
    "[HERE ]",
    "[ERROR]",
    "[WARN ]",
    "[INFO ]",
    "[DEBUG]",
};

static
void
vlogfunc(int log_level, const char* file, const char* func, int line, const char* fmt, va_list args){
    if(log_level > LOG_LEVEL)
        return;
    const char* log_text;
    if(log_level > N_LOG_STRINGS || log_level < 0)
        log_text = "[ ??? ]";
    else
        log_text = log_strings[log_level];
    static int is_tty;
    if(!is_tty){
        if(isatty(STDERR_FILENO)){
            is_tty = 2;
            }
        else
            is_tty = 1;
        }
    bool print_colors = is_tty == 2;
    // TODO: figure out how to do this in a single fprintf call.
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
static
void
logfunc(int log_level, const char* file, const char* func, int line, const char* fmt, ...){
    if(log_level > LOG_LEVEL)
        return;
    va_list args;
    va_start(args, fmt);
    vlogfunc(log_level, file, func, line, fmt, args);
    va_end(args);
    }

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
