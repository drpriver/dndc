//
// Exposes dndc as a c-extension for python.
//

#define PYTHONMODULE
#include "dndc.c"

static
Nullable(PyObject*)
pydndc_reformat(Nonnull(PyObject*), Nonnull(PyObject*), Nonnull(PyObject*));

static
Nullable(PyObject*)
pydndc_htmlgen(Nonnull(PyObject*), Nonnull(PyObject*), Nonnull(PyObject*));

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
    return PyModule_Create(&pydndc);
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
    auto e = run_the_dndc(flags, SV(""), source, &output, LS(""), NULL, func, error_list);
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
    LongString output = {};
    ErrorFunc* func = error_reporter?pydndc_collect_errors:NULL;
    PyObject* error_list = func? PyList_New(0) : NULL;
    PyObject* result = NULL;
    auto e = run_the_dndc(flags, base_str, source, &output, LS(""), NULL, func, error_list);
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
    result = PyUnicode_FromStringAndSize(output.text, output.length);
    finally:
    Py_XDECREF(error_list);
    const_free(output.text);
    return result;
    }
