#ifndef PYHEAD_H
#define PYHEAD_H
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#if PY_MAJOR_VERSION < 3
#error "Only python3 or better is supported"
#endif
#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 7
#error "Only python 3.7 or better is supported"
#endif

#endif
