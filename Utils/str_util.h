#ifndef STR_UTIL_H
#define STR_UTIL_H
// size_t
#include <stddef.h>
// memchr
#include <string.h>
// StringView, etc.
#include "long_string.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

//
// Returns a view of the string, stripped of whitespace on both ends.
// Whitespace is ' ', '\t', '\f', '\r', '\n', '\v'.
// Intentionally ignores locale and does not handle utf-8 encoded unicode
// whitespace.
//
static inline
StringView
stripped_view(const char* str, size_t len){
    for(;len;len--, str++){
        switch(*str){
            case ' ': case '\t': case '\r': case '\n': case '\f': case '\v':
                continue;
            default:
                break;
            }
        break;
        }
    for(;len;len--){
        switch(str[len-1]){
            case ' ': case '\t': case '\r': case '\n': case '\f': case '\v':
                continue;
            default:
                break;
            }
        break;
        }
    return (StringView){.text=str, .length=len};
    }

static inline
StringViewUtf16
stripped_view_utf16(const uint16_t* str, size_t len){
    for(;len;len--, str++){
        switch(*str){
            case ' ': case '\t': case '\r': case '\n': case '\f': case '\v':
                continue;
            default:
                break;
            }
        break;
        }
    for(;len;len--){
        switch(str[len-1]){
            case ' ': case '\t': case '\r': case '\n': case '\f': case '\v':
                continue;
            default:
                break;
            }
        break;
        }
    return (StringViewUtf16){.text=str, .length=len};
    }
//
// Like stripped_view, but only strips from the right.
//
static inline
StringView
rstripped_view(const char* str, size_t len){
    for(;len;len--){
        switch(str[len-1]){
            case ' ': case '\t': case '\r': case '\n': case '\f': case '\v':
                continue;
            default:
                break;
            }
        break;
        }
    return (StringView){.text=str, .length=len};
    }
//
// Like stripped_view, but only strips from the left.
//
static inline
StringView
lstripped_view(const char* str, size_t len){
    for(;len;len--, str++){
        switch(*str){
            case ' ': case '\t': case '\r': case '\n': case '\f': case '\v':
                continue;
            default:
                break;
            }
        break;
        }
    return (StringView){.text=str, .length=len};
    }

static inline
StringViewUtf16
lstripped_view_utf16(const uint16_t* str, size_t len){
    for(;len;len--, str++){
        switch(*str){
            case ' ': case '\t': case '\r': case '\n': case '\f': case '\v':
                continue;
            default:
                break;
            }
        break;
        }
    return (StringViewUtf16){.text=str, .length=len};
    }
typedef struct SplitPair {
    StringView head;
    StringView tail;
} SplitPair;

//
// Given a string + length and a splitting character, returns two views
// which are the before and after of that character. Each view is stripped of
// whitespace (as per stripped_view). If there are multiples of the splitting
// character in the string, only the first one is used as the split point.
// Call this multiple times, each time using the returned tail if you need
// to split more than once.
//
// If the splitting character is not in the string, the head will be the original
// string and the tail will be empty. The head will *not* be stripped. The
// head being the same length as the original string is how splitting failure
// is signaled.
//
// NOTE: if we fail to split, we do not strip.
//
static inline
SplitPair
stripped_split(const char* a, size_t length, char splitter){
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

static inline
SplitPair
string_split(const char* a, size_t length, char splitter){
    const char* split = memchr(a, splitter, length);
    if(!split){
        return (SplitPair){
            .head.text = a,
            .head.length = length,
            };
        }
    return (SplitPair){
        .head.text = a,
        .head.length = split-a,
        .tail.text = split+1,
        .tail.length = (a+length)-(split+1),
        };
    }

static inline
_Bool
endswith(StringView haystack, StringView needle){
    if(haystack.length < needle.length) return 0;
    if(!needle.length) return 1;
    return memcmp(haystack.text+haystack.length-needle.length, needle.text, needle.length) == 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
