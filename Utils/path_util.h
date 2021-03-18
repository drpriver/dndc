#ifndef PATH_UTIL_H
#define PATH_UTIL_H
#include <string.h>
#include "long_string.h"
#include "MStringBuilder.h"

#ifdef WINDOWS
#ifndef BACKSLASH_IS_A_PATH_SEP
#define BACKSLASH_IS_A_PATH_SEP
#endif
#endif

// Helper to distinguish what is a path separator.
static inline
force_inline
bool
is_sep(char c){
    #ifdef BACKSLASH_IS_A_PATH_SEP
    return c == '/' or c == '\\';
    #else
    return c == '/';
    #endif
    }

// Helper to find the next slash in a string, but also finding backslashes
// on Windows.
static inline
force_inline
Nullable(void*)
memsep(Nonnull(const char*)str, size_t length){
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
    return is_sep(path.text[0]);
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

//
// Appends a path separator to the builder and then writes the given string.
// If the builder is empty, a path separator is not appended. This prevents
// accidentally turning a relative path into the wrong absolute path.
//
static inline
void
msb_append_path(Nonnull(MStringBuilder*)sb, Nonnull(const char*) restrict path, size_t length){
    _check_msb_size(sb, length+1);
    if(sb->cursor)
        sb->data[sb->cursor++] = '/';
    memcpy(sb->data + sb->cursor, path, length);
    sb->cursor += length;
    }

#ifdef WINDOWS
#include <direct.h>
static inline
int
chdir(Nonnull(const char*) dirname){
    return _chdir(dirname);
    }
#endif

#endif
