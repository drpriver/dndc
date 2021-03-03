#ifndef MALLOCATOR_H
#define MALLOCATOR_H
#include "allocator.h"

static inline
force_inline
Allocator
get_mallocator(void){
    return (Allocator){.type=ALLOCATOR_MALLOC};
    }
#endif
