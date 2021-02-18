#ifndef STR_UTIL_H
#define STR_UTIL_H
#include <string.h>
#include "linear_allocator.h"
#include "common_macros.h"
#include "long_string.h"

#ifdef WINDOWS
static inline
Nonnull(char*)
strdup(Nonnull(const char*)str){
    size_t len = strlen(str)+1;
    char* result = malloc(len);
    unhandled_error_condition(!result);
    memcpy(result, str, len);
    return result;
    }
#endif

static inline
bool
str_startswith(Nonnull(const char*) haystack, size_t hay_len, Nonnull(const char*) needle, size_t needle_len){
    if(!needle_len)
        return true;
    if(hay_len < needle_len)
        return false;
    return memcmp(haystack, needle, needle_len) == 0;
    }

static inline
bool
str_endswith(Nonnull(const char*) haystack, size_t hay_len, Nonnull(const char*) needle, size_t needle_len){
    if(!needle_len)
        return true;
    if(hay_len < needle_len)
        return false;
    return memcmp(haystack+hay_len-needle_len, needle, needle_len) == 0;
    }

static inline
StringView
stripped_view(Nonnull(const char*)str, size_t len){
    for(;len;len--, str++){
        switch(*str){
            case ' ': case '\t': case '\r': case '\n': case '\f':
                continue;
            default:
                break;
            }
        break;
        }
    for(;len;len--){
        switch(str[len-1]){
            case ' ': case '\t': case '\r': case '\n': case '\f':
                continue;
            default:
                break;
            }
        break;
        }
    return (StringView){.text=str, .length=len};
    }

static inline
StringView
rstripped_view(Nonnull(const char*)str, size_t len){
    for(;len;len--){
        switch(str[len-1]){
            case ' ': case '\t': case '\r': case '\n': case '\f':
                continue;
            default:
                break;
            }
        break;
        }
    return (StringView){.text=str, .length=len};
    }

typedef struct SplitPair {
    StringView head;
    StringView tail;
} SplitPair;

// signals that we were unable to split by returning the head is the same
// length as the input length.
// NOTE: if we fail to split, we do not strip.
static inline
SplitPair
stripped_split(Nonnull(const char*)a, size_t length, char splitter){
    const char* split = memchr(a, splitter, length);
    if(!split){
        return (SplitPair){
            .head = {.text=a, .length=length},
            .tail = {},
            };
        }
    return (SplitPair){
        .head = stripped_view(a, split-a),
        .tail = stripped_view(split+1, (a+length) - (split+1)),
        };
    }

#endif
