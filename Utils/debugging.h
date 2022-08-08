#ifndef DEBUGGING_H
#define DEBUGGING_H

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nonnull
#define _Nonnull
#endif
#endif

#ifndef dbg_noinline
#if defined(__GNUC__) || defined(__clang__)
#define dbg_noinline __attribute__((__noinline__))
#else
#define dbg_noinline
#endif
#endif

typedef struct BacktraceArray BacktraceArray;
struct BacktraceArray {
    int count;
    void*_Nonnull symbols[];
};

static inline
dbg_noinline
BacktraceArray*
get_bt(void);

static inline
dbg_noinline
void
dump_bt(BacktraceArray* a);

static inline
dbg_noinline
void
bt(void);

#if defined(__APPLE__) || defined(__linux__)
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include <execinfo.h>
#include <stdlib.h>

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static inline
dbg_noinline
BacktraceArray*
get_bt(void){
    enum {bufflen=256};
    void* array[bufflen];
    int n = backtrace(array, bufflen);
    BacktraceArray* result = malloc(sizeof(*result)+n*sizeof(void*));
    result->count = n;
    __builtin_memcpy(result->symbols, array, n*sizeof(void*));
    return result;
}

static inline
dbg_noinline
void
dump_bt(BacktraceArray*_Nonnull a){
    backtrace_symbols_fd(a->symbols, a->count, 2);
}

static inline
dbg_noinline
void
bt(void){
    enum {bufflen=256};
    void* array[bufflen];
    int n = backtrace(array, bufflen);
    backtrace_symbols_fd(array, n, 2);
}
#elif defined(_WIN32)

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "Platform/Windows/windowsheader.h"
#include <dbghelp.h>
#include <stdlib.h>
#include <stdio.h>
#pragma comment(lib, "dbghelp.lib")

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static inline
dbg_noinline
void
bt(void){
    // Apparently this stuff is only safe to call from a single thread at a time.
    // But putting a critical section in here seemed like it'd be annoying.
    // Idk.
    enum {bufflen=256};
    void* array[bufflen];
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);

    unsigned frames = CaptureStackBackTrace(0, bufflen, array, NULL);
    struct {
        SYMBOL_INFO symbol;
        char buff[256];
    } sym = {0};
    sym.symbol.MaxNameLen   = 255;
    sym.symbol.SizeOfStruct = sizeof(SYMBOL_INFO);

    IMAGEHLP_LINE64 line = {
        .SizeOfStruct = sizeof(IMAGEHLP_LINE64),
    };
    DWORD disp;

    for(unsigned i = 0; i < frames; i++ ){
        BOOL addrsuccess = SymFromAddr(process, (DWORD64)array[i], 0, &sym.symbol);
        if(!addrsuccess){
            fprintf(stderr, "%2d  %-24s  0x%p\n",
                frames-i-1, "???", array[i]);
            continue;
        }
        BOOL linesuccess = SymGetLineFromAddr64(process, (DWORD64)array[i], &disp, &line);
        if(linesuccess){
            fprintf(stderr, "%2d  %-24s  from %s:%-4lu\n",
                frames-i-1, sym.symbol.Name,
                line.FileName, line.LineNumber);
        }
        else {
            fprintf(stderr, "%2d  %-24s  0x%p\n",
                frames-i-1, sym.symbol.Name,
                (void*)sym.symbol.Address);
        }
    }
}
static inline
dbg_noinline
BacktraceArray*
get_bt(void){
    static BacktraceArray bta;
    return &bta;
}

static inline
dbg_noinline
void
dump_bt(BacktraceArray* bta){
    // TODO
    (void)bta;
}


#else
static inline
dbg_noinline
void
bt(void){
    // do nothing
}
static inline
dbg_noinline
BacktraceArray*
get_bt(void){
    static BacktraceArray bta;
    return &bta;
}

static inline
dbg_noinline
void
dump_bt(BacktraceArray* bta){
    (void)bta;
}

#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
