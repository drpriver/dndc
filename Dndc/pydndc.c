//
// Exposes dndc as a c-extension for python.
//
#define DNDC_API static inline
#define PYTHONMODULE
#include "dndc.c"
#include "pyhead.h"

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
pystring_to_longstring(PyObject* pyobj, const Allocator a){
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
pystring_to_stringview(PyObject* pyobj, const Allocator a){
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
    FileCache text_cache;
    FileCache b64_cache;
} DndcPyFileCache;

static
Nullable(PyObject*)
DndcPyFileCache_remove(PyObject* self, PyObject* str){
    if(!PyUnicode_Check(str)){
        PyErr_SetString(PyExc_TypeError, "Argument to remove must be a string");
        return NULL;
        }
    auto path = pystring_borrow_stringview(str);
    auto cache = (DndcPyFileCache*)self;
    FileCache_maybe_remove(&cache->text_cache, path);
    FileCache_maybe_remove(&cache->b64_cache, path);
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
DndcPyFileCache_clear(PyObject* self){
    auto cache = (DndcPyFileCache*)self;
    FileCache_clear(&cache->text_cache);
    FileCache_clear(&cache->b64_cache);
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
DndcPyFileCache_paths(PyObject* self){
    auto cache = (DndcPyFileCache*)self;
    Py_ssize_t nfiles = cache->b64_cache.files.count + cache->text_cache.files.count;
    PyObject* result = PyList_New(nfiles);
    if(!result)
        goto error;
    Py_ssize_t index = 0;
    for(size_t i = 0, count=cache->b64_cache.files.count; i < count; i++, index++){
        auto path = &cache->b64_cache.files.data[i].sourcepath;
        PyObject* s = PyUnicode_FromStringAndSize(path->text, path->length);
        if(!s)
            goto error;
        PyList_SET_ITEM(result, index, s); // steals the reference
        }
    for(size_t i = 0, count = cache->text_cache.files.count; i < count; i++, index++){
        auto path = &cache->text_cache.files.data[i].sourcepath;
        PyObject* s = PyUnicode_FromStringAndSize(path->text, path->length);
        if(!s)
            goto error;
        PyList_SET_ITEM(result, index, s); // steals the reference
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
    auto obj = (DndcPyFileCache*)subtype->tp_alloc(subtype, 1);
    if(!obj)
        return NULL;
    obj->b64_cache = (FileCache){.allocator = get_mallocator()};
    obj->text_cache = (FileCache){.allocator = get_mallocator()};
    return (PyObject*)obj;
}

static
void
DndcPyFileCache_dealloc(PyObject* self){
    auto cache = (DndcPyFileCache*)self;
    FileCache_clear(&cache->text_cache);
    FileCache_clear(&cache->b64_cache);
    }

static
PyMethodDef DndcPyFileCache_methods[] = {
    {
        .ml_name = "remove",
        .ml_meth = (PyCFunction)DndcPyFileCache_remove,
        .ml_flags = METH_O,
        .ml_doc = "remove(filepath)\n"
                  "--\n"
                  "\n"
                  "Remove the given filepath (str) from the cache.\n",
    },
    {
        .ml_name = "clear",
        .ml_meth = (PyCFunction)DndcPyFileCache_clear,
        .ml_flags = METH_NOARGS,
        .ml_doc = "clear()\n"
                  "--\n"
                  "\n"
                  "Removes all cached files.\n",
    },
    {
        .ml_name = "paths",
        .ml_meth = (PyCFunction)DndcPyFileCache_paths,
        .ml_flags = METH_NOARGS,
        .ml_doc = "paths()\n"
                  "--\n"
                  "\n"
                  "Returns a list of the paths in the file cache.\n",
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
pydndc_htmlgen(PyObject*, PyObject*, PyObject*);

static
Nullable(PyObject*)
pydndc_anaylze_syntax_for_highlight(PyObject*, PyObject*, PyObject*);

static
PyMethodDef pydndc_methods[] = {
    {
        .ml_name = "reformat",
        .ml_meth = (PyCFunction)pydndc_reformat,
        .ml_flags = METH_VARARGS|METH_KEYWORDS,
        .ml_doc =
        "reformat(text, error_reporter=None)\n"
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
        "htmlgen(text, base_dir='.', error_reporter=None, file_cache=None, flags=0)\n"
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
        "flags: int\n"
        "    Bit flags controlling some behavior. The allowed flags are\n"
        "    exported on the module and are as follows:\n"
        "\n"
        "    DONT_INLINE_IMAGES: If set, don't embed images as base64 urls.\n"
        "                        This is overruled by USE_DND_URL_SCHEME.\n"
        "\n"
        "    NO_THREADS:         Do all work on the calling thread.\n"
        "\n"
        "    USE_DND_URL_SCHEME: Don't embed images as base64 urls. Instead\n"
        "                        Use a dnd:absolute/path/to/img url instead.\n"
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
        "message_type: int\n"
        "    The values are as follows:\n"
        "\n"
        "    ERROR_MESSAGE:     An error that caused parsing to fail and cannot\n"
        "                       be recovered from.\n"
        "\n"
        "    WARNING_MESSAGE:   Recoverable error or diagnostic.\n"
        "\n"
        "    NODELESS_MESSAGE:  An error that cannot be recovered from but\n"
        "                       that does not originate from a node or source\n"
        "                       location.\n"
        "\n"
        "    STATISTIC_MESSAGE: Not an error, a statistic like timing.\n"
        "\n"
        "    DEBUG_MESSAGE:     A debug statement, as requested by a flag.\n"
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
        .ml_name = "analyze_syntax_for_highlight",
        .ml_meth = (PyCFunction)pydndc_anaylze_syntax_for_highlight,
        .ml_flags = METH_VARARGS|METH_KEYWORDS,
        .ml_doc =
        "analyze_syntax_for_highlight(text)\n"
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
        "The dictionary is a mapping of lines (0-based) as the keys to a tuple of\n"
        "(type, col, byteoffset, length), which is Tuple[int, int, int, int].\n"
        "col, byteoffeset and length are all in bytes of utf-8.\n"
        "The type is one of the following:\n"
        "\n"
        "  DOUBLE_COLON       = 1\n"
        "  HEADER             = 2\n"
        "  NODE_TYPE          = 3\n"
        "  ATTRIBUTE          = 4\n"
        "  ATTRIBUTE_ARGUMENT = 5\n"
        "  CLASS              = 6\n"
        "  RAW_STRING         = 7\n"
        "\n"
        "These have been exported as module globals so you can refer to them by\n"
        "name instead of magic numbers.\n"
        ,
    },
    {NULL, NULL, 0, NULL}
};

static
PyModuleDef pydndc = {
    PyModuleDef_HEAD_INIT,
    .m_name="pydndc",
    .m_doc=NULL,
    .m_size=-1,
    .m_methods=pydndc_methods,
    .m_slots=NULL,
    .m_traverse=NULL,
    .m_clear=NULL,
    .m_free=NULL,
};


PyMODINIT_FUNC _Nullable
PyInit_pydndc(void){
    PyObject* mod = PyModule_Create(&pydndc);
    if(not mod)
        return NULL;
    if(PyType_Ready(&DndcPyFileCache_Type) != 0)
        return NULL;

    Py_INCREF(&DndcPyFileCache_Type);
    if(PyModule_AddObject(mod, "FileCache", (PyObject*)&DndcPyFileCache_Type) < 0){
        Py_DECREF(&DndcPyFileCache_Type);
        Py_DECREF(mod);
        return NULL;
    }
    PyModule_AddStringConstant(mod, "__version__",      DNDC_VERSION);
    PyModule_AddIntConstant(mod, "DOUBLE_COLON",        DNDC_SYNTAX_DOUBLE_COLON);
    PyModule_AddIntConstant(mod, "HEADER",              DNDC_SYNTAX_HEADER);
    PyModule_AddIntConstant(mod, "NODE_TYPE",           DNDC_SYNTAX_NODE_TYPE);
    PyModule_AddIntConstant(mod, "ATTRIBUTE",           DNDC_SYNTAX_ATTRIBUTE);
    PyModule_AddIntConstant(mod, "ATTRIBUTE_ARGUMENT",  DNDC_SYNTAX_ATTRIBUTE_ARGUMENT);
    PyModule_AddIntConstant(mod, "CLASS",               DNDC_SYNTAX_CLASS);
    PyModule_AddIntConstant(mod, "RAW_STRING",          DNDC_SYNTAX_RAW_STRING);
    PyModule_AddIntConstant(mod, "DONT_INLINE_IMAGES",  DNDC_DONT_INLINE_IMAGES);
    PyModule_AddIntConstant(mod, "NO_THREADS",          DNDC_NO_THREADS);
    PyModule_AddIntConstant(mod, "USE_DND_URL_SCHEME",  DNDC_USE_DND_URL_SCHEME);
    PyModule_AddIntConstant(mod, "STRIP_WHITESPACE",    DNDC_STRIP_WHITESPACE);
    PyModule_AddIntConstant(mod, "DONT_READ",           DNDC_DONT_READ);
    PyModule_AddIntConstant(mod, "PRINT_STATS",         DNDC_PRINT_STATS);
    PyModule_AddIntConstant(mod, "ERROR_MESSAGE",       DNDC_ERROR_MESSAGE);
    PyModule_AddIntConstant(mod, "WARNING_MESSAGE",     DNDC_WARNING_MESSAGE);
    PyModule_AddIntConstant(mod, "NODELESS_MESSAGE",    DNDC_NODELESS_MESSAGE);
    PyModule_AddIntConstant(mod, "STATISTIC_MESSAGE",   DNDC_STATISTIC_MESSAGE);
    PyModule_AddIntConstant(mod, "DEBUG_MESSAGE",       DNDC_DEBUG_MESSAGE);
    PyModule_AddIntConstant(mod, "INPUT_IS_UNTRUSTED",  DNDC_INPUT_IS_UNTRUSTED);
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
    auto fail = PyList_Append(list, tup);
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
    if(error_reporter and error_reporter == Py_None)
        error_reporter = NULL;
    if(error_reporter and !PyCallable_Check(error_reporter)){
        PyErr_SetString(PyExc_TypeError, "error_reporter must be a callable");
        return NULL;
        }
    LongString source = pystring_borrow_longstring(text);
    uint64_t flags = 0;
    // flags |= DNDC_DONT_PRINT_ERRORS;
    // flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    flags |= DNDC_REFORMAT_ONLY;
    LongString output = {};
    DndcErrorFunc* func = error_reporter?pydndc_collect_errors:NULL;
    PyObject* error_list = func? PyList_New(0) : NULL;
    PyObject* result = NULL;
    auto e = run_the_dndc(flags, LS(""), source, LS(""), &output, NULL, NULL, func, error_list, NULL, NULL, NULL, NULL, NULL);
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
    if(e.errored){
        PyErr_SetString(PyExc_ValueError, "Format error.");
        goto finally;
        }
    result = PyUnicode_FromStringAndSize(output.text, output.length);
    finally:
    Py_XDECREF(error_list);
    const_free(output.text);
    return result;
}

static
int
pydndc_add_dependencies(Nullable(void*)user_data, size_t npaths, StringView* paths){
    PyObject* list = user_data;
    for(size_t i = 0; i < npaths; i++){
        auto path = paths[i];
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
    unsigned long long flags = 0;
    _Static_assert(sizeof(flags) == sizeof(uint64_t), "");
    enum {WHITELIST = 0
        | DNDC_DONT_INLINE_IMAGES
        | DNDC_NO_THREADS
        | DNDC_USE_DND_URL_SCHEME
        | DNDC_PRINT_STATS
        | DNDC_STRIP_WHITESPACE
        | DNDC_DONT_READ
        | DNDC_INPUT_IS_UNTRUSTED
        };
    const char* const keywords[] = {"text", "base_dir", "error_reporter", "file_cache", "flags", "output_name", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O!OOKO!:htmlgen", (char**)keywords, &PyUnicode_Type, &text, &PyUnicode_Type, &base_dir, &error_reporter, &file_cache, &flags, &PyUnicode_Type, &output_name)){
        return NULL;
        }
    PopDiagnostic();
    // Check for any flags that python callers shouldn't be able to set.
    if((flags & WHITELIST) != flags){
        PyErr_SetString(PyExc_ValueError, "flags argument contains illegal bits");
        return NULL;
        }
    if(error_reporter and error_reporter == Py_None)
        error_reporter = NULL;
    if(error_reporter and !PyCallable_Check(error_reporter)){
        PyErr_SetString(PyExc_TypeError, "error_reporter must be a callable");
        return NULL;
        }
    if(file_cache and file_cache == Py_None)
        file_cache = NULL;
    if(file_cache and !PyObject_IsInstance(file_cache, (PyObject*)&DndcPyFileCache_Type)){
        PyErr_SetString(PyExc_TypeError, "file_cache must be a FileCache");
        return NULL;
        }
    LongString source = pystring_borrow_longstring(text);
    LongString base_str = base_dir? pystring_borrow_longstring(base_dir): LS("");
    // flags |= DNDC_DONT_PRINT_ERRORS;
    // flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    LongString output = {};
    DndcErrorFunc* func = error_reporter?pydndc_collect_errors:NULL;
    PyObject* error_list = func? PyList_New(0) : NULL;
    PyObject* result = NULL;
    PyObject* depends_list = PyList_New(0);
    FileCache* textcache = NULL;
    FileCache* b64cache = NULL;
    if(file_cache){
        auto cache = (DndcPyFileCache*)file_cache;
        textcache = &cache->text_cache;
        b64cache = &cache->b64_cache;
        }
    LongString outname = output_name?pystring_borrow_longstring(output_name) : LS("this.html");
    auto e = run_the_dndc(flags, base_str, source, outname, &output, b64cache, textcache, func, error_list, pydndc_add_dependencies, depends_list, NULL, NULL, NULL);
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
    if(e.errored){
        PyErr_SetString(PyExc_ValueError, "html error.");
        goto finally;
        }
    result = Py_BuildValue("s#O", output.text, (Py_ssize_t)output.length, depends_list);
    finally:
    Py_XDECREF(depends_list);
    Py_XDECREF(error_list);
    const_free(output.text);
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
    PyObject* value = Py_BuildValue("iinn", type, col, (Py_ssize_t)(begin - cd->begin), (Py_ssize_t)length);
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
    auto error = dndc_analyze_syntax(source, pydndc_collect_syntax_tokens, &cd);
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
#ifdef __clang__
#pragma clang assume_nonnull end
#elif defined(__GNUC__)
PopDiagnostic();
#endif
