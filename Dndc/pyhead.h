#ifndef PYHEAD_H
#define PYHEAD_H
#define PY_SSIZE_T_CLEAN

// Python's pytime.h triggers a visibility warning (at least on windows). We really don't care.
#ifdef __clang___
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvisibility"
#endif

#define PY_LIMITED_API

#if defined(_WIN32) && defined(_DEBUG)
// Windows release of python only ships with release lib, but the _DEBUG macro
// enables a linker comment to link against the debug lib, which will fail at link time.
// The offending header is "pyconfig.h" in the python include directory.
// So undef it.
#undef _DEBUG
#include <Python.h>
#define _DEBUG

#else
#include <Python.h>
#endif

#ifdef __clang___
#pragma clang diagnostic pop
#endif


#if PY_MAJOR_VERSION < 3
#error "Only python3 or better is supported"
#endif
#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 7
#error "Only python 3.7 or better is supported"
#endif

#endif
