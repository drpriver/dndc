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

PyDoc_STRVAR(
    draw_a_dungeon_doc,
    "draw_a_dungeon(*, blocked='n', filename='dungeon_visualization', x_lim=0, y_lim=0, z_lim=0, num_rooms=0, void_probability=0, vert_probability=0, trim_distance=4, show_voids=False, maximagesize=4,)\n"
    "--\n"
    "\n"
    "Randomly generates a dungeon graph using the Priver algorithm\n"
    "Then, saves it to the file location as a bmp.\n");

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
Nullable(PyObject*)
pydndc_reformat(Nonnull(PyObject*)mod, Nonnull(PyObject*)args, Nonnull(PyObject*)kwargs){
    (void)mod;
    PyObject* text;
    const char* const keywords[] = { "text", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!:parse_and_append_children", (char**)keywords, &PyUnicode_Type, &text)){
        return NULL;
        }
    PopDiagnostic();
    LongString source = pystring_borrow_longstring(text);
    uint64_t flags = 0;
    flags |= DNDC_SOURCE_PATH_IS_DATA_NOT_PATH;
    flags |= DNDC_OUTPUT_PATH_IS_OUT_PARAM;
    flags |= DNDC_PYTHON_IS_INIT;
    flags |= DNDC_DONT_PRINT_ERRORS;
    flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    flags |= DNDC_REFORMAT_ONLY;
    LongString output = {};
    auto e = run_the_dndc(flags, SV(""), source, &output, LS(""), NULL);
    if(e.errored){
        PyErr_SetString(PyExc_ValueError, "Format error (wow I need real error reporting)");
        return NULL;
        }
    PyObject* result = PyUnicode_FromStringAndSize(output.text, output.length);
    const_free(output.text);
    return result;
    }

static
Nullable(PyObject*)
pydndc_htmlgen(Nonnull(PyObject*)mod, Nonnull(PyObject*)args, Nonnull(PyObject*)kwargs){
    (void)mod;
    PyObject* text;
    PyObject* base_dir = NULL;
    const char* const keywords[] = {"text", "base_dir", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O!:parse_and_append_children", (char**)keywords, &PyUnicode_Type, &text, &PyUnicode_Type, &base_dir)){
        return NULL;
        }
    PopDiagnostic();
    LongString source = pystring_borrow_longstring(text);
    StringView base_str = base_dir? pystring_borrow_stringview(base_dir): SV("");
    uint64_t flags = 0;
    flags |= DNDC_SOURCE_PATH_IS_DATA_NOT_PATH;
    flags |= DNDC_OUTPUT_PATH_IS_OUT_PARAM;
    flags |= DNDC_PYTHON_IS_INIT;
    flags |= DNDC_DONT_PRINT_ERRORS;
    flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    LongString output = {};
    auto e = run_the_dndc(flags, base_str, source, &output, LS(""), NULL);
    if(e.errored){
        PyErr_SetString(PyExc_ValueError, "Format error (wow I need real error reporting)");
        return NULL;
        }
    PyObject* result = PyUnicode_FromStringAndSize(output.text, output.length);
    const_free(output.text);
    return result;
    }
