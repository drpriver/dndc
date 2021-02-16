#ifndef BYTEBUILDER_H
#define BYTEBUILDER_H
#include "common_macros.h"
#include "d_memory.h"
#include "allocator.h"
#include "mallocator.h"

typedef struct ByteBuilder {
    size_t cursor;
    size_t capacity;
    NullUnspec(unsigned char*) data;
    const Allocator* _Null_unspecified allocator;
} ByteBuilder;

static inline
void
_check_bb_size(Nonnull(ByteBuilder*), size_t);

static inline
void
force_inline
_resize_bb(Nonnull(ByteBuilder*) bb, size_t size){
    if(unlikely(!bb->allocator)){
        bb->allocator = get_mallocator();
        }
    unsigned char* new_data = Allocator_realloc(bb->allocator, bb->data, bb->capacity, size);
    assert(new_data);
    bb->data = new_data;
    bb->capacity = size;
    }

static inline
void
force_inline
_check_bb_size(Nonnull(ByteBuilder*) bb, size_t len){
    if(bb->cursor + len <= bb->capacity)
        return;
    size_t new_size = Max_literal((bb->capacity*3)/2, 32);
    while(new_size < bb->cursor+len){
        new_size *= 2;
        }
    _resize_bb(bb, new_size);
    }
static inline
void
bb_reserve(Nonnull(ByteBuilder*)bb, size_t n){
    _check_bb_size(bb, n);
    }

static inline
void
force_inline
bb_write(Nonnull(ByteBuilder*) restrict bb, Nonnull(const void*) restrict data, size_t size){
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
bb_write_qword(Nonnull(ByteBuilder*) restrict bb, uint64_t qword){
    _check_bb_size(bb, 8);
    memcpy(bb->data+bb->cursor, &qword, 8);
    bb->cursor += 8;
    }
static inline
void
force_inline
bb_write_dword(Nonnull(ByteBuilder*) restrict bb, uint32_t dword){
    _check_bb_size(bb, 4);
    memcpy(bb->data+bb->cursor, &dword, 4);
    bb->cursor += 4;
    }

static inline
void
force_inline
bb_write_word(Nonnull(ByteBuilder*) restrict bb, uint16_t word){
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
bb_write_byte(Nonnull(ByteBuilder*) restrict bb, uint8_t byte_){
    _check_bb_size(bb, 1);
    uint8_t* dst = (uint8_t*)bb->data + bb->cursor;
    *dst = byte_;
    bb->cursor += 1;
    }

static inline
ByteBuffer
bb_borrow(Nonnull(ByteBuilder*) bb){
    return (ByteBuffer){
        .buff = bb->data,
        .n_bytes = bb->cursor,
        };
    }

static inline
ByteBuffer
bb_detach(Nonnull(ByteBuilder*)bb){
    auto result = (ByteBuffer){
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
bb_destroy(Nonnull(ByteBuilder*) bb){
    if(bb->data){
        Allocator_free(bb->allocator, bb->data, bb->capacity);
        }
    *bb = (ByteBuilder){};
    }
static inline
void
bb_reset(Nonnull(ByteBuilder*)bb){
    bb->cursor = 0;
    }
#endif
