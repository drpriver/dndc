#ifndef PATH_UTIL_H
#define PATH_UTIL_H
#include <string.h>
#include "long_string.h"
#include "MStringBuilder.h"

static inline
force_inline
bool
is_sep(char c){
    #ifdef WINDOWS
    return c == '/' or c == '\\';
    #else
    return c == '/';
    #endif
    }

static inline
force_inline
Nullable(void*)
memsep(Nonnull(const char*)str, size_t length){
    char* slash = memchr(str, '/', length);
    #ifdef WINDOWS
    if(!slash)
        slash = memchr(str, '\\', length);
    #endif
    return slash;
    }


// probably more efficient way of doing these.
// Wish there was a reverse memchr
static inline
StringView
path_basename(StringView path){
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
    return (StringView){.text=basename, .length = end - basename};
    }

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

static inline
void
msb_append_path(Nonnull(MStringBuilder*)sb, const Allocator a, Nonnull(const char*) restrict path, size_t length){
    _check_msb_size(sb, a, length+1);
    sb->data[sb->cursor++] = '/';
    memcpy(sb->data + sb->cursor, path, length);
    sb->cursor += length;
    }

#ifdef WINDOWS
#include "windowsheader.h"
static inline
int
chdir(Nonnull(const char*) dirname){
    PushDiagnostic();
    SuppressDiscardQualifiers();
    return SetCurrentDirectory(dirname) != 0;
    PopDiagnostic();
    }
#endif

#endif
