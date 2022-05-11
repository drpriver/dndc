#ifndef WINCLI_H
#define WINCLI_H
#include <stdlib.h>
#include "windowsheader.h"
static inline
int
get_main_args(int* argc, char***argv){
    wchar_t* cl = GetCommandLineW();
    wchar_t** wargv = CommandLineToArgvW(cl, argc);
    size_t needed = 0;
    for(int i = 0; i < *argc; i++){
        int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wargv[i], -1, NULL, 0, NULL, NULL);
        if(!length){
            fprintf(stderr, "Invalid utf16 in command line\n");
            LocalFree(argv);
            return 1;
        }
        needed += length;
    }
    size_t array_size = (argc+1)*sizeof(*argv);
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
    LocalFree(argv);
    *argv = p;
    return 0;
}
#endif
