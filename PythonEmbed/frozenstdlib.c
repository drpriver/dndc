#include "pyhead.h"
#if PY_MINOR_VERSION > 7
#include "frozenstdlib.h"
#include "frozen/abc.py.h"
#include "frozen/importlib.py.h"
#include "frozen/importlib_external.py.h"
#include "frozen/zipimport.py.h"
#include "frozen/io.py.h"
#include "frozen/codecs.py.h"
#include "frozen/encodings.py.h"
#include "frozen/encodings.latin_1.py.h"
#include "frozen/encodings.utf_8.py.h"
#include "frozen/stat.py.h"
#include "frozen/_collections_abc.py.h"
#include "frozen/genericpath.py.h"
#include "frozen/posixpath.py.h"
#include "frozen/ntpath.py.h"
#include "frozen/os.py.h"
#include "frozen/operator.py.h"
#include "frozen/keyword.py.h"
#include "frozen/heapq.py.h"
#include "frozen/reprlib.py.h"
#include "frozen/collections.py.h"
#include "frozen/collections.abc.py.h"
#include "frozen/types.py.h"
#include "frozen/functools.py.h"
#include "frozen/weakref.py.h"
#include "frozen/copy.py.h"
#include "frozen/copyreg.py.h"
#include "frozen/typing.py.h"
#include "frozen/contextlib.py.h"
#include "frozen/enum.py.h"
#include "frozen/re.py.h"
#include "frozen/sre_compile.py.h"
#include "frozen/sre_parse.py.h"
#include "frozen/sre_constants.py.h"
#include "frozen/string.py.h"
#include "frozen/cmd.py.h"
#include "frozen/fnmatch.py.h"
#include "frozen/token.py.h"
#include "frozen/tokenize.py.h"
#include "frozen/linecache.py.h"
#include "frozen/dis.py.h"
#include "frozen/opcode.py.h"
#include "frozen/importlib.machinery.py.h"
#include "frozen/inspect.py.h"
#include "frozen/bdb.py.h"
#include "frozen/codeop.py.h"
#include "frozen/code.py.h"
#include "frozen/traceback.py.h"
#include "frozen/__future__.py.h"
#include "frozen/glob.py.h"
#include "frozen/pprint.py.h"
#include "frozen/signal.py.h"
#include "frozen/shlex.py.h"
#include "frozen/pdb.py.h"

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
    PyImport_FrozenModules = _PyImport_FrozenModules;
    }
#else
extern
void
set_frozen_modules(void){
    }
#endif
