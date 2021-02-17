#ifndef LONG_STRING_H
#define LONG_STRING_H
#include <stdlib.h>
#include <string.h>
#include "common_macros.h"
#include "error_handling.h"

typedef struct LongString {
    size_t length; // excludes the terminating NUL
    NullUnspec(char*) text;
    } LongString;
Errorable_declare(LongString);

// this happens to be compatible with D's char[]
typedef struct StringView {
    size_t length;
    Nonnull(const char*) text;
    } StringView;
Errorable_declare(StringView);

static inline
force_inline
StringView
LS_to_SV(LongString ls){
    return (StringView){.text=ls.text, .length=ls.length};
    }

static inline
StringView
cstr_to_SV(Nonnull(const char*)cstr){
    auto len = strlen(cstr);
    return (StringView){
        .length = len,
        .text = cstr,
        };
    }

static inline
force_inline
StringView
ls_to_string_view(LongString ls){
    return (StringView){ls.length, ls.text};
    }

static inline
LongString
LongString_duped_from_cstring(Nonnull(const char*) text){
    return (LongString){
        .length = strlen(text),
        .text = strdup(text),
        };
    }

static inline
void
destroy_LongString(Nonnull(LongString*) str){
    free(str->text);
    str->text = NULL;
    str->length = 0;
    }

static inline
LongString
LongString_duped_from_string(Nonnull(const char*) text, size_t length){
    char* str = malloc(length+1);
    assert(str);
    memcpy(str, text, length);
    str[length] = '\0';
    return (LongString){
        .length = length,
        .text = str,
        };
    }

static inline
LongString
LongString_borrowed_from_cstring(Nonnull(char*) text){
    return (LongString){
        .length = strlen(text),
        .text = text,
        };
    }

static inline
LongString
LongString_clone(LongString first){
    if(not first.text)
        return (LongString){};
    char* new_text = malloc(first.length+1);
    memcpy(new_text, first.text, first.length+1);
    return (LongString){
        .text = new_text,
        .length = first.length,
        };
    }

static inline
bool
LongString_equals(const LongString a, const LongString b){
    if (a.length != b.length)
        return false;
    if(a.text == b.text)
        return true;
    assert(a.text);
    assert(b.text);
    // if(not a.text)
        // return false;
    // if(not b.text)
        // return false;
    return !strcmp(a.text, b.text);
    }
#ifdef LS
#error "LS defined"
#endif

#define LS(literal) ((LongString){.text=""literal, .length=sizeof(""literal)-1})
#define SV(literal) ((StringView){.text=""literal, .length=sizeof(""literal)-1})

static inline
bool
SV_equals(const StringView a, const StringView b){
    if(a.length != b.length)
        return false;
    return memcmp(a.text, b.text, a.length) == 0;
    }

static inline
bool
LS_SV_equals(const LongString ls, const StringView sv){
    if(ls.length != sv.length)
        return false;
    return memcmp(ls.text, sv.text, sv.length)==0;
    }
#endif
