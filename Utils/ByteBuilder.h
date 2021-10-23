#ifndef BYTEBUILDER_H
#define BYTEBUILDER_H
// size_t, NULL
#include <stddef.h>
// integer types
#include <stdint.h>
// memcpy
#include <string.h>
#include "ByteBuffer.h"
#include "Allocators/allocator.h"
#include "Allocators/mallocator.h"

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

#if !defined(likely) && !defined(unlikely)
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif

#ifndef unhandled_error_condition
#define unhandled_error_condition(cond) assert(!(cond))
#endif

typedef struct ByteBuilder {
    size_t cursor;
    size_t capacity;
    unsigned char*_Null_unspecified data;
    Allocator allocator;
} ByteBuilder;

static inline
void
_check_bb_size(ByteBuilder*, size_t);

static inline
void
force_inline
_resize_bb(ByteBuilder* bb, size_t size){
    if(unlikely(!bb->allocator.type)){
        bb->allocator = get_mallocator();
        }
    unsigned char* new_data = Allocator_realloc(bb->allocator, bb->data, bb->capacity, size);
    unhandled_error_condition(!new_data);
    bb->data = new_data;
    bb->capacity = size;
    }

static inline
void
force_inline
_check_bb_size(ByteBuilder* bb, size_t len){
    if(bb->cursor + len <= bb->capacity)
        return;
    size_t new_size = (bb->capacity*3)/2;
    if(new_size < 32) new_size = 32;
    while(new_size < bb->cursor+len){
        new_size *= 2;
        }
    _resize_bb(bb, new_size);
    }

static inline
void
bb_reserve(ByteBuilder* bb, size_t n){
    _check_bb_size(bb, n);
    }

static inline
void
force_inline
bb_write(ByteBuilder* restrict bb, const void* restrict data, size_t size){
    _check_bb_size(bb, size);
    switch(size){
        case 1:  memcpy(bb->data+bb->cursor, data, 1);      break;
        case 2:  memcpy(bb->data+bb->cursor, data, 2);      break;
        case 4:  memcpy(bb->data+bb->cursor, data, 4);      break;
        case 8:  memcpy(bb->data+bb->cursor, data, 8);      break;
        default: memcpy(bb->data+bb->cursor, data, size);   break;
        }
    bb->cursor += size;
    }

static inline
void
force_inline
bb_write_qword(ByteBuilder* restrict bb, uint64_t qword){
    _check_bb_size(bb, 8);
    memcpy(bb->data+bb->cursor, &qword, 8);
    bb->cursor += 8;
    }

static inline
void
force_inline
bb_write_dword(ByteBuilder* restrict bb, uint32_t dword){
    _check_bb_size(bb, 4);
    memcpy(bb->data+bb->cursor, &dword, 4);
    bb->cursor += 4;
    }

static inline
void
force_inline
bb_write_word(ByteBuilder* restrict bb, uint16_t word){
    // assumes little-endian
    _check_bb_size(bb, 2);
    uint8_t high = word >> 8;
    uint8_t low = word & 0xffu;
    uint8_t* dst = (uint8_t*)bb->data + bb->cursor;
    *(dst++) = low;
    *dst = high;
    bb->cursor += 2;
    }

static inline
void
force_inline
bb_write_byte(ByteBuilder* restrict bb, uint8_t byte_){
    _check_bb_size(bb, 1);
    uint8_t* dst = (uint8_t*)bb->data + bb->cursor;
    *dst = byte_;
    bb->cursor += 1;
    }

static inline
ByteBuffer
bb_borrow(ByteBuilder* bb){
    return (ByteBuffer){
        .buff = bb->data,
        .n_bytes = bb->cursor,
        };
    }

static inline
ByteBuffer
bb_detach(ByteBuilder* bb){
    ByteBuffer result = {
        .buff = bb->data,
        .n_bytes = bb->cursor,
        };
    bb->capacity = 0;
    bb->cursor = 0;
    bb->data = NULL;
    return result;
    }

static inline
void
bb_destroy(ByteBuilder* bb){
    if(bb->data){
        Allocator_free(bb->allocator, bb->data, bb->capacity);
        }
    *bb = (ByteBuilder){.allocator=bb->allocator};
    }

static inline
void
bb_reset(ByteBuilder* bb){
    bb->cursor = 0;
    }
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
