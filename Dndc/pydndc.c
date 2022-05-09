//
// Exposes dndc as a c-extension for python.
//
#define DNDC_API static inline
#include "dndc.h"
#include "dndc_ast.h"
#include "dndc_long_string.h"
#include "common_macros.h"
#include "allocator.h"
#include "pyhead.h"
#include "structmember.h"
#include "mallocator.h"
#include "MStringBuilder.h"
#include "msb_extensions.h"
#include "msb_format.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#elif defined(__GNUC__)
PushDiagnostic();
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

PushDiagnostic();
SuppressUnusedFunction();
static inline
LongString
pystring_to_longstring(PyObject* pyobj, Allocator a){
    const char* text;
    Py_ssize_t length;
    text = PyUnicode_AsUTF8AndSize(pyobj, &length);
    unhandled_error_condition(!text);
    if(!length){
        return (LongString){};
    }
    char* copy = Allocator_dupe(a, text, length+1);
    return (LongString){
        .text = copy,
        .length = length,
    };
}

static inline
StringView
pystring_to_stringview(PyObject* pyobj, Allocator a){
    const char* text;
    Py_ssize_t length;
    text = PyUnicode_AsUTF8AndSize(pyobj, &length);
    unhandled_error_condition(!text);
    if(!length){
        return (StringView){};
    }
    char* copy = Allocator_dupe(a, text, length);
    return (StringView){
        .text = copy,
        .length = length,
    };
}

static inline
StringView
pystring_borrow_stringview(PyObject* pyobj){
    const char* text;
    Py_ssize_t length;
    text = PyUnicode_AsUTF8AndSize(pyobj, &length);
    unhandled_error_condition(!text);
    return (StringView){.text=text, .length=length};
}

static inline
LongString
pystring_borrow_longstring(PyObject* pyobj){
    const char* text;
    Py_ssize_t length;
    text = PyUnicode_AsUTF8AndSize(pyobj, &length);
    unhandled_error_condition(!text);
    return (LongString){.text=text, .length=length};
}
PopDiagnostic(); // unused function

//
// Wrappers a python callable so that it can be called as a DndcLogger
// user_data is actually a pointer to the  PyObject*, so it needs to be a static PyObject*
// or be in some object that is guranteed to live long enough.
static
void
pylogger(void*_Nullable user_data, int type_, const char* filename, int filename_len, int line, int col, const char* message, int message_len){
    if(!user_data) return;
    PyObject* callable = *(void**)user_data;
    if(!callable) return;
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);
    PyObject* args = Py_BuildValue("is#iis#", type_, filename, (Py_ssize_t)filename_len, line, col, message, (Py_ssize_t)message_len);
    PyObject* ret = PyObject_CallObject(callable, args);
    Py_XDECREF(ret);
    Py_XDECREF(args);
    PyErr_Restore(type, value, traceback);
}

typedef struct DndcPyFileCache DndcPyFileCache;
struct DndcPyFileCache {
    PyObject_HEAD
    DndcFileCache* text_cache;
    DndcFileCache* b64_cache;
};

static
Nullable(PyObject*)
DndcPyFileCache_remove(PyObject* self, PyObject* str){
    if(!PyUnicode_Check(str)){
        PyErr_SetString(PyExc_TypeError, "Argument to remove must be a string");
        return NULL;
    }
    StringView path = pystring_borrow_stringview(str);
    DndcPyFileCache* cache = (DndcPyFileCache*)self;
    dndc_filecache_remove(cache->text_cache, path);
    dndc_filecache_remove(cache->b64_cache, path);
    Py_RETURN_NONE;
}

static
Nullable(PyObject*)
DndcPyFileCache_clear(PyObject* self){
    DndcPyFileCache* cache = (DndcPyFileCache*)self;
    dndc_filecache_clear(cache->text_cache);
    dndc_filecache_clear(cache->b64_cache);
    Py_RETURN_NONE;
}

static
Nullable(PyObject*)
DndcPyFileCache_paths(PyObject* self){
    DndcPyFileCache* cache = (DndcPyFileCache*)self;
    Py_ssize_t nfiles = dndc_filecache_n_paths(cache->b64_cache) + dndc_filecache_n_paths(cache->text_cache);
    PyObject* result = PyList_New(nfiles);
    if(!result)
        goto error;
    Py_ssize_t index = 0;
    StringView buff[100];
    DndcFileCache* caches[2] = {cache->b64_cache, cache->text_cache};
    for(size_t c = 0; c < arrlen(caches); c++){
        DndcFileCache* ch = caches[c];
        for(size_t cookie = 0, n = dndc_filecache_cached_paths(ch, buff, arrlen(buff), &cookie);
            n != 0;
            n = dndc_filecache_cached_paths(ch, buff, arrlen(buff), &cookie)
        ){
            for(size_t i = 0; i < n; i++){
                StringView path = buff[i];
                PyObject* s = PyUnicode_FromStringAndSize(path.text, path.length);
                if(!s)
                    goto error;
                PyList_SET_ITEM(result, index++, s); // steals the reference
            }
        }
    }
    return result;
    error:
    Py_XDECREF(result);
    return NULL;
}

static
Nullable(PyObject*)
DndcPyFileCache_new(PyTypeObject* subtype, PyObject *_Null_unspecified args, PyObject *_Null_unspecified kwds){
    (void)args;
    (void)kwds;
    DndcPyFileCache* obj = (DndcPyFileCache*)subtype->tp_alloc(subtype, 1);
    if(!obj)
        return NULL;
    obj->b64_cache = dndc_create_filecache();
    obj->text_cache = dndc_create_filecache();
    return (PyObject*)obj;
}

static
void
DndcPyFileCache_dealloc(PyObject* self){
    DndcPyFileCache* cache = (DndcPyFileCache*)self;
    dndc_filecache_destroy(cache->b64_cache);
    dndc_filecache_destroy(cache->text_cache);
}

static
Nullable(PyObject*)
DndcPyFileCache_store(PyObject* self, PyObject* args, PyObject* kwargs){
    int overwrite = 1;
    PyObject* opath;
    PyObject* odata;
    const char* const keywords[] = { "path", "data", "overwrite", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!|p:store", (char**)keywords, &PyUnicode_Type, &opath, &PyUnicode_Type, &odata, &overwrite)){
        return NULL;
    }
    PopDiagnostic();
    StringView path = pystring_borrow_stringview(opath);
    StringView data = pystring_borrow_stringview(odata);
    DndcPyFileCache* cache = (DndcPyFileCache*)self;
    int result = dndc_filecache_store_text(cache->text_cache, path, data, overwrite);
    if(!result)
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static
PyMethodDef DndcPyFileCache_methods[] = {
    {
        .ml_name = "remove",
        .ml_meth = (PyCFunction)DndcPyFileCache_remove,
        .ml_flags = METH_O,
        .ml_doc =
            #if PY_INSPECT_SUPPORTS_ANNOTATIONS
            "remove(self, filepath:str) -> bool\n"
            #else
            "remove(self, filepath)\n"
            #endif
            "--\n"
            "\n"
            "Remove the given filepath (str) from the cache.\n",
    },
    {
        .ml_name = "clear",
        .ml_meth = (PyCFunction)DndcPyFileCache_clear,
        .ml_flags = METH_NOARGS,
        .ml_doc =
            #if PY_INSPECT_SUPPORTS_ANNOTATIONS
            "clear(self) -> None\n"
            #else
            "clear(self)\n"
            #endif
            "--\n"
            "\n"
            "Removes all cached files.\n",
    },
    {
        .ml_name = "paths",
        .ml_meth = (PyCFunction)DndcPyFileCache_paths,
        .ml_flags = METH_NOARGS,
        .ml_doc =
            #if PY_INSPECT_SUPPORTS_ANNOTATIONS
            "paths(self) -> list[str]\n"
            #else
            "paths(self)\n"
            #endif
            "--\n"
            "\n"
            "Returns a list of the paths in the file cache.\n",
    },
    {
        .ml_name = "store",
        .ml_meth = (PyCFunction)DndcPyFileCache_store,
        .ml_flags = METH_VARARGS | METH_KEYWORDS,
        .ml_doc =
            #if PY_INSPECT_SUPPORTS_ANNOTATIONS
            "store(self, path:str, data:str, overwrite=True) -> bool\n"
            #else
            "store(self, path, data, overwrite=True)\n"
            #endif
            "--\n"
            "\n"
            "Stores the string at the given path.\n"
            "Returns True on success, False on failure.",
    },
    {},
};


static
PyTypeObject DndcPyFileCache_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pydndc.FileCache",
    .tp_basicsize = sizeof(DndcPyFileCache),
    .tp_doc = "An object that manages a cache of files to avoid io between repeated invocations.",
    .tp_new = DndcPyFileCache_new,
    .tp_dealloc = DndcPyFileCache_dealloc,
    .tp_methods = DndcPyFileCache_methods,
};

static
Nullable(PyObject*)
pydndc_reformat(PyObject* , PyObject* , PyObject*);

static
Nullable(PyObject*)
pydndc_expand(PyObject* , PyObject* , PyObject*);

static
Nullable(PyObject*)
pydndc_htmlgen(PyObject*, PyObject*, PyObject*);

static
Nullable(PyObject*)
pydndc_stderr_logger(PyObject*, PyObject*, PyObject*);

static
Nullable(PyObject*)
pydndc_anaylze_syntax_for_highlight(PyObject*, PyObject*, PyObject*);

// returns 0 on success
static
int
pyobj_to_json(PyObject*, MStringBuilder*, int);

static
PyMethodDef pydndc_methods[] = {
    {
        .ml_name = "reformat",
        .ml_meth = (PyCFunction)pydndc_reformat,
        .ml_flags = METH_VARARGS|METH_KEYWORDS,
        .ml_doc =
        #if PY_INSPECT_SUPPORTS_ANNOTATIONS
        "reformat(text:str, logger:Callable|None=None) -> str\n"
        #else
        "reformat(text, logger=None)\n"
        #endif
        "--\n"
        "\n"
        "Reformat the given .dnd string into a nicely formatted representation.\n"
        "\n"
        "Args:\n"
        "-----\n"
        "text: str\n"
        "    The .dnd string.\n"
        "\n"
        "Optional Args:\n"
        "--------------\n"
        "logger: Callable(int, str, int, int, str)\n"
        "    A callable for reporting errors. See the discussion in htmlgen.\n"
        "\n"
        "Returns:\n"
        "--------\n"
        "str: The reformatted string.\n"
        "\n"
        "Throws:\n"
        "-------\n"
        "Throws ValueError if there is a syntax error in the given string.\n"
        ,
    },
    {
        .ml_name = "htmlgen",
        .ml_meth = (PyCFunction)pydndc_htmlgen,
        .ml_flags = METH_VARARGS|METH_KEYWORDS,
        .ml_doc =
        #if PY_INSPECT_SUPPORTS_ANNOTATIONS
        "htmlgen(text:str, base_dir:str='.', filename:str='(string input)', logger:Callable=None, file_cache:FileCache=None, flags:Flags=0, jsargs:str=None)\n"
        #else
        "htmlgen(text, base_dir='.', filename='(string input)', logger=None, file_cache=None, flags=Flags.NONE, jsargs=None)\n"
        #endif
        "--\n"
        "\n"
        "Parses and converts the .dnd string into html, returning as a string.\n"
        "Additionally, returns a list of what files the string depends upon.\n"
        "\n"
        "Args:\n"
        "-----\n"
        "text: str\n"
        "    The .dnd string.\n"
        "\n"
        "Optional Args:\n"
        "--------------\n"
        "base_dir: str\n"
        "    For relative filepaths referenced in the document, what those paths\n"
        "    are relative to. Defaults to the current directory.\n"
        "\n"
        "filename: str\n"
        "    The filename that the text came from.\n"
        "\n"
        "logger: Callable(int, str, int, int, str)\n"
        "    A callable for reporting errors. See the extended discussion below.\n"
        "\n"
        "file_cache: FileCache\n"
        "    The file cache for caching files in between invocations.\n"
        "    Create a new one and pass it in between calls.\n"
        "    It is your responsibility to remove stale files by calling\n"
        "    .remove on it.\n"
        "    If you don't pass one in, no caching will be done.\n"
        "\n"
        "flags: Flags\n"
        "    Bit flags controlling some behavior. The allowed flags are\n"
        "    exported on the module as Flags are as follows:\n"
        "\n"
        "    INPUT_IS_UNTRUSTED: Input is from an untrusted source and so\n"
        "                        should not be allowed to load files, output\n"
        "                        raw html, etc. as that could lead to\n"
        "                        data leakage, etc. JavaScript will be\n"
        "                        disabled.\n"
        "\n"
        "    FRAGMENT_ONLY:      Output an html fragment instead of a complete\n"
        "                        html document The fragment will still include \n"
        "                        the styles and script tags, they will just be \n"
        "                        before the content.\n"
        "\n"
        "    DONT_INLINE_IMAGES: If set, don't embed images as base64 urls.\n"
        "                        This is overruled by USE_DND_URL_SCHEME.\n"
        "\n"
        "    NO_THREADS:         Do all work on the calling thread.\n"
        "\n"
        "    USE_DND_URL_SCHEME: Don't embed images as base64 urls. Instead\n"
        "                        Use a dnd:///absolute/path/to/img url instead.\n"
        "                        This is for applications.\n"
        "\n"
        "    PRINT_STATS:        Generate Info messages for the logger.\n"
        "                        These are mostly information about timings of\n"
        "                        various stages of execution. Info messages are\n"
        "                        not generated if this is not set.\n"
        "\n"
        "    STRIP_WHITESPACE:   Strip trailing and leading whitespace from js\n"
        "                        and css imports. Ignores semantics, so make sure\n"
        "                        multiline strings are not disrupted by this!\n"
        "\n"
        "    DONT_READ:          Don't read any files not already in the file\n"
        "                        cache.\n"
        "\n"
        "    DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP: Attributes and directives are\n"
        "                        in separate namespaces, but that can be confusing.\n"
        "                        Set this flag to disallow that.\n"
        "\n"
        "jsargs: dict or str\n"
        "    A dict or json literal that will be exposed to js blocks as Args.\n"
        "\n"
        "Returns:\n"
        "--------\n"
        "str: The html.\n"
        "List[str]: the files the string depends on (such as an import).\n"
        "\n"
        "Throws:\n"
        "-------\n"
        "Throws ValueError if there is a syntax error in the given string.\n"
        "Can also throw due to missing files.\n"
        "Can also throw due to errors in embedded javascript blocks.\n"
        "\n"
        "\n"
        "If the logger is given, it will be called with the following\n"
        "arguments:\n"
        "\n"
        "Logger Arguments:\n"
        "-------------------------\n"
        "message_type: MsgType\n"
        "    The values are as follows:\n"
        "\n"
        "    ERROR:     An error that caused parsing to fail and cannot\n"
        "                       be recovered from.\n"
        "\n"
        "    WARNING:   Recoverable error or diagnostic.\n"
        "\n"
        "    NODELESS:  An error that cannot be recovered from but\n"
        "                       that does not originate from a node or source\n"
        "                       location.\n"
        "\n"
        "    STATISTIC: Not an error, a statistic like timing.\n"
        "\n"
        "    DEBUG:     A debug statement, as requested by a flag.\n"
        "                       May or may not have a valid filename, line,\n"
        "                       column.\n"
        "\n"
        "filename: str\n"
        "    From the given filename.\n"
        "\n"
        "line: int\n"
        "    0-based. For SystemError and Info this will be 0.\n"
        "\n"
        "col: int\n"
        "    0-based, byte index. For SystemError and Info this will be 0.\n"
        "\n"
        "message: str\n"
        "    The error message.\n"
        "\n"
        "\n"
        "The error reporter will be called even if an exception is thrown from\n"
        "parsing. The logger will be called on each error and warning\n"
        "encountered while parsing, and then the exception will be thrown. If\n"
        "the error reporter throws, the remaining errors will be skipped and\n"
        "that exception will be propagated in place of the ValueError from the\n"
        "parse error. Thus, if the error reporter throws, whether the parsing\n"
        "encountered any syntax errors will not be reported.\n"
        ,
    },
    {
        .ml_name = "expand",
        .ml_meth = (PyCFunction)pydndc_expand,
        .ml_flags = METH_VARARGS|METH_KEYWORDS,
        .ml_doc =
        #if PY_INSPECT_SUPPORTS_ANNOTATIONS
        "expand(text:str, base_dir:str='.', logger:Callable=None, file_cache:FileCache=None, flags:Flags=0, jsargs=None) -> str\n"
        #else
        "expand(text, base_dir='.', logger=None, file_cache=None, flags=Flags.NONE, jsargs=None)\n"
        #endif
        "--\n"
        "\n"
        "Parses and converts the .dnd string into an equivelant .dnd string after\n"
        "imports are resolved and js blocks are executed.\n"
        "\n"
        "Args:\n"
        "-----\n"
        "text: str\n"
        "    The .dnd string.\n"
        "\n"
        "Optional Args:\n"
        "--------------\n"
        "base_dir: str\n"
        "    For relative filepaths referenced in the document, what those paths\n"
        "    are relative to. Defaults to the current directory.\n"
        "\n"
        "logger: Callable(int, str, int, int, str)\n"
        "    A callable for reporting errors. See the discussion in htlmgen.\n"
        "\n"
        "file_cache: DndcFileCache\n"
        "    The file cache for caching files in between invocations.\n"
        "    Create a new one and pass it in between calls.\n"
        "    It is your responsibility to remove stale files by calling\n"
        "    .remove on it.\n"
        "    If you don't pass one in, no caching will be done.\n"
        "\n"
        "flags: int\n"
        "    Bit flags controlling some behavior. The allowed flags are\n"
        "    exported on the module as Flags and are as follows:\n"
        "\n"
        "    INPUT_IS_UNTRUSTED: Input is from an untrusted source and so\n"
        "                        should not be allowed to load files, output\n"
        "                        raw html, etc. as that could lead to\n"
        "                        data leakage, etc. JavaScript will be\n"
        "                        disabled.\n"
        "\n"
        "                        It doesn't really make sense to set this for this\n"
        "                        function, but it is allowed.\n"
        "\n"
        "    PRINT_STATS:        Generate Info messages for the logger.\n"
        "                        These are mostly information about timings of\n"
        "                        various stages of execution. Info messages are\n"
        "                        not generated if this is not set.\n"
        "\n"
        "    DONT_READ:          Don't read any files not already in the file\n"
        "                        cache.\n"
        "\n"
        "    DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP: Attributes and directives are\n"
        "                        in separate namespaces, but that can be confusing.\n"
        "                        Set this flag to disallow that.\n"
        "\n"
        "jsargs: dict or str\n"
        "    A dict or json literal that will be exposed to js blocks as Args.\n"
        "\n"
        "Returns:\n"
        "--------\n"
        "str: The dnd string.\n"
        "\n"
        "Throws:\n"
        "-------\n"
        "Throws ValueError if there is a syntax error in the given string.\n"
        "Can also throw due to missing files.\n"
        "Can also throw due to errors in embedded javascript blocks.\n"
        "\n"
    },
    {
        .ml_name = "analyze_syntax_for_highlight",
        .ml_meth = (PyCFunction)pydndc_anaylze_syntax_for_highlight,
        .ml_flags = METH_VARARGS|METH_KEYWORDS,
        .ml_doc =
        #if PY_INSPECT_SUPPORTS_ANNOTATIONS
        "analyze_syntax_for_highlight(text->str) -> dict[0, tuple[SyntaxType, int, int, int]]\n"
        #else
        "analyze_syntax_for_highlight(text)\n"
        #endif
        "--\n"
        "\n"
        "Analyzes the .dnd string and returns a dictionary of syntactic regions.\n"
        "\n"
        "Args:\n"
        "-----\n"
        "text: str\n"
        "    The .dnd string.\n"
        "\n"
        "Returns:\n"
        "--------\n"
        "dict: The dict of syntactic regions.\n"
        "The dictionary is a mapping of lines (0-based) as the keys to a list, \n"
        "containing tuples of (type, col, byteoffset, length), which is \n"
        "Tuple[int, int, int, int].\n"
        "col, byteoffeset and length are all in bytes of utf-8.\n"
        "The type is one of the Syntax enum members.\n"
        ,
    },
    {
        .ml_name = "stderr_logger",
        .ml_meth = (PyCFunction)pydndc_stderr_logger,
        .ml_flags = METH_VARARGS|METH_KEYWORDS,
        .ml_doc = "stderr_logger(type, filename, line, col, message)\n"
            "--\n"
            "\n"
            "An implementation of the dndc logger protocol that just logs to stderr.\n"
    },
    {NULL, NULL, 0, NULL}
};

static
PyModuleDef pydndc = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name="pydndc",
    .m_doc= ""
        "dnd is a convenient format for writing plain text documents that can\n"
        "turn into rich dungeons for roleplaying games. It is not limited to\n"
        "roleplaying games -- it can be used as a general purpose format that\n"
        "easily exports to html, while offering powerful in-document scripting\n"
        "and convenient syntax.\n"
        ,
    .m_size=-1,
    .m_methods=pydndc_methods,
    .m_slots=NULL,
    .m_traverse=NULL,
    .m_clear=NULL,
    .m_free=NULL,
};

static
PyStructSequence_Field syntax_fields[] = {
    {"type",   "SynType. The type of the syntactic region."},
    {"column", "int: Byte offset from beginning of line."},
    {"offset", "int: Byte offset from beginning of doc."},
    {"length", "int: Byte length of region."},
    {0},
};

static
PyStructSequence_Desc syntax_desc = {
    .name = "SyntaxRegion",
    .doc = "A syntax region in the document",
    .fields = syntax_fields,
    .n_in_sequence = arrlen(syntax_fields)-1,
};
static
PyStructSequence_Field location_fields[] = {
    {"filename",   "str: source file"},
    {"row", "int: the row in the original file"},
    {"column", "int: column within the row"},
    {0},
};

static
PyStructSequence_Desc location_desc = {
    .name = "Location",
    .doc = "The source location of a node",
    .fields = location_fields,
    .n_in_sequence = arrlen(location_fields)-1,
};

static PyTypeObject *SyntaxRegion, *Location;


static inline
int
add_doc(PyObject* obj, const char* text){
    PyObject* doc = PyUnicode_FromString(text);
    if(!doc) return 1;
    PyObject_SetAttrString(obj, "__doc__", doc);
    Py_DECREF(doc);
    return 0;
}

static PyTypeObject DndcContextPyType, DndcNodePyType;

static PyObject* node_type_enum;

PyMODINIT_FUNC _Nullable
PyInit_pydndc(void){
    if(dndc_version() != DNDC_NUMERIC_VERSION){
        PyErr_SetString(PyExc_RuntimeError, "dndc version mismatch");
        return NULL;
    }
    PyObject* mod = PyModule_Create(&pydndc);
    PyObject* enu_mod = NULL;
    PyObject* intenum = NULL;
    PyObject* intflag = NULL;
    PyObject* synvalues = NULL;
    PyObject* messagevalues = NULL;
    PyObject* flagvalues = NULL;
    PyObject* ntvalues = NULL;
    PyObject* filecache_type = NULL;
    PyObject* ctx_type = NULL;
    PyObject* node_type = NULL;
    PyObject* synenum = NULL;
    PyObject* msgenum = NULL;
    PyObject* ntenum = NULL;
    PyObject* flagenum = NULL;
    PyObject* args = NULL;
    PyObject* kwargs = NULL;
    PyObject* name = NULL;
    PyObject* modname = NULL;
    PyObject* version = NULL;
    if(!mod) goto fail;
    modname = PyModule_GetNameObject(mod); // new ref
    if(!modname) goto fail;
    kwargs = PyDict_New();
    if(!kwargs) goto fail;
    if(PyDict_SetItemString(kwargs, "module", modname) < 0)
        goto fail;

    if(PyType_Ready(&DndcPyFileCache_Type) != 0)
        goto fail;
    Py_INCREF(&DndcPyFileCache_Type);
    filecache_type = (PyObject*)&DndcPyFileCache_Type;
    if(PyModule_AddObjectRef(mod, "FileCache", filecache_type) < 0)
        goto fail;

    if(PyType_Ready(&DndcContextPyType) < 0)
        return NULL;
    Py_INCREF(&DndcContextPyType);
    ctx_type = (PyObject*)&DndcContextPyType;
    if(PyModule_AddObjectRef(mod, "Context", ctx_type) < 0)
        goto fail;

    if(PyType_Ready(&DndcNodePyType) < 0)
        return NULL;
    Py_INCREF(&DndcNodePyType);
    node_type = (PyObject*)&DndcNodePyType;
    if(PyModule_AddObjectRef(mod, "Node", node_type) < 0)
        goto fail;

    PyModule_AddStringConstant(mod, "__version__",     DNDC_VERSION);
    SyntaxRegion = PyStructSequence_NewType(&syntax_desc);
    // pydoc basically shits the bed if a class doesn't have a __module__ or a __doc__.
    PyObject_SetAttrString((PyObject*)SyntaxRegion, "__module__", modname);
    if(PyModule_AddObjectRef(mod, "SyntaxRegion", (PyObject*)SyntaxRegion) < 0)
        goto fail;

    Location = PyStructSequence_NewType(&location_desc);
    // pydoc basically shits the bed if a class doesn't have a __module__ or a __doc__.
    PyObject_SetAttrString((PyObject*)Location, "__module__", modname);
    if(PyModule_AddObjectRef(mod, "Location", (PyObject*)Location) < 0)
        goto fail;

    enu_mod = PyImport_ImportModule("enum"); //new ref
    if(!enu_mod) goto fail;
    intenum = PyObject_GetAttrString(enu_mod, "IntEnum"); // new ref
    if(!intenum) goto fail;
    intflag = PyObject_GetAttrString(enu_mod, "IntFlag"); // new ref
    if(!intflag) goto fail;
    synvalues = PyDict_New();
    if(!synvalues) goto fail;
    messagevalues = PyDict_New();
    if(!messagevalues) goto fail;
    flagvalues = PyDict_New();
    if(!flagvalues) goto fail;
    ntvalues = PyDict_New();
    if(!ntvalues) goto fail;
    // syntax constants
    #define ADDSYNTAXCONSTANT(x) do { \
        PyObject* v = PyLong_FromLong(DNDC_SYNTAX_##x); \
        if(!v) goto fail; \
        if(PyDict_SetItemString(synvalues, #x, v) < 0){ \
            Py_DECREF(v); \
            goto fail; \
        } \
        Py_DECREF(v); \
    }while(0)
    ADDSYNTAXCONSTANT(DOUBLE_COLON);
    ADDSYNTAXCONSTANT(HEADER);
    ADDSYNTAXCONSTANT(NODE_TYPE);
    ADDSYNTAXCONSTANT(ATTRIBUTE);
    ADDSYNTAXCONSTANT(DIRECTIVE);
    ADDSYNTAXCONSTANT(ATTRIBUTE_ARGUMENT);
    ADDSYNTAXCONSTANT(CLASS);
    ADDSYNTAXCONSTANT(RAW_STRING);
    ADDSYNTAXCONSTANT(JS_COMMENT);
    ADDSYNTAXCONSTANT(JS_STRING);
    ADDSYNTAXCONSTANT(JS_REGEX);
    ADDSYNTAXCONSTANT(JS_NUMBER);
    ADDSYNTAXCONSTANT(JS_KEYWORD);
    ADDSYNTAXCONSTANT(JS_KEYWORD_VALUE);
    ADDSYNTAXCONSTANT(JS_VAR);
    ADDSYNTAXCONSTANT(JS_IDENTIFIER);
    ADDSYNTAXCONSTANT(JS_BUILTIN);
    ADDSYNTAXCONSTANT(JS_NODETYPE);
    ADDSYNTAXCONSTANT(JS_BRACE);
    #undef ADDSYNTAXCONSTANT
    name = PyUnicode_FromString("SynType");
    if(!name) goto fail;
    args = PyTuple_Pack(2, name, synvalues); // does not steal
    if(!args) goto fail;
    Py_DECREF(name); name = NULL;
    synenum = PyObject_Call(intenum, args, kwargs);
    Py_DECREF(args); args = NULL;
    if(!synenum) goto fail;
    if(add_doc(synenum, "The type of a syntactic region.") != 0) goto fail;
    if(PyModule_AddObjectRef(mod, "SynType", synenum) < 0)
        goto fail;

    #define ADDFLAGCONSTANT(x) do {\
        PyObject* v = PyLong_FromLong(DNDC_##x); \
        if(!v) goto fail; \
        if(PyDict_SetItemString(flagvalues, #x, v) < 0){ \
            Py_DECREF(v); \
            goto fail; \
        } \
        Py_DECREF(v); \
    }while(0)
    // flags
    ADDFLAGCONSTANT(INPUT_IS_UNTRUSTED);
    ADDFLAGCONSTANT(FRAGMENT_ONLY);
    ADDFLAGCONSTANT(DONT_INLINE_IMAGES);
    ADDFLAGCONSTANT(NO_THREADS);
    ADDFLAGCONSTANT(USE_DND_URL_SCHEME);
    ADDFLAGCONSTANT(STRIP_WHITESPACE);
    ADDFLAGCONSTANT(DONT_READ);
    ADDFLAGCONSTANT(PRINT_STATS);
    ADDFLAGCONSTANT(DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP);
    {
        PyObject* v = PyLong_FromLong(0);
        if(!v) goto fail;
        if(PyDict_SetItemString(flagvalues, "NONE", v) < 0){
            Py_DECREF(v);
            goto fail;
        }
        Py_DECREF(v);
    }
    #undef ADDFLAGCONSTANT
    name = PyUnicode_FromString("Flags");
    if(!name) goto fail;
    args = PyTuple_Pack(2, name, flagvalues); // does not steal
    if(!args) goto fail;
    Py_DECREF(name); name = NULL;
    flagenum = PyObject_Call(intflag, args, kwargs);
    Py_DECREF(args); args = NULL;
    if(!flagenum) goto fail;
    if(add_doc(flagenum, "Flags for controlling the behavior of htmlgen.") != 0)
        goto fail;
    if(PyModule_AddObjectRef(mod, "Flags", flagenum) < 0)
        goto fail;

    #define ADDMSGCONSTANT(x) do {\
        PyObject* v = PyLong_FromLong(DNDC_##x##_MESSAGE); \
        if(!v) goto fail; \
        if(PyDict_SetItemString(messagevalues, #x, v) < 0){ \
            Py_DECREF(v); \
            goto fail; \
        } \
        Py_DECREF(v); \
    }while(0)
    // error message types
    ADDMSGCONSTANT(ERROR);
    ADDMSGCONSTANT(WARNING);
    ADDMSGCONSTANT(NODELESS);
    ADDMSGCONSTANT(STATISTIC);
    ADDMSGCONSTANT(DEBUG);
    #undef ADDMSGCONSTANT
    name = PyUnicode_FromString("MsgType");
    if(!name) goto fail;
    args = PyTuple_Pack(2, name, messagevalues); // does not steal
    if(!args) goto fail;
    Py_DECREF(name); name = NULL;
    msgenum = PyObject_Call(intenum, args, kwargs);
    Py_DECREF(args); args = NULL;
    if(!msgenum) goto fail;
    if(add_doc(msgenum, "The type of a message sent to the error reporting function.") != 0)
        goto fail;
    if(PyModule_AddObjectRef(mod, "MsgType", msgenum) < 0)
        goto fail;
    PyModule_AddIntConstant(mod, "INT_VERSION", DNDC_NUMERIC_VERSION);
    version = Py_BuildValue("(iii)", DNDC_MAJOR, DNDC_MINOR, DNDC_MICRO);
    if(PyModule_AddObjectRef(mod, "version", version) < 0)
        goto fail;

    #define ADDNTCONSTANT(x) do { \
        PyObject* v = PyLong_FromLong(DNDC_NODE_TYPE_##x); \
        if(!v) goto fail; \
        if(PyDict_SetItemString(ntvalues, #x, v) < 0){ \
            Py_DECREF(v); \
            goto fail; \
        } \
        Py_DECREF(v); \
    } while(0)
    #define X(a, b) ADDNTCONSTANT(a);
    DNDCNODETYPES(X);
    #undef X
    #undef ADDNTCONSTANT
    name = PyUnicode_FromString("NodeType");
    if(!name) goto fail;
    args = PyTuple_Pack(2, name, ntvalues); // does not steal
    if(!args) goto fail;
    Py_DECREF(name); name = NULL;
    ntenum = PyObject_Call(intenum, args, kwargs);
    Py_DECREF(args); args = NULL;
    if(add_doc(ntenum, "The type of a node") != 0)
        goto fail;
    if(PyModule_AddObjectRef(mod, "NodeType", ntenum) < 0)
        goto fail;

    if(0){
        fail:
        Py_XDECREF(mod);
        mod = NULL;
    }
    Py_XDECREF(version);
    Py_XDECREF(enu_mod);
    Py_XDECREF(intenum);
    Py_XDECREF(intflag);
    Py_XDECREF(synvalues);
    Py_XDECREF(ntvalues);
    Py_XDECREF(messagevalues);
    Py_XDECREF(flagvalues);
    Py_XDECREF(filecache_type);
    Py_XDECREF(ctx_type);
    Py_XDECREF(node_type);
    Py_XDECREF(synenum);
    Py_XDECREF(msgenum);
    Py_XDECREF(flagenum);
    // Py_XDECREF(ntenum);
    node_type_enum = ntenum; // steal ref for static
    Py_XDECREF(args);
    Py_XDECREF(kwargs);
    Py_XDECREF(name);
    Py_XDECREF(modname);
    return mod;
}

static
void
pydndc_collect_errors(Nullable(void*)user_data, int type, const char* filename, int filename_len, int line, int col, const char* message, int message_len){
    PyObject* tup = Py_BuildValue("is#iis#", type, filename, (Py_ssize_t)filename_len, line, col, message, (Py_ssize_t)message_len);
    if(!tup)
        return;
    PyObject* list = user_data;
    int fail = PyList_Append(list, tup);
    (void)fail;
    Py_DECREF(tup);
}

static
PyObject*_Nullable
pydndc_stderr_logger(PyObject* mod, PyObject* args, PyObject* kwargs){
    (void)mod;
    const char* const keywords[] = { "type", "filename", "line", "col", "message", NULL};
    int type, line, col;
    PyObject* filename, *message;
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "iO!iiO!|:stderr_logger", (char**)keywords, &type, &PyUnicode_Type, &filename, &line, &col, &PyUnicode_Type, &message)){
        return NULL;
    }
    PopDiagnostic();
    LongString mess = pystring_borrow_longstring(message);
    StringView fn = pystring_borrow_stringview(filename);
    dndc_stderr_log_func(NULL, type, fn.text, fn.length, line, col, mess.text, mess.length);
    Py_RETURN_NONE;
}

static
Nullable(PyObject*)
pydndc_reformat(PyObject* mod, PyObject* args, PyObject* kwargs){
    (void)mod;
    PyObject* text;
    PyObject* logger = NULL;
    const char* const keywords[] = { "text", "logger", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O:reformat", (char**)keywords, &PyUnicode_Type, &text, &logger)){
        return NULL;
    }
    PopDiagnostic();
    if(logger && logger == Py_None)
        logger = NULL;
    if(logger && !PyCallable_Check(logger)){
        PyErr_SetString(PyExc_TypeError, "logger must be a callable");
        return NULL;
    }
    StringView source = pystring_borrow_stringview(text);
    LongString output = {};
    DndcLogFunc* func = logger?pydndc_collect_errors:NULL;
    PyObject* error_list = func? PyList_New(0) : NULL;
    PyObject* result = NULL;
    int e = dndc_format(source, &output, func, error_list);
    if(PyErr_Occurred()){
        goto finally;
    }
    if(logger){
        Py_ssize_t length = PyList_Size(error_list);
        for(Py_ssize_t i = 0; i < length; i++){
            PyObject* list_item = PyList_GetItem(error_list, i);
            PyObject* call_result = PyObject_Call(logger, list_item, NULL);
            if(call_result == NULL)
                goto finally;
            Py_XDECREF(call_result);
        }
    }
    if(e){
        PyErr_SetString(PyExc_ValueError, "Format error.");
        goto finally;
    }
    result = PyUnicode_FromStringAndSize(output.text, output.length);
    finally:
    Py_XDECREF(error_list);
    dndc_free_string(output);
    return result;
}

static
int
pydndc_add_dependencies(Nullable(void*)user_data, size_t npaths, StringView* paths){
    PyObject* list = user_data;
    for(size_t i = 0; i < npaths; i++){
        StringView path = paths[i];
        PyObject* str = PyUnicode_FromStringAndSize(path.text, path.length);
        PyList_Append(list, str);
        Py_XDECREF(str);
    }
    return 0;
}

static
Nullable(PyObject*)
pydndc_htmlgen(PyObject* mod, PyObject* args, PyObject* kwargs){
    (void)mod;
    PyObject* text;
    PyObject* base_dir = NULL;
    PyObject* logger = NULL;
    PyObject* file_cache = NULL;
    PyObject* jsargs = NULL;
    PyObject* filename = NULL;
    unsigned long long flags = 0;
    _Static_assert(sizeof(flags) == sizeof(uint64_t), "");
    enum {WHITELIST = 0
        | DNDC_INPUT_IS_UNTRUSTED
        | DNDC_FRAGMENT_ONLY
        | DNDC_DONT_INLINE_IMAGES
        | DNDC_NO_THREADS
        | DNDC_USE_DND_URL_SCHEME
        | DNDC_STRIP_WHITESPACE
        | DNDC_DONT_READ
        | DNDC_PRINT_STATS
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
    };
    const char* const keywords[] = {"text", "base_dir", "filename", "logger", "file_cache", "flags", "jsargs", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O!O!OOKO:htmlgen", (char**)keywords, &PyUnicode_Type, &text, &PyUnicode_Type, &base_dir, &PyUnicode_Type, &filename, &logger, &file_cache, &flags, &jsargs)){
        return NULL;
    }
    PopDiagnostic();
    // Check for any flags that python callers shouldn't be able to set.
    if((flags & WHITELIST) != flags){
        PyErr_SetString(PyExc_ValueError, "flags argument contains illegal bits");
        return NULL;
    }
    if(logger && logger == Py_None)
        logger = NULL;
    if(logger && !PyCallable_Check(logger)){
        PyErr_SetString(PyExc_TypeError, "logger must be a callable");
        return NULL;
    }
    if(file_cache && file_cache == Py_None)
        file_cache = NULL;
    if(file_cache && !PyObject_IsInstance(file_cache, (PyObject*)&DndcPyFileCache_Type)){
        PyErr_SetString(PyExc_TypeError, "file_cache must be a DndcFileCache");
        return NULL;
    }
    LongString jsargs_ls = LS("");
    MStringBuilder jsbuilder = {.allocator = get_mallocator()};
    if(jsargs && PyUnicode_Check(jsargs)){
        jsargs_ls = pystring_borrow_longstring(jsargs);
    }
    else if(jsargs){
        if(pyobj_to_json(jsargs, &jsbuilder, 0) != 0){
            if(jsbuilder.capacity){
                msb_destroy(&jsbuilder);
            }
            return NULL;
        }
        jsargs_ls = msb_borrow_ls(&jsbuilder);
    }
    StringView source = pystring_borrow_stringview(text);
    StringView base_str = base_dir? pystring_borrow_stringview(base_dir): SV("");
    // flags |= DNDC_DONT_PRINT_ERRORS;
    // flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    LongString output = {};
    DndcLogFunc* func = logger?pydndc_collect_errors:NULL;
    PyObject* error_list = func? PyList_New(0) : NULL;
    PyObject* result = NULL;
    PyObject* depends_list = PyList_New(0);
    DndcFileCache* textcache = NULL;
    DndcFileCache* b64cache = NULL;
    if(file_cache){
        DndcPyFileCache* cache = (DndcPyFileCache*)file_cache;
        textcache = cache->text_cache;
        b64cache = cache->b64_cache;
    }
    StringView source_path = filename?pystring_borrow_stringview(filename): SV("(string input)");
    int e = dndc_compile_dnd_file(flags, base_str, source, source_path, &output, b64cache, textcache, func, error_list, pydndc_add_dependencies, depends_list, NULL, jsargs_ls);
    if(PyErr_Occurred()){
        result = NULL;
        goto finally;
    }
    if(logger){
        Py_ssize_t length = PyList_Size(error_list);
        for(Py_ssize_t i = 0; i < length; i++){
            PyObject* list_item = PyList_GetItem(error_list, i);
            PyObject* call_result = PyObject_Call(logger, list_item, NULL);
            if(call_result == NULL)
                goto finally;
            Py_XDECREF(call_result);
        }
    }
    if(e){
        PyErr_SetString(PyExc_ValueError, "html error.");
        goto finally;
    }
    result = Py_BuildValue("s#O", output.text, (Py_ssize_t)output.length, depends_list);
    finally:
    Py_XDECREF(depends_list);
    Py_XDECREF(error_list);
    dndc_free_string(output);
    if(jsbuilder.capacity){
        msb_destroy(&jsbuilder);
    }
    return result;
}
static
Nullable(PyObject*)
pydndc_expand(PyObject* mod, PyObject* args, PyObject* kwargs){
    (void)mod;
    PyObject* text;
    PyObject* base_dir = NULL;
    PyObject* logger = NULL;
    PyObject* file_cache = NULL;
    PyObject* jsargs = NULL;
    unsigned long long flags = 0;
    _Static_assert(sizeof(flags) == sizeof(uint64_t), "");
    enum {WHITELIST = 0
        | DNDC_INPUT_IS_UNTRUSTED
        | DNDC_DONT_READ
        | DNDC_PRINT_STATS
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
    };
    const char* const keywords[] = {"text", "base_dir", "logger", "file_cache", "flags", "jsargs", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O!OOKO:expand", (char**)keywords, &PyUnicode_Type, &text, &PyUnicode_Type, &base_dir, &logger, &file_cache, &flags, &jsargs)){
        return NULL;
    }
    PopDiagnostic();
    // Check for any flags that python callers shouldn't be able to set.
    if((flags & WHITELIST) != flags){
        PyErr_SetString(PyExc_ValueError, "flags argument contains illegal bits");
        return NULL;
    }
    if(logger && logger == Py_None)
        logger = NULL;
    if(logger && !PyCallable_Check(logger)){
        PyErr_SetString(PyExc_TypeError, "logger must be a callable");
        return NULL;
    }
    if(file_cache && file_cache == Py_None)
        file_cache = NULL;
    if(file_cache && !PyObject_IsInstance(file_cache, (PyObject*)&DndcPyFileCache_Type)){
        PyErr_SetString(PyExc_TypeError, "file_cache must be a DndcFileCache");
        return NULL;
    }
    if(jsargs && !PyUnicode_Check(jsargs)){
        PyErr_SetString(PyExc_TypeError, "jsargs must be a str");
        return NULL;
    }
    LongString jsargs_ls = jsargs? pystring_borrow_longstring(jsargs) : LS("");
    StringView source = pystring_borrow_stringview(text);
    StringView base_str = base_dir? pystring_borrow_stringview(base_dir): SV("");
    // flags |= DNDC_DONT_PRINT_ERRORS;
    // flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_OUTPUT_EXPANDED_DND;
    flags |= DNDC_ALLOW_BAD_LINKS;
    LongString output = {};
    DndcLogFunc* func = logger?pydndc_collect_errors:NULL;
    PyObject* error_list = func? PyList_New(0) : NULL;
    PyObject* result = NULL;
    DndcFileCache* textcache = NULL;
    DndcFileCache* b64cache = NULL;
    if(file_cache){
        DndcPyFileCache* cache = (DndcPyFileCache*)file_cache;
        textcache = cache->text_cache;
        b64cache = cache->b64_cache;
    }
    int e = dndc_compile_dnd_file(flags, base_str, source, SV(""), &output, b64cache, textcache, func, error_list, NULL, NULL, NULL, jsargs_ls);
    if(PyErr_Occurred()){
        result = NULL;
        goto finally;
    }
    if(logger){
        Py_ssize_t length = PyList_Size(error_list);
        for(Py_ssize_t i = 0; i < length; i++){
            PyObject* list_item = PyList_GetItem(error_list, i);
            PyObject* call_result = PyObject_Call(logger, list_item, NULL);
            if(call_result == NULL)
                goto finally;
            Py_XDECREF(call_result);
        }
    }
    if(e){
        PyErr_SetString(PyExc_ValueError, "html error.");
        goto finally;
    }
    result = Py_BuildValue("s#", output.text, (Py_ssize_t)output.length);
    finally:
    Py_XDECREF(error_list);
    dndc_free_string(output);
    return result;
}

struct CollectData {
    const char* begin;
    PyObject* dict;
};

static
void
pydndc_collect_syntax_tokens(Nullable(void*)user_data, int type, int line, int col, const char* begin, size_t length){
    if(PyErr_Occurred())
        return;
    assert(user_data);
    struct CollectData* cd = user_data;
    PyObject* d = cd->dict;
    PyObject* key = PyLong_FromLong(line);
    PyObject* value = PyStructSequence_New(SyntaxRegion);
    PyStructSequence_SET_ITEM(value, 0, PyLong_FromLong(type));
    PyStructSequence_SET_ITEM(value, 1, PyLong_FromLong(col));
    PyStructSequence_SET_ITEM(value, 2, PyLong_FromSsize_t(begin - cd->begin));
    PyStructSequence_SET_ITEM(value, 3, PyLong_FromSize_t(length));
    // PyObject* value = Py_BuildValue("iinn", type, col, (Py_ssize_t)(begin - cd->begin), (Py_ssize_t)length);
    if(!key) goto Lfail;
    if(!value) goto Lfail;
    PyObject * list;
    if(PyDict_Contains(d, key)){
        list = PyDict_GetItem(d, key); // borrow
        assert(list);
    }
    else {
        list = PyList_New(0);
        if(!list) goto Lfail;
        int fail = PyDict_SetItem(d, key, list); // does not steal
        // list is kept alive by the dict.
        Py_XDECREF(list);
        if(fail) goto Lfail;
    }
    int fail = PyList_Append(list, value);
    (void)fail;
    Lfail:
    Py_XDECREF(key);
    Py_XDECREF(value);
}

static
Nullable(PyObject*)
pydndc_anaylze_syntax_for_highlight(PyObject* mod, PyObject* args, PyObject* kwargs){
    (void)mod;
    PyObject* text;
    const char* const keywords[] = {"text", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!:analyze_syntax_for_highlight", (char**)keywords, &PyUnicode_Type, &text)){
        return NULL;
    }
    PopDiagnostic();
    StringView source = pystring_borrow_stringview(text);
    struct CollectData cd = {
        .dict = PyDict_New(),
        .begin = source.text,
    };
    if(!cd.dict)
        return NULL;
    int error = dndc_analyze_syntax(source, pydndc_collect_syntax_tokens, &cd);
    if(PyErr_Occurred()){
        Py_XDECREF(cd.dict);
        return NULL;
    }
    if(error){
        PyErr_SetString(PyExc_RuntimeError, "Unknown error while collecting tokens");
        Py_XDECREF(cd.dict);
        return NULL;
    }
    if(PyErr_Occurred()){
        Py_XDECREF(cd.dict);
        return NULL;
    }
    return cd.dict;
}

static
int
pyobj_to_json(PyObject* o, MStringBuilder* msb, int depth){
    if(depth > 10){
        PyErr_SetString(PyExc_ValueError, "Overly nested data structure is not allowed for js vars. Depth exceeds 10");
        return 1;
    }
    int result = 0;
    if(PyUnicode_Check(o)){
        msb_write_char(msb, '"');
        StringView sv = pystring_borrow_stringview(o);
        msb_write_json_escaped_str(msb, sv.text, sv.length);
        msb_write_char(msb, '"');
        return 0;
    }
    if(o == Py_None){
        msb_write_literal(msb, "null");
        return 0;
    }
    if(o == Py_True){
        msb_write_literal(msb, "true");
        return 0;
    }
    if(o == Py_False){
        msb_write_literal(msb, "false");
        return 0;
    }
    if(sizeof(long long) == sizeof(uint64_t)){
        if(PyLong_Check(o)){
            long long val = PyLong_AsLongLong(o);
            msb_write_int64(msb, val);
            return 0;
        }
    }
    if(PySequence_Check(o)){
        PyObject* seq = PySequence_Fast(o, "Expected a fast sequence"); // new reference
        if(!seq) return 1;
        Py_ssize_t length = PySequence_Fast_GET_SIZE(seq);
        msb_write_char(msb, '[');
        for(Py_ssize_t i = 0; i < length; i++){
            if(i != 0) msb_write_char(msb, ',');
            PyObject* item = PySequence_Fast_GET_ITEM(seq, i); // borrowed ref
            if(pyobj_to_json(item, msb, depth+1) != 0){
                result = 1;
                goto finish_seq;
            }
        }
        msb_write_char(msb, ']');
        finish_seq:
        Py_XDECREF(seq);
        return result;
    }
    if(PyMapping_Check(o)){
        PyObject* items = PyMapping_Items(o); // new reference
        if(!items) return 1;
        Py_ssize_t length = PyList_GET_SIZE(items);
        msb_write_char(msb, '{');
        for(Py_ssize_t i = 0; i < length; i++){
            if(i != 0) msb_write_char(msb, ',');
            PyObject* item = PyList_GET_ITEM(items, i); // borrowed item
            PyObject* key = PyTuple_GET_ITEM(item, 0); // borrowed reference
            PyObject* value = PyTuple_GET_ITEM(item, 1); // borrowed reference
            msb_write_char(msb, '"');
            if(PyUnicode_Check(key)){
                StringView sv = pystring_borrow_stringview(key);
                msb_write_json_escaped_str(msb, sv.text, sv.length);
            }
            else {
                PyObject* rkey = PyObject_Repr(key); // new reference
                if(!rkey){
                    result = 1;
                    goto finish_dict;
                }
                StringView sv = pystring_borrow_stringview(rkey);
                msb_write_json_escaped_str(msb, sv.text, sv.length);
                Py_XDECREF(rkey);
            }
            msb_write_literal(msb, "\":");
            if(pyobj_to_json(value, msb, depth+1) != 0){
                result = 1;
                goto finish_dict;
            }
        }
        msb_write_char(msb, '}');
        finish_dict:
        Py_XDECREF(items);
        return result;
    }
    PyObject* r = PyObject_Repr(o); // new ref
    if(!r) return 1;
    StringView sv = pystring_borrow_stringview(r);
    msb_write_str(msb, sv.text, sv.length);
    // finish_arbitrary:
    Py_XDECREF(r);
    return result;
}

typedef struct {
    PyObject_HEAD
    PyObject* logger;
    DndcContext* ctx;
    PyObject* _Nullable filename;
    PyObject* prev;
} DndcContextPy;

static
PyObject* _Nullable
DndcContextPy_new(PyTypeObject* type, PyObject* args, PyObject* kwargs){
    PyObject* flags = NULL;
    PyObject* filename = NULL;
    DndcPyFileCache * cache=NULL;
    const char* const keywords[] = {"flags", "filename", "filecache", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "|O!O!O!:Context", (char**)keywords, &PyLong_Type, &flags, &PyUnicode_Type, &filename, &DndcPyFileCache_Type, &cache)){
        return NULL;
    }
    PopDiagnostic();
    DndcContextPy* self = (DndcContextPy*)type->tp_alloc(type, 0);
    if(!self) return NULL;
    unsigned long long fl = flags?PyLong_AsUnsignedLongLong(flags):0;
    enum {WHITELIST = 0
        | DNDC_INPUT_IS_UNTRUSTED
        | DNDC_FRAGMENT_ONLY
        | DNDC_DONT_INLINE_IMAGES
        | DNDC_NO_THREADS
        | DNDC_USE_DND_URL_SCHEME
        | DNDC_STRIP_WHITESPACE
        | DNDC_DONT_READ
        | DNDC_PRINT_STATS
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
    };
    fl &= WHITELIST;
    self->ctx = dndc_create_ctx(fl | DNDC_ALLOW_BAD_LINKS, cache?cache->b64_cache:NULL, cache?cache->text_cache:NULL);
    dndc_ctx_set_logger(self->ctx, pylogger, &self->logger);
    self->filename = filename;
    if(filename) Py_INCREF(filename);
    self->prev = NULL;
    return (PyObject*)self;
}

static PyMemberDef DndcContextPy_members[] = {
    {"logger", T_OBJECT, offsetof(DndcContextPy, logger), 0, "A callable implementing the dndc logger protocol."},
    {"filename", T_OBJECT, offsetof(DndcContextPy, filename), READONLY, "The filename of the root document."},
    {}  /* Sentinel */
};

static PyObject* DndcNode_make(DndcContextPy*, DndcNodeHandle);

static
PyObject*_Nullable
DndcContextPy_node_from_int(PyObject* s, PyObject* arg){
    if(!PyLong_Check(arg))
        return PyErr_Format(PyExc_TypeError, "node_from_int takes an int");
    long id = PyLong_AsLong(arg);
    DndcContextPy* self = (DndcContextPy*)s;
    if(dndc_ctx_node_invalid(self->ctx, id)){
        return PyErr_Format(PyExc_ValueError, "%R is an invalid node id", arg);
    }
    return DndcNode_make(self, id);
}

static
PyObject*_Nullable
DndcContextPy_node_by_id(PyObject* s, PyObject* arg){
    if(!PyUnicode_Check(arg))
        return PyErr_Format(PyExc_TypeError, "node_by_id takes a str");
    DndcStringView sv = pystring_borrow_stringview(arg);
    DndcContextPy* self = (DndcContextPy*)s;
    DndcNodeHandle id = dndc_ctx_node_by_id(self->ctx, sv);
    if(id == DNDC_NODE_HANDLE_INVALID)
        Py_RETURN_NONE;
    return DndcNode_make(self, id);
}

static
PyObject*_Nullable
DndcContextPy_format_tree(PyObject* s, PyObject*_Nullable args){
    (void)args;
    DndcContextPy* self = (DndcContextPy*)s;
    DndcLongString ls;
    int err = dndc_ctx_format_tree(self->ctx, &ls);
    if(err)
        return PyErr_Format(PyExc_ValueError, "Tree can't be formatted");
    PyObject* result = PyUnicode_FromStringAndSize(ls.text, ls.length);
    dndc_free_string(ls);
    return result;
}

static
PyObject*_Nullable
DndcContextPy_expand(PyObject* s, PyObject*_Nullable args){
    (void)args;
    DndcContextPy* self = (DndcContextPy*)s;
    DndcLongString ls;
    int err = dndc_ctx_expand_to_dnd(self->ctx, &ls);
    if(err)
        return PyErr_Format(PyExc_ValueError, "Tree can't be expanded");
    PyObject* result = PyUnicode_FromStringAndSize(ls.text, ls.length);
    dndc_free_string(ls);
    return result;
}

static
PyObject*_Nullable
DndcContextPy_render(PyObject* s, PyObject*_Nullable args){
    (void)args;
    DndcContextPy* self = (DndcContextPy*)s;
    DndcLongString ls;
    int err = dndc_ctx_render_to_html(self->ctx, &ls);
    if(err)
        return PyErr_Format(PyExc_ValueError, "Tree can't be rendered");
    PyObject* result = PyUnicode_FromStringAndSize(ls.text, ls.length);
    dndc_free_string(ls);
    return result;
}

static
PyObject*_Nullable
DndcContextPy_make_node(PyObject* s, PyObject* args, PyObject* kwargs){
    PyObject* type;
    PyObject *header = NULL;
    DndcContextPy* self = (DndcContextPy*)s;
    const char* const keywords[] = {"type", "header", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O!:make_node", (char**)keywords, &type, &PyUnicode_Type, &header)){
        return NULL;
    }
    PopDiagnostic();
    if(!PyLong_Check(type))
        return PyErr_Format(PyExc_TypeError, "Type must be integral");
    StringView h = header?dndc_ctx_dup_sv(self->ctx, pystring_borrow_stringview(header)): SV("");
    DndcNodeHandle n = dndc_ctx_make_node(self->ctx, PyLong_AsLong(type), h, DNDC_NODE_HANDLE_INVALID);
    if(n == DNDC_NODE_HANDLE_INVALID)
        return header?PyErr_Format(PyExc_ValueError, "Unable to make a node with type: %R, header: %R", type, header):PyErr_Format(PyExc_ValueError, "Unable to make a node with type: %R", type);

    return DndcNode_make(self, n);
}

static
PyObject* _Nullable
DndcContextPy_resolve_imports(PyObject* s, PyObject* arg){
    (void)arg;
    DndcContextPy* self = (DndcContextPy*)s;
    int err = dndc_ctx_resolve_imports(self->ctx);
    if(err)
        return PyErr_Format(PyExc_RuntimeError, "Bad imports (Check the errors).");
    Py_RETURN_NONE;
}

static
PyObject* _Nullable
DndcContextPy_execute_js(PyObject* s, PyObject* args, PyObject* kwargs){
    PyObject *jsargs = NULL;
    DndcContextPy* self = (DndcContextPy*)s;
    const char* const keywords[] = {"jsargs", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "|O:execute_js", (char**)keywords, &jsargs)){
        return NULL;
    }
    PopDiagnostic();
    DndcLongString jsargs_ls = LS("");

    MStringBuilder jsbuilder = {.allocator=get_mallocator()};

    if(jsargs && PyUnicode_Check(jsargs)){
        jsargs_ls = pystring_borrow_longstring(jsargs);
    }
    else if(jsargs){
        if(pyobj_to_json(jsargs, &jsbuilder, 0) != 0){
            if(jsbuilder.capacity){
                msb_destroy(&jsbuilder);
            }
            return NULL;
        }
        jsargs_ls = msb_borrow_ls(&jsbuilder);
    }
    int err = dndc_ctx_execute_js(self->ctx, jsargs_ls);
    msb_destroy(&jsbuilder);
    if(err) return PyErr_Format(PyExc_RuntimeError, "Bad js block execution (Check the errors).");
    Py_RETURN_NONE;
}

static
PyObject* _Nullable
DndcContextPy_gather_links(PyObject* s, PyObject* arg){
    (void)arg;
    DndcContextPy* self = (DndcContextPy*)s;
    int err = dndc_ctx_gather_links(self->ctx);
    if(err)
        return PyErr_Format(PyExc_RuntimeError, "Bad imports (Check the errors).");
    Py_RETURN_NONE;
}

static
PyObject* _Nullable
DndcContextPy_resolve_links(PyObject* s, PyObject* arg){
    (void)arg;
    DndcContextPy* self = (DndcContextPy*)s;
    int err = dndc_ctx_resolve_links(self->ctx);
    if(err)
        return PyErr_Format(PyExc_RuntimeError, "Bad imports (Check the errors).");
    Py_RETURN_NONE;
}

static
PyObject* _Nullable
DndcContextPy_build_toc(PyObject* s, PyObject* arg){
    (void)arg;
    DndcContextPy* self = (DndcContextPy*)s;
    int err = dndc_ctx_build_toc(self->ctx);
    if(err)
        return PyErr_Format(PyExc_RuntimeError, "Bad imports (Check the errors).");
    Py_RETURN_NONE;
}

static
PyObject*_Nullable
DndcContextPy_select_nodes(PyObject* s, PyObject* args, PyObject* kwargs){
    DndcContextPy* self = (DndcContextPy*)s;
    DndcContext* ctx = self->ctx;
    PyObject *nt=NULL, *attrs=NULL, *klasses=NULL;
    PyObject* fastklasses=NULL, *fastattrs=NULL;
    PyObject* result = NULL;
    size_t cls_count = 0;
    size_t attr_count = 0;
    DndcStringView *classes = NULL, *attributes = NULL;
    Allocator allocator = get_mallocator();
    const char* const keywords[] = {"type", "attributes", "classes", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "|OOO:select_nodes", (char**)keywords, &nt, &attrs, &klasses)){
        return NULL;
    }
    PopDiagnostic();
    if(attrs == Py_None)
        attrs = NULL;
    if(attrs){
        fastattrs = PySequence_Fast(attrs, "attributes must be a sequence of strings"); // new ref
        if(!fastattrs) goto fail;
    }
    if(klasses == Py_None)
        klasses = NULL;
    if(klasses){
        fastklasses = PySequence_Fast(klasses, "classes must be a sequence of strings"); // new ref
        if(!fastklasses) goto fail;
    }
    if(nt == Py_None)
        nt = NULL;
    if(nt && !PyLong_Check(nt)) {
        result = PyErr_Format(PyExc_TypeError, "type must be a NodeType, got %R", nt);
        goto fail;
    }
    long type = DNDC_NODE_TYPE_INVALID;
    if(nt)
        type = PyLong_AsLong(nt);
    if(type < 0 || type > DNDC_NODE_TYPE_INVALID){
        result = PyErr_Format(PyExc_ValueError, "value of type must fit within the range of NodeType, got %R", nt);
        goto fail;
    }
    if(fastklasses) cls_count = PySequence_Size(fastklasses);
    if(fastattrs) attr_count = PySequence_Size(fastattrs);
    if(cls_count)
        classes = Allocator_alloc(allocator, sizeof(*classes)*cls_count);
    if(attr_count)
        attributes = Allocator_alloc(allocator, sizeof(*attributes)*attr_count);
    for(size_t i = 0; i < cls_count; i++){
        PyObject* item = PySequence_GetItem(fastklasses, i); // new ref
        if(!PyUnicode_Check(item)){
            result = PyErr_Format(PyExc_TypeError, "classes must be strings, got %R", item);
            Py_DECREF(item);
            goto fail;
        }
        classes[i] = pystring_borrow_stringview(item);
        Py_DECREF(item);
    }
    for(size_t i = 0; i < attr_count; i++){
        PyObject* item = PySequence_GetItem(fastattrs, i); // new ref
        if(!PyUnicode_Check(item)){
            result = PyErr_Format(PyExc_TypeError, "attributes must be strings, got %R", item);
            Py_DECREF(item);
            goto fail;
        }
        attributes[i] = pystring_borrow_stringview(item);
        Py_DECREF(item);
    }
    result = PyList_New(0);
    DndcNodeHandle buff[1024];
    size_t cookie = 0;
    size_t n_nodes;
    while((n_nodes = dndc_ctx_select_nodes(ctx, &cookie, type, attributes, attr_count, classes, cls_count, buff, arrlen(buff)))){
        for(size_t i = 0; i < n_nodes; i++){
            PyObject* n = DndcNode_make(self, buff[i]); // new ref
            int err = PyList_Append(result, n);
            Py_XDECREF(n);
            if(err){
                Py_XDECREF(result);
                result = NULL;
                goto fail;
            }
        }
    }

    fail:
    Py_XDECREF(fastklasses);
    Py_XDECREF(fastattrs);
    if(classes)
        Allocator_free(allocator, classes, sizeof(*classes)*cls_count);
    if(attributes)
        Allocator_free(allocator, attributes, sizeof(*attributes)*attr_count);
    return result;
}

static
PyObject* _Nullable
DndcContextPy_pseudo_clone(PyObject* s, PyObject* args){
    (void)args;
    DndcContextPy* self = (DndcContextPy*)s;
    DndcContext* ctx = self->ctx;
    DndcContext* newctx = dndc_ctx_shallow_clone(ctx);

    DndcContextPy* newself = (DndcContextPy*)self->ob_base.ob_type->tp_alloc(self->ob_base.ob_type, 0);
    if(!newself) return NULL;
    newself->logger = Py_XNewRef(self->logger);
    dndc_ctx_set_logger(newctx, pylogger, &newself->logger);
    newself->ctx = newctx;
    newself->filename = self->filename;
    newself->prev = s;
    Py_XINCREF(newself->prev);
    Py_XINCREF(newself->filename);
    return (PyObject*)newself;
}

static
PyObject* _Nullable
DndcContextPy_clone(PyObject* s, PyObject* args){
    (void)args;
    DndcContextPy* self = (DndcContextPy*)s;
    DndcContext* ctx = self->ctx;
    DndcContext* newctx = dndc_ctx_clone(ctx);

    DndcContextPy* newself = (DndcContextPy*)self->ob_base.ob_type->tp_alloc(self->ob_base.ob_type, 0);
    if(!newself) return NULL;
    newself->logger = Py_XNewRef(self->logger);
    dndc_ctx_set_logger(newctx, pylogger, &newself->logger);
    newself->ctx = newctx;
    newself->filename = self->filename;
    newself->prev = NULL;
    Py_XINCREF(newself->filename);
    return (PyObject*)newself;
}

static
PyObject* _Nullable
DndcContextPy_add_link(PyObject* s, PyObject* args, PyObject* kwargs){
    DndcContextPy* self = (DndcContextPy*)s;
    DndcContext* ctx = self->ctx;
    PyObject* key, * value;
    const char* const keywords[] = {"key", "value", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "UU|:add_link", (char**)keywords, &key, &value)){
        return NULL;
    }
    PopDiagnostic();
    StringView k = pystring_borrow_stringview(key);
    StringView v = pystring_borrow_stringview(value);
    int e  = dndc_ctx_add_link(ctx, k, v);
    if(e) return PyErr_Format(PyExc_ValueError, "Invalid link pair: %R, %R", key, value);
    Py_RETURN_NONE;
}

DNDC_API
int
dndc_ctx_to_json(DndcContext* ctx, DndcLongString*out);
static
PyObject* _Nullable
DndcContextPy_to_json(PyObject* s, PyObject* arg){
    (void)arg;
    DndcContextPy* self = (DndcContextPy*)s;
    LongString string;
    int err = dndc_ctx_to_json(self->ctx, &string);
    if(err) return PyErr_Format(PyExc_ValueError, "Problem converting to json.");
    PyObject* result = PyUnicode_FromStringAndSize(string.text, string.length);
    dndc_free_string(string);
    return result;
}


static PyMethodDef DndcContextPy_methods[] = {
    {
        .ml_name="node_by_id",
        .ml_meth=DndcContextPy_node_by_id,
        .ml_flags=METH_O,
        .ml_doc=
            #if PY_INSPECT_SUPPORTS_ANNOTATIONS
            "node_by_id(self, id:str) -> Optional[Node]\n"
            #else
            "node_by_id(self, id)\n"
            #endif
            "--\n"
            "\n"
            "Gets a node by its string id.\n",
    },
    {
        .ml_name="node_from_int",
        .ml_meth=DndcContextPy_node_from_int,
        .ml_flags=METH_O,
        .ml_doc="node_from_int(self, handle)\n"
            "--\n"
            "\n"
            "Creates a node from its internal ID or None if invalid",
    },
    {
        .ml_name="format_tree",
        .ml_meth=DndcContextPy_format_tree,
        .ml_flags=METH_NOARGS,
        .ml_doc="format_tree(self)\n"
            "--\n"
            "\n"
            "Formats from the root node to .dnd",
    },
    {
        .ml_name="expand",
        .ml_meth=DndcContextPy_expand,
        .ml_flags=METH_NOARGS,
        .ml_doc="expand(self)\n"
            "--\n"
            "\n"
            "Renders the tree into a .dnd string.\n"
            "This method can call the logger.\n",
    },
    {
        .ml_name="render",
        .ml_meth=DndcContextPy_render,
        .ml_flags=METH_NOARGS,
        .ml_doc="render(self)\n"
            "--\n"
            "\n"
            "Renders the tree from the root into an html document.\n"
            "This method can call the logger."
            ,
    },
    {
        .ml_name="make_node",
        .ml_meth=(PyCFunction)DndcContextPy_make_node,
        .ml_flags=METH_VARARGS|METH_KEYWORDS,
        .ml_doc="make_node(self, type, header=None)\n"
            "--\n"
            "\n"
            "Makes a node of the given type, optionally initialized with the\n"
            "header.",
    },
    {
        .ml_name="resolve_imports",
        .ml_meth=DndcContextPy_resolve_imports,
        .ml_flags=METH_NOARGS,
        .ml_doc="resolve_imports(self)\n"
            "--\n"
            "\n"
            "Iterates over all of the IMPORT nodes or nodes marked with the\n"
            "import_ flag and reads/parses the files they point to into the\n"
            "document appropriately.\n"
            "This method can call the logger.\n",
    },
    {
        .ml_name="execute_js",
        .ml_meth=(PyCFunction)DndcContextPy_execute_js,
        .ml_flags=METH_VARARGS|METH_KEYWORDS,
        .ml_doc="execute_js(self, jsargs='null')\n"
            "--\n"
            "\n"
            "Executes all of the JS nodes in the tree, with the given jsargs\n"
            "available to them as Args.\n"
            "This method can call the logger.\n",
    },
    {
        .ml_name="gather_links",
        .ml_meth=DndcContextPy_gather_links,
        .ml_flags=METH_NOARGS,
        .ml_doc="gather_links(self)\n"
            "--\n"
            "\n"
            "Walks the tree, noting the internal link targets.",
    },
    {
        .ml_name="resolve_links",
        .ml_meth=DndcContextPy_resolve_links,
        .ml_flags=METH_NOARGS,
        .ml_doc="resolve_links(self)\n"
            "--\n"
            "\n"
            "Might be removed in the future. Gets even more link targets.\n"
            "This method can call the logger.\n",
    },
    {
        .ml_name="build_toc",
        .ml_meth=DndcContextPy_build_toc,
        .ml_flags=METH_NOARGS,
        .ml_doc="build_toc(self)\n"
            "--\n"
            "\n"
            "Builds the TOC node if there is one.",
    },
    {
        .ml_name="select_nodes",
        .ml_meth=(PyCFunction)DndcContextPy_select_nodes,
        .ml_flags=METH_VARARGS|METH_KEYWORDS,
        .ml_doc="select_nodes(self, type=None, attributes=None, classes=None)\n"
            "--\n"
            "\n"
            "Selects nodes from the document, matching the various criteria.\n"
            "Any that are None is skipped. All criteria must match.",
    },
    {
        .ml_name="clone",
        .ml_meth=DndcContextPy_clone,
        .ml_flags=METH_NOARGS,
        .ml_doc="clone(self)\n"
            "--\n"
            "\n"
            "Deep clone the context.",
    },
    {
        .ml_name="pseudo_clone",
        .ml_meth=DndcContextPy_pseudo_clone,
        .ml_flags=METH_NOARGS,
        .ml_doc="pseudo_clone(self)\n"
            "--\n"
            "\n"
            "Clones the context, but reuses safe resources with this context.\n"
            "Returns a reference to this context, preventing its deallocation\n"
            "until it is safe to do so.",
    },
    {
        .ml_name="add_link",
        .ml_meth=(PyCFunction)DndcContextPy_add_link,
        .ml_flags=METH_VARARGS|METH_KEYWORDS,
        .ml_doc="add_link(self, key, value)\n"
            "--\n"
            "\n"
            "Adds a link target to the doc (for use in [] links).",
    },
    {
        .ml_name="_to_json",
        .ml_meth=DndcContextPy_to_json,
        .ml_flags=METH_NOARGS,
        .ml_doc="_to_json(self)\n"
            "--\n"
            "\n"
            "Convert context to a json string.",
    },
    {} /* Sentinel */
};


static
PyObject *_Nullable
DndcContextPy_get_root(PyObject *s, void *_Nullable p){
    (void)p;
    DndcContextPy* self = (DndcContextPy*)s;
    DndcContext* ctx = self->ctx;
    DndcNodeHandle handle = dndc_ctx_get_root(ctx);
    if(handle == DNDC_NODE_HANDLE_INVALID){
        PushDiagnostic(); SuppressNullableConversion();
        handle = dndc_ctx_make_root(ctx, self->filename?pystring_borrow_stringview(self->filename):SV("(string input)"));
        PopDiagnostic();
    }
    return DndcNode_make(self, handle);
}

static
int
DndcContextPy_set_root(PyObject * s, PyObject * o, void * p){
    (void)p;
    DndcContextPy* self = (DndcContextPy*)s;
    DndcContext* ctx = self->ctx;
    DndcNodeHandle handle;
    if(PyLong_Check(o))
        handle = PyLong_AsLong(o);
    else
        handle = DNDC_NODE_HANDLE_INVALID;
    int ret = dndc_ctx_set_root(ctx, handle);
    (void) ret;
    return 0;
}

static
PyObject* _Nullable
DndcContextPy_get_base(PyObject* s, void*_Nullable p){
    (void)p;
    DndcContextPy* self = (DndcContextPy*)s;
    DndcContext* ctx = self->ctx;
    StringView base;
    int err = dndc_ctx_get_base(ctx, &base);
    if(err){
        return PyErr_Format(PyExc_RuntimeError, "wtf");
    }
    return PyUnicode_FromStringAndSize(base.text, base.length);
}

static
int
DndcContextPy_set_base(PyObject* s, PyObject*_Nullable args, void*_Nullable p){
    (void)p;
    if(!args)
        return (void)PyErr_Format(PyExc_AttributeError, "Deletion of base_dir unsupported"), -1;
    if(!PyUnicode_Check(args))
        return (void)PyErr_Format(PyExc_TypeError, "base_dir must be a string"), -1;
    DndcContextPy* self = (DndcContextPy*)s;
    DndcContext* ctx = self->ctx;
    PushDiagnostic(); SuppressNullableConversion();
    StringView sv = pystring_borrow_stringview(args);
    PopDiagnostic();
    sv = dndc_ctx_dup_sv(ctx, sv);
    int err = dndc_ctx_set_base(ctx, sv);
    if(err)
        return (void)PyErr_Format(PyExc_RuntimeError, "wtf"), -1;
    return 0;
}


static PyGetSetDef DndcContextPy_getset[] = {
    {"root", DndcContextPy_get_root, DndcContextPy_set_root, "The root node of the tree (may be None).", NULL},
    {"base_dir", DndcContextPy_get_base, DndcContextPy_set_base, "Files are imported relative to this path (may be empty string which means .)", NULL},
    {} /* Sentinel */
};

static
void
DndcContextPy_dealloc(PyObject* o){
    // fprintf(stderr, "Deallocing ctx: %p\n", o);
    DndcContextPy* self = (DndcContextPy*)o;
    dndc_ctx_destroy(self->ctx);
    self->ctx = NULL;
    Py_CLEAR(self->logger);
    Py_CLEAR(self->filename);
    Py_CLEAR(self->prev);
    Py_TYPE(self)->tp_free((PyObject *) self);
}


static PyTypeObject DndcContextPyType  = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pydndc.Context",
    .tp_doc = "Context(flags=Flags.NONE, filename=None, filecache=None)\n"
            "--\n"
            "\n"
            "A dndc parsing context that encapsulates all of the nodes, caches, etc.\n"
            "of the parsing process. This is a lower-level api. To just convert a\n"
            "simple string to html, see `htmlgen`.\n",
    .tp_basicsize = sizeof(DndcContextPy),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = DndcContextPy_new,
    .tp_members = DndcContextPy_members,
    .tp_methods = DndcContextPy_methods,
    .tp_getset = DndcContextPy_getset,
    .tp_dealloc = DndcContextPy_dealloc,
};

typedef struct DndcNodePy DndcNodePy;
struct DndcNodePy {
    PyObject_HEAD
    DndcContextPy* pyctx;
    DndcNodeHandle handle;
};

static
void
DndcNode_dealloc(PyObject* o){
    // fprintf(stderr, "Deallocing node: %p\n", o);
    DndcNodePy* self = (DndcNodePy*)o;
    Py_CLEAR(self->pyctx);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static
PyObject* _Nullable
DndcNodePy_set_attribute(PyObject* s, PyObject* args, PyObject* kwargs){
    PyObject* key, * value=NULL;
    const char* const keywords[] = { "key", "value", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O!:set_attribute", (char**)keywords, &PyUnicode_Type, &key, &PyUnicode_Type, &value)){
        return NULL;
    }
    PopDiagnostic();
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    dndc_node_set_attribute(ctx, self->handle,
            dndc_ctx_dup_sv(ctx, pystring_borrow_stringview(key)),
            value?dndc_ctx_dup_sv(ctx, pystring_borrow_stringview(value)):SV(""));
    Py_RETURN_NONE;
}
static
PyObject* _Nullable
DndcNodePy_get_attribute(PyObject* s, PyObject* args, PyObject* kwargs){
    PyObject* key;
    const char* const keywords[] = { "key", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|:get_attribute", (char**)keywords, &PyUnicode_Type, &key)){
        return NULL;
    }
    PopDiagnostic();
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcStringView attr = {0};
    int err = dndc_node_get_attribute(ctx, self->handle, pystring_borrow_stringview(key), &attr);
    if(err)
        return PyErr_Format(PyExc_KeyError, "No attribute for %R", key);
    if(!attr.length)
        Py_RETURN_NONE;
    return PyUnicode_FromStringAndSize(attr.text, attr.length);
}

static
PyObject* _Nullable
DndcNodePy_has_attribute(PyObject* s, PyObject* args, PyObject* kwargs){
    PyObject* key;
    const char* const keywords[] = { "key", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|:has_attribute", (char**)keywords, &PyUnicode_Type, &key)){
        return NULL;
    }
    PopDiagnostic();
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    int has_it = dndc_node_has_attribute(ctx, self->handle, pystring_borrow_stringview(key));
    if(has_it)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}
static
PyObject* _Nullable
DndcNodePy_attributes(PyObject* s, void* args){
    (void)args;
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcNodeHandle handle = self->handle;
    size_t nattrs = dndc_node_attributes_count(ctx, handle);
    if(!nattrs)
        return PyTuple_New(0);
    DndcAttributePair buff[16];
    size_t cookie = 0;
    size_t tup_idx = 0;
    PyObject* result = PyTuple_New(nattrs);
    while(tup_idx < nattrs){
        size_t n = dndc_node_attributes(ctx, handle, &cookie, buff, arrlen(buff));
        if(!n){
            Py_DECREF(result);
            return PyErr_Format(PyExc_RuntimeError, "Wtf");
        }
        for(size_t buf_idx = 0; buf_idx < n; buf_idx++){
            DndcStringView k = buff[buf_idx].key;
            DndcStringView v = buff[buf_idx].value;
            PyObject* key = PyUnicode_FromStringAndSize(k.text, k.length); // new ref
            PyObject* value; // will be new ref
            if(v.length)
                value = PyUnicode_FromStringAndSize(v.text, v.length);
            else{
                value = Py_None;
                Py_INCREF(value);
            }
            PyObject* item = PyTuple_Pack(2, key, value); // new ref
            Py_DECREF(key);
            Py_DECREF(value);
            PyTuple_SetItem(result, tup_idx++, item); // steals ref
        }
    }
    return result;
}

static
PyObject*_Nullable
DndcNodePy_classes(PyObject* s, void* args){
    (void)args;
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcNodeHandle handle = self->handle;
    size_t nclasses = dndc_node_classes_count(ctx, handle);
    if(!nclasses)
        return PyTuple_New(0);
    DndcStringView buff[16];
    size_t cookie = 0;
    size_t tup_idx = 0;
    PyObject* result = PyTuple_New(nclasses);
    while(tup_idx < nclasses){
        size_t n = dndc_node_classes(ctx, handle, &cookie, buff, arrlen(buff));
        if(!n){
            Py_DECREF(result);
            return PyErr_Format(PyExc_RuntimeError, "Wtf");
        }
        for(size_t buf_idx = 0; buf_idx < n; buf_idx++){
            DndcStringView c = buff[buf_idx];
            PyObject* cls = PyUnicode_FromStringAndSize(c.text, c.length); // new ref
            PyTuple_SetItem(result, tup_idx++, cls); // steals ref
        }
    }
    return result;
}

static
PyObject*_Nullable
DndcNodePy_add_class(PyObject* s, PyObject* args){
    if(!PyUnicode_Check(args)){
        return PyErr_Format(PyExc_TypeError, "Expected a string, got %R", args);
    }
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcNodeHandle handle = self->handle;
    int err = dndc_node_add_class(ctx, handle, dndc_ctx_dup_sv(ctx, pystring_borrow_stringview(args)));
    if(err)
        return PyErr_Format(PyExc_RuntimeError, "Problem adding %R", args);
    Py_RETURN_NONE;
}

static
PyObject*_Nullable
DndcNodePy_remove_class(PyObject* s, PyObject* args){
    if(!PyUnicode_Check(args)){
        return PyErr_Format(PyExc_TypeError, "Expected a string, got %R", args);
    }
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcNodeHandle handle = self->handle;
    int err = dndc_node_remove_class(ctx, handle, dndc_ctx_dup_sv(ctx, pystring_borrow_stringview(args)));
    if(err)
        return PyErr_Format(PyExc_RuntimeError, "Problem removing %R", args);
    Py_RETURN_NONE;
}

static
PyObject*_Nullable
DndcNodePy_execute_js(PyObject* s, PyObject* args){
    if(!PyUnicode_Check(args)) return PyErr_Format(PyExc_TypeError, "js script must be a string");
    LongString script = pystring_borrow_longstring(args);
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcNodeHandle handle = self->handle;
    int err = dndc_node_execute_js(ctx, handle, script);
    if(err){
        return PyErr_Format(PyExc_RuntimeError, "Problem while executing javascript");
    }
    Py_RETURN_NONE;
}

static
PyObject* _Nullable
DndcNodePy_parse(PyObject* s, PyObject* args, PyObject* kwargs){
    PyObject* text, * filename=NULL;
    const char* const keywords[] = { "text", "filename", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O!:parse", (char**)keywords, &PyUnicode_Type, &text, &PyUnicode_Type, &filename)){
        return NULL;
    }
    PopDiagnostic();
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    int err = dndc_ctx_parse_string(ctx, self->handle, filename?dndc_ctx_dup_sv(ctx, pystring_borrow_stringview(filename)):SV("(string input)"), dndc_ctx_dup_sv(ctx, pystring_borrow_stringview(text)));
    if(err){
        return PyErr_Format(PyExc_ValueError, "Error while parsing");
    }
    Py_RETURN_NONE;
}
static
PyObject* _Nullable
DndcNodePy_parse_file(PyObject* s, PyObject* args, PyObject* kwargs){
    PyObject* path;
    const char* const keywords[] = { "path", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|:parse", (char**)keywords, &PyUnicode_Type, &path)){
        return NULL;
    }
    PopDiagnostic();
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    int err = dndc_ctx_parse_file(ctx, self->handle, dndc_ctx_dup_sv(ctx, pystring_borrow_stringview(path)));
    if(err){
        return PyErr_Format(PyExc_ValueError, "Error while parsing");
    }
    Py_RETURN_NONE;
}
static
PyObject* _Nullable
DndcNodePy_make_child(PyObject* s, PyObject* args, PyObject* kwargs){
    PyObject* type;
    PyObject *header = NULL;
    DndcNodePy* self = (DndcNodePy*)s;
    const char* const keywords[] = {"type", "header", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O!:make_child", (char**)keywords, &type, &PyUnicode_Type, &header)){
        return NULL;
    }
    PopDiagnostic();
    if(!PyLong_Check(type))
        return PyErr_Format(PyExc_TypeError, "Type must be integral");
    DndcContext* ctx = self->pyctx->ctx;
    StringView h = header?dndc_ctx_dup_sv(ctx, pystring_borrow_stringview(header)): SV("");
    DndcNodeHandle n = dndc_ctx_make_node(ctx, PyLong_AsLong(type), h, self->handle);
    if(n == DNDC_NODE_HANDLE_INVALID)
        return header?PyErr_Format(PyExc_ValueError, "Unable to make a node with type: %R, header: %R", type, header):PyErr_Format(PyExc_ValueError, "Unable to make a node with type: %R", type);

    return DndcNode_make(self->pyctx, n);
}


static
PyObject *_Nullable
DndcNodePy_get_header(PyObject *s, void *_Nullable p){
    (void)p;
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcStringView sv;
    int err = dndc_node_get_header(ctx, self->handle, &sv);
    assert(!err);
    return PyUnicode_FromStringAndSize(sv.text, sv.length);
}
static
int
DndcNodePy_set_header(PyObject * s, PyObject * o, void * p){
    (void)p;
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    if(!o) {
        dndc_node_set_header(ctx, self->handle, SV(""));
        return 0;
    }
    if(!PyUnicode_Check(o)) return 0;
    dndc_node_set_header(ctx, self->handle, dndc_ctx_dup_sv(ctx, pystring_borrow_stringview(o)));
    return 0;
}

// ----

static
PyObject *_Nullable
DndcNodePy_get_type(PyObject *s, void *_Nullable p){
    (void)p;
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    return PyObject_CallOneArg(node_type_enum, PyLong_FromLong(dndc_node_get_type(ctx, self->handle)));
}
static
int
DndcNodePy_set_type(PyObject * s, PyObject * o, void * p){
    (void)p;
    if(!o){
        PyErr_SetString(PyExc_AttributeError, "del is unsupported for type");
        return 1;
    }
    if(!PyLong_Check(o)) {
        PyErr_SetString(PyExc_TypeError, "type must be an int");
        return 1;
    }
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    int ret = dndc_node_set_type(ctx, self->handle, PyLong_AsLong(o));
    if(ret){
        PyErr_SetString(PyExc_ValueError, "Invalid type value");
        return 1;
    }
    return 0;
}

// ---

static
PyObject *_Nullable
DndcNodePy_get_id(PyObject *s, void *_Nullable p){
    (void)p;
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcStringView sv = {0};
    int err = dndc_node_get_id(ctx, self->handle, &sv);
    assert(!err);
    if(!sv.length) return PyUnicode_FromString("");
    MStringBuilder temp = {.allocator = get_mallocator()};
    msb_write_kebab(&temp, sv.text, sv.length);
    StringView b = msb_borrow_sv(&temp);
    PyObject* result = PyUnicode_FromStringAndSize(b.text, b.length);
    msb_destroy(&temp);
    return result;
}
static
int
DndcNodePy_set_id(PyObject * s, PyObject * o, void * p){
    (void)p;
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    if(!o) {
        dndc_node_set_id(ctx, self->handle, SV(""));
        return 0;
    }
    if(!PyUnicode_Check(o)) return 0;
    dndc_node_set_id(ctx, self->handle, dndc_ctx_dup_sv(ctx, pystring_borrow_stringview(o)));
    return 0;
}

// ---

static
PyObject *_Nullable
DndcNodePy_get_parent(PyObject *s, void *_Nullable p){
    (void)p;
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcNodeHandle parent = dndc_node_get_parent(ctx, self->handle);
    if(parent == DNDC_NODE_HANDLE_INVALID)
        Py_RETURN_NONE;
    return DndcNode_make(self->pyctx, parent);
}

// ---

static
PyObject *_Nullable
DndcNodePy_get_children(PyObject *s, void *_Nullable p){
    (void)p;
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    size_t n = dndc_node_children_count(ctx, self->handle);
    PyObject* tup = PyTuple_New(n);
    DndcNodeHandle buff[1024];
    size_t cookie = 0;
    size_t done = 0;
    while(done < n){
        size_t n_read = dndc_node_get_children(ctx, self->handle, &cookie, buff, sizeof buff / sizeof buff[0]);
        for(size_t i = 0; i < n_read; i++){
            PyTuple_SET_ITEM(tup, i+done, DndcNode_make(self->pyctx, buff[i]));
        }
        done += n_read;
    }
    return tup;
}

static
PyObject* _Nullable
DndcNodePy_get_location(PyObject* s, void*_Nullable p){
    (void)p;
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcNodeLocation loc;
    int err = dndc_node_location(ctx, self->handle, &loc);
    if(err) return PyErr_Format(PyExc_RuntimeError, "Invalid node");
    PyObject* result = PyStructSequence_New(Location);
    PyStructSequence_SET_ITEM(result, 0, PyUnicode_FromStringAndSize(loc.filename.text, loc.filename.length)); // steals the newly created ref
    PyStructSequence_SET_ITEM(result, 1, PyLong_FromLong(loc.row));
    PyStructSequence_SET_ITEM(result, 2, PyLong_FromLong(loc.column));

    return result;
}

static
PyObject* _Nullable
DndcNodePy_getflag(PyObject* s, void*_Nullable p){
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcNodeHandle handle = self->handle;
    int flag = (int)(intptr_t)p;
    if(dndc_node_get_flags(ctx, handle) & flag)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static
int
DndcNodePy_setflag(PyObject* s, PyObject*_Nullable a, void*_Nullable p){
    if(!a){
        PyErr_Format(PyExc_AttributeError, "Deletion of flags is not supported");
        return -1;
    }
    int flag = (int)(intptr_t)p;
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcNodeHandle handle = self->handle;
    int flags = dndc_node_get_flags(ctx, handle);
    if(PyObject_IsTrue(a))
        flags |= flag;
    else
        flags &= ~flag;
    int err = dndc_node_set_flags(ctx, handle, flags);
    (void)err;
    return 0;
}


// ---

static
PyObject* _Nullable
DndcNodePy_format(PyObject* s, PyObject* arg){
    if(!PyLong_Check(arg)) return PyErr_Format(PyExc_TypeError, "Need an int argument for indent");
    long indent = PyLong_AsLong(arg);
    if(indent < 0 || indent > 50) return PyErr_Format(PyExc_ValueError, "Indent value invalid: %R", arg);
    LongString ls;
    DndcNodePy* self = (DndcNodePy*)s;
    int err = dndc_node_format(self->pyctx->ctx, self->handle, indent, &ls);
    if(err){
        return PyErr_Format(PyExc_ValueError, "Node can't be formatted");
    }
    PyObject* result = PyUnicode_FromStringAndSize(ls.text, ls.length);
    dndc_free_string(ls);
    return result;
}
static
PyObject* _Nullable
DndcNodePy_render(PyObject* s, PyObject* arg){
    (void)arg;
    DndcNodePy* self = (DndcNodePy*)s;
    LongString html;
    int e = dndc_node_render_to_html(self->pyctx->ctx, self->handle, &html);
    if(e){
        return PyErr_Format(PyExc_ValueError, "Unable to render node to html.");
    }
    PyObject* result = PyUnicode_FromStringAndSize(html.text, html.length);
    dndc_free_string(html);
    return result;
}

static
PyObject* _Nullable
DndcNodePy_append_child(PyObject* s, PyObject* arg){
    if(PyUnicode_Check(arg)){
        // special case for strings
        DndcNodePy* self = (DndcNodePy*)s;
        DndcStringView content = dndc_ctx_dup_sv(self->pyctx->ctx, pystring_borrow_stringview(arg));
        DndcNodeHandle child = dndc_ctx_make_node(self->pyctx->ctx, DNDC_NODE_TYPE_STRING, content, self->handle);
        (void)child;
        Py_RETURN_NONE;
    }
    if(!Py_IS_TYPE(arg, &DndcNodePyType)) return PyErr_Format(PyExc_TypeError, "Need a node argument for append");
    DndcNodePy* self = (DndcNodePy*)s;
    DndcNodePy* child = (DndcNodePy*)arg;
    if(self->pyctx != child->pyctx)
        return PyErr_Format(PyExc_ValueError, "Nodes from different contexts cannot be mixed");
    int err = dndc_node_append_child(self->pyctx->ctx, self->handle, child->handle);
    if(err){
        return PyErr_Format(PyExc_ValueError, "Node could not be appended");
    }
    Py_RETURN_NONE;
}

static
PyObject* _Nullable
DndcNodePy_detach(PyObject* s, PyObject* arg){
    (void)arg;
    DndcNodePy* self = (DndcNodePy*)s;
    dndc_node_detach(self->pyctx->ctx, self->handle);
    Py_RETURN_NONE;
}

static
PyObject* _Nullable
DndcNodePy_tree_repr(PyObject* s, PyObject* arg){
    (void)arg;
    DndcNodePy* self = (DndcNodePy*)s;
    LongString string;
    int err = dndc_node_tree_repr(self->pyctx->ctx, self->handle, &string);
    if(err) return PyErr_Format(PyExc_ValueError, "Problem printing tree");
    PyObject* result = PyUnicode_FromStringAndSize(string.text, string.length);
    dndc_free_string(string);
    return result;
}

DNDC_API
int
dndc_node_to_json(DndcContext* ctx, DndcNodeHandle dnh, DndcLongString*out);

static
PyObject* _Nullable
DndcNodePy_to_json(PyObject* s, PyObject* arg){
    (void)arg;
    DndcNodePy* self = (DndcNodePy*)s;
    LongString string;
    int err = dndc_node_to_json(self->pyctx->ctx, self->handle, &string);
    if(err) return PyErr_Format(PyExc_ValueError, "Problem converting to json.");
    PyObject* result = PyUnicode_FromStringAndSize(string.text, string.length);
    dndc_free_string(string);
    return result;
}


static PyGetSetDef DndcNodePy_getset[] = {
    {
        .name="header",
        .get=DndcNodePy_get_header,
        .set=DndcNodePy_set_header,
        .doc="The title of the node (or value for STRING nodes).",
        .closure=NULL,
    },
    {
        .name="type",
        .get=DndcNodePy_get_type,
        .set=DndcNodePy_set_type,
        .doc="The type of the node.",
        .closure=NULL,
    },
    {
        .name="id",
        .get=DndcNodePy_get_id,
        .set=DndcNodePy_set_id,
        .doc="The string id of the node (may be None).",
        .closure=NULL,
    },
    {
        .name="parent",
        .get=DndcNodePy_get_parent,
        .set=NULL,
        .doc="The parent node of this node (may be None).",
        .closure=NULL,
    },
    {
        .name="children",
        .get=DndcNodePy_get_children,
        .set=NULL,
        .doc="The child nodes of this node.",
        .closure=NULL,
    },
    {
        .name="location",
        .get=DndcNodePy_get_location,
        .set=NULL,
        .doc="Where in its source document this node originates.",
        .closure=NULL,
    },
    {
        .name="classes",
        .get=DndcNodePy_classes,
        .set=NULL,
        .doc="The classes of this node.",
        .closure=NULL,
    },
    {
        .name="attributes",
        .get=DndcNodePy_attributes,
        .set=NULL,
        .doc="The attributes set on this node.",
        .closure=NULL,
    },
    {
        .name="import_",
        .get=DndcNodePy_getflag,
        .set=DndcNodePy_setflag,
        .doc="Whether this is a node where its children should be treated as paths to files to import.",
        .closure=(void*)DNDC_NODEFLAG_IMPORT,
    },
    {
        .name="noid",
        .get=DndcNodePy_getflag,
        .set=DndcNodePy_setflag,
        .doc="Whether to suppress automatic generation of the string id of this node from the header.",
        .closure=(void*)DNDC_NODEFLAG_NOID,
    },
    {
        .name="hide",
        .get=DndcNodePy_getflag,
        .set=DndcNodePy_setflag,
        .doc="Whether to skip this node when rendering.",
        .closure=(void*)DNDC_NODEFLAG_HIDE,
    },
    {
        .name="noinline",
        .get=DndcNodePy_getflag,
        .set=DndcNodePy_setflag,
        .doc="Whether to inline the referenced contents for imgs and such or to generate a link.",
        .closure=(void*)DNDC_NODEFLAG_NOINLINE,
    },

    {} /* Sentinel */
};
static PyMemberDef DndcNodePy_members[] = {
    {"ctx", T_OBJECT, offsetof(DndcNodePy, pyctx), READONLY, "The Context that this node is from."},
    {"handle", T_UINT, offsetof(DndcNodePy, handle), READONLY, "The opaque integer handle that can be used to uniquely identify this node."},
    {}  /* Sentinel */
};

static PyMethodDef DndcNodePy_methods[] = {
    {
        .ml_name="set_attribute",
        .ml_meth=(PyCFunction)DndcNodePy_set_attribute,
        .ml_flags=METH_VARARGS|METH_KEYWORDS,
        .ml_doc="set_attribute(self, key, value)\n"
            "--\n"
            "\n"
            "Set an attribute.",
    },
    {
        .ml_name="get_attribute",
        .ml_meth=(PyCFunction)DndcNodePy_get_attribute,
        .ml_flags=METH_VARARGS|METH_KEYWORDS,
        .ml_doc="get_attribute(self, key)\n"
            "--\n"
            "\n"
            "Get an attribute.",
    },
    {
        .ml_name="has_attribute",
        .ml_meth=(PyCFunction)DndcNodePy_has_attribute,
        .ml_flags=METH_VARARGS|METH_KEYWORDS,
        .ml_doc="has_attribute(self, key)\n"
            "--\n"
            "\n"
            "If it has an attribute.",
    },
    {
        .ml_name="remove_class",
        .ml_meth=DndcNodePy_remove_class,
        .ml_flags=METH_O,
        .ml_doc="remove_class(self, class_)\n"
            "--\n"
            "\n"
            "Removes a class from the class list.",
    },
    {
        .ml_name="add_class",
        .ml_meth=DndcNodePy_add_class,
        .ml_flags=METH_O,
        .ml_doc="add_class(self, class_)\n"
            "--\n"
            "\n"
            "Adds a class to the class list.",
    },
    {
        .ml_name="execute_js",
        .ml_meth=DndcNodePy_execute_js,
        .ml_flags=METH_O,
        .ml_doc="execute_js(self, script)\n"
            "--\n"
            "\n"
            "Executes javascript in the context of this node.",
    },
    {
        .ml_name="parse",
        .ml_meth=(PyCFunction)DndcNodePy_parse,
        .ml_flags=METH_VARARGS|METH_KEYWORDS,
        .ml_doc="parse(self, text, filename=None)\n"
            "--\n"
            "\n"
            "Parse a dnd string.\n"
            "This method can call the logger.\n",
    },
    {
        .ml_name="parse_file",
        .ml_meth=(PyCFunction)DndcNodePy_parse_file,
        .ml_flags=METH_VARARGS|METH_KEYWORDS,
        .ml_doc="parse_file(self, path)\n"
            "--\n"
            "\n"
            "Parse a dnd file.\n"
            "This method can call the logger.\n",

    },
    {
        .ml_name="format",
        .ml_meth=DndcNodePy_format,
        .ml_flags=METH_O,
        .ml_doc="format(self, indent)\n"
            "--\n"
            "\n"
            "Format a node.\n"
            "This method can call the logger.\n"
            ,
    },
    {
        .ml_name="render",
        .ml_meth=DndcNodePy_render,
        .ml_flags=METH_NOARGS,
        .ml_doc="render(self)\n"
            "--\n"
            "\n"
            "render node to html fragment",
    },
    {
        .ml_name="append_child",
        .ml_meth=DndcNodePy_append_child,
        .ml_flags=METH_O,
        .ml_doc=
            #if PY_INSPECT_SUPPORTS_ANNOTATIONS
            "append_child(self, child:Union[Node, str]) -> None\n"
            #else
            "append_child(self, child)\n"
            #endif
            "--\n"
            "\n"
            "Append a node as a child of this node.\n"
    },
    {
        .ml_name="detach",
        .ml_meth=DndcNodePy_detach,
        .ml_flags=METH_NOARGS,
        .ml_doc=
            "detach()\n"
            "--\n"
            "\n"
            "Detaches this node from its parent."
    },
    {
        .ml_name="make_child",
        .ml_meth=(PyCFunction)DndcNodePy_make_child,
        .ml_flags=METH_VARARGS|METH_KEYWORDS,
        .ml_doc=
            #if PY_INSPECT_SUPPORTS_ANNOTATIONS
            "make_child(self, type:NodeType, header:str=None)\n"
            #else
            "make_child(self, type, header=None)\n"
            #endif
            "--\n"
            "\n"
            "Creates a child node of the given type as a child of this node, optionally with the given header\n"
    },
    {
        .ml_name="tree_repr",
        .ml_meth=DndcNodePy_tree_repr,
        .ml_flags=METH_NOARGS,
        .ml_doc="tree_repr(self)\n"
            "--\n"
            "\n"
            "Debug tree representation.",
    },
    {
        .ml_name="_to_json",
        .ml_meth=DndcNodePy_to_json,
        .ml_flags=METH_NOARGS,
        .ml_doc="_to_json(self)\n"
            "--\n"
            "\n"
            "Convert node to a json string.",
    },
    {} /* Sentinel */
};

static
PyObject*_Nullable
DndcNodePy_repr(PyObject* s){
    DndcNodePy* self = (DndcNodePy*)s;
    DndcContext* ctx = self->pyctx->ctx;
    DndcNodeHandle handle = self->handle;
    StringView sv;
    int err = dndc_node_get_header(ctx, handle, &sv);
    if(err) return PyErr_Format(PyExc_ValueError, "Repr of invalid node");
    int type = dndc_node_get_type(ctx, handle);
    const char* typename = "";
    switch(type){
#define X(a, b) case DNDC_NODE_TYPE_##a: typename = #a; break;
        DNDCNODETYPES(X)
#undef X
    }
    PyObject* h = PyUnicode_FromStringAndSize(sv.text, sv.length);
    size_t n_children = dndc_node_children_count(ctx, handle);

    PyObject* result = PyUnicode_FromFormat("Node(%s, %R, [%zu children], handle=%u)", typename, h, n_children, self->handle);
    Py_DECREF(h);
    return result;
}


static PyTypeObject DndcNodePyType  = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pydndc.Node",
    .tp_doc = "Node",
    .tp_basicsize = sizeof(DndcNodePy),
    .tp_itemsize = 0,
    .tp_members = DndcNodePy_members,
    .tp_methods = DndcNodePy_methods,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = DndcNode_dealloc,
    .tp_getset = DndcNodePy_getset,
    .tp_repr = DndcNodePy_repr,
};


static
PyObject*
DndcNode_make(DndcContextPy* ctx, DndcNodeHandle handle){
    Py_INCREF(ctx);
    DndcNodePy* o = PyObject_New(DndcNodePy, &DndcNodePyType);
    o->handle = handle;
    o->pyctx = ctx;
    return (PyObject*)o;
}





#ifdef __clang__
#pragma clang assume_nonnull end
#elif defined(__GNUC__)
PopDiagnostic();
#endif

#include "dndc.c"
