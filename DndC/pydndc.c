//
// Exposes dndc as a c-extension for python.
//

#include "dndc_flags.h"
#define PYTHONMODULE
#include "dndc.c"

static
Nullable(PyObject*)
pydndc_reformat(Nonnull(PyObject*), Nonnull(PyObject*), Nonnull(PyObject*));

static
Nullable(PyObject*)
pydndc_htmlgen(Nonnull(PyObject*), Nonnull(PyObject*), Nonnull(PyObject*));

static
Nullable(PyObject*)
pydndc_anaylze_syntax_for_highlight(Nonnull(PyObject*), Nonnull(PyObject*), Nonnull(PyObject*));

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
        "htmlgen(text, base_dir='.', error_reporter=None)\n"
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
        "Returns:\n"
        "--------\n"
        "str: The html.\n"
        "List[str]: the files the string depends on (such as an import).\n"
        "\n"
        "Throws:\n"
        "-------\n"
        "Throws ValueError if there is a syntax error in the given string.\n"
        "Can also throw due to missing files.\n"
        "Can also throw due to errors in embedded python blocks.\n"
        "\n"
        "\n"
        "If the error_reporter is given, it will be called with the following\n"
        "arguments:\n"
        "\n"
        "Error Reporter Arguments:\n"
        "-------------------------\n"
        "message_type: int\n"
        "    The values are as follows:\n"
        "        0: Error. An error that caused parsing to fail.\n"
        "        1: Warning. Something is fishy or otherwise not good.\n"
        "        2: SystemError. Originated from the system, not the text.\n"
        "        3: Info. Not an error, a statistic like timing.\n"
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
        "The dictionary a mapping of lines (0-based) as the keys to a tuple of\n"
        "(type, col, byteoffset, length)\n"
        "col, byteoffeset and length are all in bytes of utf-8.\n"
        "The type is one of the following:\n"
        "  DOUBLE_COLON       = 1\n"
        "  HEADER             = 2\n"
        "  NODE_TYPE          = 3\n"
        "  ATTRIBUTE          = 4\n"
        "  ATTRIBUTE_ARGUMENT = 5\n"
        "  CLASS              = 6\n"
        "  RAW_STRING         = 7\n"
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


PyMODINIT_FUNC
PyInit_pydndc(void) {
    auto e = docparse_init_types();
    if(e.errored)
        return NULL;
    PyObject* mod = PyModule_Create(&pydndc);
    if(not mod)
        return NULL;

    PyModule_AddStringConstant(mod, "__version__", DNDC_VERSION);
    PyModule_AddIntConstant(mod, "DOUBLE_COLON", DNDC_SYNTAX_DOUBLE_COLON);
    PyModule_AddIntConstant(mod, "HEADER", DNDC_SYNTAX_HEADER);
    PyModule_AddIntConstant(mod, "NODE_TYPE", DNDC_SYNTAX_NODE_TYPE);
    PyModule_AddIntConstant(mod, "ATTRIBUTE", DNDC_SYNTAX_ATTRIBUTE);
    PyModule_AddIntConstant(mod, "ATTRIBUTE_ARGUMENT", DNDC_SYNTAX_ATTRIBUTE_ARGUMENT);
    PyModule_AddIntConstant(mod, "CLASS", DNDC_SYNTAX_CLASS);
    PyModule_AddIntConstant(mod, "RAW_STRING", DNDC_SYNTAX_RAW_STRING);
    return mod;
}

static
void
pydndc_collect_errors(Nullable(void*)user_data, int type, const char* _Nonnull filename, int filename_len, int line, int col, const char* _Nonnull message, int message_len){
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
pydndc_reformat(Nonnull(PyObject*)mod, Nonnull(PyObject*)args, Nonnull(PyObject*)kwargs){
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
    flags |= DNDC_SOURCE_PATH_IS_DATA_NOT_PATH;
    flags |= DNDC_OUTPUT_PATH_IS_OUT_PARAM;
    flags |= DNDC_PYTHON_IS_INIT;
    // flags |= DNDC_DONT_PRINT_ERRORS;
    // flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    flags |= DNDC_REFORMAT_ONLY;
    LongString output = {};
    ErrorFunc* func = error_reporter?pydndc_collect_errors:NULL;
    PyObject* error_list = func? PyList_New(0) : NULL;
    PyObject* result = NULL;
    auto e = run_the_dndc(flags, SV(""), source, &output, (DependsArg){.path=LS("")}, NULL, func, error_list);
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
void
pydndc_add_dependency(Nullable(void*)user_data, StringView path){
    if(PyErr_Occurred())
        return;
    PyObject* list = user_data;
    PyObject* str = PyUnicode_FromStringAndSize(path.text, path.length);
    PyList_Append(list, str);
    Py_XDECREF(str);
}

static
Nullable(PyObject*)
pydndc_htmlgen(Nonnull(PyObject*)mod, Nonnull(PyObject*)args, Nonnull(PyObject*)kwargs){
    (void)mod;
    PyObject* text;
    PyObject* base_dir = NULL;
    PyObject* error_reporter = NULL;
    const char* const keywords[] = {"text", "base_dir", "error_reporter", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O!O:htmlgen", (char**)keywords, &PyUnicode_Type, &text, &PyUnicode_Type, &base_dir, &error_reporter)){
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
    StringView base_str = base_dir? pystring_borrow_stringview(base_dir): SV("");
    uint64_t flags = 0;
    flags |= DNDC_SOURCE_PATH_IS_DATA_NOT_PATH;
    flags |= DNDC_OUTPUT_PATH_IS_OUT_PARAM;
    flags |= DNDC_PYTHON_IS_INIT;
    // flags |= DNDC_DONT_PRINT_ERRORS;
    // flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    flags |= DNDC_DEPENDS_IS_CALLBACK;
    LongString output = {};
    ErrorFunc* func = error_reporter?pydndc_collect_errors:NULL;
    PyObject* error_list = func? PyList_New(0) : NULL;
    PyObject* result = NULL;
    PyObject* depends_list = PyList_New(0);
    DependsArg depends = {
        .callback = pydndc_add_dependency,
        .user_data = depends_list,
        };
    auto e = run_the_dndc(flags, base_str, source, &output, depends, NULL, func, error_list);
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
    Nonnull(const char*) begin;
    Nonnull(PyObject*) dict;
};

static
void
pydndc_collect_syntax_tokens(Nullable(void*)user_data, int type, int line, int col, Nonnull(const char*)begin, size_t length){
    assert(user_data);
    struct CollectData* cd = user_data;
    PyObject* d = cd->dict;
    if(PyErr_Occurred())
        return;
    PyObject* key = PyLong_FromLong(line);
    PyObject* value = Py_BuildValue("iinn", type, col, (Py_ssize_t)(begin - cd->begin), (Py_ssize_t)length);
    // TODO: handle allocation failures I guess.
    assert(key);
    assert(value);
    PyObject * list;
    if(PyDict_Contains(d, key)){
        list = PyDict_GetItem(d, key); // borrow
        assert(list);
        }
    else {
        list = PyList_New(0);
        assert(list);
        auto fail = PyDict_SetItem(d, key, list); // does not steal
        assert(fail == 0);
        Py_XDECREF(list);
        }
    auto fail = PyList_Append(list, value);
    assert(fail == 0);
    Py_XDECREF(key);
    Py_XDECREF(value);
}

static
Nullable(PyObject*)
pydndc_anaylze_syntax_for_highlight(Nonnull(PyObject*)mod, Nonnull(PyObject*)args, Nonnull(PyObject*)kwargs){
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
