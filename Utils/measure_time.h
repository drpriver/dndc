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
#include "SDLhead.h"
static inline uint64_t get_t(void){
    return 1000* SDL_GetTicks();
    }
#endif


#endif
