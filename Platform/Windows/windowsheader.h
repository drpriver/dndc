#ifndef WINDOWSHEADER_H
#define WINDOWSHEADER_H
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
// Idk why, but this conflicts with builtin
// so hacky #define
#define _mm_prefetch _WINDOWS_MM_PREFETCH
#include <Windows.h>
#undef _mm_prefetch
#undef ERROR

// TODO: isolate windows garbage
#undef ERROR
#if LOG_LEVEL > 0
#define ERROR(fmt, ...) logfunc(LOG_LEVEL_ERROR, __FILE__, __func__, __LINE__, "" fmt, ##__VA_ARGS__)
#else
#define ERROR(...)
#endif

#endif
