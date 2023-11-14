//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
#ifndef PATH_UTIL_H
#define PATH_UTIL_H
// size_t
#include <stddef.h>
// memchr
#include <string.h>
// StringView and currently force_inline, which is kind of janky.
#include "long_string.h"

#ifndef BACKSLASH_IS_A_PATH_SEP
enum {
#ifdef _WIN32
    BACKSLASH_IS_A_PATH_SEP=1
#else
    BACKSLASH_IS_A_PATH_SEP=0
#endif
};
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

// Helper to distinguish what is a path separator.
static inline
force_inline
_Bool
is_sep(char c){
    if(BACKSLASH_IS_A_PATH_SEP)
        return c == '/' || c == '\\';
    else
        return c == '/';
}

// Helper to find the next slash in a string, but also finding backslashes
// on Windows.
static inline
force_inline
void*_Nullable
memsep(const char* str, size_t length){
    char* slash = memchr(str, '/', length);
    if(BACKSLASH_IS_A_PATH_SEP && !slash)
        slash = memchr(str, '\\', length);
    return slash;
}

//
// Returns if the path is an absolute path (aka starts from /).
//
static inline
_Bool
path_is_abspath(StringView path){
    if(!path.length)
        return 0;
#ifndef _WIN32
    return is_sep(path.text[0]);
#else
    if(is_sep(path.text[0]))
        return 1;
    if(path.length > 2){
        if(path.text[1] == ':')
            return 1;
    }
    return 0;
#endif
}

//
// Returns the filename component of a path. If the path ends
// with a slash, returns the empty string.
// This is different than the posix utility basename.
//
static inline
StringView
path_basename(StringView path){
    if(!path.length)
        return path;
    const char* basename = path.text;
    const char* end = path.text + path.length;
    // probably more efficient way of doing these.
    // Wish there was a reverse memchr
    for(;basename != end;){
        const char* slash = memsep(basename, end-basename);
        if(!slash)
            break;
        basename = slash+1;
    }
    return (StringView){.text=basename, .length = end - basename};
}

//
// Returns the directory portion of the filename.
// Trailing slashes are stripped from the result, unless the final
// path is exactly equal to '/'. This allows you to distinguish
// '/foo' from 'foo' without needing to return a '.' that's not in
// the original string (as doing so would break pointer arithmetic).
//
static inline
StringView
path_dirname(StringView path){
    if(!path.length)
        return path;
    const char* basename = path.text;
    const char* end = path.text + path.length;
    for(;basename != end;){
        const char* slash = memsep(basename, end-basename);
        if(!slash)
            break;
        basename = slash+1;
    }
    StringView result = {.text=path.text, .length = basename - path.text};
    while(result.length > 1 && is_sep(result.text[result.length-1])){
        result.length--;
    }
    return result;
}

static inline
StringView
path_extension(StringView path){
    if(!path.length) return path;
    size_t off = path.length;
    while(off--){
        if(path.text[off] == '.'){
            return (StringView){.text=path.text+off, .length=path.length-off};
        }
        if(is_sep(path.text[off]))
            return SV("");
    }
    return SV("");
}

//
// Removes the extension part of a string.
//
// Turns /foo/bar/baz.html into /foo/bar/baz
// Turns /foo/bar into /foo/bar
//
static inline
StringView
path_strip_extension(StringView path){
    if(!path.length)
        return path;
    size_t offset = path.length;
    while(offset--){
        if(path.text[offset] == '.')
            return (StringView){.text=path.text, .length = offset};
        if(is_sep(path.text[offset]))
            return path;
    }
    return path;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
