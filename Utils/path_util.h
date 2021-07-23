#ifndef PATH_UTIL_H
#define PATH_UTIL_H
// size_t
#include <stddef.h>
// bool
#include <stdbool.h>
// memchr
#include <string.h>
// StringView and currently force_inline, which is kind of janky.
#include "long_string.h"

#ifdef _WIN32
#ifndef BACKSLASH_IS_A_PATH_SEP
#define BACKSLASH_IS_A_PATH_SEP
#endif
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
bool
is_sep(char c){
    #ifdef BACKSLASH_IS_A_PATH_SEP
    return c == '/' || c == '\\';
    #else
    return c == '/';
    #endif
    }

// Helper to find the next slash in a string, but also finding backslashes
// on Windows.
static inline
force_inline
void*_Nullable
memsep(const char* str, size_t length){
    char* slash = memchr(str, '/', length);
    #ifdef BACKSLASH_IS_A_PATH_SEP
    if(!slash)
        slash = memchr(str, '\\', length);
    #endif
    return slash;
    }

//
// Returns if the path is an absolute path (aka starts from /).
// BUG: This doesn't handle windows correctly.
//      In the future, it will handle things like drives.
//
static inline
bool
path_is_abspath(StringView path){
    if(!path.length)
        return false;
    // FIXME: this is wrong on windows as you can include a drive letter.
#ifndef _WIN32
    return is_sep(path.text[0]);
#else
    if(is_sep(path.text[0]))
        return true;
    if(path.length > 2){
        if(path.text[1] == ':')
            return true;
        }
    return false;
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
    auto result = (StringView){.text=path.text, .length = basename - path.text};
    while(result.length > 1 and is_sep(result.text[result.length-1])){
        result.length--;
        }
    return result;
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
        if(path.text[offset] == '.'){
            return (StringView){.text=path.text, .length = offset};
            }
        if(is_sep(path.text[offset]))
            return path;
        }
    return path;
    }

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
