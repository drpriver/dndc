#ifndef MEM_UTIL_H
#define MEM_UTIL_H

#include <string.h>
#include <assert.h>

// 0 on success, 1 on failure
// Inserts a buffer into an array at the given position.
static inline
int
meminsert(size_t whence, void* restrict dst, size_t capacity, size_t used, const void* restrict src, size_t length){
    if(capacity - used < length) return 1;
    if(whence > used) return 1;
    if(used == whence){
        memmove(((char*)dst)+whence, src, length);
        return 0;
    }
    size_t tail = used - whence;
    memmove(((char*)dst)+whence+length, ((char*)dst)+whence, tail);
    memmove(((char*)dst)+whence, src, length);
    return 0;
}

// Logically "removes" a section of an array by shifting the stuff after
// it forward.
static inline
void
memremove(size_t whence, void* dst, size_t used, size_t length){
    assert(length + whence <= used);
    size_t tail = used - whence - length;
    if(tail) memmove(((char*)dst)+whence, ((char*)dst)+whence+length, tail);

}

#endif
