#ifndef BYTE_BUFFER_H
#define BYTE_BUFFER_H
// for size_t
#include <stdlib.h>
#include "common_macros.h"
#include "error_handling.h"
typedef struct ByteBuffer {
    Nonnull(void*) buff;
    size_t n_bytes;
} ByteBuffer;
Errorable_declare(ByteBuffer);
#endif
