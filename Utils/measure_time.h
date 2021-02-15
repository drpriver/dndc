#ifndef MEASURE_TIME_H
#define MEASURE_TIME_H

#ifndef WINDOWS
#include <time.h>
#include <stdint.h>
#include <unistd.h>
// returns microseconds
static inline uint64_t get_t(void){
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return t.tv_sec * 1000000llu + t.tv_nsec/1000;
    }
#else


#include "windowsheader.h"
static LARGE_INTEGER freq;
static inline uint64_t get_t(void){
    LARGE_INTEGER time;
    if(!freq.QuadPart){
        BOOL ok = QueryPerformanceFrequency(&freq);
        assert(ok == TRUE);
    }

    BOOL ok = QueryPerformanceCounter(&time);
    assert(ok == TRUE);
    return  (1000000llu * time.QuadPart) / freq.QuadPart;
}
#endif

#endif
