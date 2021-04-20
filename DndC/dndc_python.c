#ifndef DNDC_PYTHON_C
#define DNDC_PYTHON_C
#include "dndc_funcs.h"
#include "dndc_types.h"
#include "msb_extensions.h"
#include "msb_format.h"
#include "path_util.h"

/* Python */

#include "pyhead.h"
// not a fan that the includes are so generic (why not <Python/code.h>?)
#include <frameobject.h>
#include <code.h>

PushDiagnostic();
SuppressUnusedFunction();
static inline
LongString
pystring_to_longstring(Nonnull(PyObject*)pyobj, const Allocator a){
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
PopDiagnostic();

static inline
StringView
pystring_to_stringview(Nonnull(PyObject*)pyobj, const Allocator a){
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
pystring_borrow_stringview(Nonnull(PyObject*)pyobj){
    const char* text;
    Py_ssize_t length;
    text = PyUnicode_AsUTF8AndSize(pyobj, &length);
    unhandled_error_condition(!text);
    return (StringView){.text=text, .length=length};
    }
PushDiagnostic();
SuppressUnusedFunction();
static inline
LongString
pystring_borrow_longstring(Nonnull(PyObject*)pyobj){
    const char* text;
    Py_ssize_t length;
    text = PyUnicode_AsUTF8AndSize(pyobj, &length);
    unhandled_error_condition(!text);
    return (LongString){.text=text, .length=length};
    }
PopDiagnostic();


typedef struct NodeTypeEnum {
    PyObject_HEAD
    NodeType type;
    }NodeTypeEnum;

static
Nullable(PyObject*)
NodeTypeEnum_repr(Nonnull(NodeTypeEnum*)e){
    if(e->type > NODE_INVALID or e->type < 0){
        PyErr_Format(PyExc_RuntimeError, "Somehow we have an enum with an invalid value: %d", (int)e->type);
        return NULL;
        }
    auto name = nodenames[e->type];
    return PyUnicode_FromFormat("NodeType.%s", name.text);
    }

static
PyObject* _Nullable
NodeTypeEnum_getattr(Nonnull(NodeTypeEnum*)e, Nonnull(const char*)name){
    if(e->type > NODE_INVALID or e->type < 0){
        PyErr_Format(PyExc_RuntimeError, "Somehow we have an enum with an invalid value: %d", (int)e->type);
        return NULL;
        }
    if(strcmp(name, "name")==0){
        auto enu_name = nodenames[e->type];
        return PyUnicode_FromStringAndSize(enu_name.text, enu_name.length);
        }
    if(strcmp(name, "value")==0){
        return PyLong_FromLong(e->type);
        }
    PyErr_Format(PyExc_AttributeError, "Unknown attribute on NodeTypeEnum: %s", name);
    return NULL;
    }

// decl
static PyTypeObject NodeTypeEnumType;

static
PyObject* _Nullable
NodeTypeEnum_richcmp(Nonnull(PyObject*)a, Nonnull(PyObject*)b, int cmp){
    auto check = PyObject_IsInstance(b, (PyObject*)&NodeTypeEnumType);
    if(check == -1)
        return NULL;
    if(check == 0){
        Py_RETURN_NOTIMPLEMENTED;
        }
    auto lhs = (NodeTypeEnum*)a;
    auto rhs = (NodeTypeEnum*)b;
    if(cmp == Py_EQ){
        if(lhs->type == rhs->type)
            Py_RETURN_TRUE;
        else
            Py_RETURN_FALSE;
        }
    if(cmp == Py_NE){
        if(lhs->type != rhs->type)
            Py_RETURN_TRUE;
        else
            Py_RETURN_FALSE;
        }
    Py_RETURN_NOTIMPLEMENTED;
    }

// definition
static PyTypeObject NodeTypeEnumType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "docparser.NodeType",
    .tp_basicsize = sizeof(NodeTypeEnum),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "NodeType Enum",
    .tp_repr = (reprfunc)NodeTypeEnum_repr,
    .tp_getattr = (getattrfunc)&NodeTypeEnum_getattr,
    .tp_richcompare = &NodeTypeEnum_richcmp,
    };

static
Nonnull(PyObject*)
make_node_type_enum(NodeType t){
    NodeTypeEnum* self = (NodeTypeEnum*)NodeTypeEnumType.tp_alloc(&NodeTypeEnumType, 0);
    unhandled_error_condition(!self);
    self->type = t;
    return (PyObject*)self;
    }

typedef Nullable(PyObject*) (*_Nonnull NodeMethod)(Nonnull(DndcContext*), NodeHandle, Nonnull(PyObject*), Nullable(PyObject*));

typedef struct NodeBoundMethod {
    PyObject_HEAD
    Nonnull(DndcContext*)ctx;
    NodeHandle handle;
    NodeMethod func;
    } NodeBoundMethod;

static
Nullable(PyObject*)
NodeBound_call(Nonnull(PyObject*)self, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    auto meth = (NodeBoundMethod*)self;
    return meth->func(meth->ctx, meth->handle, args, kwargs);
    }

static PyTypeObject NodeBoundMethodType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "docparser.NodeBoundMethod",
    .tp_basicsize = sizeof(NodeBoundMethod),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Node bound method",
    .tp_call = &NodeBound_call,
    };

static
Nonnull(PyObject*)
make_node_bound_method(Nonnull(DndcContext*)ctx, NodeHandle handle, NodeMethod func){
    NodeBoundMethod* self = (NodeBoundMethod*)NodeBoundMethodType.tp_alloc(&NodeBoundMethodType, 0);
    unhandled_error_condition(!self);
    self->ctx = ctx;
    self->handle = handle;
    self->func = func;
    return (PyObject*)self;
    }

Nullable(PyObject*)
py_parse_and_append_children(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    PyObject* text;
    const char* const keywords[] = { "text", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!:parse_and_append_children", (char**)keywords, &PyUnicode_Type, &text)){
        return NULL;
        }
    PopDiagnostic();
    // We dupe this as we have no guarantee that the python
    // string will last beyond this execution and we store pointers
    // into the original source string.
    auto source_text = pystring_to_longstring(text, ctx->allocator);
    auto old_filename = ctx->filename;

    auto parse_e = dndc_parse(ctx, handle, SV("(generated string from script)"), source_text.text);
    if(parse_e.errored){
        PyErr_SetString(PyExc_ValueError, "Error while parsing");
        return NULL;
        }

    ctx->filename = old_filename;
    Py_RETURN_NONE;
    }

typedef struct DndClassesList {
    PyObject_HEAD
    Nonnull(DndcContext*)ctx;
    NodeHandle handle;
    } DndClassesList;

static
Py_ssize_t
DndClasses_length(Nonnull(DndClassesList*)list){
    auto node = get_node(list->ctx, list->handle);
    return (Py_ssize_t)node->classes.count;
    }

static
Nullable(PyObject*)
DndClasses_getitem(Nonnull(DndClassesList*)list, Py_ssize_t index){
    auto node = get_node(list->ctx, list->handle);
    auto length = node->classes.count;
    if(index < 0){
        index += length;
        }
    if(index >= length){
        PyErr_SetString(PyExc_IndexError, "Index out of bounds");
        return NULL;
        }
    auto sv = node->classes.data[index];
    return PyUnicode_FromStringAndSize(sv.text, sv.length);
    }

static
int
DndClasses_contains(Nonnull(DndClassesList*)list, PyObject*_Nonnull query){
    if(!PyUnicode_Check(query)){
        PyErr_SetString(PyExc_TypeError, "Only strings can be in classes lists");
        return -1;
        }
    auto node = get_node(list->ctx, list->handle);
    size_t n_classes = node->classes.count;
    if(!n_classes)
        return 0;

    auto key_sv = pystring_borrow_stringview(query);
    for(size_t i = 0; i < n_classes; i++){
        auto class_string = node->classes.data[i];
        if(SV_equals(class_string, key_sv))
            return 1;
        }
    return 0;
    }

static
int
DndClasses_setitem(Nonnull(DndClassesList*)list, Py_ssize_t index, Nullable(PyObject*) value){
    if(!value){
        PyErr_SetString(PyExc_NotImplementedError, "Deletion is unsupported");
        return -1;
        }
    auto node = get_node(list->ctx, list->handle);
    auto nclasses = node->classes.count;
    if(index < 0){
        index += nclasses;
        }
    if(index >= nclasses){
        PyErr_SetString(PyExc_IndexError, "Index out of bounds");
        return -1;
        }
    node->classes.data[index] = pystring_to_stringview((Nonnull(PyObject*))value, list->ctx->allocator);
    return 0;
    }

static
Nullable(PyObject*)
DndClasses_append(Nonnull(DndClassesList*)list, Nonnull(PyObject*)args){
    PyObject* text;
    if(!PyArg_ParseTuple(args, "O!:append", &PyUnicode_Type, &text))
        return NULL;
    auto node = get_node(list->ctx, list->handle);
    StringView sv = pystring_to_stringview(text, list->ctx->allocator);
    Marray_push(StringView)(&node->classes, list->ctx->allocator, sv);
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
DndClasses_repr(Nonnull(DndClassesList*)list){
    auto node = get_node(list->ctx, list->handle);
    MStringBuilder msb = {.allocator=list->ctx->temp_allocator};
    msb_write_char(&msb, '[');
    for(size_t i = 0; i < node->classes.count; i++){
        if(i != 0)
            msb_write_str(&msb, ", ", 2);
        msb_write_char(&msb, '\'');
        auto sv = node->classes.data[i];
        msb_write_str(&msb, sv.text, sv.length);
        msb_write_char(&msb, '\'');
        }
    msb_write_char(&msb, ']');
    auto str = msb_borrow(&msb);
    auto result = PyUnicode_FromStringAndSize(str.text, str.length);
    msb_destroy(&msb);
    return result;
    }


static PyMethodDef DndClassesList_methods[] = {
    {"append", (PyCFunction)&DndClasses_append, METH_VARARGS, "add a class string"},
    {NULL, NULL, 0, NULL}, // Sentinel
    };

static PySequenceMethods DndClasses_sq_methods = {
    .sq_length = (lenfunc)DndClasses_length,
    .sq_concat = NULL,
    .sq_repeat = NULL,
    .sq_item = (ssizeargfunc)DndClasses_getitem,
    .was_sq_slice = NULL,
    .sq_ass_item = (ssizeobjargproc)DndClasses_setitem,
    .was_sq_ass_slice = NULL,
    .sq_contains = (objobjproc)DndClasses_contains,
    .sq_inplace_concat = NULL,
    .sq_inplace_repeat = NULL,
};
static PyTypeObject DndClassesListType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "docparser.ClassList",
    .tp_basicsize = sizeof(DndClassesList),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Classes List Wrapper",
    .tp_methods = DndClassesList_methods,
    .tp_getattro = PyObject_GenericGetAttr,
    .tp_as_sequence = &DndClasses_sq_methods,
    .tp_repr = (reprfunc)&DndClasses_repr,
    };

static
Nonnull(PyObject*)
make_classes_list(Nonnull(DndcContext*)ctx, NodeHandle handle){
    DndClassesList* self = (DndClassesList*)DndClassesListType.tp_alloc(&DndClassesListType, 0);
    unhandled_error_condition(!self);
    self->ctx = ctx;
    self->handle = handle;
    return (PyObject*)self;
    }

typedef struct DndAttributesMap {
    PyObject_HEAD
    Nonnull(DndcContext*)ctx;
    NodeHandle handle;
    } DndAttributesMap;

static
Nonnull(PyObject*)
DndAttributesMap_items(Nonnull(DndAttributesMap*)map, Nonnull(PyObject*)unused){
    (void)unused;
    auto node = get_node(map->ctx, map->handle);
    auto attributes = &node->attributes;
    size_t count = attributes->count;
    PyObject* result = PyList_New(count);
    for(size_t i = 0; i < count; i++){
        auto attr = &attributes->data[i];
        // new ref
        auto item = Py_BuildValue("s#s#", attr->key.text, attr->key.length, attr->value.text, attr->value.length);
        // but then steals the ref
        PyList_SET_ITEM(result, i, item);
        }
    return result;
    }

static
Py_ssize_t
DndAttributesMap_length(Nonnull(DndAttributesMap*)list){
    auto node = get_node(list->ctx, list->handle);
    return (Py_ssize_t)node->attributes.count;
    }

static
Nullable(PyObject*)
DndAttributesMap_getitem(Nonnull(DndAttributesMap*)map, Nonnull(PyObject*) key){
    if(!PyUnicode_Check(key)){
        PyErr_SetString(PyExc_TypeError, "Attribute maps must be indexed by strings");
        return NULL;
        }
    auto key_sv = pystring_borrow_stringview(key);
    auto node = get_node(map->ctx, map->handle);
    auto attributes = &node->attributes;
    auto count = attributes->count;
    for(size_t i = 0; i < count; i++){
        auto attr = &attributes->data[i];
        if(SV_equals(attr->key, key_sv))
            return PyUnicode_FromStringAndSize(attr->value.text, attr->value.length);
        }
    PyErr_Format(PyExc_KeyError, "Unknown attribute: '%s'", key_sv.text);
    return NULL;
    }

static
int
DndAttributesMap_contains(Nonnull(DndAttributesMap*)map, Nonnull(PyObject*) key){
    if(!PyUnicode_Check(key)){
        PyErr_SetString(PyExc_TypeError, "Attribute maps must be indexed by strings");
        return -1;
        }
    auto key_sv = pystring_borrow_stringview(key);
    auto node = get_node(map->ctx, map->handle);
    auto attributes = &node->attributes;
    auto count = attributes->count;
    for(size_t i = 0; i < count; i++){
        auto attr = &attributes->data[i];
        if(SV_equals(attr->key, key_sv))
            return 1;
        }
    return 0;
    }

static
int
DndAttributesMap_setitem(Nonnull(DndAttributesMap*)map, Nonnull(PyObject*) key, Nullable(PyObject*) value){
    if(!PyUnicode_Check(key)){
        PyErr_SetString(PyExc_TypeError, "Attribute maps must be indexed by strings");
        return -1;
        }
    if(value and !PyUnicode_Check(value)){
        PyErr_SetString(PyExc_TypeError, "Attribute maps can only have string values");
        return -1;
        }
    auto key_sv = pystring_borrow_stringview(key);
    auto node = get_node(map->ctx, map->handle);
    auto attributes = &node->attributes;
    auto count = attributes->count;
    for(size_t i = 0; i < count; i++){
        auto attr = &attributes->data[i];
        if(SV_equals(attr->key, key_sv)){
            if(value){
                attr->value = pystring_to_stringview((Nonnull(PyObject*))value, map->ctx->allocator);
                return 0;
                }
            else {
                Marray_remove(Attribute)(attributes, i);
                return 0;
                }
            }
        }
    if(!value){
        PyErr_Format(PyExc_KeyError, "Unknown attribute: '%s'", key_sv.text);
        return -1;
        }
    const char* key_copy = Allocator_dupe(map->ctx->allocator, key_sv.text, key_sv.length);
    auto attr = Marray_alloc(Attribute)(&node->attributes, map->ctx->allocator);
    attr->key.length = key_sv.length;
    attr->key.text = key_copy;
    attr->value = pystring_to_stringview((Nonnull(PyObject*))value, map->ctx->allocator);
    return 0;
    }

static
Nullable(PyObject*)
DndAttributesMap_repr(Nonnull(DndAttributesMap*)map){
    auto node = get_node(map->ctx, map->handle);
    auto attributes = &node->attributes;
    auto count = attributes->count;
    MStringBuilder msb = {.allocator = map->ctx->temp_allocator};
    msb_write_char(&msb, '{');
    for(size_t i = 0; i < count; i++){
        auto attr = &attributes->data[i];
        if(i != 0)
            msb_write_str(&msb,  ", ", 2);
        msb_write_char(&msb,  '\'');
        auto key = attr->key;
        msb_write_str(&msb, key.text, key.length);
        msb_write_char(&msb, '\'');
        msb_write_char(&msb, ':');
        msb_write_char(&msb, ' ');
        msb_write_char(&msb, '\'');
        auto val = attr->value;
        msb_write_str(&msb, val.text, val.length);
        msb_write_char(&msb, '\'');
        }
    msb_write_char(&msb, '}');
    auto str = msb_borrow(&msb);
    auto result = PyUnicode_FromStringAndSize(str.text, str.length);
    msb_destroy(&msb);
    return result;
    }


static
Nullable(PyObject*)
DndAttributesMap_add(Nonnull(DndAttributesMap*)map, Nonnull(PyObject*)arg){
    if(!PyUnicode_Check(arg)){
        PyErr_SetString(PyExc_TypeError, "Argument to add must be a string");
        return NULL;
        }
    auto ctx = map->ctx;
    auto key = pystring_to_stringview(arg, ctx->allocator);
    auto node = get_node(ctx, map->handle);
    auto attributes = &node->attributes;
    auto attr = Marray_alloc(Attribute)(attributes, ctx->allocator);
    attr->key = key;
    attr->value = SV("");
    Py_RETURN_NONE;
    }
static PyMethodDef DndAttributesMap_methods[] = {
    {"items", (PyCFunction)&DndAttributesMap_items, METH_NOARGS, "returns a list of (key, value) tuples"},
    {"add", (PyCFunction)&DndAttributesMap_add, METH_O, "Add a single string item to the attributes. It's corresponding value will be the empty string."},
    {NULL, NULL, 0, NULL}, // Sentinel
    };

static PySequenceMethods DndAttributesMap_sq_methods = {
    .sq_length = (lenfunc)&DndAttributesMap_length,
    .sq_concat = NULL,
    .sq_repeat = NULL,
    .sq_item = NULL,
    .was_sq_slice = NULL,
    .sq_ass_item = NULL,
    .was_sq_ass_slice = NULL,
    .sq_contains = (objobjproc)&DndAttributesMap_contains,
    .sq_inplace_concat = NULL,
    .sq_inplace_repeat = NULL,
};

static PyMappingMethods DndAttributesMap_map_methods = {
    .mp_length = (lenfunc)&DndAttributesMap_length,
    .mp_subscript = (binaryfunc)&DndAttributesMap_getitem,
    .mp_ass_subscript = (objobjargproc)&DndAttributesMap_setitem,
};

static PyTypeObject DndAttributesMapType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "docparser.AttributesMap",
    .tp_basicsize = sizeof(DndAttributesMap),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Attributes Map Wrapper",
    .tp_methods = DndAttributesMap_methods,
    .tp_getattro = PyObject_GenericGetAttr,
    .tp_setattro = PyObject_GenericSetAttr,
    .tp_as_mapping = &DndAttributesMap_map_methods,
    .tp_as_sequence = &DndAttributesMap_sq_methods,
    .tp_repr = (reprfunc)&DndAttributesMap_repr,
    };

static
Nonnull(PyObject*)
make_attributes_map(Nonnull(DndcContext*)ctx, NodeHandle handle){
    DndAttributesMap* self = (DndAttributesMap*)DndAttributesMapType.tp_alloc(&DndAttributesMapType, 0);
    unhandled_error_condition(!self);
    self->ctx = ctx;
    self->handle = handle;
    return (PyObject*)self;
    }

typedef struct DndNode {
    PyObject_HEAD
    Nonnull(DndcContext*)ctx;
    NodeHandle handle;
    } DndNode;

static PyMethodDef DndNode_methods[] = {
    {NULL, NULL, 0, NULL}, // Sentinel
    };
static PyObject* _Nullable DndNode_getattr(Nonnull(DndNode*), Nonnull(const char*));
static int DndNode_setattr(Nonnull(DndNode*), Nonnull(const char*), Nullable(PyObject *));
static Nullable(PyObject*) DndNode_repr(Nonnull(DndNode*));


static PyTypeObject DndNodeType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "docparser.Node",
    .tp_basicsize = sizeof(DndNode),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Node Wrapper",
    .tp_methods = DndNode_methods,
    .tp_getattr = (getattrfunc)&DndNode_getattr,
    .tp_setattr = (setattrfunc)&DndNode_setattr,
    .tp_repr = (reprfunc)&DndNode_repr,
    };

static
Nullable(PyObject*)
DndNode_repr(Nonnull(DndNode*)self){
    auto node = get_node(self->ctx, self->handle);
    // format a buffer as python apparently doesn't support %.*s
    MStringBuilder msb = {.allocator=self->ctx->temp_allocator};
    if(not node->classes.count)
        MSB_FORMAT(&msb, "Node(", nodenames[node->type], ", '", node->header, "', [", (int)node->children.count, "children])");
    else {
        MSB_FORMAT(&msb, "Node(", nodenames[node->type].text);
        for(size_t i = 0; i < node->classes.count;i++){
            auto class = &node->classes.data[i];
            MSB_FORMAT(&msb, ".", *class);
            }
        MSB_FORMAT(&msb, ", '", node->header, "', [", (int)node->children.count, "children])");
    }
    auto text = msb_borrow(&msb);
    auto result = PyUnicode_FromStringAndSize(text.text, text.length);
    msb_destroy(&msb);
    return result;
    }

static
Nullable(PyObject*)
py_node_set_err(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE)){
        PyErr_SetString(PyExc_ValueError, "Method called with invalid handle: 'err'");
        return NULL;
        }
    auto node = get_node(ctx, handle);
    const char* msg;
    Py_ssize_t length;
    const char* const keywords[] = { "msg", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "s#:err", (char**)keywords, &msg, &length)){
        return NULL;
        }
    PopDiagnostic();
    MStringBuilder sb = {.allocator=ctx->allocator};
    msb_write_str(&sb, msg, length);
    node_set_err(ctx, node, msb_detach(&sb));
    PyErr_SetString(PyExc_Exception, "Node threw error.");
    return NULL;
    }

static
Nullable(PyObject*)
make_py_node(Nonnull(DndcContext*)ctx, NodeHandle handle){
    DndNode* self = (DndNode*)DndNodeType.tp_alloc(&DndNodeType, 0);
    unhandled_error_condition(!self);
    self->ctx = ctx;
    self->handle = handle;
    return (PyObject*)self;
    }

static
Nullable(PyObject*)
py_change_root_node(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle; // we're abusing my pyboundmethod machinery
    DndNode* new_root;
    const char* const keywords[] = { "new_root", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!:set_root_node", (char**)keywords, &DndNodeType, &new_root)){
        return NULL;
        }
    PopDiagnostic();
    assert(new_root->ctx == ctx);
    ctx->root_handle = new_root->handle;
    auto node = get_node(new_root->ctx, new_root->handle);
    node->parent = new_root->handle;
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
py_make_string_node(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle;
    PyObject* arg;
    const char* const keywords[] = { "text", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!:make_string", (char**)keywords, &PyUnicode_Type, &arg)){
        return NULL;
        }
    PopDiagnostic();
    auto sv = pystring_to_stringview(arg, ctx->allocator);
    auto new_handle = alloc_handle(ctx);
    {
    auto node = get_node(ctx, new_handle);
    node->header = sv;
    node->type = NODE_STRING;
    }
    return make_py_node(ctx, new_handle);
    }

static
Nullable(PyObject*)
py_kebab(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle;
    const char* text;
    Py_ssize_t length;
    const char* const keywords[] = { "text", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "s#:kebab", (char**)keywords, &text, &length)){
        return NULL;
        }
    PopDiagnostic();
    MStringBuilder sb = {.allocator=ctx->temp_allocator};
    msb_write_kebab(&sb, text, length);
    auto kebabed = msb_borrow(&sb);
    PyObject* result = PyUnicode_FromStringAndSize(kebabed.text, kebabed.length);
    msb_destroy(&sb);
    return result;
    }

static
Nullable(PyObject*)
py_add_dependency(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle;
    PyObject* text;
    const char* const keywords[] = { "text", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!:add_dependency", (char**)keywords, &PyUnicode_Type, &text)){
        return NULL;
        }
    PopDiagnostic();
    StringView sv = pystring_to_stringview(text, ctx->allocator);
    Marray_push(StringView)(&ctx->dependencies, ctx->allocator, sv);
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
py_make_node(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle;
    NodeTypeEnum* type;
    PyObject* text = NULL;
    PyObject* classes = NULL;
    PyObject* class_sq = NULL;
    PyObject* attributes = NULL;
    PyObject* attributes_sq = NULL;
    const char* const keywords[] = { "type", "header", "classes", "attributes", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|O!OO:make_node", (char**)keywords, &NodeTypeEnumType, &type, &PyUnicode_Type, &text, &classes, &attributes)){
        return NULL;
        }
    PopDiagnostic();
    if(classes){
        class_sq = PySequence_Fast(classes, "make_node needs 'classes' to be a sequence of strings");
        if(class_sq == NULL){
            goto err;
            }
        auto sq_length = PySequence_Fast_GET_SIZE(class_sq);
        for(Py_ssize_t i = 0; i < sq_length; i++){
            auto item = PySequence_Fast_GET_ITEM(class_sq, i);
            if(!PyUnicode_Check(item)){
                PyErr_SetString(PyExc_TypeError, "make node needs 'classes' to be a sequence of strings. Non-string found.");
                goto err;
                }
            }
        }
    if(attributes){
        attributes_sq = PySequence_Fast(attributes, "make_node needs 'attributes' to be a sequence of strings");
        if(attributes_sq == NULL){
            goto err;
            }
        auto sq_length = PySequence_Fast_GET_SIZE(attributes_sq);
        for(Py_ssize_t i = 0; i < sq_length; i++){
            auto item = PySequence_Fast_GET_ITEM(attributes_sq, i);
            if(!PyUnicode_Check(item)){
                PyErr_SetString(PyExc_TypeError, "make node needs 'attributes' to be a sequence of strings. Non-string found.");
                goto err;
                }
            }
        }
    {
    auto new_handle = alloc_handle(ctx);
    {
    auto node = get_node(ctx, new_handle);
    {
    auto frame = PyEval_GetFrame(); // borrowed ref
    if(frame){
        node->row = PyFrame_GetLineNumber(frame) - 1;
        auto code = frame->f_code;
        node->filename = pystring_to_stringview(code->co_filename, ctx->allocator);
        }
    }
    if(text){
        node->header = pystring_to_stringview(text, ctx->allocator);
        }
    if(class_sq){
        auto sq_length = PySequence_Fast_GET_SIZE(class_sq);
        for(Py_ssize_t i = 0; i < sq_length; i++){
            auto item = PySequence_Fast_GET_ITEM(class_sq, i);
            auto c = Marray_alloc(StringView)(&node->classes, ctx->allocator);
            *c = pystring_to_stringview(item, ctx->allocator);
            }
        }
    if(attributes_sq){
        auto sq_length = PySequence_Fast_GET_SIZE(attributes_sq);
        for(Py_ssize_t i = 0; i < sq_length; i++){
            auto item = PySequence_Fast_GET_ITEM(attributes_sq, i);
            auto a = Marray_alloc(Attribute)(&node->attributes, ctx->allocator);
            a->key = pystring_to_stringview(item, ctx->allocator);
            a->value = SV("");
            }
        }
    node->type = type->type;
    Marray(NodeHandle)* node_store = NULL;;
    switch(node->type){
        case NODE_IMPORT:
            PyErr_SetString(PyExc_ValueError, "Creating import nodes from python is not supported");
            return NULL;
        case NODE_DEPENDENCIES:
            node_store = &ctx->dependencies_nodes;
            break;
        case NODE_STYLESHEETS:
            node_store = &ctx->stylesheets_nodes;
            break;
        case NODE_LINKS:
            node_store = &ctx->link_nodes;
            break;
        case NODE_SCRIPTS:
            node_store = &ctx->script_nodes;
            break;
        case NODE_PYTHON:
            node_store = &ctx->python_nodes;
            break;
        case NODE_DATA:
            node_store = &ctx->data_nodes;
            break;
        case NODE_TITLE:
            ctx->titlenode = new_handle;
            break;
        case NODE_NAV:
            ctx->navnode = new_handle;
            break;
        default:
            break;
        }
    if(node_store)
        Marray_push(NodeHandle)(node_store, ctx->allocator, new_handle);
    }
    return make_py_node(ctx, new_handle);
    }
    err:
    Py_XDECREF(class_sq);
    Py_XDECREF(attributes_sq);
    return NULL;
    }

static
Nullable(PyObject*)
py_set_data(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle;
    PyObject* key = NULL;
    PyObject* value = NULL;
    const char* const keywords[] = { "key", "value", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!:set_data", (char**)keywords, &PyUnicode_Type, &key, &PyUnicode_Type, &value)){
        return NULL;
        }
    PopDiagnostic();
    auto new_data = Marray_alloc(DataItem)(&ctx->rendered_data, ctx->allocator);
    new_data->key = pystring_to_stringview(key, ctx->allocator);
    new_data->value = pystring_to_longstring(value, ctx->allocator);
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
py_detach_node(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    const char* const keywords[] = { NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, ":detach_node", (char**)keywords)){
        return NULL;
        }
    PopDiagnostic();
    auto node = get_node(ctx, handle);
    if(NodeHandle_eq(node->parent, INVALID_NODE_HANDLE)){
        Py_RETURN_NONE;
        }
    if(NodeHandle_eq(handle, ctx->root_handle)){
        ctx->root_handle = INVALID_NODE_HANDLE;
        node->parent = INVALID_NODE_HANDLE;
        Py_RETURN_NONE;
        }
    auto parent = get_node(ctx, node->parent);
    node->parent = INVALID_NODE_HANDLE;
    for(size_t i = 0; i < parent->children.count; i++){
        if(NodeHandle_eq(handle, parent->children.data[i])){
            Marray_remove__NodeHandle(&parent->children, i);
            goto after;
            }
        }
    PyErr_SetString(PyExc_RuntimeError, "Somehow a node was not a child of its parents");
    return NULL;
    after:;
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
py_add_child_node(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    const char* const keywords[] = { "new_root", NULL, };
    NodeHandle new_handle;
    PyObject* arg;
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O:add_child", (char**)keywords, &arg)){
        return NULL;
        }
    PopDiagnostic();
    if(PyObject_IsInstance(arg, (PyObject*)&DndNodeType)){
        auto new_child = (DndNode*)arg;
        new_handle = new_child->handle;
        }
    else if(PyUnicode_Check(arg)){
        auto sv = pystring_to_stringview(arg, ctx->allocator);
        new_handle = alloc_handle(ctx);
        auto node = get_node(ctx, new_handle);
        node->header = sv;
        node->type = NODE_STRING;
        }
    else {
        PyErr_SetString(PyExc_TypeError, "Argument 'to_add' child must be a node or a string");
        return NULL;
        }
    auto child_node = get_node(ctx, new_handle);
    if(!NodeHandle_eq(child_node->parent, INVALID_NODE_HANDLE)){
        PyErr_SetString(PyExc_ValueError, "Node needs to be an orphan to be added as a child of another node.");
        return NULL;
        }
    if(NodeHandle_eq(handle, new_handle)){
        PyErr_SetString(PyExc_ValueError, "Node can't be a child of itself");
        return NULL;
        }
    append_child(ctx, handle, new_handle);
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
py_select_nodes(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle;
    NodeTypeEnum* type_ = NULL;
    PyObject* result = NULL;
    PyObject* attributes_ = NULL;
    PyObject* classes_ = NULL;
    PyObject* class_sq = NULL;
    PyObject* attributes_sq = NULL;
    Marray(StringView) attributes = {};
    Marray(StringView) classes = {};
    const char* const keywords[] = {"type", "classes", "attributes", NULL};
    PushDiagnostic();
    SuppressCastQual();
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "|O!OO:select_nodes", (char**)keywords, &NodeTypeEnumType, &type_, &classes_, &attributes_)){
        return NULL;
        }
    PopDiagnostic();
    NodeType type = type_? type_->type:NODE_INVALID;
    if(classes_){
        class_sq = PySequence_Fast(classes_, "select_nodes needs 'classes' to be a sequence of strings");
        if(class_sq == NULL){
            goto cleanup;
            }
        auto sq_length = PySequence_Fast_GET_SIZE(class_sq);
        for(Py_ssize_t i = 0; i < sq_length; i++){
            auto item = PySequence_Fast_GET_ITEM(class_sq, i);
            if(!PyUnicode_Check(item)){
                PyErr_SetString(PyExc_TypeError, "select_nodes needs 'classes' to be a sequence of strings. Non-string found.");
                goto cleanup;
                }
            Marray_push(StringView)(&classes, ctx->temp_allocator, pystring_borrow_stringview(item));
            }
        }
    if(attributes_){
        attributes_sq = PySequence_Fast(attributes_, "select_nodes needs 'attributes' to be a sequence of strings");
        if(attributes_sq == NULL){
            goto cleanup;
            }
        auto sq_length = PySequence_Fast_GET_SIZE(attributes_sq);
        for(Py_ssize_t i = 0; i < sq_length; i++){
            auto item = PySequence_Fast_GET_ITEM(attributes_sq, i);
            if(!PyUnicode_Check(item)){
                PyErr_SetString(PyExc_TypeError, "select_nodes 'attributes' to be a sequence of strings. Non-string found.");
                goto cleanup;
                }
            Marray_push(StringView)(&attributes, ctx->temp_allocator, pystring_borrow_stringview(item));
            }
        }
    if(not classes_ and not attributes_ and not type_){
        result = PyList_New(ctx->nodes.count);
        for(size_t i = 0; i < ctx->nodes.count; i++){
            PyObject* item = make_py_node(ctx, (NodeHandle){.index=i});
            PyList_SetItem(result, i, item);
            }
        }
    else {
        result = PyList_New(0);
        for(size_t i = 0; i < ctx->nodes.count; i++){
            auto node = &ctx->nodes.data[i];
            if(type != NODE_INVALID){
                if(node->type != type)
                    goto Continue;
                }
            for(size_t a = 0; a < attributes.count; a++){
                auto attr = attributes.data[a];
                if(not node_has_attribute(node, attr))
                    goto Continue;
                }
            for(size_t c = 0; c < classes.count; c++){
                if(not node_has_class(node, classes.data[c]))
                    goto Continue;
                }
            PyList_Append(result, make_py_node(ctx, (NodeHandle){.index=i}));
            Continue:;
            }
        }
    cleanup:
    Py_XDECREF(attributes_sq);
    Py_XDECREF(class_sq);
    Marray_cleanup(StringView)(&classes, ctx->temp_allocator);
    Marray_cleanup(StringView)(&attributes, ctx->temp_allocator);
    return result;
    }

static
Nullable(PyObject*)
py_read_file(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle;
    const char* const keywords[] = {"path", NULL};
    PyObject* arg;
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!:read_file", (char**)keywords, &PyUnicode_Type, &arg)){
        return NULL;
        }
    PopDiagnostic();
    StringView path = pystring_borrow_stringview(arg);
    auto e = ctx_load_source_file(ctx, path);
    if(e.errored){
        // Use a string builder as PyErr_Format doesn't handle '%.*s' as
        // they roll their own sprintf.
        MStringBuilder sb = {.allocator = ctx->temp_allocator};
        msb_write_literal(&sb, "No such file: '");
        if(ctx->base_directory.length){
            msb_write_str(&sb, ctx->base_directory.text, ctx->base_directory.length);
            msb_write_char(&sb, '/');
            }
        msb_write_str(&sb, path.text, path.length);
        msb_write_char(&sb, '\'');
        auto str = msb_borrow(&sb);
        PyErr_SetString(PyExc_FileNotFoundError, str.text);
        msb_destroy(&sb);
        return NULL;
        }
    auto text = e.result;
    return PyUnicode_FromStringAndSize(text.text, text.length);
    }


static Nonnull(PyObject*) make_py_ctx(Nonnull(DndcContext*));

static
Errorable_f(void)
execute_python_string(Nonnull(DndcContext*)ctx, Nonnull(const char*)text, NodeHandle handle){
    PyCompilerFlags flags = {
#if PY_MINOR_VERSION > 7
        .cf_flags = PyCF_SOURCE_IS_UTF8,
        .cf_feature_version = PY_MINOR_VERSION,
#endif
        };
    PyObject* glbl = PyDict_New();
    PyObject* nodetypes = PyDict_New();
    for(size_t i = 0; i < arrlen(nodenames); i++){
        auto enu = make_node_type_enum(i);
        PyDict_SetItemString(nodetypes, nodenames[i].text, enu);
        Py_XDECREF(enu);
        }
    auto nt = _PyNamespace_New(nodetypes);
    unhandled_error_condition(!nt);
    Py_XDECREF(nodetypes);
    PyDict_SetItemString(glbl, "NodeType", nt);
    Py_XDECREF(nt);
    PyObject* pynode = make_py_node(ctx, handle);
    PyDict_SetItemString(glbl, "node", pynode);
    Py_XDECREF(pynode);
    PyObject* pyctx = make_py_ctx(ctx);
    PyDict_SetItemString(glbl, "ctx", pyctx);
    Py_XDECREF(pyctx);

    PyDict_SetItemString(glbl, "__builtins__", PyEval_GetBuiltins());

    auto node = get_node(ctx, handle);
    char buff[1024];
    if(node->filename.length < 1024){
        memcpy(buff, node->filename.text, node->filename.length);
        buff[node->filename.length] = 0;
        }
    else {
        memcpy(buff, node->filename.text, 1023);
        buff[1023] = 0;
        }
    PyObject* code = Py_CompileStringExFlags(text, buff, Py_file_input, &flags, 0);
    auto c = (PyCodeObject*)code;
    PyObject* result;
    if(!code){
        result = NULL;
        }
    else {
        auto old_co_name = c->co_name;
        c->co_name = PyUnicode_FromString(":python");
        Py_XDECREF(old_co_name);
        c->co_firstlineno+= node->row +1;
        result = PyEval_EvalCode(code, glbl, glbl);
        }
    // result = PyRun_StringFlags(text, Py_file_input, glbl, glbl, &flags);
    Py_XDECREF(glbl);
    if(!result){
        PyObject *type, *value, *traceback;
        PyErr_Fetch(&type, &value, &traceback);
        PyErr_NormalizeException(&type, &value, &traceback);
        // FIXME: More robust signalling of errors.
        if(ctx->error.message.length){
            }
        else{
            PyObject* exc_str = PyObject_Str(value);
            unhandled_error_condition(!exc_str);
            const char* exc_text = PyUnicode_AsUTF8(exc_str);
            unhandled_error_condition(!exc_text);
            auto python_block = get_node(ctx, handle);
            auto old_row = python_block->row;
            auto new_row = old_row;
            if(traceback){
                auto tb = (PyTracebackObject*)traceback;
                // since we mucked with the code up above, the lineno is actually
                // accurate.
                auto lineno = tb->tb_lineno;
                new_row = lineno-1; // the error reporting adds 1
                }
            else if(type == PyExc_SyntaxError){
                auto se = (PySyntaxErrorObject*)value;
                auto lineno = PyLong_AsLong(se->lineno);
                new_row += lineno;
                }
            // kind of hacky, but meh;
            const char* type_text = ((PyTypeObject*)type)->tp_name;
            unhandled_error_condition(!type_text);
            // NASTY: modding the line number
            python_block->row = new_row;
            MStringBuilder sb = {.allocator=ctx->allocator};
            msb_write_str(&sb, type_text, strlen(type_text));
            msb_write_str(&sb, exc_text, strlen(exc_text));
            node_set_err(ctx, python_block, msb_detach(&sb));
            python_block->row = old_row;
            Py_XDECREF(exc_str);
            }
        Py_XDECREF(type);
        Py_XDECREF(value);
        Py_XDECREF(traceback);
        Py_XDECREF(result);
        Py_XDECREF(code);
        return (Errorable(void)){PARSE_ERROR};
        }
    Py_XDECREF(result);
    Py_XDECREF(code);
    return (Errorable(void)){};
    }

static
Nullable(PyObject*)
DndNode_getattr(Nonnull(DndNode*)obj, Nonnull(const char*)name){
    auto len = strlen(name);
#define CHECK(lit) (len == sizeof(""lit)-1 and memcmp(name, ""lit, sizeof(""lit)-1)==0)
    // TODO: we can probably do this more optimally
    if(CHECK("parent")){
        auto node = get_node(obj->ctx, obj->handle);
        return make_py_node(obj->ctx, node->parent);
        }
    else if(CHECK("type")){
        auto node = get_node(obj->ctx, obj->handle);
        return make_node_type_enum(node->type);
        }
    else if(CHECK("children")){
        auto node = get_node(obj->ctx, obj->handle);
        auto result = PyTuple_New(node->children.count);
        if(!result)
            return result;
        for(size_t i = 0; i < node->children.count; i++){
            auto child = node->children.data[i];
            auto pynode = make_py_node(obj->ctx, child);
            auto fail = PyTuple_SetItem(result, i, pynode);
            //meh
            unhandled_error_condition(fail != 0);
            }
        return result;
        }
    else if(CHECK("header")){
        auto node = get_node(obj->ctx, obj->handle);
        return PyUnicode_FromStringAndSize(node->header.text, node->header.length);
        }
    else if(CHECK("attributes")){
        return make_attributes_map(obj->ctx, obj->handle);
        }
    else if(CHECK("classes")){
        return make_classes_list(obj->ctx, obj->handle);
        }
    else if(CHECK("parse")){
        return make_node_bound_method(obj->ctx, obj->handle, &py_parse_and_append_children);
        }
    else if(CHECK("detach")){
        return make_node_bound_method(obj->ctx, obj->handle, &py_detach_node);
        }
    else if(CHECK("add_child")){
        return make_node_bound_method(obj->ctx, obj->handle, &py_add_child_node);
        }
    else if(CHECK("err")){
        return make_node_bound_method(obj->ctx, obj->handle, &py_node_set_err);
        }
    else if(CHECK("id")){
        auto node = get_node(obj->ctx, obj->handle);
        auto id = node_get_id(node);
        if(id){
            MStringBuilder temp = {.allocator = obj->ctx->temp_allocator};
            msb_write_kebab(&temp, id->text, id->length);
            auto kebabed = msb_borrow(&temp);
            auto result = PyUnicode_FromStringAndSize(kebabed.text, kebabed.length);
            msb_destroy(&temp);
            return result;
            }
        else {
            PyErr_SetString(PyExc_AttributeError, "This node has no id");
            return NULL;
            }
        }
    PyErr_Format(PyExc_AttributeError, "Unknown attribute: %s", name);
    return NULL;
    }

static
int
DndNode_setattr(Nonnull(DndNode*)obj, Nonnull(const char*)name, Nullable(PyObject*) value){
    auto len = strlen(name);
    if(!value){
        PyErr_SetString(PyExc_TypeError, "deletion of attributes is not supported");
        return -1;
        }
    if(CHECK("parent")){
        PyErr_SetString(PyExc_TypeError, "parent cannot be reassigned");
        return -1;
        }
    else if(CHECK("attributes")){
        PyErr_SetString(PyExc_TypeError, "attributes cannot be reassigned");
        return -1;
        }
    else if(CHECK("classes")){
        PyErr_SetString(PyExc_TypeError, "classes cannot be reassigned");
        return -1;
        }
    else if(CHECK("type")){
        if(PyObject_IsInstance(value, (PyObject *)&NodeTypeEnumType)){
            auto ty = (NodeTypeEnum*)value;
            auto node = get_node(obj->ctx, obj->handle);
            switch(ty->type){
                case NODE_NAV:
                    obj->ctx->navnode = obj->handle;
                    break;
                case NODE_TITLE:
                    obj->ctx->titlenode = obj->handle;
                    break;
                case NODE_STYLESHEETS:
                    Marray_push(NodeHandle)(&obj->ctx->stylesheets_nodes, obj->ctx->allocator, obj->handle);
                    break;
                case NODE_DEPENDENCIES:
                    Marray_push(NodeHandle)(&obj->ctx->dependencies_nodes, obj->ctx->allocator, obj->handle);
                    break;
                case NODE_LINKS:
                    Marray_push(NodeHandle)(&obj->ctx->link_nodes, obj->ctx->allocator, obj->handle);
                    break;
                case NODE_SCRIPTS:
                    Marray_push(NodeHandle)(&obj->ctx->script_nodes, obj->ctx->allocator, obj->handle);
                    break;
                case NODE_DATA:
                    Marray_push(NodeHandle)(&obj->ctx->data_nodes, obj->ctx->allocator, obj->handle);
                    break;
                case NODE_PYTHON:
                    PyErr_SetString(PyExc_ValueError, "Setting a node to PYTHON not supported.");
                    return -1;
                case NODE_IMPORT:
                    PyErr_SetString(PyExc_ValueError, "Setting a node to IMPORT not supported.");
                    return -1;
                case NODE_ROOT:
                case NODE_TEXT:
                case NODE_DIV:
                case NODE_STRING:
                case NODE_PARA:
                case NODE_HEADING:
                case NODE_HR:
                case NODE_TABLE:
                case NODE_TABLE_ROW:
                case NODE_IMAGE:
                case NODE_BULLETS:
                case NODE_RAW:
                case NODE_PRE:
                case NODE_LIST:
                case NODE_LIST_ITEM:
                case NODE_KEYVALUE:
                case NODE_KEYVALUEPAIR:
                case NODE_IMGLINKS:
                case NODE_COMMENT:
                case NODE_MD:
                case NODE_CONTAINER:
                case NODE_INVALID:
                case NODE_QUOTE:
                    break;
                }
            node->type = ty->type;
            return 0;
            }
        else {
            PyErr_SetString(PyExc_TypeError, "node type must be a NodeType");
            return -1;
            }
        }
    else if(CHECK("header")){
        if(!PyUnicode_Check(value)){
            PyErr_SetString(PyExc_TypeError, "Header must be a string");
            return -1;
            }
        auto node = get_node(obj->ctx, obj->handle);
        node->header = pystring_to_stringview((Nonnull(PyObject*))value, obj->ctx->allocator);
        return 0;
        }
#undef CHECK
    PyErr_Format(PyExc_AttributeError, "Unknown attribute: %s", name);
    return -1;
    }

// TODO:
//  Possibly all these DndcContext* should actually be DndcContext** so they can get nulled.
//
// just a bare wrapper around the parse context
typedef struct DndContext {
    PyObject_HEAD
    Nonnull(DndcContext*) ctx;
    } DndContext;

// static PyMethodDef DndContext_methods[] = {
    // {NULL, NULL, 0, NULL}, // sentinel
    // };

static PyObject* _Nullable DndContext_getattr(Nonnull(DndContext*), Nonnull(const char*));

static PyTypeObject DndContextType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "docparse.DndcContext",
    .tp_basicsize = sizeof(DndContext),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "DndcContext",
    // can you just leave this out?
    // .tp_methods = DndContext_methods,
    .tp_getattr = (getattrfunc)&DndContext_getattr,
    };

static
PyObject* _Nullable
DndContext_getattr(Nonnull(DndContext*)pyctx, Nonnull(const char*)attr){
    auto ctx = pyctx->ctx;
    auto len = strlen(attr);
#define CHECK(lit) (len == sizeof(""lit)-1 and memcmp(attr, ""lit, sizeof(""lit)-1)==0)
    if(CHECK("root")){
        if(NodeHandle_eq(ctx->root_handle, INVALID_NODE_HANDLE)){
            PyErr_SetString(PyExc_AttributeError, "There is currently no root node");
            return NULL;
            }
        return make_py_node(ctx, ctx->root_handle);
        }
    if(CHECK("set_root")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_change_root_node);
        }
    if(CHECK("make_string")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_make_string_node);
        }
    if(CHECK("make_node")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_make_node);
        }
    if(CHECK("add_dependency")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_add_dependency);
        }
    if(CHECK("kebab")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_kebab);
        }
    if(CHECK("set_data")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_set_data);
        }
    if(CHECK("outfile")){
        auto filename = ctx->outputfile.length?path_basename(LS_to_SV(ctx->outputfile)):SV("");
        return PyUnicode_FromStringAndSize(filename.text, filename.length);
        }
    if(CHECK("outdir")){
        auto outdir = path_dirname(LS_to_SV(ctx->outputfile));
        if(!outdir.length)
            outdir = SV(".");
        return PyUnicode_FromStringAndSize(outdir.text, outdir.length);
        }
    if(CHECK("outpath")){
        return PyUnicode_FromStringAndSize(ctx->outputfile.text, ctx->outputfile.length);
        }
    if(CHECK("sourcepath")){
        return PyUnicode_FromStringAndSize(ctx->filename.text, ctx->filename.length);
        }
    if(CHECK("base")){
        auto base = ctx->base_directory;
        if(!base.length)
            base = SV(".");
        return PyUnicode_FromStringAndSize(base.text, base.length);
        }
    if(CHECK("all_nodes")){
        PyObject* result = PyList_New(ctx->nodes.count);
        for(size_t i = 0; i < ctx->nodes.count; i++){
            PyObject* item = make_py_node(ctx, (NodeHandle){.index=i});
            PyList_SetItem(result, i, item);
            }
        return result;
        }
    if(CHECK("select_nodes")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_select_nodes);
        }
    if(CHECK("read_file")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_read_file);
        }
#undef CHECK
    PyErr_Format(PyExc_AttributeError, "Unknown attribute: '%s'", attr);
    return NULL;
    }

static
Nonnull(PyObject*)
make_py_ctx(Nonnull(DndcContext*)ctx){
    DndContext* self = (DndContext*)DndContextType.tp_alloc(&DndContextType, 0);
    unhandled_error_condition(!self);
    self->ctx = ctx;
    return (PyObject*)self;
    }

static
Errorable_f(void)
docparse_init_types(void){
    Errorable(void) result = {};
    if(PyType_Ready(&DndNodeType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&DndClassesListType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&DndAttributesMapType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&NodeBoundMethodType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&NodeTypeEnumType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&DndContextType) < 0)
        Raise(GENERIC_ERROR);
    return result;
    }

#include "terminal_logger.c"
#define SUPPRESS_BUILTIN_MODS
#define NO_AUX

static struct _inittab mods[] = {
    {NULL, 0}, // sentinel
    };

#ifndef PYTHONMODULE
static
int
init_python_interpreter(uint64_t flags){
    if(flags & DNDC_PYTHON_UNISOLATED){
        Py_Initialize();
        return 0;
        }
#if PY_MINOR_VERSION > 7
    PyStatus status;
    PyPreConfig preconfig;
    PyPreConfig_InitIsolatedConfig(&preconfig);
    preconfig.use_environment = 0;
    preconfig.utf8_mode = 1;
    status = Py_PreInitialize(&preconfig);
    if(PyStatus_Exception(status)){
        goto fail;
        }
    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);
    config.buffered_stdio = 0;
    config.write_bytecode = 0;
    config.quiet = 1;
    config.install_signal_handlers = 1;
    config.module_search_paths_set = 1;
    config.site_import = 0;
    config.isolated = 1;
    config.use_hash_seed = 1;
    config.hash_seed = 358780669; // very random
    // config.import_time = 1;
    // Initialize all paths otherwise python tries to do it itself in silly ways.
    PyConfig_SetString(&config, &config.program_name, L"");
    PyConfig_SetString(&config, &config.pythonpath_env, L"");
    PyConfig_SetString(&config, &config.home, L"");
    PyConfig_SetString(&config, &config.executable, L"");
    PyConfig_SetString(&config, &config.base_executable, L"");
    PyConfig_SetString(&config, &config.prefix, L"");
    PyConfig_SetString(&config, &config.base_prefix, L"");
    PyConfig_SetString(&config, &config.exec_prefix, L"");
    PyConfig_SetString(&config, &config.base_exec_prefix, L"");
    int import_fail = PyImport_ExtendInittab(&mods[0]);
    if(import_fail < 0){
        goto fail;
        }
    status = Py_InitializeFromConfig(&config);
    if(PyStatus_Exception(status)){
        goto fail;
        }
    PyConfig_Clear(&config);
    return 0;

    fail:
    PyConfig_Clear(&config);
    if(PyStatus_IsExit(status)){
        return status.exitcode;
        }
    return 1;
#else
    Py_Initialize();
    return 0;
#endif
    }
#include "frozenstdlib.h"
static Errorable_f(void) docparse_init_types(void);
static
Errorable_f(void)
init_python_docparser(uint64_t flags){
    Errorable(void) result = {};
    if(flags & DNDC_PYTHON_IS_INIT)
        return result;
    if(Py_IsInitialized())
        return result;
    if(!(flags & DNDC_PYTHON_UNISOLATED))
        set_frozen_modules();
    auto fail = init_python_interpreter(flags);
    if(fail){
        ERROR("Failed to init python interpreter");
        Raise(GENERIC_ERROR);
        }
    {
        auto e = docparse_init_types();
        if(e.errored)
            return e;
    }
    return result;
    }

PushDiagnostic();
SuppressUnusedFunction();
static inline
void
end_interpreter(void){
    Py_Finalize();
    }
PopDiagnostic();
#endif
#endif
