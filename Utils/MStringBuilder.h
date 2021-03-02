#ifndef MSTRING_BUILDER_H
#define MSTRING_BUILDER_H
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include "common_macros.h"
#include "long_string.h"
#include "allocator.h"

typedef struct MStringBuilder {
    size_t cursor;
    size_t capacity;
    NullUnspec(char*) data;
    const Allocator allocator;
} MStringBuilder;

// Dealloc the data and zeros out the builder.
// Unneeded if you called msb_detach.
static inline
void
msb_destroy(Nonnull(MStringBuilder*) msb){
    Allocator_free(msb->allocator, msb->data, msb->capacity);
    msb->data=0;
    msb->cursor=0;
    msb->capacity=0;
    }

static inline
force_inline
void
_check_msb_size(Nonnull(MStringBuilder*), size_t);

// Nul-terminates the builder without actually increasing the length
// of the string.
static inline
void
msb_nul_terminate(Nonnull(MStringBuilder*) msb){
    _check_msb_size(msb, 1);
    msb->data[msb->cursor] = '\0';
    }


// Ensures additional capacity is present in the builder.
// Avoids re-allocs and thus potential copies
static inline
void
msb_reserve(Nonnull(MStringBuilder*)msb, size_t additional_capacity){
    _check_msb_size(msb, additional_capacity);
    }

// Moves the ownership of the sring from the builder to the caller.
// Ensures nul-termination.
// Builder can be reused afterwards; its fields are zeroed.
static inline
LongString
msb_detach(Nonnull(MStringBuilder*) msb){
    assert(msb->data);
    msb_nul_terminate(msb);
    LongString result = {};
    result.text = msb->data;
    result.length = msb->cursor;
    msb->data = NULL;
    msb->capacity = 0;
    msb->cursor = 0;
    return result;
    }

// "Borrows" the current contents of the builder and returns a nul-terminated
// string view to those contents.  Keep uses of the borrowed string tightly
// scoped as any further use of the builder can cause a reallocation.  It's
// also confusing to have the contents of the string view change under you.
static inline
StringView
msb_borrow(Nonnull(MStringBuilder*) msb){
    msb_nul_terminate(msb);
    assert(msb->data);
    return (StringView) {
        .text = msb->data,
        .length = msb->cursor,
        };
    }


// "Resets" the builder. Logically clears the contents of the builder
// (although it avoids actually touching the data) and sets the length to 0.
// Does not dealloc the data, so you can build up a string, borrow it,
// reset and do that again. This is particularly useful for creating strings
// that are then consumed by normal c-apis that take a c str as they almost
// always will copy the string themselves.
static inline
void
msb_reset(Nonnull(MStringBuilder*) msb){
    msb->cursor = 0;
    }

// Internal function, resizes the builder to the new size.
static inline
void
_resize_msb(Nonnull(MStringBuilder*) msb, size_t size){
    char* new_data = Allocator_realloc(msb->allocator, msb->data, msb->capacity, size);
    unhandled_error_condition(!new_data);
    msb->data = new_data;
    msb->capacity = size;
    }

// Internal function, ensures there is enough additional capacity.
static inline
force_inline
void
_check_msb_size(Nonnull(MStringBuilder*) msb, size_t len){
    if (msb->cursor + len > msb->capacity){
        size_t new_size = Max_literal((msb->capacity*3)/2, 32);
        while(new_size < msb->cursor+len){
            new_size *= 2;
            }
        _resize_msb(msb, new_size);
        }
    }

// Writes a string into the builder. You must know the length.
// If you have a c-str, strlen it yourself.
static inline
void
msb_write_str(Nonnull(MStringBuilder*) restrict msb, NullUnspec(const char*) restrict str, size_t len){
    if(not len)
        return;
    _check_msb_size(msb, len);
    memcpy(msb->data + msb->cursor, str, len);
    msb->cursor += len;
    }

// Write a single char into the builder.
// This is actually kind of slow, relatively speaking, as it checks
// the size every time.
// It often will be better to write an extension method that reserves enough
// space and then writes to the data buffer directly.
static inline
force_inline
void
msb_write_char(Nonnull(MStringBuilder*) msb, char c){
    _check_msb_size(msb, 1);
    msb->data[msb->cursor++] = c;
    }

static inline
void
msb_erase(Nonnull(MStringBuilder*) msb, size_t len){
    if(len > msb->cursor){
        msb->cursor = 0;
        msb->data[0] = '\0';
        return;
        }
    msb->cursor -= len;
    msb->data[msb->cursor] = '\0';
    }

// Sprintf into the builder. The builder figures out how long the resulting
// string will be and ensures that much additional space.
printf_func(2, 3)
static inline
int
msb_sprintf(Nonnull(MStringBuilder*)msb, Nonnull(const char*) restrict fmt, ...){
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);
    auto _msg_size = vsnprintf(NULL, 0, fmt, args)+1;
    va_end(args);
    _check_msb_size(msb, _msg_size);
    auto result = vsprintf(msb->data + msb->cursor, fmt, args2);
    msb->cursor += result;
    va_end(args2);
    return result;
    }

// Like msb_sprintf, but for a va_list.
static inline
int
msb_vsprintf(Nonnull(MStringBuilder*)msb, Nonnull(const char*)restrict fmt, va_list args){
    va_list args2;
    va_copy(args2, args);
    auto _msg_size = vsnprintf(NULL, 0, fmt, args)+1;
    va_end(args);
    _check_msb_size(msb, _msg_size);
    auto result = vsprintf(msb->data + msb->cursor, fmt, args2);
    msb->cursor += result;
    va_end(args2);
    return result;
    }

// Writes a string literal into the builder. Avoids the need to strlen
// as the literals size is known at compile time.
#define msb_write_literal(msb, lit) msb_write_str(msb, ""lit, sizeof(""lit)-1)


#endif
