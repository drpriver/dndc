//
// Copyright © 2021-2022, David Priver
//
#ifndef MSTRING_BUILDER_H
#define MSTRING_BUILDER_H
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "long_string.h"
#include "Allocators/allocator.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline __attribute__((always_inline))
#else
#define force_inline
#endif
#endif

//
// A type for building up a string without having to deal with things like sprintf
// or strcat.
typedef struct MStringBuilder MStringBuilder;
struct MStringBuilder {
    size_t cursor;
    size_t capacity;
    char*_Null_unspecified data;
    const Allocator allocator;
};

//
// Dealloc the data and zeros out the builder.
// Unneeded if you called `msb_detach_ls` or `msb_detach_sv`.
static inline
void
msb_destroy(MStringBuilder* msb){
    Allocator_free(msb->allocator, msb->data, msb->capacity);
    msb->data=0;
    msb->cursor=0;
    msb->capacity=0;
}

static inline
force_inline
void
_check_msb_remaining_size(MStringBuilder*, size_t);

static inline
void
_msb_resize(MStringBuilder*, size_t);


//
// Nul-terminates the builder without actually increasing the length
// of the string.
static inline
void
msb_nul_terminate(MStringBuilder* msb){
    _check_msb_remaining_size(msb, 1);
    msb->data[msb->cursor] = '\0';
}


//
// Ensures additional capacity is present in the builder.
// Avoids re-allocs and thus potential copies
static inline
void
msb_ensure_additional(MStringBuilder* msb, size_t additional_capacity){
    _check_msb_remaining_size(msb, additional_capacity);
}

//
// Moves the ownership of the sring from the builder to the caller.
// Ensures nul-termination.
// Builder can be reused afterwards; its fields are zeroed.
static inline
LongString
msb_detach_ls(MStringBuilder* msb){
    assert(msb->data);
    msb_nul_terminate(msb);
    _msb_resize(msb, msb->cursor+1);
    LongString result = {0};
    result.text = msb->data;
    result.length = msb->cursor;
    msb->data = NULL;
    msb->capacity = 0;
    msb->cursor = 0;
    return result;
}

static inline
StringView
msb_detach_sv(MStringBuilder* msb){
    StringView result = {0};
    _msb_resize(msb, msb->cursor);
    result.text = msb->data;
    result.length = msb->cursor;
    msb->data = NULL;
    msb->capacity = 0;
    msb->cursor = 0;
    return result;
}

//
// "Borrows" the current contents of the builder and returns a nul-terminated
// string view to those contents.  Keep uses of the borrowed string tightly
// scoped as any further use of the builder can cause a reallocation.  It's
// also confusing to have the contents of the string view change under you.
static inline
StringView
msb_borrow_sv(MStringBuilder* msb){
    return (StringView) {
        .text = msb->data,
        .length = msb->cursor,
    };
}

// "Borrows" a nul-terminated string
static inline
LongString
msb_borrow_ls(MStringBuilder* msb){
    msb_nul_terminate(msb);
    assert(msb->data);
    return (LongString) {
        .text = msb->data,
        .length = msb->cursor,
    };
}

//
// "Resets" the builder. Logically clears the contents of the builder
// (although it avoids actually touching the data) and sets the length to 0.
// Does not dealloc the data, so you can build up a string, borrow it,
// reset and do that again. This is particularly useful for creating strings
// that are then consumed by normal c-apis that take a c str as they almost
// always will copy the string themselves.
static inline
void
msb_reset(MStringBuilder* msb){
    msb->cursor = 0;
}

// Internal function, resizes the builder to the new size.
static inline
void
_msb_resize(MStringBuilder* msb, size_t size){
    char* new_data = Allocator_realloc(msb->allocator, msb->data, msb->capacity, size);
    unhandled_error_condition(!new_data);
    msb->data = new_data;
    msb->capacity = size;
}

// Internal function, ensures there is enough additional capacity.
static inline
force_inline
void
_check_msb_remaining_size(MStringBuilder* msb, size_t len){
    if(msb->cursor + len > msb->capacity){
        size_t new_size = msb->capacity?(msb->capacity*3)/2:16;
        while(new_size < msb->cursor+len){
            new_size *= 2;
        }
        if(new_size & 15){
            new_size += (16- (new_size&15));
        }
        _msb_resize(msb, new_size);
    }
}

//
// Writes a string into the builder. You must know the length.
// If you have a c-str, strlen it yourself.
static inline
void
msb_write_str(MStringBuilder* restrict msb, const char*_Null_unspecified restrict str, size_t len){
    if(!len)
        return;
    _check_msb_remaining_size(msb, len);
    (memcpy)(msb->data + msb->cursor, str, len);
    msb->cursor += len;
}

//
// Write a single char into the builder.
// This is actually kind of slow, relatively speaking, as it checks
// the size every time.
// It often will be better to write an extension method that reserves enough
// space and then writes to the data buffer directly.
static inline
force_inline
void
msb_write_char(MStringBuilder* msb, char c){
    _check_msb_remaining_size(msb, 1);
    msb->data[msb->cursor++] = c;
}

//
// Write a repeated pattern of characters into the builder.
// Pay attention to the order of arguments, as C will allow implicit
// conversions!
static inline
force_inline
void
msb_write_nchar(MStringBuilder* msb, char c, size_t n){
    if(n == 0)
        return;
    _check_msb_remaining_size(msb, n);
    memset(msb->data + msb->cursor, c, n);
    msb->cursor += n;
}

//
// Erases the given number of characters from the end of the builder.
static inline
void
msb_erase(MStringBuilder* msb, size_t len){
    if(!msb->cursor) return;
    if(len > msb->cursor){
        msb->cursor = 0;
        msb->data[0] = '\0';
        return;
    }
    msb->cursor -= len;
    msb->data[msb->cursor] = '\0';
}

static inline
char
msb_peek(MStringBuilder* msb){
    if(!msb->cursor) return 0;
    return msb->data[msb->cursor-1];
}

// Writes a string literal into the builder. Avoids the need to strlen
// as the literal's size is known at compile time.
// The "" forces it to be a string literal.
#define msb_write_literal(msb, lit) msb_write_str(msb, ""lit, sizeof(""lit)-1)

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
