//
// Exposes dndc as a c-extension for python.
//
#define DNDC_API static inline
#include "dndc.h"
#include "dndc_long_string.h"
#include "common_macros.h"
#include "allocator.h"
#include "pyhead.h"
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

typedef struct DndcPyFileCache {
    PyObject_HEAD
    DndcFileCache* text_cache;
    DndcFileCache* b64_cache;
} DndcPyFileCache;

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
pydndc_anaylze_syntax_for_highlight(PyObject*, PyObject*, PyObject*);

// returns 0 on success
static
int
pyobj_to_json(PyObject*, MStringBuilder*);

static
PyMethodDef pydndc_methods[] = {
    {
        .ml_name = "reformat",
        .ml_meth = (PyCFunction)pydndc_reformat,
        .ml_flags = METH_VARARGS|METH_KEYWORDS,
        .ml_doc =
        #if PY_INSPECT_SUPPORTS_ANNOTATIONS
        "reformat(text:str, error_reporter:Callable|None=None) -> str\n"
        #else
        "reformat(text, error_reporter=None)\n"
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
        "error_reporter: Callable(int, str, int, int, str)\n"
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
        "htmlgen(text:str, base_dir:str='.', error_reporter:Callable=None, file_cache:FileCache=None, flags:Flags=0, output_name:str=None, jsvars:str=None)\n"
        #else
        "htmlgen(text, base_dir='.', error_reporter=None, file_cache=None, flags=0, output_name=None, jsvars=None)\n"
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
        "error_reporter: Callable(int, str, int, int, str)\n"
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
        "    PRINT_STATS:        Generate Info messages for the error_reporter.\n"
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
        "output_name: str\n"
        "   Several features depend on knowing what the ultimate name of the file will be.\n"
        "   APIs such as ctx.outpath etc. in js blocks for example.\n"
        "   Note that we do not actually write to this path.\n"
        "\n"
        "   This path is *NOT* adjusted by the base_directory argument.\n"
        "\n"
        "jsvars: dict or str\n"
        "    A dict or json literal that will be exposed to js blocks as VARS.\n"
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
        "If the error_reporter is given, it will be called with the following\n"
        "arguments:\n"
        "\n"
        "Error Reporter Arguments:\n"
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
        "    This will be '(string input)' for the primary text.\n"
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
        "parsing. The error_reporter will be called on each error and warning\n"
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
        "expand(text:str, base_dir:str='.', error_reporter:Callable=None, file_cache:FileCache=None, flags:Flags=0, output_name:str=None) -> str\n"
        #else
        "expand(text, base_dir='.', error_reporter=None, file_cache=None, flags=0, output_name=None)\n"
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
        "error_reporter: Callable(int, str, int, int, str)\n"
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
        "    PRINT_STATS:        Generate Info messages for the error_reporter.\n"
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
        "output_name: str\n"
        "    The final name of the html file. This is used in js scripting.\n"
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

static PyTypeObject* SyntaxRegion;


static inline
int
add_doc(PyObject* obj, const char* text){
    PyObject* doc = PyUnicode_FromString(text);
    if(!doc) return 1;
    PyObject_SetAttrString(obj, "__doc__", doc);
    Py_DECREF(doc);
    return 0;
}

PyMODINIT_FUNC _Nullable
PyInit_pydndc(void){
    PyObject* mod = PyModule_Create(&pydndc);
    PyObject* enu_mod = NULL;
    PyObject* intenum = NULL;
    PyObject* intflag = NULL;
    PyObject* synvalues = NULL;
    PyObject* messagevalues = NULL;
    PyObject* flagvalues = NULL;
    PyObject* filecache_type = NULL;
    PyObject* synenum = NULL;
    PyObject* msgenum = NULL;
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
    if(PyModule_AddObjectRef(mod, "FileCache", filecache_type) < 0){
        goto fail;
    }
    PyModule_AddStringConstant(mod, "__version__",     DNDC_VERSION);
    SyntaxRegion = PyStructSequence_NewType(&syntax_desc);
    // pydoc basically shits the bed if a class doesn't have a __module__ or a __doc__.
    PyObject_SetAttrString((PyObject*)SyntaxRegion, "__module__", modname);
    if(PyModule_AddObjectRef(mod, "SyntaxRegion", (PyObject*)SyntaxRegion) < 0)
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
    Py_XDECREF(messagevalues);
    Py_XDECREF(flagvalues);
    Py_XDECREF(filecache_type);
    Py_XDECREF(synenum);
    Py_XDECREF(msgenum);
    Py_XDECREF(flagenum);
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
    if(!tup){
        return;
    }
    PyObject* list = user_data;
    int fail = PyList_Append(list, tup);
    (void)fail;
    Py_XDECREF(tup);
}

static
Nullable(PyObject*)
pydndc_reformat(PyObject* mod, PyObject* args, PyObject* kwargs){
    (void)mod;
    PyObject* text;
    PyObject* error_reporter = NULL;
    const char* const keywords[] = { "text", "error_reporter", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O:reformat", (char**)keywords, &PyUnicode_Type, &text, &error_reporter)){
        return NULL;
    }
    PopDiagnostic();
    if(error_reporter && error_reporter == Py_None)
        error_reporter = NULL;
    if(error_reporter && !PyCallable_Check(error_reporter)){
        PyErr_SetString(PyExc_TypeError, "error_reporter must be a callable");
        return NULL;
    }
    StringView source = pystring_borrow_stringview(text);
    LongString output = {};
    DndcErrorFunc* func = error_reporter?pydndc_collect_errors:NULL;
    PyObject* error_list = func? PyList_New(0) : NULL;
    PyObject* result = NULL;
    int e = dndc_format(source, &output, func, error_list);
    if(PyErr_Occurred()){
        goto finally;
    }
    if(error_reporter){
        Py_ssize_t length = PyList_Size(error_list);
        for(Py_ssize_t i = 0; i < length; i++){
            PyObject* list_item = PyList_GetItem(error_list, i);
            PyObject* call_result = PyObject_Call(error_reporter, list_item, NULL);
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
    PyObject* error_reporter = NULL;
    PyObject* file_cache = NULL;
    PyObject* output_name = NULL;
    PyObject* jsvars = NULL;
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
    const char* const keywords[] = {"text", "base_dir", "error_reporter", "file_cache", "flags", "output_name", "jsvars", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O!OOKO!O:htmlgen", (char**)keywords, &PyUnicode_Type, &text, &PyUnicode_Type, &base_dir, &error_reporter, &file_cache, &flags, &PyUnicode_Type, &output_name, &jsvars)){
        return NULL;
    }
    PopDiagnostic();
    // Check for any flags that python callers shouldn't be able to set.
    if((flags & WHITELIST) != flags){
        PyErr_SetString(PyExc_ValueError, "flags argument contains illegal bits");
        return NULL;
    }
    if(error_reporter && error_reporter == Py_None)
        error_reporter = NULL;
    if(error_reporter && !PyCallable_Check(error_reporter)){
        PyErr_SetString(PyExc_TypeError, "error_reporter must be a callable");
        return NULL;
    }
    if(file_cache && file_cache == Py_None)
        file_cache = NULL;
    if(file_cache && !PyObject_IsInstance(file_cache, (PyObject*)&DndcPyFileCache_Type)){
        PyErr_SetString(PyExc_TypeError, "file_cache must be a DndcFileCache");
        return NULL;
    }
    LongString jsvars_ls = LS("");
    MStringBuilder jsbuilder = {.allocator = get_mallocator()};
    if(jsvars && PyUnicode_Check(jsvars)){
        jsvars_ls = pystring_borrow_longstring(jsvars);
    }
    else if(jsvars){
        if(pyobj_to_json(jsvars, &jsbuilder) != 0){
            if(jsbuilder.capacity){
                msb_destroy(&jsbuilder);
            }
            return NULL;
        }
        jsvars_ls = msb_borrow_ls(&jsbuilder);
    }
    StringView source = pystring_borrow_stringview(text);
    StringView base_str = base_dir? pystring_borrow_stringview(base_dir): SV("");
    // flags |= DNDC_DONT_PRINT_ERRORS;
    // flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    LongString output = {};
    DndcErrorFunc* func = error_reporter?pydndc_collect_errors:NULL;
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
    StringView outname = output_name?pystring_borrow_stringview(output_name) : SV("this.html");
    int e = dndc_compile_dnd_file(flags, base_str, source, SV(""), outname, &output, b64cache, textcache, func, error_list, pydndc_add_dependencies, depends_list, NULL, jsvars_ls);
    if(PyErr_Occurred()){
        result = NULL;
        goto finally;
    }
    if(error_reporter){
        Py_ssize_t length = PyList_Size(error_list);
        for(Py_ssize_t i = 0; i < length; i++){
            PyObject* list_item = PyList_GetItem(error_list, i);
            PyObject* call_result = PyObject_Call(error_reporter, list_item, NULL);
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
    PyObject* error_reporter = NULL;
    PyObject* file_cache = NULL;
    PyObject* output_name = NULL;
    PyObject* jsvars = NULL;
    unsigned long long flags = 0;
    _Static_assert(sizeof(flags) == sizeof(uint64_t), "");
    enum {WHITELIST = 0
        | DNDC_INPUT_IS_UNTRUSTED
        | DNDC_DONT_READ
        | DNDC_PRINT_STATS
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
    };
    const char* const keywords[] = {"text", "base_dir", "error_reporter", "file_cache", "flags", "output_name", "jsvars", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O!OOKO!O:expand", (char**)keywords, &PyUnicode_Type, &text, &PyUnicode_Type, &base_dir, &error_reporter, &file_cache, &flags, &PyUnicode_Type, &output_name, &jsvars)){
        return NULL;
    }
    PopDiagnostic();
    // Check for any flags that python callers shouldn't be able to set.
    if((flags & WHITELIST) != flags){
        PyErr_SetString(PyExc_ValueError, "flags argument contains illegal bits");
        return NULL;
    }
    if(error_reporter && error_reporter == Py_None)
        error_reporter = NULL;
    if(error_reporter && !PyCallable_Check(error_reporter)){
        PyErr_SetString(PyExc_TypeError, "error_reporter must be a callable");
        return NULL;
    }
    if(file_cache && file_cache == Py_None)
        file_cache = NULL;
    if(file_cache && !PyObject_IsInstance(file_cache, (PyObject*)&DndcPyFileCache_Type)){
        PyErr_SetString(PyExc_TypeError, "file_cache must be a DndcFileCache");
        return NULL;
    }
    if(jsvars && !PyUnicode_Check(jsvars)){
        PyErr_SetString(PyExc_TypeError, "jsvars must be a str");
        return NULL;
    }
    LongString jsvars_ls = jsvars? pystring_borrow_longstring(jsvars) : LS("");
    StringView source = pystring_borrow_stringview(text);
    StringView base_str = base_dir? pystring_borrow_stringview(base_dir): SV("");
    // flags |= DNDC_DONT_PRINT_ERRORS;
    // flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_OUTPUT_EXPANDED_DND;
    flags |= DNDC_ALLOW_BAD_LINKS;
    LongString output = {};
    DndcErrorFunc* func = error_reporter?pydndc_collect_errors:NULL;
    PyObject* error_list = func? PyList_New(0) : NULL;
    PyObject* result = NULL;
    DndcFileCache* textcache = NULL;
    DndcFileCache* b64cache = NULL;
    if(file_cache){
        DndcPyFileCache* cache = (DndcPyFileCache*)file_cache;
        textcache = cache->text_cache;
        b64cache = cache->b64_cache;
    }
    StringView outname = output_name?pystring_borrow_stringview(output_name) : SV("this.html");
    int e = dndc_compile_dnd_file(flags, base_str, source, SV(""), outname, &output, b64cache, textcache, func, error_list, NULL, NULL, NULL, jsvars_ls);
    if(PyErr_Occurred()){
        result = NULL;
        goto finally;
    }
    if(error_reporter){
        Py_ssize_t length = PyList_Size(error_list);
        for(Py_ssize_t i = 0; i < length; i++){
            PyObject* list_item = PyList_GetItem(error_list, i);
            PyObject* call_result = PyObject_Call(error_reporter, list_item, NULL);
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
pyobj_to_json(PyObject* o, MStringBuilder* msb){
    int result = 0;
    if(PySequence_Check(o)){
        PyObject* seq = PySequence_Fast(o, "Expected a fast sequence"); // new reference
        if(!seq) return 1;
        Py_ssize_t length = PySequence_Fast_GET_SIZE(seq);
        msb_write_char(msb, '[');
        for(Py_ssize_t i = 0; i < length; i++){
            if(i != 0) msb_write_char(msb, ',');
            PyObject* item = PySequence_Fast_GET_ITEM(seq, i); // borrowed ref
            if(pyobj_to_json(item, msb) != 0){
                result = 1;
                goto finish_seq;
            }
        }
        msb_write_char(msb, ']');
        finish_seq:
        Py_XDECREF(seq);
        return result;
    }
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
            if(pyobj_to_json(value, msb) != 0){
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



#ifdef __clang__
#pragma clang assume_nonnull end
#elif defined(__GNUC__)
PopDiagnostic();
#endif

#include "dndc.c"
