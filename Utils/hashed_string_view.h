#ifndef HASHED_STRING_VIEW_H
#define HASHED_STRING_VIEW_H

#include <stdint.h>
#include <string.h>
#include "Utils/hash_func.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline __forceinline
#else
#define force_inline
#endif
#endif

typedef struct HashedStringView HashedStringView;

struct HashedStringView {
    union {
        uint64_t length_hash;
        struct {
            uint32_t length;
            uint32_t hash;
        };
    };
    const char*_Nullable text;
};


static inline
force_inline
HashedStringView
HshSV(const char* text, size_t length){
    HashedStringView result = {
        .length = (uint32_t)length,
        .hash = hash_align1(text, length),
        .text = text,
    };
    return result;
}

static inline
force_inline
int
HshSV_eq(HashedStringView a, HashedStringView b){
    if(a.length_hash != b.length_hash) return 0;
    return memcmp(a.text, b.text, a.length) == 0;
}

#define HSHSV(lit) HshSV("" lit, sizeof(lit)-1)

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
