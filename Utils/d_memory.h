#ifndef d_memory_h
#define d_memory_h
#include <stdlib.h>
#include "common_macros.h"
#include "error_handling.h"
typedef struct ByteBuffer {
    Nonnull(void*) buff;
    size_t n_bytes;
} ByteBuffer;
Errorable_declare(ByteBuffer);
#endif
