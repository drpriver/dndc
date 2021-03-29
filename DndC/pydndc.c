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
        "reformat(text)\n"
        "--\n"
        "\n"
        "Reformat the given .dnd string into a nicely formatted representation.\n"
        "\n"
        "Args:\n"
        "  text (str): The .dnd string\n"
        "\n"
        "Returns: str\n"
        "  The reformatted string\n"
        "\n"
        "Throws: ValueError\n"
        "  Throws ValueError if there is a syntax error in the given string\n"
        ,
    },
    {
        .ml_name = "htmlgen",
        .ml_meth = (PyCFunction)pydndc_htmlgen,
        .ml_flags = METH_VARARGS|METH_KEYWORDS,
        .ml_doc =
        "htmlgen(text, base_dir='.')\n"
        "--\n"
        "\n"
        "Parses and converts the .dnd string into html, returning as a string.\n"
        "Args:\n"
        "  text (str): The .dnd string\n"
        "  base_dir (str): For relative filepaths referenced in the document,\n"
        "                 what those paths are relative to.\n"
        "                 Defaults to the current directory.\n"
        "\n"
        "Returns: str\n"
        "  The html\n"
        "\n"
        "Throws: ValueError\n"
        "  Throws ValueError if there is a syntax error in the given string.\n"
        "  Can also throw due to missing files.\n"
        "  Can also throw due to errors in embedded python blocks.\n"
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
