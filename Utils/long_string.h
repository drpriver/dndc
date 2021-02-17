#ifndef LONG_STRING_H
#define LONG_STRING_H
#include <stdlib.h>
#include <string.h>
#include "common_macros.h"
#include "error_handling.h"

// LongStrings and StringViews are very similar.
// A LongString is basically a StringView with a guranteed nul-terminator.
// It is unspecified if a StringView has a nul-terminator.
typedef struct LongString {
    size_t length; // excludes the terminating NUL
    NullUnspec(const char*) text;
} LongString;
Errorable_declare(LongString);

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
bool
LS_equals(const LongString a, const LongString b){
    if (a.length != b.length)
        return false;
    if(a.text == b.text)
        return true;
    assert(a.text);
    assert(b.text);
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
    if(a.text == b.text)
        return true;
    assert(a.text);
    assert(b.text);
    return memcmp(a.text, b.text, a.length) == 0;
    }

static inline
bool
LS_SV_equals(const LongString ls, const StringView sv){
    if(ls.length != sv.length)
        return false;
    if(ls.text == sv.text)
        return true;
    assert(ls.text);
    assert(sv.text);
    return memcmp(ls.text, sv.text, sv.length)==0;
    }

// Maybe it's UB (idk) but this works for LongStrings as well.
// Although maybe I should just use strcmp for those.
static inline
int
StringView_cmp(Nonnull(const void*)a, Nonnull(const void*) b){
    // TODO: There's probably a cleaner way to implement this.
    Nonnull(const StringView*)lhs = a;
    Nonnull(const StringView*)rhs = b;
    auto l1 = lhs->length;
    auto l2 = rhs->length;
    if(l1 == l2){
        if(!l1)
            return 0;
        if(lhs->text == rhs->text)
            return 0;
        return memcmp(lhs->text, rhs->text, l1);
        }
    if(!lhs->length)
        return -(int)(unsigned char)rhs->text[0];
    if(!rhs->length)
        return (int)(unsigned char)lhs->text[0];
    int prefix_cmp = memcmp(lhs->text, rhs->text, lhs->length > rhs->length?rhs->length:lhs->length);
    if(prefix_cmp)
        return prefix_cmp;
    if(lhs->length > rhs->length){
        return (int)(unsigned char)lhs->text[rhs->length];
        }
    return -(int)(unsigned char)rhs->text[lhs->length];
    }

#endif
