#include "pyhead.h"
#if PY_MINOR_VERSION > 7
#include "frozenstdlib.h"
#include "abc.py.h"
#include "importlib_bootstrap.py.h"
#include "importlib_bootstrap_external.py.h"
#include "zipimport.py.h"
#include "io.py.h"
#include "codecs.py.h"
#include "encodings.py.h"
#include "encodings.latin_1.py.h"
#include "encodings.utf_8.py.h"
#include "stat.py.h"
#include "_collections_abc.py.h"
#include "genericpath.py.h"
#include "posixpath.py.h"
#include "ntpath.py.h"
#include "os.py.h"
#include "operator.py.h"
#include "keyword.py.h"
#include "heapq.py.h"
#include "reprlib.py.h"
#include "collections.py.h"
#include "collections.abc.py.h"
#include "types.py.h"
#include "functools.py.h"
#include "weakref.py.h"
#include "copy.py.h"
#include "copyreg.py.h"
#include "typing.py.h"
#include "contextlib.py.h"
#include "enum.py.h"
#include "re.py.h"
#include "sre_compile.py.h"
#include "sre_parse.py.h"
#include "sre_constants.py.h"
#include "string.py.h"
#include "cmd.py.h"
#include "fnmatch.py.h"
#include "token.py.h"
#include "tokenize.py.h"
#include "linecache.py.h"
#include "dis.py.h"
#include "opcode.py.h"
#include "importlib.machinery.py.h"
#include "inspect.py.h"
#include "bdb.py.h"
#include "codeop.py.h"
#include "code.py.h"
#include "traceback.py.h"
#include "__future__.py.h"
#include "glob.py.h"
#include "pprint.py.h"
#include "signal.py.h"
#include "shlex.py.h"
#include "pdb.py.h"

static const struct _frozen _PyImport_FrozenModules[] = {
    /* importlib */
    {"_frozen_importlib", _Py_M__importlib_bootstrap,
        (int)sizeof(_Py_M__importlib_bootstrap)},
    {"_frozen_importlib_external", _Py_M__importlib_bootstrap_external,
        (int)sizeof(_Py_M__importlib_bootstrap_external)},
    // we don't use zip import, so don't include it
    // {"zipimport", _Py_M__zipimport,
        // (int)sizeof(_Py_M__zipimport)},
    /* Required to initialize at all */
    {"abc", _Py_M__abc, (int)sizeof(_Py_M__abc)},
    {"io", _Py_M__io, (int)sizeof(_Py_M__io)},
    {"codecs", _Py_M__codecs, (int)sizeof(_Py_M__codecs)},
    {"encodings", _Py_M__encodings, -(int)sizeof(_Py_M__encodings)},
    {"encodings.utf_8", _Py_M__encodingsutf_8, -(int)sizeof(_Py_M__encodingsutf_8)},
    {"encodings.latin_1", _Py_M__encodingslatin_1, -(int)sizeof(_Py_M__encodingslatin_1)},
    /* stdlib */
    /* Weird collection, but they allow typing, re, os and pdb to work */
    /* Might be useful to get some together that would allow numpy to run */
    {"stat", _Py_M__stat, (int)sizeof(_Py_M__stat)},
    {"_collections_abc", _Py_M___collections_abc, (int)sizeof(_Py_M___collections_abc)},
    {"genericpath", _Py_M__genericpath, (int)sizeof(_Py_M__genericpath)},
    {"posixpath", _Py_M__posixpath, (int)sizeof(_Py_M__posixpath)},
    {"ntpath", _Py_M__ntpath, (int)sizeof(_Py_M__ntpath)},
    {"os", _Py_M__os, (int)sizeof(_Py_M__os)},
    {"operator", _Py_M__operator, (int)sizeof(_Py_M__operator)},
    {"keyword", _Py_M__keyword, (int)sizeof(_Py_M__keyword)},
    {"heapq", _Py_M__heapq, (int)sizeof(_Py_M__heapq)},
    {"reprlib", _Py_M__reprlib, (int)sizeof(_Py_M__reprlib)},
    {"collections", _Py_M__collections, -(int)sizeof(_Py_M__collections)},
    {"collections.abc", _Py_M__collectionsabc, (int)sizeof(_Py_M__collectionsabc)},
    {"types", _Py_M__types, (int)sizeof(_Py_M__types)},
    {"functools", _Py_M__functools, (int)sizeof(_Py_M__functools)},
    {"weakref", _Py_M__weakref, (int)sizeof(_Py_M__weakref)},
    {"copy", _Py_M__copy, (int)sizeof(_Py_M__copy)},
    {"copyreg", _Py_M__copyreg, (int)sizeof(_Py_M__copyreg)},
    {"typing", _Py_M__typing, (int)sizeof(_Py_M__typing)},
    {"contextlib", _Py_M__contextlib, (int)sizeof(_Py_M__contextlib)},
    {"enum", _Py_M__enum, (int)sizeof(_Py_M__enum)},
    {"re", _Py_M__re, (int)sizeof(_Py_M__re)},
    {"sre_constants", _Py_M__sre_constants, (int)sizeof(_Py_M__sre_constants)},
    {"sre_compile", _Py_M__sre_compile, (int)sizeof(_Py_M__sre_compile)},
    {"sre_parse", _Py_M__sre_parse, (int)sizeof(_Py_M__sre_parse)},
    {"string", _Py_M__string, (int)sizeof(_Py_M__string)},
    {"cmd", _Py_M__cmd, (int)sizeof(_Py_M__cmd)},
    {"fnmatch", _Py_M__fnmatch, (int)sizeof(_Py_M__fnmatch)},
    {"token", _Py_M__token, (int)sizeof(_Py_M__token)},
    {"tokenize", _Py_M__tokenize, (int)sizeof(_Py_M__tokenize)},
    {"linecache", _Py_M__linecache, (int)sizeof(_Py_M__linecache)},
    {"dis", _Py_M__dis, (int)sizeof(_Py_M__dis)},
    {"opcode", _Py_M__opcode, (int)sizeof(_Py_M__opcode)},
    {"importlib", _Py_M__importlibmachinery, -(int)sizeof(_Py_M__importlibmachinery)},
    {"importlib.machinery", _Py_M__importlibmachinery, -(int)sizeof(_Py_M__importlibmachinery)},
    {"inspect", _Py_M__inspect, (int)sizeof(_Py_M__inspect)},
    {"bdb", _Py_M__bdb, (int)sizeof(_Py_M__bdb)},
    {"code", _Py_M__code, (int)sizeof(_Py_M__code)},
    {"codeop", _Py_M__codeop, (int)sizeof(_Py_M__codeop)},
    {"traceback", _Py_M__traceback, (int)sizeof(_Py_M__traceback)},
    {"__future__", _Py_M____future__, (int)sizeof(_Py_M____future__)},
    {"glob", _Py_M__glob, (int)sizeof(_Py_M__glob)},
    {"pprint", _Py_M__pprint, (int)sizeof(_Py_M__pprint)},
    {"signal", _Py_M__signal, (int)sizeof(_Py_M__signal)},
    {"shlex", _Py_M__shlex, (int)sizeof(_Py_M__shlex)},
    {"pdb", _Py_M__pdb, (int)sizeof(_Py_M__pdb)},
    /* sentinel */
    {0, 0, 0},
};

extern
void
set_frozen_modules(void){
#if 0
    // Code for monitoring the size of the frozen modules
    for(int i = 0;i < arrlen(_PyImport_FrozenModules);i++){
        auto f = &_PyImport_FrozenModules[i];
        HEREPrint(f->name);
        HEREPrint(f->size);
    }
#endif
    PyImport_FrozenModules = _PyImport_FrozenModules;
    }
#else
extern
void
set_frozen_modules(void){
    }
#endif
extern
struct FrozenPyVersion
get_frozen_version(void){
    return (struct FrozenPyVersion){PY_MAJOR_VERSION, PY_MINOR_VERSION};
    }
