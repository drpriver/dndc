//
// Copyright © 2021-2023, David Priver
//
#ifndef WINCLI_H
#define WINCLI_H
#include <stdlib.h>
#include <stdio.h>
#include "windowsheader.h"
#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")
// I am honestly confused if we need to do this
// or not as it seems like the regular main's argv is being passed as utf8 or whatever??
// But maybe I've set my windows box to use utf8?
static inline
int
get_main_args(int* argc, char***argv){
    wchar_t* cl = GetCommandLineW();
    wchar_t** wargv = CommandLineToArgvW(cl, argc);
    size_t needed = 0;
    // NOTE: the length returned by WideCharToMultiByte includes the terminating nul and is in code points.
    for(int i = 0; i < *argc; i++){
        int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wargv[i], -1, NULL, 0, NULL, NULL);
        if(!length){
            fprintf(stderr, "Invalid utf16 in command line\n");
            LocalFree(argv);
            return 1;
        }
        needed += length;
    }
    size_t array_size = (*argc+1)*sizeof(*argv);
    void* p = malloc(array_size+needed);
    char** result = p;
    char* c = array_size + (char*)p;
    for(int i = 0; i < *argc; i++){
        result[i] = c;
        int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wargv[i], -1, c, needed, NULL, NULL);
        needed -= length;
        c += length;
    }
    result[*argc] = NULL; // argv is null-terminated
    LocalFree(wargv);
    *argv = result;
    return 0;
}
#endif
