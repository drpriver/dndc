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

#ifdef __clang__
#pragma clang assume_nonnull begin
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
PopDiagnostic();

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
PushDiagnostic();
SuppressUnusedFunction();
static inline
LongString
pystring_borrow_longstring(PyObject* pyobj){
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
NodeTypeEnum_repr(NodeTypeEnum* e){
    if(e->type > NODE_INVALID or e->type < 0){
        PyErr_Format(PyExc_RuntimeError, "Somehow we have an enum with an invalid value: %d", (int)e->type);
        return NULL;
        }
    auto name = NODENAMES[e->type];
    return PyUnicode_FromFormat("NodeType.%s", name.text);
    }

static
PyObject* _Nullable
NodeTypeEnum_getattr(NodeTypeEnum* e, const char* name){
    if(e->type > NODE_INVALID or e->type < 0){
        PyErr_Format(PyExc_RuntimeError, "Somehow we have an enum with an invalid value: %d", (int)e->type);
        return NULL;
        }
    if(strcmp(name, "name")==0){
        auto enu_name = NODENAMES[e->type];
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
NodeTypeEnum_richcmp(PyObject* a, PyObject* b, int cmp){
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
    .tp_name = "dndc_python.NodeType",
    .tp_basicsize = sizeof(NodeTypeEnum),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "NodeType Enum",
    .tp_repr = (reprfunc)NodeTypeEnum_repr,
    .tp_getattr = (getattrfunc)&NodeTypeEnum_getattr,
    .tp_richcompare = &NodeTypeEnum_richcmp,
    };

static
Nullable(PyObject*)
make_node_type_enum(NodeType t){
    NodeTypeEnum* self = (NodeTypeEnum*)NodeTypeEnumType.tp_alloc(&NodeTypeEnumType, 0);
    if(!self) return NULL;
    self->type = t;
    return (PyObject*)self;
    }

typedef Nullable(PyObject*) (*_Nonnull NodeMethod)(DndcContext* , NodeHandle, PyObject*, Nullable(PyObject*));

typedef struct NodeBoundMethod {
    PyObject_HEAD
    DndcContext* ctx;
    NodeHandle handle;
    NodeMethod func;
} NodeBoundMethod;

static
Nullable(PyObject*)
NodeBound_call(PyObject* self, PyObject* args, Nullable(PyObject*)kwargs){
    auto meth = (NodeBoundMethod*)self;
    return meth->func(meth->ctx, meth->handle, args, kwargs);
    }

static PyTypeObject NodeBoundMethodType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "dndc_python.NodeBoundMethod",
    .tp_basicsize = sizeof(NodeBoundMethod),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Node bound method",
    .tp_call = &NodeBound_call,
    };

static
Nullable(PyObject*)
make_node_bound_method(DndcContext* ctx, NodeHandle handle, NodeMethod func){
    NodeBoundMethod* self = (NodeBoundMethod*)NodeBoundMethodType.tp_alloc(&NodeBoundMethodType, 0);
    if(!self) return NULL;
    self->ctx = ctx;
    self->handle = handle;
    self->func = func;
    return (PyObject*)self;
    }

static
Nullable(PyObject*)
py_parse_and_append_children(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
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
    auto source_text = pystring_to_longstring(text, ctx->string_allocator);
    auto old_filename = ctx->filename;

    auto parse_e = dndc_parse(ctx, handle, SV("(generated string from script)"), source_text.text);
    if(parse_e.errored){
        PyErr_SetString(PyExc_ValueError, "Error while parsing");
        return NULL;
        }

    ctx->filename = old_filename;
    Py_RETURN_NONE;
    }

typedef struct DndclassesList {
    PyObject_HEAD
    DndcContext* ctx;
    NodeHandle handle;
} DndclassesList;

static
Py_ssize_t
Dndclasses_length(DndclassesList* list){
    auto node = get_node(list->ctx, list->handle);
    if(!node->classes)
        return 0;
    return (Py_ssize_t)node->classes->count;
    }

static
Nullable(PyObject*)
Dndclasses_getitem(DndclassesList* list, Py_ssize_t index){
    auto node = get_node(list->ctx, list->handle);
    auto length = node->classes?node->classes->count:0;
    if(index < 0){
        index += length;
        }
    if(index >= length){
        PyErr_SetString(PyExc_IndexError, "Index out of bounds");
        return NULL;
        }
    auto sv = node->classes->data[index];
    return PyUnicode_FromStringAndSize(sv.text, sv.length);
    }

static
int
Dndclasses_contains(DndclassesList* list, PyObject* query){
    if(!PyUnicode_Check(query)){
        PyErr_SetString(PyExc_TypeError, "Only strings can be in classes lists");
        return -1;
        }
    auto node = get_node(list->ctx, list->handle);
    size_t n_classes = node->classes?node->classes->count:0;
    if(!n_classes)
        return 0;

    auto key_sv = pystring_borrow_stringview(query);
    RARRAY_FOR_EACH(cls, node->classes){
        if(SV_equals(*cls, key_sv))
            return 1;
        }
    return 0;
    }

static
int
Dndclasses_setitem(DndclassesList* list, Py_ssize_t index, Nullable(PyObject*) value){
    if(!value){
        PyErr_SetString(PyExc_NotImplementedError, "Deletion is unsupported");
        return -1;
        }
    auto node = get_node(list->ctx, list->handle);
    auto nclasses = node->classes?node->classes->count:0;
    if(index < 0){
        index += nclasses;
        }
    if(index >= nclasses){
        PyErr_SetString(PyExc_IndexError, "Index out of bounds");
        return -1;
        }
    node->classes->data[index] = pystring_to_stringview((Nonnull(PyObject*))value, list->ctx->allocator);
    return 0;
    }

static
Nullable(PyObject*)
Dndclasses_append(DndclassesList* list, PyObject* args){
    PyObject* text;
    if(!PyArg_ParseTuple(args, "O!:append", &PyUnicode_Type, &text))
        return NULL;
    auto node = get_node(list->ctx, list->handle);
    StringView sv = pystring_to_stringview(text, list->ctx->allocator);
    node->classes = Rarray_push(StringView)(node->classes, list->ctx->allocator, sv);
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
Dndclasses_repr(DndclassesList* list){
    auto node = get_node(list->ctx, list->handle);
    MStringBuilder msb = {.allocator=list->ctx->temp_allocator};
    msb_write_char(&msb, '[');
    size_t count = node->classes?node->classes->count:0;
    for(size_t i = 0; i < count; i++){
        if(i != 0)
            msb_write_str(&msb, ", ", 2);
        msb_write_char(&msb, '\'');
        auto sv = node->classes->data[i];
        msb_write_str(&msb, sv.text, sv.length);
        msb_write_char(&msb, '\'');
        }
    msb_write_char(&msb, ']');
    auto str = msb_borrow(&msb);
    auto result = PyUnicode_FromStringAndSize(str.text, str.length);
    msb_destroy(&msb);
    return result;
    }


static PyMethodDef DndclassesList_methods[] = {
    {"append", (PyCFunction)&Dndclasses_append, METH_VARARGS, "add a class string"},
    {NULL, NULL, 0, NULL}, // Sentinel
    };

static PySequenceMethods Dndclasses_sq_methods = {
    .sq_length = (lenfunc)Dndclasses_length,
    .sq_concat = NULL,
    .sq_repeat = NULL,
    .sq_item = (ssizeargfunc)Dndclasses_getitem,
    .was_sq_slice = NULL,
    .sq_ass_item = (ssizeobjargproc)Dndclasses_setitem,
    .was_sq_ass_slice = NULL,
    .sq_contains = (objobjproc)Dndclasses_contains,
    .sq_inplace_concat = NULL,
    .sq_inplace_repeat = NULL,
};
static PyTypeObject DndclassesListType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "dndc_python.ClassList",
    .tp_basicsize = sizeof(DndclassesList),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Classes List Wrapper",
    .tp_methods = DndclassesList_methods,
    .tp_getattro = PyObject_GenericGetAttr,
    .tp_as_sequence = &Dndclasses_sq_methods,
    .tp_repr = (reprfunc)&Dndclasses_repr,
    };

static
Nullable(PyObject*)
make_classes_list(DndcContext* ctx, NodeHandle handle){
    DndclassesList* self = (DndclassesList*)DndclassesListType.tp_alloc(&DndclassesListType, 0);
    if(!self) return NULL;
    self->ctx = ctx;
    self->handle = handle;
    return (PyObject*)self;
    }

typedef struct DndAttributesMap {
    PyObject_HEAD
    DndcContext* ctx;
    NodeHandle handle;
    } DndAttributesMap;

static
PyObject*
DndAttributesMap_items(DndAttributesMap* map, PyObject* unused){
    (void)unused;
    auto node = get_node(map->ctx, map->handle);
    auto attributes = node->attributes;
    size_t count = attributes?attributes->count:0;
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
DndAttributesMap_length(DndAttributesMap* list){
    auto node = get_node(list->ctx, list->handle);
    if(!node->attributes)
        return 0;
    return (Py_ssize_t)node->attributes->count;
    }

static
Nullable(PyObject*)
DndAttributesMap_getitem(DndAttributesMap* map, PyObject* key){
    if(!PyUnicode_Check(key)){
        PyErr_SetString(PyExc_TypeError, "Attribute maps must be indexed by strings");
        return NULL;
        }
    auto key_sv = pystring_borrow_stringview(key);
    auto node = get_node(map->ctx, map->handle);
    RARRAY_FOR_EACH(attr, node->attributes){
        if(SV_equals(attr->key, key_sv))
            return PyUnicode_FromStringAndSize(attr->value.text, attr->value.length);
        }
    PyErr_Format(PyExc_KeyError, "Unknown attribute: '%s'", key_sv.text);
    return NULL;
    }

static
int
DndAttributesMap_contains(DndAttributesMap* map, PyObject* key){
    if(!PyUnicode_Check(key)){
        PyErr_SetString(PyExc_TypeError, "Attribute maps must be indexed by strings");
        return -1;
        }
    auto key_sv = pystring_borrow_stringview(key);
    auto node = get_node(map->ctx, map->handle);
    RARRAY_FOR_EACH(attr, node->attributes){
        if(SV_equals(attr->key, key_sv))
            return 1;
        }
    return 0;
    }

// Value should be verified to either be NULL or a PyUnicode object.
static
int
dnd_attributes_map_set_item(DndcContext* ctx, NodeHandle handle, StringView key_sv, Nullable(PyObject*)value){
    auto node = get_node(ctx, handle);
    auto attributes = node->attributes;
    auto count = attributes?attributes->count:0;
    for(size_t i = 0; i < count; i++){
        auto attr = &attributes->data[i];
        if(SV_equals(attr->key, key_sv)){
            if(value){
                attr->value = pystring_to_stringview((Nonnull(PyObject*))value, ctx->string_allocator);
                return 0;
                }
            else {
                Rarray_remove(Attribute)(attributes, i);
                return 0;
                }
            }
        }
    if(!value){
        PyErr_Format(PyExc_KeyError, "Unknown attribute: '%s'", key_sv.text);
        return -1;
        }
    const char* key_copy = Allocator_dupe(ctx->string_allocator, key_sv.text, key_sv.length);
    auto attr = Rarray_alloc(Attribute)(&node->attributes, ctx->allocator);
    attr->key.length = key_sv.length;
    attr->key.text = key_copy;
    attr->value = pystring_to_stringview((Nonnull(PyObject*))value, ctx->string_allocator);
    return 0;
    }

static
int
DndAttributesMap_setitem(DndAttributesMap* map, PyObject* key, Nullable(PyObject*) value){
    if(!PyUnicode_Check(key)){
        PyErr_SetString(PyExc_TypeError, "Attribute maps must be indexed by strings");
        return -1;
        }
    if(value and !PyUnicode_Check(value)){
        PyErr_SetString(PyExc_TypeError, "Attribute maps can only have string values");
        return -1;
        }
    auto key_sv = pystring_borrow_stringview(key);
    return dnd_attributes_map_set_item(map->ctx, map->handle, key_sv, value);
    }

static
Nullable(PyObject*)
DndAttributesMap_repr(DndAttributesMap* map){
    auto ctx = map->ctx;
    auto node = get_node(ctx, map->handle);
    auto attributes = node->attributes;
    auto count = attributes?attributes->count:0;
    MStringBuilder msb = {.allocator = ctx->temp_allocator};
    msb_write_char(&msb, '{');
    for(size_t i = 0; i < count; i++){
        auto attr = &attributes->data[i];
        if(i != 0)
            msb_write_str(&msb,  ", ", 2);
        msb_write_char(&msb,  '\'');
        auto key = attr->key;
        msb_write_str(&msb, key.text, key.length);
        msb_write_literal(&msb, "': '");
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
DndAttributesMap_add(DndAttributesMap* map, PyObject* arg){
    if(!PyUnicode_Check(arg)){
        PyErr_SetString(PyExc_TypeError, "Argument to add must be a string");
        return NULL;
        }
    auto ctx = map->ctx;
    auto key = pystring_to_stringview(arg, ctx->allocator);
    auto node = get_node(ctx, map->handle);
    auto attributes = &node->attributes;
    auto attr = Rarray_alloc(Attribute)(attributes, ctx->allocator);
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
    .tp_name = "dndc_python.AttributesMap",
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
Nullable(PyObject*)
make_attributes_map(DndcContext* ctx, NodeHandle handle){
    DndAttributesMap* self = (DndAttributesMap*)DndAttributesMapType.tp_alloc(&DndAttributesMapType, 0);
    if(!self) return NULL;
    self->ctx = ctx;
    self->handle = handle;
    return (PyObject*)self;
    }

typedef struct DndNode {
    PyObject_HEAD
    DndcContext* ctx;
    NodeHandle handle;
    } DndNode;

static PyMethodDef DndNode_methods[] = {
    {NULL, NULL, 0, NULL}, // Sentinel
    };
static PyObject* _Nullable DndNode_getattr(DndNode*, const char*);
static PyObject* _Nullable DndNode_getattro(DndNode*, PyObject*);
static int DndNode_setattr(DndNode*, const char*, Nullable(PyObject *));
static int DndNode_setattro(DndNode*, PyObject*, Nullable(PyObject *));
static Nullable(PyObject*) DndNode_repr(DndNode*);


static PyTypeObject DndNodeType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "dndc_python.Node",
    .tp_basicsize = sizeof(DndNode),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Node Wrapper",
    .tp_methods = DndNode_methods,
    .tp_getattr = (getattrfunc)&DndNode_getattr,
    .tp_setattr = (setattrfunc)&DndNode_setattr,
    .tp_getattro = (getattrofunc)&DndNode_getattro,
    .tp_setattro = (setattrofunc)&DndNode_setattro,
    .tp_repr = (reprfunc)&DndNode_repr,
};

static
Nullable(PyObject*)
DndNode_repr(DndNode* self){
    auto node = get_node(self->ctx, self->handle);
    // format a buffer as python apparently doesn't support %.*s
    MStringBuilder msb = {.allocator=self->ctx->temp_allocator};
    size_t class_count = node->classes?node->classes->count:0;
    if(not class_count)
        MSB_FORMAT(&msb, "Node(", NODENAMES[node->type], ", '", node->header, "', [", (int)node->children.count, " children])");
    else {
        MSB_FORMAT(&msb, "Node(", NODENAMES[node->type].text);
        RARRAY_FOR_EACH(class, node->classes){
            MSB_FORMAT(&msb, ".", *class);
            }
        MSB_FORMAT(&msb, ", '", node->header, "', [", (int)node->children.count, " children])");
    }
    auto text = msb_borrow(&msb);
    auto result = PyUnicode_FromStringAndSize(text.text, text.length);
    msb_destroy(&msb);
    return result;
    }

static
Nullable(PyObject*)
py_node_set_err(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
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
    MStringBuilder sb = {.allocator=ctx->string_allocator};
    msb_write_str(&sb, msg, length);
    node_set_err(ctx, node, msb_detach(&sb));
    PyErr_SetString(PyExc_Exception, "Node threw error.");
    return NULL;
    }

static
Nullable(PyObject*)
make_py_node(DndcContext* ctx, NodeHandle handle){
    DndNode* self = (DndNode*)DndNodeType.tp_alloc(&DndNodeType, 0);
    if(!self) return NULL;
    self->ctx = ctx;
    self->handle = handle;
    return (PyObject*)self;
    }

static
Nullable(PyObject*)
py_change_root_node(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
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
py_make_string_node(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
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
    auto sv = pystring_to_stringview(arg, ctx->string_allocator);
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
py_kebab(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
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
py_add_dependency(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
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
    StringView sv = pystring_to_stringview(text, ctx->string_allocator);
    Marray_push(StringView)(&ctx->dependencies, ctx->allocator, sv);
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
py_make_node(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
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
        node->filename = pystring_to_stringview(code->co_filename, ctx->string_allocator);
        }
    }
    if(text){
        node->header = pystring_to_stringview(text, ctx->string_allocator);
        }
    if(class_sq){
        auto sq_length = PySequence_Fast_GET_SIZE(class_sq);
        for(Py_ssize_t i = 0; i < sq_length; i++){
            auto item = PySequence_Fast_GET_ITEM(class_sq, i);
            auto c = Rarray_alloc(StringView)(&node->classes, ctx->allocator);
            *c = pystring_to_stringview(item, ctx->string_allocator);
            }
        }
    if(attributes_sq){
        auto sq_length = PySequence_Fast_GET_SIZE(attributes_sq);
        for(Py_ssize_t i = 0; i < sq_length; i++){
            auto item = PySequence_Fast_GET_ITEM(attributes_sq, i);
            auto a = Rarray_alloc(Attribute)(&node->attributes, ctx->allocator);
            a->key = pystring_to_stringview(item, ctx->string_allocator);
            a->value = SV("");
            }
        }
    node->type = type->type;
    Marray(NodeHandle)* node_store = NULL;
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
        case NODE_JS:
        case NODE_PYTHON:
            node_store = &ctx->user_script_nodes;
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
py_set_data(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
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
    new_data->key = pystring_to_stringview(key, ctx->string_allocator);
    new_data->value = pystring_to_longstring(value, ctx->string_allocator);
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
py_detach_node(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
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
        if(NodeHandle_eq(handle, node_children(parent)[i])){
            node_remove_child(parent, i, ctx->allocator);
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
py_add_child_node(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
    const char* const keywords[] = { "child", NULL, };
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
        auto sv = pystring_to_stringview(arg, ctx->string_allocator);
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
py_replace_child_node(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
    const char* const keywords[] = { "child", "newchild", NULL, };
    DndNode* child;
    DndNode* newchild;
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!O!:replace_child", (char**)keywords, &DndNodeType, &child, &DndNodeType, &newchild)){
        return NULL;
        }
    PopDiagnostic();
    NodeHandle new_handle = newchild->handle;
    NodeHandle prev_handle = child->handle;
    auto prevchild_node = get_node(ctx, prev_handle);
    auto newchild_node = get_node(ctx, new_handle);
    if(!NodeHandle_eq(newchild_node->parent, INVALID_NODE_HANDLE)){
        PyErr_SetString(PyExc_ValueError, "Node needs to be an orphan to be added as a child of another node.");
        return NULL;
        }
    if(NodeHandle_eq(handle, new_handle)){
        PyErr_SetString(PyExc_ValueError, "Node can't be a child of itself");
        return NULL;
        }
    if(!NodeHandle_eq(handle, prevchild_node->parent)){
        PyErr_SetString(PyExc_ValueError, "Node to replace is not a child of this node");
        return NULL;
        }
    auto parent_node = get_node(ctx, handle);
    auto count = parent_node->children.count;
    auto data = node_children(parent_node);
    for(size_t i = 0; i < count; i++){
        auto c = data[i];
        if(NodeHandle_eq(c, prev_handle)){
            data[i] = new_handle;
            prevchild_node->parent = INVALID_NODE_HANDLE;
            newchild_node->parent = handle;
            Py_RETURN_NONE;
            }
        }
    PyErr_SetString(PyExc_AssertionError, "Internal logic error when replacing nodes");
    return NULL;
    }

static
Nullable(PyObject*)
py_select_nodes(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
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
            MARRAY_FOR_EACH(attr, attributes){
                if(not node_has_attribute(node, *attr))
                    goto Continue;
                }
            MARRAY_FOR_EACH(class_, classes){
                if(not node_has_class(node, *class_))
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
py_read_file(DndcContext* ctx, NodeHandle handle, PyObject* args, Nullable(PyObject*)kwargs){
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


static Nullable(PyObject*) make_py_ctx(DndcContext*);

static
Errorable_f(void)
execute_python_string(DndcContext* ctx, const char* text, NodeHandle handle){
    PyCompilerFlags flags = {
#if PY_MINOR_VERSION > 7
        .cf_flags = PyCF_SOURCE_IS_UTF8,
        .cf_feature_version = PY_MINOR_VERSION,
#endif
        };
    // I'm not checking for failure here with these python API functions.
    // Technically you should do so to handle low memory.
    PyObject* glbl = PyDict_New();
    PyObject* nodetypes = PyDict_New();
    for(size_t i = 0; i < arrlen(NODENAMES); i++){
        auto enu = make_node_type_enum(i);
        PyDict_SetItemString(nodetypes, NODENAMES[i].text, enu);
        Py_XDECREF(enu);
        }
    auto nt = _PyNamespace_New(nodetypes);
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
            const char* exc_text = PyUnicode_AsUTF8(exc_str);
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
            // NASTY: modding the line number
            python_block->row = new_row;
            MStringBuilder sb = {.allocator=ctx->string_allocator};
            msb_write_str(&sb, type_text, strlen(type_text));
            msb_write_literal(&sb, ": ");
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

static inline
Nullable(PyObject*)
DndNode_getattr_ls(DndNode* obj, LongString name){
    auto ctx = obj->ctx;
    #define CHECK(lit, N) ({_Static_assert(sizeof(lit)-1==N, "'" lit "' is not length " #N); memcmp(lit, name.text, N)==0;})
    switch(name.length){
        case 2:{
            if(CHECK("id", 2)){
            auto node = get_node(ctx, obj->handle);
            auto id = node_get_id(node);
            if(id){
                MStringBuilder temp = {.allocator = ctx->temp_allocator};
                msb_write_kebab(&temp, id->text, id->length);
                auto kebabed = msb_borrow(&temp);
                auto result = PyUnicode_FromStringAndSize(kebabed.text, kebabed.length);
                msb_destroy(&temp);
                return result;
                }
            else {
                // It's unclear whether you should get empty string, None or AttributeError if you try to access a node without an id.
                // Py_RETURN_NONE;
                PyErr_SetString(PyExc_AttributeError, "This node has no id");
                return NULL;
                }
            }
        }break;
        case 3:{
            if(CHECK("err", 3)){
                return make_node_bound_method(ctx, obj->handle, &py_node_set_err);
            }
        }break;
        case 4:{
            if(CHECK("type", 4)){
                auto node = get_node(ctx, obj->handle);
                return make_node_type_enum(node->type);
            }
        }break;
        case 5:{
            if(CHECK("parse", 5)){
                return make_node_bound_method(ctx, obj->handle, &py_parse_and_append_children);
            }
        }break;
        case 6:{
            if(CHECK("header", 6)){
                auto node = get_node(ctx, obj->handle);
                return PyUnicode_FromStringAndSize(node->header.text, node->header.length);
                }
            else if(CHECK("parent", 6)){
                auto node = get_node(ctx, obj->handle);
                return make_py_node(ctx, node->parent);
                }
            else if(CHECK("detach", 6)){
                return make_node_bound_method(ctx, obj->handle, &py_detach_node);
                }
        }break;
        case 7:{
            if(CHECK("classes", 7)){
                return make_classes_list(ctx, obj->handle);
                }
        }break;
        case 8:{
            if(CHECK("children", 8)){
                auto node = get_node(ctx, obj->handle);
                auto result = PyTuple_New(node->children.count);
                if(!result)
                    return result;
                for(size_t i = 0; i < node->children.count; i++){
                    auto child = node_children(node)[i];
                    auto pynode = make_py_node(ctx, child);
                    auto fail = PyTuple_SetItem(result, i, pynode);
                    if(fail){
                        Py_XDECREF(pynode);
                        Py_XDECREF(result);
                        return NULL;
                        }
                    }
                return result;
                }
        }break;
        case 9:{
            if(CHECK("add_child", 9)){
                return make_node_bound_method(ctx, obj->handle, &py_add_child_node);
                }
        }break;
        case 10:{
            if(CHECK("attributes", 10)){
                return make_attributes_map(ctx, obj->handle);
                }
        }break;
        case 13:{
            if(CHECK("replace_child", 13)){
                return make_node_bound_method(ctx, obj->handle, &py_replace_child_node);
                }
        }break;
        }
    #undef CHECK
    PyErr_Format(PyExc_AttributeError, "Unknown attribute: %s", name.text);
    return NULL;
    }

static
Nullable(PyObject*)
DndNode_getattr(DndNode* obj, const char* name){
    auto len = strlen(name);
    return DndNode_getattr_ls(obj, (LongString){.text=name, .length=len});
    }
static
Nullable(PyObject*)
DndNode_getattro(DndNode* obj, PyObject* name){
    auto ls = pystring_borrow_longstring(name);
    return DndNode_getattr_ls(obj, ls);
    }

static inline
int
DndNode_setattr_ls(DndNode* obj, LongString name, Nullable(PyObject*) value){
    if(!value){
        PyErr_SetString(PyExc_TypeError, "deletion of attributes is not supported");
        return -1;
        }
    auto ctx = obj->ctx;
    #define CHECK(lit, N) ({_Static_assert(sizeof(lit)-1==N, "'" lit "' is not length " #N); memcmp(lit, name.text, N)==0;})
    switch(name.length){
        case 2:{
            if(CHECK("id", 2)){
                if(PyUnicode_Check(value))
                    return dnd_attributes_map_set_item(ctx, obj->handle, SV("id"), value);
                else {
                    PyErr_SetString(PyExc_TypeError, "Attempt to set node id to a non-string.");
                    return -1;
                }
            }
        }break;
        case 3:{
            if(CHECK("err", 3)){
                PyErr_SetString(PyExc_TypeError, "method error cannot be reassigned");
                return -1;
            }
        }break;
        case 4:{
            if(CHECK("type", 4)){
                if(PyObject_IsInstance(value, (PyObject *)&NodeTypeEnumType)){
                    auto ty = (NodeTypeEnum*)value;
                    auto node = get_node(ctx, obj->handle);
                    switch(ty->type){
                        case NODE_NAV:
                            ctx->navnode = obj->handle;
                            break;
                        case NODE_TITLE:
                            ctx->titlenode = obj->handle;
                            break;
                        case NODE_STYLESHEETS:
                            Marray_push(NodeHandle)(&ctx->stylesheets_nodes, ctx->allocator, obj->handle);
                            break;
                        case NODE_DEPENDENCIES:
                            Marray_push(NodeHandle)(&ctx->dependencies_nodes, ctx->allocator, obj->handle);
                            break;
                        case NODE_LINKS:
                            Marray_push(NodeHandle)(&ctx->link_nodes, ctx->allocator, obj->handle);
                            break;
                        case NODE_SCRIPTS:
                            Marray_push(NodeHandle)(&ctx->script_nodes, ctx->allocator, obj->handle);
                            break;
                        case NODE_DATA:
                            Marray_push(NodeHandle)(&ctx->data_nodes, ctx->allocator, obj->handle);
                            break;
                        case NODE_JS:
                            PyErr_SetString(PyExc_ValueError, "Setting a node to JS not supported.");
                            return -1;
                        case NODE_PYTHON:
                            PyErr_SetString(PyExc_ValueError, "Setting a node to PYTHON not supported.");
                            return -1;
                        case NODE_IMPORT:
                            PyErr_SetString(PyExc_ValueError, "Setting a node to IMPORT not supported.");
                            return -1;
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
        }break;
        case 5:{
            if(CHECK("parse", 5)){
                PyErr_SetString(PyExc_TypeError, "method parse cannot be reassigned");
                return -1;
            }
        }break;
        case 6:{
            if(CHECK("header", 6)){
                if(!PyUnicode_Check(value)){
                    PyErr_SetString(PyExc_TypeError, "Header must be a string");
                    return -1;
                    }
                if(PyUnicode_GetLength(value) == 0){
                    auto node = get_node(ctx, obj->handle);
                    node->header.length = 0;
                    node->header.text = "";
                    return 0;
                    }
                auto node = get_node(ctx, obj->handle);
                node->header = pystring_to_stringview((Nonnull(PyObject*))value, ctx->string_allocator);
                return 0;
                }
            else if(CHECK("parent", 6)){
                PyErr_SetString(PyExc_TypeError, "parent cannot be reassigned");
                return -1;
                }
            else if(CHECK("detach", 6)){
                PyErr_SetString(PyExc_TypeError, "method detach cannot be reassigned");
                return -1;
                }
        }break;
        case 7:{
            if(CHECK("classes", 7)){
                PyErr_SetString(PyExc_TypeError, "classes cannot be reassigned");
                return -1;
                }
        }break;
        case 8:{
            if(CHECK("children", 8)){
                PyErr_SetString(PyExc_TypeError, "children cannot be directly reassigned");
                return -1;
                }
        }break;
        case 9:{
            if(CHECK("add_child", 9)){
                PyErr_SetString(PyExc_TypeError, "method add_child cannot be reassigned");
                return -1;
                }
        }break;
        case 10:{
            if(CHECK("attributes", 10)){
                PyErr_SetString(PyExc_TypeError, "attributes cannot be reassigned");
                return -1;
                }
        }break;
        case 13:{
            if(CHECK("replace_child", 13)){
                PyErr_SetString(PyExc_TypeError, "method replace_child cannot be reassigned");
                return -1;
                }
        }break;
        }
#undef CHECK
    PyErr_Format(PyExc_AttributeError, "Unknown attribute: %s", name);
    return -1;
    }
static
int
DndNode_setattr(DndNode* obj, const char* name, Nullable(PyObject*) value){
    auto len = strlen(name);
    LongString ls = {.text=name, .length=len};
    return DndNode_setattr_ls(obj, ls, value);
    }
static
int
DndNode_setattro(DndNode* obj, PyObject* name, Nullable(PyObject*) value){
    auto ls = pystring_borrow_longstring(name);
    return DndNode_setattr_ls(obj, ls, value);
    }

// just a bare wrapper around the parse context
typedef struct Dndcontext {
    PyObject_HEAD
    DndcContext* ctx;
} Dndcontext;

// static PyMethodDef Dndcontext_methods[] = {
    // {NULL, NULL, 0, NULL}, // sentinel
    // };

static PyObject* _Nullable Dndcontext_getattr(Dndcontext*, const char*);
static PyObject* _Nullable Dndcontext_getattro(Dndcontext*, PyObject*);

static PyTypeObject DndcontextType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "dndc.DndcContext",
    .tp_basicsize = sizeof(Dndcontext),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "DndcContext",
    // can you just leave this out?
    // .tp_methods = Dndcontext_methods,
    .tp_getattr = (getattrfunc)&Dndcontext_getattr,
    .tp_getattro = (getattrofunc)&Dndcontext_getattro,
    };

static PyObject* _Nullable
Dndcontext_getattr_ls(Dndcontext* pyctx, LongString name){
    #define CHECK(lit, N) ({_Static_assert(sizeof(lit)-1==N, "'" lit "' is not length " #N); memcmp(lit, name.text, N)==0;})
    auto ctx = pyctx->ctx;
    switch(name.length){
        case 4:{
            if(CHECK("root", 4)){
                if(NodeHandle_eq(ctx->root_handle, INVALID_NODE_HANDLE)){
                    PyErr_SetString(PyExc_AttributeError, "There is currently no root node");
                    return NULL;
                    }
                return make_py_node(ctx, ctx->root_handle);
                }
            if(CHECK("base", 4)){
                auto base = ctx->base_directory;
                if(!base.length)
                    base = LS(".");
                return PyUnicode_FromStringAndSize(base.text, base.length);
                }
        }break;
        case 5:{
            if(CHECK("kebab", 5)){
                return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_kebab);
                }
        }break;
        case 6:{
            if(CHECK("outdir", 6)){
                auto outdir = path_dirname(LS_to_SV(ctx->outputfile));
                if(!outdir.length)
                    outdir = SV(".");
                return PyUnicode_FromStringAndSize(outdir.text, outdir.length);
                }
        }break;
        case 7:{
            if(CHECK("outfile", 7)){
                auto filename = ctx->outputfile.length?path_basename(LS_to_SV(ctx->outputfile)):SV("");
                return PyUnicode_FromStringAndSize(filename.text, filename.length);
                }
            if(CHECK("outpath", 7)){
                return PyUnicode_FromStringAndSize(ctx->outputfile.text, ctx->outputfile.length);
                }
        }break;
        case 8:{
            if(CHECK("set_root", 8)){
                return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_change_root_node);
                }
            if(CHECK("set_data", 8)){
                return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_set_data);
                }
        }break;
        case 9:{
            if(CHECK("make_node", 9)){
                return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_make_node);
                }
            if(CHECK("all_nodes", 9)){
                PyObject* result = PyList_New(ctx->nodes.count);
                if(!result) return NULL;
                for(size_t i = 0; i < ctx->nodes.count; i++){
                    PyObject* item = make_py_node(ctx, (NodeHandle){.index=i});
                    PyList_SetItem(result, i, item);
                    }
                return result;
                }
            if(CHECK("read_file", 9)){
                return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_read_file);
                }
        }break;
        case 10:{
            if(CHECK("sourcepath", 10)){
                return PyUnicode_FromStringAndSize(ctx->filename.text, ctx->filename.length);
                }
        }break;
        case 11:{
            if(CHECK("make_string", 11)){
                return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_make_string_node);
                }
        }break;
        case 12:{
            if(CHECK("select_nodes", 12)){
                return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_select_nodes);
                }
        }break;
        case 14:{
            if(CHECK("add_dependency", 14)){
                return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_add_dependency);
                }
        }break;
    }
#undef CHECK
    PyErr_Format(PyExc_AttributeError, "Unknown attribute: '%s'", name.text);
    return NULL;
    }

static
PyObject* _Nullable
Dndcontext_getattr(Dndcontext* pyctx, const char* attr){
    auto len = strlen(attr);
    LongString ls = {.text=attr, .length=len};
    return Dndcontext_getattr_ls(pyctx, ls);
    }
static
PyObject* _Nullable
Dndcontext_getattro(Dndcontext* pyctx, PyObject* attr){
    auto ls = pystring_borrow_longstring(attr);
    return Dndcontext_getattr_ls(pyctx, ls);
    }

static
Nullable(PyObject*)
make_py_ctx(DndcContext* ctx){
    Dndcontext* self = (Dndcontext*)DndcontextType.tp_alloc(&DndcontextType, 0);
    if(!self) return NULL;
    self->ctx = ctx;
    return (PyObject*)self;
    }

static
Errorable_f(void)
internal_dndc_python_init_types(void){
    Errorable(void) result = {};
    if(PyType_Ready(&DndNodeType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&DndclassesListType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&DndAttributesMapType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&NodeBoundMethodType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&NodeTypeEnumType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&DndcontextType) < 0)
        Raise(GENERIC_ERROR);
    return result;
    }



static inline
int
init_python_interpreter(uint64_t flags){
#ifdef PYTHONMODULE
    (void)flags;
    return 0;
#else
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
#endif
    }

#ifndef PYTHONMODULE
#ifdef __clang__
#pragma clang assume_nonnull end
#include "frozenstdlib.h"
#pragma clang assume_nonnull begin
#else
#include "frozenstdlib.h"
#endif
#endif

static
Errorable_f(void)
internal_init_dndc_python_interpreter(uint64_t flags){
    Errorable(void) result = {};
#ifdef PYTHONMODULE
    (void)flags;
    return result;
#else
    if(flags & DNDC_PYTHON_IS_INIT)
        return result;
    if(Py_IsInitialized())
        return result;
    if(!(flags & DNDC_PYTHON_UNISOLATED)){
        struct FrozenPyVersion frozen_version = get_frozen_version();
        if(frozen_version.major != PY_MAJOR_VERSION || frozen_version.minor != PY_MINOR_VERSION){
            #ifdef ERROR
            ERROR("Mismatch between the frozen stdlib and the version of python compiled against");
            #endif
            Raise(GENERIC_ERROR);
            }
        set_frozen_modules();
        }
    auto fail = init_python_interpreter(flags);
    if(fail){
        #ifdef ERROR
        ERROR("Failed to init python interpreter");
        #endif
        Raise(GENERIC_ERROR);
        }
    {
        auto e = internal_dndc_python_init_types();
        if(e.errored)
            return e;
    }
    return result;
#endif
    }

PushDiagnostic();
SuppressUnusedFunction();
static inline
void
end_interpreter(void){
#ifndef PYTHONMODULE
    Py_Finalize();
#endif
    }
PopDiagnostic();

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
