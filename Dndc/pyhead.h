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
// Python 3.7 has a bug in PyStructSequence_NewType and isn't worth supporting
// at this point:
//   https://bugs.python.org/issue28709
// So, just support 3.8 or better
#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 8
#error "Only python 3.8 or better is supported"
#endif

// inspect.py doesn't support native functions having type annotations in the
// docstring yet.
// If it encounters annotations it just gives you the horrible (...) signature.
// If I feel like it, I'll submit a patch allowing annotations as it is trivial.
#if 0
#define PY_INSPECT_SUPPORTS_ANNOTATIONS 1
#else
#define PY_INSPECT_SUPPORTS_ANNOTATIONS 0
#endif


#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 10
// Shim for older pythons.
static inline
int
PyModule_AddObjectRef(PyObject* mod, const char* name, PyObject* value){
    int result = PyModule_AddObject(mod, name, value);
    if(result == 0){ // 0 is success, so above call stole our ref
        Py_INCREF(value);
    }
    return result;
}
static inline
PyObject*
Py_XNewRef(PyObject* o){
    if(!o) return NULL;
    Py_INCREF(o);
    return o;
}
#endif
#if PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION < 9
// shim
static inline
PyObject*
PyObject_CallOneArg(PyObject* callable, PyObject* arg){
    PyObject* tup = PyTuple_Pack(1, arg); // new ref
    if(!tup) return NULL;
    PyObject* result = PyObject_CallObject(callable, tup);
    Py_DECREF(tup);
    return result;
}

static inline
int
Py_IS_TYPE(const PyObject* o, const PyTypeObject* type){
    return o->ob_type == type;
}

#endif

#endif
