#ifndef DNDC_QJS_C
#define DNDC_QJS_C
#include "dndc.h"
#include "dndc_funcs.h"
#include "dndc_types.h"
PushDiagnostic();
#pragma clang diagnostic ignored "-Wunused-parameter"
#include "quickjs.h"
PopDiagnostic();
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum { ZERO_NODE_VALUE = (INVALID_NODE_HANDLE._value-1) };
static inline
void*_Nullable
NodeHandle_to_opaque(NodeHandle handle){
    if(!handle._value){
        return (void*)(uintptr_t)ZERO_NODE_VALUE;
        }
    return (void*)(uintptr_t)handle._value;
    }

#define JSMETHOD(name) \
    static QJSValue \
    name(QJSContext* jsctx, QJSValueConst thisValue, int argc, QJSValueConst* argv)
#define JSSETTER(name) \
    static QJSValue \
    name(QJSContext* jsctx, QJSValueConst thisValue, QJSValueConst arg)
#define JSGETTER(name) \
    static QJSValue \
    name(QJSContext* jsctx, QJSValueConst thisValue)

//
// Utility or free functions
//
//
static
QJSValue
js_make_dndc_context(QJSContext*, DndcContext*);

static
QJSValue
js_make_dndc_node(QJSContext*, NodeHandle);

static
void
set_js_traceback(DndcContext* ctx, QJSContext* jsctx);

static
warn_unused
bool
js_dndc_get_node_handle(QJSContext* jsctx, QJSValueConst obj, NodeHandle* out);
static
warn_unused
bool
js_dndc_get_attributes_handle(QJSContext* jsctx, QJSValueConst obj, NodeHandle* out);
static
warn_unused
bool
js_dndc_get_classlist_handle(QJSContext* jsctx, QJSValueConst obj, NodeHandle* out);

static inline
LongString
jsstring_to_longstring(QJSContext* jsctx, QJSValueConst v, Allocator a);

static inline
force_inline
StringView
jsstring_to_stringview(QJSContext* jsctx, QJSValueConst v, Allocator a);

static inline
force_inline
StringView
jsstring_make_stringview_js_allocated(QJSContext* jsctx, QJSValueConst v);

static
QJSValue
js_freeze_object(QJSContext* jsctx, QJSValueConst obj);

JSMETHOD(js_console_log);

//
// DndcContext
//

static JSClassID JS_DNDC_CONTEXT_CLASS_ID;

static
JSClassDef JS_DNDC_CONTEXT_CLASS = {
    .class_name = "DndcContext",
    };

static
DndcContext*_Nullable
js_get_dndc_context(QJSContext*, QJSValue);

//
// DndcContext methods
//
JSGETTER(js_dndc_context_get_root);
JSSETTER(js_dndc_context_set_root);
JSGETTER(js_dndc_context_get_outfile);
JSGETTER(js_dndc_context_get_outpath);
JSGETTER(js_dndc_context_get_sourcepath);
JSGETTER(js_dndc_context_get_base);
JSGETTER(js_dndc_context_get_all_nodes);
JSMETHOD(js_dndc_context_make_string);
JSMETHOD(js_dndc_context_make_node);
JSMETHOD(js_dndc_context_add_dependency);
JSMETHOD(js_dndc_context_kebab);
JSMETHOD(js_dndc_context_set_data);
JSMETHOD(js_dndc_context_read_file);
JSMETHOD(js_dndc_context_select_nodes);
JSMETHOD(js_dndc_context_to_string);

static
const
JSCFunctionListEntry JS_DNDC_CONTEXT_FUNCS[] = {
    JS_CGETSET_DEF("root", js_dndc_context_get_root, js_dndc_context_set_root),
    JS_CGETSET_DEF("outfile", js_dndc_context_get_outfile, NULL),
    JS_CGETSET_DEF("outpath", js_dndc_context_get_outpath, NULL),
    JS_CGETSET_DEF("sourcepath", js_dndc_context_get_sourcepath, NULL),
    JS_CGETSET_DEF("base", js_dndc_context_get_base, NULL),
    JS_CGETSET_DEF("all_nodes", js_dndc_context_get_all_nodes, NULL),
    JS_CFUNC_DEF("make_string", 1, js_dndc_context_make_string),
    JS_CFUNC_DEF("make_node", 2, js_dndc_context_make_node),
    JS_CFUNC_DEF("add_dependency", 1, js_dndc_context_add_dependency),
    JS_CFUNC_DEF("kebab", 1, js_dndc_context_kebab),
    JS_CFUNC_DEF("set_data", 2, js_dndc_context_set_data),
    JS_CFUNC_DEF("read_file", 2, js_dndc_context_read_file),
    JS_CFUNC_DEF("select_nodes", 1, js_dndc_context_select_nodes),
    JS_CFUNC_DEF("toString", 0, js_dndc_context_to_string),
    };
//
// DndcNode
//

static JSClassID JS_DNDC_NODE_CLASS_ID;
static
JSClassDef JS_DNDC_NODE_CLASS = {
    .class_name = "DndcNode",
    };
//
// DndcNode methods
//
JSGETTER(js_dndc_node_get_parent);
JSSETTER(js_dndc_node_set_type);
JSGETTER(js_dndc_node_get_type);
JSGETTER(js_dndc_node_get_children);
JSMETHOD(js_dndc_node_to_string);
JSGETTER(js_dndc_node_get_header);
JSSETTER(js_dndc_node_set_header);
JSGETTER(js_dndc_node_get_id);
JSSETTER(js_dndc_node_set_id);
JSMETHOD(js_dndc_node_parse);
JSMETHOD(js_dndc_node_detach);
JSMETHOD(js_dndc_node_add_child);
JSMETHOD(js_dndc_node_replace_child);
JSGETTER(js_dndc_node_get_attributes);
JSGETTER(js_dndc_node_get_classes);
JSMETHOD(js_dndc_node_err);
JSMETHOD(js_dndc_node_has_class);

static
const
JSCFunctionListEntry JS_DNDC_NODE_FUNCS[] = {
    JS_CGETSET_DEF("parent", js_dndc_node_get_parent, NULL),
    JS_CGETSET_DEF("type", js_dndc_node_get_type, js_dndc_node_set_type),
    JS_CGETSET_DEF("children", js_dndc_node_get_children, NULL),
    JS_CFUNC_DEF("toString", 0, js_dndc_node_to_string),
    JS_CGETSET_DEF("header", js_dndc_node_get_header, js_dndc_node_set_header),
    JS_CGETSET_DEF("id", js_dndc_node_get_id, js_dndc_node_set_id),
    JS_CFUNC_DEF("parse", 1, js_dndc_node_parse),
    JS_CFUNC_DEF("detach", 0, js_dndc_node_detach),
    JS_CFUNC_DEF("add_child", 1, js_dndc_node_add_child),
    JS_CFUNC_DEF("replace_child", 2, js_dndc_node_replace_child),
    JS_CGETSET_DEF("attributes", js_dndc_node_get_attributes, NULL),
    JS_CGETSET_DEF("classes", js_dndc_node_get_classes, NULL),
    JS_CFUNC_DEF("err", 1, js_dndc_node_err),
    JS_CFUNC_DEF("has_class", 1, js_dndc_node_has_class),
};

//
// DndcNodeAttributes
//
static JSClassID JS_DNDC_ATTRIBUTES_CLASS_ID;

static
JSClassDef JS_DNDC_ATTRIBUTES_CLASS = {
    .class_name = "DndcNodeAttributes",
    };
//
// DndcNodeAttributes methods
//
JSMETHOD(js_dndc_attributes_get);
JSMETHOD(js_dndc_attributes_set);
JSMETHOD(js_dndc_attributes_to_string);
JSMETHOD(js_dndc_attributes_entries);

static
const
JSCFunctionListEntry JS_DNDC_ATTRIBUTES_FUNCS[] = {
    JS_CFUNC_DEF("get", 1, js_dndc_attributes_get),
    JS_CFUNC_DEF("set", 1, js_dndc_attributes_set),
    JS_CFUNC_DEF("toString", 0, js_dndc_attributes_to_string),
    JS_CFUNC_DEF("entries", 0, js_dndc_attributes_entries),
    JS_ALIAS_DEF("[Symbol.iterator]", "entries" ),
};

//
// DndcNodeClassList
//
static JSClassID JS_DNDC_CLASSLIST_CLASS_ID;

static
JSClassDef JS_DNDC_CLASSLIST_CLASS = {
    .class_name = "DndcNodeClassList",
};
//
// DndcNodeCassList methods
//
JSMETHOD(js_dndc_classlist_append);
JSMETHOD(js_dndc_classlist_to_string);
JSMETHOD(js_dndc_classlist_values);

static
const
JSCFunctionListEntry JS_DNDC_CLASSLIST_FUNCS[] = {
    JS_CFUNC_DEF("toString", 0, js_dndc_classlist_to_string),
    JS_CFUNC_DEF("append", 1, js_dndc_classlist_append),
    JS_CFUNC_DEF("values", 0, js_dndc_classlist_values),
    JS_ALIAS_DEF("[Symbol.iterator]", "values" ),
};

static
void*
js_arena_malloc(JSMallocState*s, size_t size){
    // This is stupid that we have to store the size in
    // the pointer, but how else are we going to
    // support realloc.
    size_t true_size = size + sizeof(size_t);
    size_t* p = ArenaAllocator_alloc(s->opaque, true_size);
    *p = true_size;
    return p+1;
    }

static
void
js_arena_free(JSMallocState*s, void* ptr){
    (void)ptr, (void)s;
    }

// This is so dumb. They know what size of allocation
// they have, why don't they tell us?
static
void*_Nullable js_arena_realloc(JSMallocState*s, void* pointer, size_t size){
    if(!size)
        return NULL;
    void* result;
    if(pointer){
        size_t* true_pointer = ((size_t*)pointer)-1;
        size_t true_size = size + sizeof(size_t);
        size_t* new_pointer = ArenaAllocator_realloc(s->opaque, true_pointer, *true_pointer, true_size);
        *new_pointer = true_size;
        result = new_pointer+1;
        }
    else
        result = js_arena_malloc(s, size);
    return result;
    }

static
QJSRuntime*
new_qjs_rt(ArenaAllocator* aa){
    static const JSMallocFunctions mf = {
        .js_malloc = js_arena_malloc,
        .js_free = js_arena_free,
        .js_realloc = js_arena_realloc,
        };
    QJSRuntime* rt = NULL;
    rt = JS_NewRuntime2(&mf, aa);
    JS_NewClassID(&JS_DNDC_CONTEXT_CLASS_ID);
    if(JS_NewClass(rt, JS_DNDC_CONTEXT_CLASS_ID, &JS_DNDC_CONTEXT_CLASS) < 0){
        unhandled_error_condition(0);
        }
    JS_NewClassID(&JS_DNDC_ATTRIBUTES_CLASS_ID);
    if(JS_NewClass(rt, JS_DNDC_ATTRIBUTES_CLASS_ID, &JS_DNDC_ATTRIBUTES_CLASS) < 0){
        unhandled_error_condition(0);
        }
    JS_NewClassID(&JS_DNDC_CLASSLIST_CLASS_ID);
    if(JS_NewClass(rt, JS_DNDC_CLASSLIST_CLASS_ID, &JS_DNDC_CLASSLIST_CLASS) < 0){
        unhandled_error_condition(0);
        }
    JS_NewClassID(&JS_DNDC_NODE_CLASS_ID);
    if(JS_NewClass(rt, JS_DNDC_NODE_CLASS_ID, &JS_DNDC_NODE_CLASS) < 0){
        unhandled_error_condition(0);
        }
    return rt;
    }

static
void
free_qjs_rt(QJSRuntime* rt, ArenaAllocator* arena){
    (void)rt;
#if 0
    ArenaAllocatorStats stats = ArenaAllocator_stats(arena);
    fprintf(stderr, "used: %zu\n", stats.used);
    fprintf(stderr, "capacity: %zu\n", stats.capacity);
    fprintf(stderr, "%% used: %f\n", 100.*(double)stats.used / (double)stats.capacity);
    fprintf(stderr, "n arenas: %zu\n", stats.arena_count);
    fprintf(stderr, "total big: %zu\n", stats.big_used);
    fprintf(stderr, "n big: %zu\n", stats.big_count);
#endif
    ArenaAllocator_free_all(arena);
    }

//
// The main execution function.
//
static
Errorable_f(void)
execute_qjs_string(QJSRuntime* rt, DndcContext* ctx, const char* str, size_t length, NodeHandle handle){
    Errorable(void) result = {};
    QJSContext* jsctx = NULL;
    // rt = JS_NewRuntime();
    if(!rt){
        result.errored = ALLOC_FAILURE;
        goto cleanup;
    }
    jsctx = JS_NewContext(rt);
    if(!jsctx){
        result.errored = ALLOC_FAILURE;
        goto cleanup;
    }

    // setup DndcContext class
    {
        QJSValue proto = JS_NewObject(jsctx); // new ref
        JS_SetPropertyFunctionList(jsctx, proto, JS_DNDC_CONTEXT_FUNCS, arrlen(JS_DNDC_CONTEXT_FUNCS));
        JS_SetClassProto(jsctx, JS_DNDC_CONTEXT_CLASS_ID, proto); // steals ref
    }

    // setup DndcAttributes class
    {
        QJSValue proto = JS_NewObject(jsctx); // new ref
        JS_SetPropertyFunctionList(jsctx, proto, JS_DNDC_ATTRIBUTES_FUNCS, arrlen(JS_DNDC_ATTRIBUTES_FUNCS));
        JS_SetClassProto(jsctx, JS_DNDC_ATTRIBUTES_CLASS_ID, proto); // steals ref
    }
    // setup DndcClassList class
    {
        QJSValue proto = JS_NewObject(jsctx); // new ref
        JS_SetPropertyFunctionList(jsctx, proto, JS_DNDC_CLASSLIST_FUNCS, arrlen(JS_DNDC_CLASSLIST_FUNCS));
        JS_SetClassProto(jsctx, JS_DNDC_CLASSLIST_CLASS_ID, proto); // steals ref
    }

    // setup DndcNode class
    {
        QJSValue proto = JS_NewObject(jsctx); // new ref
        JS_SetPropertyFunctionList(jsctx, proto, JS_DNDC_NODE_FUNCS, arrlen(JS_DNDC_NODE_FUNCS));
        JS_SetClassProto(jsctx, JS_DNDC_NODE_CLASS_ID, proto); // steals ref
    }

    // Stash the DndcContext into the QJSContext...
    // That's a lot of context!
    JS_SetContextOpaque(jsctx, ctx);

    {
        QJSValue global_obj, console, dctx, node, node_types;
        global_obj = JS_GetGlobalObject(jsctx); // new ref
        dctx = js_make_dndc_context(jsctx, ctx); // new_ref
        node = js_make_dndc_node(jsctx, handle); // new_ref
        node_types = JS_NewObject(jsctx); // new ref
        for(size_t i = 0; i < arrlen(NODENAMES)-1;i++){
            JS_SetPropertyStr(jsctx, node_types, NODENAMES[i].text, JS_NewUint32(jsctx, i));
            }
        JS_SetPropertyStr(jsctx, global_obj, "NodeType", node_types); // steals ref

        console = JS_NewObject(jsctx); // new ref
        JS_SetPropertyStr(jsctx, console, "log", JS_NewCFunction(jsctx, js_console_log, "log", 1)); // create and steal ref all in one go.
        JS_SetPropertyStr(jsctx, global_obj, "console", console); // steals ref
        JS_SetPropertyStr(jsctx, global_obj, "ctx", dctx); // steals ref
        JS_SetPropertyStr(jsctx, global_obj, "node", node); // steals ref
        JS_FreeValue(jsctx, global_obj); // decref
    }
    {
        const char* filename;
        {
            Node* node = get_node(ctx, handle);
            filename = Allocator_strndup(ctx->string_allocator, node->filename.text, node->filename.length);
        }

        QJSValue err = JS_Eval(jsctx, str, length, filename, 0);
        if(JS_IsException(err)){
            // FIXME: More robust signalling of errors.
            if(ctx->error.message.length){
                // error message already set.
                }
            else {
                set_js_traceback(ctx, jsctx);
                }
            result.errored = GENERIC_ERROR;
            }
        JS_FreeValue(jsctx, err);
    }

    cleanup:
    if(jsctx)
        JS_FreeContext(jsctx);
    // if(rt)
        // JS_FreeRuntime(rt);
    return result;
    }

//
// implementations
//

static inline
LongString
jsstring_to_longstring(QJSContext* jsctx, QJSValueConst v, Allocator a){
    size_t len;
    const char* str = JS_ToCStringLen(jsctx, &len, v);
    if(!str){
        return (LongString){};
        }
    // inefficient to dupe the string, but I don't know
    // the lifetime of the JS_ToCStringLen.
    const char* astr = Allocator_strndup(a, str, len);
    JS_FreeCString(jsctx, str);
    return (LongString){.length=len, .text=astr};
    }

static inline
force_inline
StringView
jsstring_to_stringview(QJSContext* jsctx, QJSValueConst v, Allocator a){
    return LS_to_SV(jsstring_to_longstring(jsctx, v, a));
    }

static inline
force_inline
StringView
jsstring_make_stringview_js_allocated(QJSContext* jsctx, QJSValueConst v){
    size_t len;
    const char* str = JS_ToCStringLen(jsctx, &len, v);
    if(!str){
        return (StringView){};
        }
    return (StringView){.text=str, .length=len};
    }


// returns false on error.
static
warn_unused
bool
js_dndc_get_node_handle(QJSContext* jsctx, QJSValueConst obj, NodeHandle* out){
    uintptr_t p = (uintptr_t)(JS_GetOpaque2(jsctx, obj, JS_DNDC_NODE_CLASS_ID));
    if(!p){
        return false;
        }
    if(p == ZERO_NODE_VALUE)
        out->_value = 0;
    else
        out->_value = p;
    return true;
    }

static
warn_unused
bool
js_dndc_get_attributes_handle(QJSContext* jsctx, QJSValueConst obj, NodeHandle* out){
    uintptr_t p = (uintptr_t)(JS_GetOpaque2(jsctx, obj, JS_DNDC_ATTRIBUTES_CLASS_ID));
    if(!p){
        return false;
        }
    if(p == ZERO_NODE_VALUE)
        out->_value = 0;
    else
        out->_value = p;
    return true;
    }

static
warn_unused
bool
js_dndc_get_classlist_handle(QJSContext* jsctx, QJSValueConst obj, NodeHandle* out){
    uintptr_t p = (uintptr_t)(JS_GetOpaque2(jsctx, obj, JS_DNDC_CLASSLIST_CLASS_ID));
    if(!p){
        return false;
        }
    if(p == ZERO_NODE_VALUE)
        out->_value = 0;
    else
        out->_value = p;
    return true;
    }

static
void
set_js_traceback(DndcContext* ctx, QJSContext* jsctx){
    MStringBuilder msb = {.allocator = ctx->string_allocator};
    QJSValue exception_val = JS_GetException(jsctx);
    int is_error = JS_IsError(jsctx, exception_val);
    {
        const char *str = JS_ToCString(jsctx, exception_val);
        if(str){
            msb_write_str(&msb, str, strlen(str));
            msb_write_char(&msb, '\n');
            JS_FreeCString(jsctx, str);
        }
        else {
            msb_write_literal(&msb, "[exception]\n");
        }
    }
    if(is_error){
        QJSValue val = JS_GetPropertyStr(jsctx, exception_val, "stack");
        if(!JS_IsUndefined(val)){
            const char *str = JS_ToCString(jsctx, val);
            if(str){
                msb_write_str(&msb, str, strlen(str));
                msb_write_char(&msb, '\n');
                JS_FreeCString(jsctx, str);
            }
            else {
                msb_write_literal(&msb, "[exception]\n");
            }
        }
        JS_FreeValue(jsctx, val);
    }
    JS_FreeValue(jsctx, exception_val);
    msb_erase(&msb, 2); // XXX: I get the one, but why two?
    ctx->error.message = msb_detach(&msb);
    }

static
QJSValue
js_console_log(QJSContext *jsctx, QJSValueConst thisValue, int argc, QJSValueConst *argv)
{
    (void)thisValue;
    int line_num = -1;
    const char* filename = NULL;
    JS_get_caller_location(jsctx, &filename, NULL, &line_num);
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return JS_UNDEFINED;
    if(!ctx->error_func)
        return JS_UNDEFINED;
    MStringBuilder msb = {.allocator = ctx->temp_allocator};

    for(int i = 0; i < argc; i++){
        if(i != 0)
            msb_write_char(&msb, ' ');
        size_t len;
        const char *str = JS_ToCStringLen(jsctx, &len, argv[i]);
        if(!str)
            return JS_EXCEPTION;
        msb_write_str(&msb, str, len);
        JS_FreeCString(jsctx, str);
    }
    auto msg = msb_borrow(&msb);
    ctx->error_func(ctx->error_user_data, DNDC_DEBUG_MESSAGE, filename?:"js", filename?strlen(filename):2, line_num-1, -1, msg.text, msg.length);
    msb_destroy(&msb);
    if(filename)
        JS_FreeCString(jsctx, filename);
    return JS_UNDEFINED;
}


static
QJSValue
js_freeze_object(QJSContext* jsctx, QJSValueConst obj){
    QJSValue global_obj = JS_GetGlobalObject(jsctx); // new ref
    QJSValue Object = JS_GetPropertyStr(jsctx, global_obj, "Object");
    QJSValue freeze = JS_GetPropertyStr(jsctx, Object, "freeze");
    QJSValue called = JS_Call(jsctx, freeze, Object, 1, &obj);
    JS_FreeValue(jsctx, global_obj);
    JS_FreeValue(jsctx, Object);
    JS_FreeValue(jsctx, freeze);
    return called;
    }


//
// DndcNode methods
//


JSMETHOD(js_dndc_node_parse){
    if(argc != 1){
        return JS_ThrowTypeError(jsctx, "parse must be given a single string argument");
        }
    QJSValueConst str = argv[0];
    if(!JS_IsString(str)){
        return JS_ThrowTypeError(jsctx, "parse must be given a single string argument");
        }
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    LongString text = jsstring_to_longstring(jsctx, str, ctx->string_allocator);
    auto old_filename = ctx->filename;
    auto parse_e = dndc_parse(ctx, handle, SV("(generated string from script)"), text.text);
    if(parse_e.errored){
        return JS_ThrowInternalError(jsctx, "Error while parsing");
        }
    ctx->filename = old_filename;
    return JS_UNDEFINED;
    }

JSMETHOD(js_dndc_node_detach){
    if(argc != 0){
        return JS_ThrowTypeError(jsctx, "detach take no arguments");
        }
    (void)argv;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);
    if(NodeHandle_eq(node->parent, INVALID_NODE_HANDLE))
        return JS_UNDEFINED;
    if(NodeHandle_eq(handle, ctx->root_handle)){
        ctx->root_handle = INVALID_NODE_HANDLE;
        node->parent = INVALID_NODE_HANDLE;
        return JS_UNDEFINED;
        }
    Node* parent = get_node(ctx, node->parent);
    node->parent = INVALID_NODE_HANDLE;
    for(size_t i = 0; i < parent->children.count; i++){
        if(NodeHandle_eq(handle, node_children(parent)[i])){
            node_remove_child(parent, i, ctx->allocator);
            goto after;
            }
        }
    return JS_ThrowRangeError(jsctx, "Somehow a node was not a child of its parent");

    after:;
    return JS_UNDEFINED;
    }

JSMETHOD(js_dndc_node_add_child){
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "need 1 argument to add_child");
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    QJSValueConst arg = argv[0];
    NodeHandle child;
    if(JS_IsString(arg)){
        StringView sv = jsstring_to_stringview(jsctx, arg, ctx->string_allocator);
        child = alloc_handle(ctx);
        Node* node = get_node(ctx, child);
        node->header = sv;
        node->type = NODE_STRING;
        }
    else {
        if(!js_dndc_get_node_handle(jsctx, arg, &child))
            return JS_EXCEPTION;
        }
    assert(!NodeHandle_eq(child, INVALID_NODE_HANDLE));
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));

    Node* child_node = get_node(ctx, child);
    if(!NodeHandle_eq(child_node->parent, INVALID_NODE_HANDLE)){
        return JS_ThrowTypeError(jsctx, "Node needs to be an orphan to be added as a child of another node");
        }
    if(NodeHandle_eq(handle, child))
        return JS_ThrowTypeError(jsctx, "Node can't be a child of itself");
    append_child(ctx, handle, child);
    return JS_UNDEFINED;
    }

JSMETHOD(js_dndc_node_replace_child){
    if(argc != 2)
        return JS_ThrowTypeError(jsctx, "need 2 arguments to replace_child");
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    QJSValueConst child_arg = argv[0];
    QJSValueConst newchild_arg = argv[1];
    NodeHandle child, new_child;
    if(!js_dndc_get_node_handle(jsctx, child_arg, &child))
        return JS_EXCEPTION;
    if(!js_dndc_get_node_handle(jsctx, newchild_arg, &new_child))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(child, INVALID_NODE_HANDLE));
    assert(!NodeHandle_eq(new_child, INVALID_NODE_HANDLE));
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    if(NodeHandle_eq(child, new_child))
        return JS_ThrowTypeError(jsctx, "two args must be distinct");
    Node* prevchild_node = get_node(ctx, child);
    Node* newchild_node = get_node(ctx, new_child);

    if(!NodeHandle_eq(newchild_node->parent, INVALID_NODE_HANDLE)){
        return JS_ThrowTypeError(jsctx, "Node needs to be an orphan to be added as a child of another node");
        }
    if(NodeHandle_eq(handle, child))
        return JS_ThrowTypeError(jsctx, "Node can't be a child of itself");
    if(!NodeHandle_eq(handle, prevchild_node->parent)){
        return JS_ThrowTypeError(jsctx, "Node to replace is not a child of this node");
        }
    Node* parent_node = get_node(ctx, handle);
    size_t count = parent_node->children.count;
    NodeHandle* data = node_children(parent_node);
    for(size_t i = 0; i < count; i++){
        NodeHandle c = data[i];
        if(NodeHandle_eq(c, child)){
            data[i] = new_child;
            prevchild_node->parent = INVALID_NODE_HANDLE;
            newchild_node->parent = handle;
            return JS_UNDEFINED;
            }
        }
    return JS_ThrowInternalError(jsctx, "Internal logic error when replacing nodes");
    }

static
QJSValue
js_make_dndc_node(QJSContext*jsctx, NodeHandle handle){
    QJSValue obj = JS_NewObjectClass(jsctx, JS_DNDC_NODE_CLASS_ID);
    if(JS_IsException(obj))
        return obj;
    JS_SetOpaque(obj, NodeHandle_to_opaque(handle));
    return obj;
    }

static
QJSValue
js_dndc_node_set_type(QJSContext* jsctx, QJSValueConst thisValue, QJSValueConst arg){
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);
    int32_t type;
    if(JS_ToInt32(jsctx, &type, arg))
        return JS_ThrowTypeError(jsctx, "Expected an integer when trying to set node type");
    if(type < 0 or type >= NODE_INVALID)
        return JS_ThrowTypeError(jsctx, "Integer out of range for valid node types.");
    switch(type){
        case NODE_NAV:
            ctx->navnode = handle;
            break;
        case NODE_TITLE:
            ctx->titlenode = handle;
            break;
        case NODE_STYLESHEETS:
            Marray_push(NodeHandle)(&ctx->stylesheets_nodes, ctx->allocator, handle);
            break;
        case NODE_DEPENDENCIES:
            Marray_push(NodeHandle)(&ctx->dependencies_nodes, ctx->allocator, handle);
            break;
        case NODE_LINKS:
            Marray_push(NodeHandle)(&ctx->link_nodes, ctx->allocator, handle);
            break;
        case NODE_SCRIPTS:
            Marray_push(NodeHandle)(&ctx->script_nodes, ctx->allocator, handle);
            break;
        case NODE_DATA:
            Marray_push(NodeHandle)(&ctx->data_nodes, ctx->allocator, handle);
            break;
        case NODE_JS:
            return JS_ThrowTypeError(jsctx, "Setting a node to JS is not supported");
        case NODE_PYTHON:
            return JS_ThrowTypeError(jsctx, "Setting a node to PYTHON not supported.");
        case NODE_IMPORT:
            return JS_ThrowTypeError(jsctx, "Setting a node to IMPORT not supported.");
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
    node->type = type;
    return JS_UNDEFINED;
    }

static
QJSValue
js_dndc_node_get_type(QJSContext* jsctx, QJSValueConst thisValue){
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);
    return JS_NewInt32(jsctx, node->type);
    }

static
QJSValue
js_dndc_node_get_children(QJSContext* jsctx, QJSValueConst thisValue){
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);
    QJSValue array = JS_NewArray(jsctx);
    if(JS_IsException(array))
        return JS_EXCEPTION;
    // TODO: do these as a batch.
    NODE_CHILDREN_FOR_EACH(child, node){
        QJSValue n = js_make_dndc_node(jsctx, *child);
        QJSValue call = JS_ArrayPush(jsctx, array, 1, &n);
        JS_FreeValue(jsctx, n);
        if(JS_IsException(call)){
            JS_FreeValue(jsctx, array);
            return call;
            }
        }
    QJSValue froze = js_freeze_object(jsctx, array);
    if(JS_IsException(froze)){
        JS_FreeValue(jsctx, array);
        return froze;
        }
    else {
        JS_FreeValue(jsctx, froze);
        return array;
        }
    }

JSGETTER(js_dndc_node_get_header){
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);

    return JS_NewStringLen(jsctx, node->header.text, node->header.length);
    }

JSSETTER(js_dndc_node_set_header){
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);
    StringView new_header = jsstring_to_stringview(jsctx, arg, ctx->string_allocator);
    if(!new_header.text)
        return JS_EXCEPTION;
    node->header = new_header;
    return JS_UNDEFINED;
    }

JSGETTER(js_dndc_node_get_id){
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);
    auto id = node_get_id(node);
    if(!id){
        return JS_NewString(jsctx, "");
        }
    MStringBuilder msb = {.allocator = ctx->temp_allocator};
    msb_write_kebab(&msb, id->text, id->length);
    StringView keb = msb_borrow(&msb);
    QJSValue result = JS_NewStringLen(jsctx, keb.text, keb.length);
    msb_destroy(&msb);
    return result;
    }

JSSETTER(js_dndc_node_set_id){
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);
    if(!JS_IsString(arg)){
        return JS_ThrowTypeError(jsctx, "id must be a string");
        }
    StringView new_id = jsstring_to_stringview(jsctx, arg, ctx->string_allocator);
    node_set_attribute(node, ctx->allocator, SV("id"), new_id);
    return JS_UNDEFINED;
    }

static
QJSValue
js_dndc_node_to_string(QJSContext* jsctx, QJSValueConst thisValue, int argc, QJSValueConst* argv){
    (void)argc, (void)argv;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);
    MStringBuilder msb = {.allocator=ctx->temp_allocator};
    size_t class_count = node->classes?node->classes->count:0;
    if(!class_count)
        MSB_FORMAT(&msb, "Node(", NODENAMES[node->type], ", '", node->header, "', [", (int)node->children.count, " children])");
    else {
        MSB_FORMAT(&msb, "Node(", NODENAMES[node->type].text);
        RARRAY_FOR_EACH(class, node->classes){
            MSB_FORMAT(&msb, ".", *class);
            }
        MSB_FORMAT(&msb, ", '", node->header, "', [", (int)node->children.count, " children])");
        }
    StringView text = msb_borrow(&msb);
    QJSValue result = JS_NewStringLen(jsctx, text.text, text.length);
    msb_destroy(&msb);
    return result;
    }

static
QJSValue
js_dndc_node_get_parent(QJSContext* jsctx, QJSValueConst thisValue){
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    Node* node = get_node(ctx, handle);
    NodeHandle parent_handle = node->parent;
    if(NodeHandle_eq(parent_handle, INVALID_NODE_HANDLE)){
        return JS_NULL;
        }
    return js_make_dndc_node(jsctx, parent_handle);
    }

JSGETTER(js_dndc_node_get_attributes){
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    QJSValue obj = JS_NewObjectClass(jsctx, JS_DNDC_ATTRIBUTES_CLASS_ID);
    if(JS_IsException(obj))
        return obj;
    JS_SetOpaque(obj, NodeHandle_to_opaque(handle));
    return obj;
    }

JSGETTER(js_dndc_node_get_classes){
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    QJSValue obj = JS_NewObjectClass(jsctx, JS_DNDC_CLASSLIST_CLASS_ID);
    if(JS_IsException(obj))
        return obj;
    JS_SetOpaque(obj, NodeHandle_to_opaque(handle));
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    Node* node = get_node(ctx, handle);
    uint32_t i = 0;
    RARRAY_FOR_EACH(c, node->classes){
        JS_DefinePropertyValueUint32(jsctx, obj, i, JS_NewStringLen(jsctx, c->text, c->length), JS_PROP_ENUMERABLE);
        i++;
        }
    JS_DefinePropertyValueStr(jsctx, obj, "length", JS_NewUint32(jsctx, i), JS_PROP_ENUMERABLE);
    return obj;
    }

JSMETHOD(js_dndc_node_err){
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "err must be called with 1 string argument");
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    Node* node = get_node(ctx, handle);
    LongString msg = jsstring_to_longstring(jsctx, argv[0], ctx->string_allocator);
    if(!msg.text)
        return JS_EXCEPTION;
    node_set_err(ctx, node, msg);
    return JS_ThrowTypeError(jsctx, "placeholder");
    }
JSMETHOD(js_dndc_node_has_class){
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "err must be called with 1 string argument");
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    Node* node = get_node(ctx, handle);
    StringView msg = jsstring_to_stringview(jsctx, argv[0], ctx->temp_allocator);
    if(!msg.text)
        return JS_EXCEPTION;
    bool has_it = node_has_class(node, msg);
    Allocator_free(ctx->temp_allocator, msg.text, msg.length);
    return has_it? JS_TRUE : JS_FALSE;
    }

//
// DndcContext methods
//
JSMETHOD(js_dndc_context_make_string){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "Need 1 string arg to make_string");
    QJSValueConst arg = argv[1];
    if(!JS_IsString(arg))
        return JS_ThrowTypeError(jsctx, "Need 1 string arg to make_string");
    StringView sv = jsstring_to_stringview(jsctx, arg, ctx->string_allocator);
    NodeHandle new_handle = alloc_handle(ctx);
    {
        Node* node = get_node(ctx, new_handle);
        node->header = sv;
        node->type = NODE_STRING;
    }
    return js_make_dndc_node(jsctx, new_handle);
    }

JSMETHOD(js_dndc_context_make_node){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(argc == 0 or argc > 2)
        return JS_ThrowTypeError(jsctx, "Need type arg and an optional options obj as arguments to make_node");
    QJSValueConst obj = argc == 1? JS_UNDEFINED:argv[1];
    int32_t type;
    if(JS_ToInt32(jsctx, &type, argv[0]))
        return JS_EXCEPTION;
    if(type < 0 || type >= NODE_INVALID)
        return JS_ThrowTypeError(jsctx, "type argument invalid");
    QJSValue header = JS_UNDEFINED;
    QJSValue classes = JS_UNDEFINED;
    QJSValue attributes = JS_UNDEFINED;
    QJSValue failure = JS_UNDEFINED;
    if(argc != 1){
        header = JS_GetPropertyStr(jsctx, obj, "header");
        classes = JS_GetPropertyStr(jsctx, obj, "classes");
        attributes = JS_GetPropertyStr(jsctx, obj, "attributes");
        }
    NodeHandle handle = alloc_handle(ctx);
    Node* node = get_node(ctx, handle);
    node->type = type;
    if(!JS_IsUndefined(header)){
        StringView sv = jsstring_to_stringview(jsctx, header, ctx->string_allocator);
        if(!sv.text){
            failure = JS_EXCEPTION;
            goto fail;
            }
        node->header = sv;
        }
    if(!JS_IsUndefined(classes)){
        if(!JS_IsArray(jsctx, classes)){
            failure = JS_ThrowTypeError(jsctx, "classes should be an array");
            goto fail;
            }
        QJSValue length_ = JS_GetPropertyStr(jsctx, classes, "length");
        int32_t length;
        if(JS_ToInt32(jsctx, &length, length_)){
            JS_FreeValue(jsctx, length_);
            failure = JS_EXCEPTION;
            goto fail;
            }
        for(int32_t i = 0; i < length; i++){
            QJSValue s = JS_GetPropertyUint32(jsctx, classes, i);
            StringView sv = jsstring_to_stringview(jsctx, s, ctx->string_allocator);
            JS_FreeValue(jsctx, s);
            if(!sv.text){
                failure = JS_EXCEPTION;
                goto fail;
                }
            node->classes = Rarray_push(StringView)(node->classes, ctx->allocator, sv);
            }
        }
    if(!JS_IsUndefined(attributes)){
        if(!JS_IsArray(jsctx, attributes)){
            failure = JS_ThrowTypeError(jsctx, "attributes should be an array");
            goto fail;
            }
        QJSValue length_ = JS_GetPropertyStr(jsctx, attributes, "length");
        int32_t length;
        if(JS_ToInt32(jsctx, &length, length_)){
            JS_FreeValue(jsctx, length_);
            failure = JS_EXCEPTION;
            goto fail;
            }
        for(int32_t i = 0; i < length; i++){
            QJSValue s = JS_GetPropertyUint32(jsctx, attributes, i);
            StringView sv = jsstring_to_stringview(jsctx, s, ctx->string_allocator);
            JS_FreeValue(jsctx, s);
            if(!sv.text){
                failure = JS_EXCEPTION;
                goto fail;
                }
            node_set_attribute(node, ctx->allocator, sv, SV(""));
            }
        }
    Marray(NodeHandle)* node_store = NULL;
    switch(type){
        case NODE_IMPORT:
            failure = JS_ThrowTypeError(jsctx, "Creating import nodes from qjs is not supported");
            goto fail;
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
            ctx->titlenode = handle;
            break;
        case NODE_NAV:
            ctx->navnode = handle;
            break;
        default:
            break;
        }
    if(node_store)
        Marray_push(NodeHandle)(node_store, ctx->allocator, handle);
    JS_FreeValue(jsctx, header);
    JS_FreeValue(jsctx, classes);
    JS_FreeValue(jsctx, attributes);
    return js_make_dndc_node(jsctx, handle);

    fail:
    JS_FreeValue(jsctx, header);
    JS_FreeValue(jsctx, classes);
    JS_FreeValue(jsctx, attributes);
    return failure;
    }

JSMETHOD(js_dndc_context_add_dependency){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "Need 1 string argument to add_dependency");
    StringView sv = jsstring_to_stringview(jsctx, argv[0], ctx->string_allocator);
    if(!sv.text)
        return JS_EXCEPTION;
    Marray_push(StringView)(&ctx->dependencies, ctx->allocator, sv);
    return JS_UNDEFINED;
    }

JSMETHOD(js_dndc_context_kebab){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "Need 1 string argument to kebab");
    StringView sv = jsstring_to_stringview(jsctx, argv[0], ctx->temp_allocator);
    if(!sv.text)
        return JS_EXCEPTION;
    MStringBuilder msb = {.allocator = ctx->temp_allocator};
    msb_write_kebab(&msb, sv.text, sv.length);
    auto keb = msb_borrow(&msb);
    QJSValue result = JS_NewStringLen(jsctx, keb.text, keb.length);
    msb_destroy(&msb);
    Allocator_free(ctx->temp_allocator, sv.text, sv.length);
    return result;
    }

JSMETHOD(js_dndc_context_set_data){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(argc != 2)
        return JS_ThrowTypeError(jsctx, "Need 2 string argument to set_data");
    StringView key = jsstring_to_stringview(jsctx, argv[0], ctx->string_allocator);
    if(!key.text)
        return JS_EXCEPTION;
    LongString value = jsstring_to_longstring(jsctx, argv[1], ctx->string_allocator);
    if(!value.text)
        return JS_EXCEPTION;
    DataItem* new_data = Marray_alloc(DataItem)(&ctx->rendered_data, ctx->allocator);
    new_data->key = key;
    new_data->value = value;
    return JS_UNDEFINED;
    }

JSMETHOD(js_dndc_context_read_file){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "Need 1 string argument to read_file");
    StringView path = jsstring_to_stringview(jsctx, argv[0], ctx->temp_allocator);
    if(!path.text)
        return JS_EXCEPTION;
    auto e = ctx_load_source_file(ctx, path);
    Allocator_free(ctx->temp_allocator, path.text, path.length);
    if(e.errored){
        QJSValue error = JS_ThrowTypeError(jsctx, "bad path");
        return error;
        }
    auto text = e.result;
    return JS_NewStringLen(jsctx, text.text, text.length);
    }

JSMETHOD(js_dndc_context_select_nodes){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "Need 1 obj argument to select_nodes");
    QJSValue arg = argv[0];
    if(!JS_IsObject(arg))
        return JS_ThrowTypeError(jsctx, "Need 1 obj argument to select_nodes");
    LinearAllocator la = new_linear_storage(1024*1024, "select_nodes allocator");
    Allocator tmp = {.type = ALLOCATOR_LINEAR, ._data = &la};
    int32_t type = -1;
    Marray(StringView) attributes_array = {};
    Marray(StringView) classes_array = {};
    {
        QJSValue jstype_ = JS_GetPropertyStr(jsctx, arg, "type");
        if(JS_IsException(jstype_))
            return jstype_;
        if(!JS_IsUndefined(jstype_)){
            int32_t type_;
            if(JS_ToInt32(jsctx, &type_, jstype_))
                return JS_EXCEPTION;
            if(type_ < 0 || type_ >= NODE_INVALID)
                return JS_ThrowTypeError(jsctx, "type argument invalid");
            type = type_;
            }
    }
    QJSValue classes = JS_UNDEFINED;
    QJSValue attributes = JS_UNDEFINED;
    QJSValue failure = JS_UNDEFINED;
    classes = JS_GetPropertyStr(jsctx, arg, "classes");
    attributes = JS_GetPropertyStr(jsctx, arg, "attributes");
    if(!JS_IsUndefined(classes)){
        if(!JS_IsArray(jsctx, classes)){
            failure = JS_ThrowTypeError(jsctx, "classes should be an array");
            goto fail;
            }
        QJSValue length_ = JS_GetPropertyStr(jsctx, classes, "length");
        int32_t length;
        if(JS_ToInt32(jsctx, &length, length_)){
            JS_FreeValue(jsctx, length_);
            failure = JS_EXCEPTION;
            goto fail;
            }
        if(length > 0)
            Marray_ensure_total(StringView)(&classes_array, tmp, length);
        for(int32_t i = 0; i < length; i++){
            QJSValue s = JS_GetPropertyUint32(jsctx, classes, i);
            StringView sv = jsstring_to_stringview(jsctx, s, tmp);
            JS_FreeValue(jsctx, s);
            if(!sv.text){
                failure = JS_EXCEPTION;
                goto fail;
                }
            classes_array.data[classes_array.count++] = sv;
            }
        }
    if(!JS_IsUndefined(attributes)){
        if(!JS_IsArray(jsctx, attributes)){
            failure = JS_ThrowTypeError(jsctx, "attributes should be an array");
            goto fail;
            }
        QJSValue length_ = JS_GetPropertyStr(jsctx, attributes, "length");
        int32_t length;
        if(JS_ToInt32(jsctx, &length, length_)){
            JS_FreeValue(jsctx, length_);
            failure = JS_EXCEPTION;
            goto fail;
            }
        if(length > 0)
            Marray_ensure_total(StringView)(&attributes_array, tmp, length);
        for(int32_t i = 0; i < length; i++){
            QJSValue s = JS_GetPropertyUint32(jsctx, attributes, i);
            StringView sv = jsstring_to_stringview(jsctx, s, tmp);
            JS_FreeValue(jsctx, s);
            if(!sv.text){
                failure = JS_EXCEPTION;
                goto fail;
                }
            attributes_array.data[attributes_array.count++] = sv;
            }
        }
    QJSValue result = JS_NewArray(jsctx);
    if(!classes_array.count && !attributes_array.count && type < 0){
        for(size_t i = 0; i < ctx->nodes.count; i++){
            JS_SetPropertyUint32(jsctx, result, i, js_make_dndc_node(jsctx, (NodeHandle){.index=i}));
            }
        }
    else {
        size_t idx = 0;
        for(size_t i = 0; i < ctx->nodes.count; i++){
            Node* node = &ctx->nodes.data[i];
            if(type >= 0){
                if(node->type != type)
                    goto Continue;
                }
            MARRAY_FOR_EACH(attr, attributes_array){
                if(!node_has_attribute(node, *attr))
                    goto Continue;
                }
            MARRAY_FOR_EACH(class_, classes_array){
                if(!node_has_class(node, *class_))
                    goto Continue;
                }
            JS_SetPropertyUint32(jsctx, result, idx++, js_make_dndc_node(jsctx, (NodeHandle){.index=i}));
            Continue:;
            }
        }
    destroy_linear_storage(&la);
    JS_FreeValue(jsctx, classes);
    JS_FreeValue(jsctx, attributes);
    return result;

    fail:
    destroy_linear_storage(&la);
    JS_FreeValue(jsctx, classes);
    JS_FreeValue(jsctx, attributes);
    return failure;
    }

JSMETHOD(js_dndc_context_to_string){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(argc != 0)
        return JS_ThrowTypeError(jsctx, "toString takes no arguments.");
    (void)argv;
    MStringBuilder msb = {.allocator = ctx->temp_allocator};
    msb_write_literal(&msb, "{\n");
    MSB_FORMAT(&msb, "  nodes: [", ctx->nodes.count, " nodes],\n");
    MSB_FORMAT(&msb, "  filename: \"", ctx->filename, "\",\n");
    MSB_FORMAT(&msb, "  base: \"", ctx->base_directory, "\",\n");
    MSB_FORMAT(&msb, "  outputfile: \"", ctx->outputfile, "\",\n");
    MSB_FORMAT(&msb, "  dependencies: [", ctx->dependencies.count, " dependencies],\n");
    // TODO: hex format
    MSB_FORMAT(&msb, "  flags: ", ctx->flags, ",\n");
    msb_write_literal(&msb, "}");
    StringView text = msb_borrow(&msb);
    QJSValue result = JS_NewStringLen(jsctx, text.text, text.length);
    msb_destroy(&msb);
    return result;
    }

static
DndcContext*_Nullable
js_get_dndc_context(QJSContext* ctx, QJSValue thisValue){
    return JS_GetOpaque2(ctx, thisValue, JS_DNDC_CONTEXT_CLASS_ID);
    }

static
QJSValue
js_make_dndc_context(QJSContext*jsctx, DndcContext* ctx){
    QJSValue obj = JS_NewObjectClass(jsctx, JS_DNDC_CONTEXT_CLASS_ID);
    if(JS_IsException(obj))
        return obj;
    JS_SetOpaque(obj, ctx);
    return obj;
    }

static
QJSValue
js_dndc_context_get_root(QJSContext* jsctx, QJSValueConst thisValue){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(NodeHandle_eq(ctx->root_handle, INVALID_NODE_HANDLE)){
        return JS_NULL;
        }
    return js_make_dndc_node(jsctx, ctx->root_handle);
    }

static
QJSValue
js_dndc_context_set_root(QJSContext* jsctx, QJSValueConst thisValue, QJSValueConst node){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(JS_IsNull(node)){
        ctx->root_handle = INVALID_NODE_HANDLE;
        return JS_UNDEFINED;
        }
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, node, &handle))
        return JS_EXCEPTION;
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return JS_NULL;
    ctx->root_handle = handle;
    return JS_UNDEFINED;
    }

JSGETTER(js_dndc_context_get_outfile){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    StringView filename = ctx->outputfile.length?path_basename(LS_to_SV(ctx->outputfile)):SV("");
    return JS_NewStringLen(jsctx, filename.text, filename.length);
    }

JSGETTER(js_dndc_context_get_outpath){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    LongString filename = ctx->outputfile;
    return JS_NewStringLen(jsctx, filename.text, filename.length);
    }

JSGETTER(js_dndc_context_get_sourcepath){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    StringView filename = ctx->filename;
    return JS_NewStringLen(jsctx, filename.text, filename.length);
    }

JSGETTER(js_dndc_context_get_base){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    LongString base = ctx->base_directory;
    if(!base.length)
        base = LS(".");
    return JS_NewStringLen(jsctx, base.text, base.length);
    }

JSGETTER(js_dndc_context_get_all_nodes){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    QJSValue result = JS_NewArray(jsctx);
    for(size_t i = 0; i < ctx->nodes.count; i++){
        QJSValue n = js_make_dndc_node(jsctx, (NodeHandle){._value=i});
        JS_ArrayPush(jsctx, result, 1, &n);
        JS_FreeValue(jsctx, n);
        }
    return result;
    }

//
// DndcNodeAttributes
//

JSMETHOD(js_dndc_attributes_get){
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "get takes 1 argument");
    QJSValueConst arg = argv[0];
    if(!JS_IsString(arg))
        return JS_ThrowTypeError(jsctx, "get takes 1 string argument");
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_attributes_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    StringView key = jsstring_make_stringview_js_allocated(jsctx, arg);
    if(!key.text)
        return JS_EXCEPTION;
    Node* node = get_node(ctx, handle);
    StringView* value = node_get_attribute(node, key);
    JS_FreeCString(jsctx, key.text);
    if(!value)
        return JS_UNDEFINED;
    else
        return JS_NewStringLen(jsctx, value->text, value->length);
    }

JSMETHOD(js_dndc_attributes_set){
    if(argc == 0 || argc > 2)
        return JS_ThrowTypeError(jsctx, "set takes 1 or 2 arguments");
    QJSValueConst key_arg = argv[0];
    if(!JS_IsString(key_arg))
        return JS_ThrowTypeError(jsctx, "get takes 1 string argument");
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_attributes_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    StringView key = jsstring_to_stringview(jsctx, key_arg, ctx->string_allocator); // extra alloc if key already exists. Could fix this.
    if(!key.text)
        return JS_EXCEPTION;
    Node* node = get_node(ctx, handle);
    if(argc == 1){
        StringView value = jsstring_to_stringview(jsctx, argv[1], ctx->string_allocator);
        if(!value.text)
            return JS_EXCEPTION;
        node_set_attribute(node, ctx->allocator, key, value);
        }
    else {
        node_set_attribute(node, ctx->allocator, key, SV(""));
        }
    return JS_UNDEFINED;
    }

JSMETHOD(js_dndc_attributes_to_string){
    if(argc != 0){
        return JS_ThrowTypeError(jsctx, "toString take no arguments");
        }
    (void)argv;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_attributes_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);
    MStringBuilder msb = {.allocator = ctx->temp_allocator};
    msb_write_literal(&msb, "{ ");
    RARRAY_FOR_EACH(kv, node->attributes){
        MSB_FORMAT(&msb, "\n  ", kv->key, ": \"", kv->value, "\",");
        }
    msb_erase(&msb, 1);
    msb_write_literal(&msb, "\n}");
    StringView text = msb_borrow(&msb);
    QJSValue result = JS_NewStringLen(jsctx, text.text, text.length);
    msb_destroy(&msb);
    return result;
    }

JSMETHOD(js_dndc_attributes_entries){
    if(argc != 0){
        return JS_ThrowTypeError(jsctx, "toString take no arguments");
        }
    (void)argv;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_attributes_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);
    QJSValue result = JS_NewArray(jsctx);
    RARRAY_FOR_EACH(kv, node->attributes){
        QJSValue pair = JS_NewArray(jsctx);
        QJSValue js_kv[2] = {
            JS_NewStringLen(jsctx, kv->key.text, kv->key.length),
            JS_NewStringLen(jsctx, kv->value.text, kv->value.length),
            };
        QJSValue call = JS_ArrayPush(jsctx, pair, 2, js_kv);
        assert(!JS_IsException(call));
        JS_FreeValue(jsctx, js_kv[0]);
        JS_FreeValue(jsctx, js_kv[1]);
        JS_ArrayPush(jsctx, result, 1, &pair);
        JS_FreeValue(jsctx, pair);
        }
    QJSValue values = JS_GetPropertyStr(jsctx, result, "values");
    QJSValue realresult = JS_Call(jsctx, values, result, 0, NULL);
    JS_FreeValue(jsctx, values);
    JS_FreeValue(jsctx, result);
    result = realresult;
    return result;
    }
//
// DndcNodeClassList methods
//

JSMETHOD(js_dndc_classlist_append){
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "append takes 1 string argument");
    QJSValueConst arg = argv[0];
    if(!JS_IsString(arg))
        return JS_ThrowTypeError(jsctx, "append takes 1 string argument");
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_classlist_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    Node* node = get_node(ctx, handle);
    StringView c = jsstring_to_stringview(jsctx, arg, ctx->string_allocator);
    if(!c.text)
        return JS_EXCEPTION;
    node->classes = Rarray_push(StringView)(node->classes, ctx->allocator, c);
    return JS_UNDEFINED;
    }

JSMETHOD(js_dndc_classlist_to_string){
    if(argc != 0)
        return JS_ThrowTypeError(jsctx, "append takes no argument");
    (void)argv;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_classlist_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    Node* node = get_node(ctx, handle);
    MStringBuilder msb = {.allocator = ctx->temp_allocator};
    msb_write_char(&msb, '[');
    RARRAY_FOR_EACH(c, node->classes){
        MSB_FORMAT(&msb, "\"", *c, "\", ");
        }
    if(msb.cursor != 1)
        msb_erase(&msb, 2);
    msb_write_char(&msb, ']');
    StringView text = msb_borrow(&msb);
    QJSValue result = JS_NewStringLen(jsctx, text.text, text.length);
    msb_destroy(&msb);
    return result;
    }

JSMETHOD(js_dndc_classlist_values){
    if(argc != 0)
        return JS_ThrowTypeError(jsctx, "values takes no argument");
    (void)argv;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_classlist_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    Node* node = get_node(ctx, handle);
    QJSValue array = JS_NewArray(jsctx);
    unsigned i = 0;
    RARRAY_FOR_EACH(c, node->classes){
        JS_SetPropertyUint32(jsctx, array, i, JS_NewStringLen(jsctx, c->text, c->length));
        i++;
        }
    QJSValue values = JS_GetPropertyStr(jsctx, array, "values");
    QJSValue result = JS_Call(jsctx, values, array, 0, NULL);
    JS_FreeValue(jsctx, array);
    JS_FreeValue(jsctx, values);
    return result;
    }

#undef JSMETHOD
#undef JSSETTER
#undef JSGETTER

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
