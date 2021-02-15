#ifndef PYHEAD_H
#define PYHEAD_H
#define PY_SSIZE_T_CLEAN
#ifdef DARWIN
// #include <Python/Python.h>
#include <Python.h>
#else
#include <Python.h>
#endif

#if PY_MAJOR_VERSION < 3
#error "Only python3 or better is supported"
#endif
#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 8
#error "Only python 3.8 or better is supported"
#endif

#endif
