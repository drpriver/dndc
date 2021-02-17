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
} MStringBuilder;

static inline
void
msb_destroy(Nonnull(MStringBuilder*) msb, const Allocator a){
    Allocator_free(a, msb->data, msb->capacity);
    msb->data=0;
    msb->cursor=0;
    msb->capacity=0;
    }

static inline
force_inline
void
_check_msb_size(Nonnull(MStringBuilder*), const Allocator, size_t);

static inline
void
msb_nul_terminate(Nonnull(MStringBuilder*) msb, const Allocator a){
    _check_msb_size(msb, a, 1);
    msb->data[msb->cursor] = '\0';
    }

static inline
void
msb_reserve(Nonnull(MStringBuilder*)msb, const Allocator a, size_t additional_capacity){
    _check_msb_size(msb, a, additional_capacity);
    }


static inline
LongString
msb_detach(Nonnull(MStringBuilder*) msb, const Allocator a){
    assert(msb->data);
    msb_nul_terminate(msb, a);
    LongString result = {};
    result.text = msb->data;
    result.length = msb->cursor;
    msb->data = NULL;
    msb->capacity = 0;
    msb->cursor = 0;
    return result;
    }

static inline
StringView
msb_borrow(Nonnull(MStringBuilder*) msb, const Allocator a){
    msb_nul_terminate(msb, a);
    assert(msb->data);
    return (StringView) {
        .text = msb->data,
        .length = msb->cursor,
        };
    }

static inline
size_t
msb_len(Nonnull(MStringBuilder*)msb){
    return msb->cursor;
    }

static inline
void
msb_reset(Nonnull(MStringBuilder*) msb){
    msb->cursor = 0;
    }

static inline
MStringBuilder
create_msb(size_t size, const Allocator a){
    MStringBuilder msb = {
        .data = Allocator_alloc(a, size),
        .capacity=size,
        .cursor = 0,
        };
    unhandled_error_condition(!msb.data);
    return msb;
    }

static inline
void
_resize_msb(Nonnull(MStringBuilder*) msb, const Allocator a, size_t size){
    char* new_data = Allocator_realloc(a, msb->data, msb->capacity, size);
    unhandled_error_condition(!new_data);
    msb->data = new_data;
    msb->capacity = size;
    }

static inline
force_inline
void
_check_msb_size(Nonnull(MStringBuilder*) msb, const Allocator a, size_t len){
    if (msb->cursor + len > msb->capacity){
        size_t new_size = Max_literal((msb->capacity*3)/2, 32);
        while(new_size < msb->cursor+len){
            new_size *= 2;
            }
        _resize_msb(msb, a, new_size);
        }
    }

static inline
void
msb_write_str(Nonnull(MStringBuilder*) restrict msb, const Allocator a, NullUnspec(const char*) restrict str, size_t len){
    if(not len)
        return;
    _check_msb_size(msb, a, len);
    memcpy(msb->data + msb->cursor, str, len);
    msb->cursor += len;
    }

static inline
void
msb_write_cstr(Nonnull(MStringBuilder*) restrict msb, const Allocator a, Nonnull(const char*) restrict str){
    auto len = strlen(str);
    msb_write_str(msb, a, str, len);
    }

static inline
force_inline
void
msb_write_char(Nonnull(MStringBuilder*) msb, const Allocator a, char c){
    _check_msb_size(msb, a, 1);
    msb->data[msb->cursor++] = c;
    }

static inline
void
msb_insert_char(Nonnull(MStringBuilder*) msb, const Allocator a, int index, char c){
    _check_msb_size(msb, a, 1);
    int move_length = msb->cursor - index;
    if(move_length)
        memmove(msb->data+index+1, msb->data+index, move_length);
    msb->data[index] = c;
    msb->cursor++;
    }

static inline
void
msb_erase_char_at(Nonnull(MStringBuilder*) msb, int index){
    assert(index >= 0);
    assert(index < msb->cursor);
    int move_length = msb->cursor - index - 1;
    if(move_length){
        memmove(msb->data+index, msb->data+index+1, move_length);
        }
    msb->cursor--;
    }

// Write a multibyte character literal like 'foo'.
// 'foo' is backwards though, so fixes up the byte order as well.
static inline
void
msb_write_multibyte_char(Nonnull(MStringBuilder*) msb, const Allocator a, unsigned int c){
    if(c & 0xff000000)
        msb_write_char(msb, a, (c & 0xff000000)>>24);
    if(c & 0xff0000)
        msb_write_char(msb, a, (c & 0xff0000)>>16);
    if(c & 0xff00)
        msb_write_char(msb, a, (c & 0xff00)>>8);
    if(c & 0xff)
        msb_write_char(msb, a, c & 0xff);
    }

static inline
void
msb_write_repeated_char(Nonnull(MStringBuilder*) msb, const Allocator a, char c, int n){
    _check_msb_size(msb, a, n);
    for(int i = 0; i < n; i++){
        msb->data[msb->cursor++] = c;
        }
    }

static inline
char
msb_peek_end(Nonnull(MStringBuilder*) msb){
    assert(msb->data);
    return msb->data[msb->cursor-1];
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

static inline
void
msb_rstrip(Nonnull(MStringBuilder*)msb){
    while(msb->cursor){
        auto c = msb->data[msb->cursor-1];
        switch(c){
            case ' ': case '\n': case '\t': case '\r':
                msb->cursor--;
                msb->data[msb->cursor] = '\0';
                break;
            default:
                return;
            }
        }
    }
static inline
void
msb_lstrip(Nonnull(MStringBuilder*)msb){
    int n_whitespace = 0;
    for(size_t i = 0; i < msb->cursor; i++){
        switch(msb->data[i]){
            case ' ': case '\n': case '\t': case '\r':
                n_whitespace++;
                break;
            default:
                goto endloop;
            }
        }
    endloop:;
    if(!n_whitespace)
        return;
    memmove(msb->data, msb->data+n_whitespace, msb->cursor-n_whitespace);
    msb->cursor-= n_whitespace;
    return;
    }

static inline
void
msb_strip(Nonnull(MStringBuilder*)msb){
    msb_rstrip(msb);
    msb_lstrip(msb);
    }

static inline
void
msb_read_file(Nonnull(MStringBuilder*) msb, const Allocator a, Nonnull(FILE*) restrict fp){
    // do it 1024 bytes at a time? maybe we can do it faster? idk
    enum {SB_READ_FILE_SIZE=4096};
    for(;;){
        _check_msb_size(msb, a, SB_READ_FILE_SIZE);
        auto numread = fread(msb->data + msb->cursor, 1, SB_READ_FILE_SIZE, fp);
        msb->cursor += numread;
        if (numread != SB_READ_FILE_SIZE)
            break;
        }
    }

printf_func(3, 4)
static inline
int
msb_sprintf(Nonnull(MStringBuilder*)msb, const Allocator a, Nonnull(const char*) restrict fmt, ...){
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);
    auto _msg_size = vsnprintf(NULL, 0, fmt, args)+1;
    va_end(args);
    _check_msb_size(msb, a, _msg_size);
    auto result = vsprintf(msb->data + msb->cursor, fmt, args2);
    msb->cursor += result;
    va_end(args2);
    return result;
    }

static inline
int
msb_vsprintf(Nonnull(MStringBuilder*)msb, const Allocator a, Nonnull(const char*)restrict fmt, va_list args){
    va_list args2;
    va_copy(args2, args);
    auto _msg_size = vsnprintf(NULL, 0, fmt, args)+1;
    va_end(args);
    _check_msb_size(msb, a, _msg_size);
    auto result = vsprintf(msb->data + msb->cursor, fmt, args2);
    msb->cursor += result;
    va_end(args2);
    return result;
    }

static inline
void
msb_ljust(Nonnull(MStringBuilder*)msb, const Allocator a, char c, int total_length){
    if(msb->cursor > total_length)
        return;
    auto n = total_length - msb->cursor;
    msb_write_repeated_char(msb, a, c, n);
    }

static inline
void
msb_rjust(Nonnull(MStringBuilder*)msb, const Allocator a, char c, int total_length){
    if(msb->cursor > total_length)
        return;
    auto n = total_length - msb->cursor;
    _check_msb_size(msb, a, n);
    memmove(msb->data+n, msb->data, msb->cursor);
    for(int i = 0; i < n; i++){
        msb->data[i] = c;
        }
    msb->cursor +=n;
    }

#define msb_write_literal(msb, a, lit) msb_write_str(msb, a, ""lit, sizeof(""lit)-1)


#endif
