//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
#ifndef MSB_BASE64_H
#define MSB_BASE64_H
#include "MStringBuilder.h"
#include "base64.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// Writes the base64 representation of the data buffer into the builder.
static inline
void
msb_write_b64(MStringBuilder* restrict sb, const void* data, size_t length){
    size_t size_needed = base64_encode_size(length);
    if(unlikely(!size_needed))
        return;
    int err = _check_msb_remaining_size(sb, size_needed);
    if(unlikely(err)) return;
    size_t size_used = base64_encode(sb->data + sb->cursor, size_needed, data, length);
    assert(size_used == size_needed);
    sb->cursor += size_used;
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
