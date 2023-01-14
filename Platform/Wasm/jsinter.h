//
// Copyright © 2021-2023, David Priver
//
#ifndef JSINTER_H
#define JSINTER_H
#include <stdlib.h>
#include <string.h>
#include "Utils/long_string.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
// Javascript will malloc on our heap and fill out this structure.
typedef struct PString PString;
struct PString {
    // length does not include nul
    size_t length;
    // nul terminated
    unsigned char text[];
};

static inline
StringView
PString_to_sv(PString* pstr){
    StringView text = {.text=(char*)pstr->text, .length=pstr->length};
    return text;
}

static inline
PString*
LongString_to_new_PString(LongString source){
    PString* result = malloc(sizeof(*result)+source.length+1);
    result->length = source.length;
    memcpy(result->text, source.text, source.length);
    result->text[result->length] = 0;
    return result;
}

extern
void
log_string(const void*, size_t);
extern void logi32(int32_t);
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
