#ifndef DWINSTRING_H
#define DWINSTRING_H
// NOTE: This file might be include from c++. So it needs to be a dual-lang
// header.
#include "Utils/long_string.h"
#include "Allocators/allocator.h"

#ifndef _WIN32
#error "Only valid on win32"
#endif

#include "windowsheader.h"

typedef struct WinString WinString;
struct WinString {
    wchar_t* text;
    size_t nchars_with_zero; // includes terminating null character
};

static inline
WinString
utf8_to_wstring(Allocator a, const char* text){
    int n_needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    wchar_t* result = (wchar_t*)Allocator_alloc(a, n_needed*sizeof(*result));
    if(!result){
        return {NULL, 0};
    }
    int n_written = MultiByteToWideChar(CP_UTF8, 0, text, -1, result, n_needed);
    return (WinString){result, (size_t)n_written};
}

static inline
LongString
make_utf8_string_from_windows_string(Allocator a, wchar_t* text, size_t nchars_with_zero){
    size_t needed_size = WideCharToMultiByte(CP_UTF8, 0, text, nchars_with_zero, NULL, 0, NULL, NULL); // Call with 0 length outbuffer to get the output length.
    char* result = (char*)Allocator_alloc(a, needed_size);
    WideCharToMultiByte(CP_UTF8, 0, text, nchars_with_zero, result, needed_size, NULL, NULL);
    return (LongString){needed_size, result};
}


#endif
