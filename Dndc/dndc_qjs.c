#ifndef DNDC_QJS_C
#define DNDC_QJS_C
#include "msb_extensions.h"
#include "MStringBuilder.h"
#include "dndc.h"
#include "dndc_funcs.h"
#include "dndc_types.h"
#include "msb_format.h"
#include "str_util.h"
#include "file_util.h"
#include <sys/stat.h>
#if defined(__APPLE__) || defined(__linux__)
#include <fts.h>
#elif defined(_WIN32)
#endif

PushDiagnostic()
#ifdef __clang__
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#include "quickjs.h"
PopDiagnostic()

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum { ZERO_NODE_VALUE = INVALID_NODE_HANDLE_VALUE-1 };
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
#define JSMAGICSETTER(name) \
    static QJSValue \
    name(QJSContext* jsctx, QJSValueConst thisValue, QJSValueConst arg, int magic)
#define JSGETTER(name) \
    static QJSValue \
    name(QJSContext* jsctx, QJSValueConst thisValue)
#define JSMAGICGETTER(name) \
    static QJSValue \
    name(QJSContext* jsctx, QJSValueConst thisValue, int magic)

//
// Utility or free functions
//

static
QJSValue
js_make_dndc_context(QJSContext*, DndcContext*);

static
QJSValue
js_make_dndc_node(QJSContext*, NodeHandle);

static
void
log_js_traceback(DndcContext* ctx, QJSContext* jsctx, NodeHandle handle);

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
LongString
jsstring_to_kebabed(QJSContext* jsctx, QJSValueConst v, Allocator a);

static inline
force_inline
StringView
jsstring_to_stringview(QJSContext* jsctx, QJSValueConst v, Allocator a);

static inline
force_inline
StringView
jsstring_make_stringview_js_allocated(QJSContext* jsctx, QJSValueConst v);

JSMETHOD(js_console_log);

//
// File System functions
//
JSMETHOD(js_load_file_as_base64);
JSMETHOD(js_load_file);
JSMETHOD(js_list_dnd_files);
JSMETHOD(js_path_exists);
JSMETHOD(js_write_file);
// JSMETHOD(js_file_exists);
// JSMETHOD(js_dir_exists);

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
js_get_dndc_context(QJSContext*, QJSValueConst);

//
// DndcContext methods
//
JSGETTER(js_dndc_context_get_root);
JSSETTER(js_dndc_context_set_root);
JSGETTER(js_dndc_context_get_sourcepath);
JSGETTER(js_dndc_context_get_base);
JSGETTER(js_dndc_context_get_all_nodes);
JSMETHOD(js_dndc_context_make_string);
JSMETHOD(js_dndc_context_make_node);
JSMETHOD(js_dndc_context_add_dependency);
JSMETHOD(js_dndc_context_kebab);
JSMETHOD(js_dndc_context_html_escape);
JSMETHOD(js_dndc_context_select_nodes);
JSMETHOD(js_dndc_context_to_string);
JSMETHOD(js_dndc_context_add_link);

static
const
JSCFunctionListEntry JS_DNDC_CONTEXT_FUNCS[] = {
    JS_CGETSET_DEF("root", js_dndc_context_get_root, js_dndc_context_set_root),
    JS_CGETSET_DEF("sourcepath", js_dndc_context_get_sourcepath, NULL),
    JS_CGETSET_DEF("base", js_dndc_context_get_base, NULL),
    JS_CGETSET_DEF("all_nodes", js_dndc_context_get_all_nodes, NULL),
    JS_CFUNC_DEF("make_string", 1, js_dndc_context_make_string),
    JS_CFUNC_DEF("make_node", 2, js_dndc_context_make_node),
    JS_CFUNC_DEF("add_dependency", 1, js_dndc_context_add_dependency),
    JS_CFUNC_DEF("kebab", 1, js_dndc_context_kebab),
    JS_CFUNC_DEF("html_escape", 1, js_dndc_context_html_escape),
    JS_CFUNC_DEF("select_nodes", 1, js_dndc_context_select_nodes),
    JS_CFUNC_DEF("toString", 0, js_dndc_context_to_string),
    JS_CFUNC_DEF("add_link", 2, js_dndc_context_add_link),
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
JSMAGICSETTER(js_dndc_node_flag_setter);
JSMAGICGETTER(js_dndc_node_flag_getter);
JSGETTER(js_dndc_node_get_children);
JSGETTER(js_dndc_node_get_location);
JSMETHOD(js_dndc_node_to_string);
JSGETTER(js_dndc_node_get_header);
JSSETTER(js_dndc_node_set_header);
JSGETTER(js_dndc_node_get_id);
JSSETTER(js_dndc_node_set_id);
JSGETTER(js_dndc_node_get_internal_id);
JSMETHOD(js_dndc_node_parse);
JSMETHOD(js_dndc_node_detach);
JSMETHOD(js_dndc_node_make_child);
JSMETHOD(js_dndc_node_add_child);
JSMETHOD(js_dndc_node_replace_child);
JSMETHOD(js_dndc_node_insert_child);
JSGETTER(js_dndc_node_get_attributes);
JSGETTER(js_dndc_node_get_classes);
JSMETHOD(js_dndc_node_err);
JSMETHOD(js_dndc_node_has_class);
JSMETHOD(js_dndc_node_clone);
JSMETHOD(js_dndc_node_set);
JSMETHOD(js_dndc_node_get);

static
const
JSCFunctionListEntry JS_DNDC_NODE_FUNCS[] = {
    JS_CGETSET_DEF("parent", js_dndc_node_get_parent, NULL),
    JS_CGETSET_DEF("type", js_dndc_node_get_type, js_dndc_node_set_type),
    JS_CGETSET_MAGIC_DEF("noinline", js_dndc_node_flag_getter, js_dndc_node_flag_setter, NODEFLAG_NOINLINE),
    JS_CGETSET_MAGIC_DEF("noid", js_dndc_node_flag_getter, js_dndc_node_flag_setter, NODEFLAG_NOID),
    JS_CGETSET_MAGIC_DEF("hide", js_dndc_node_flag_getter, js_dndc_node_flag_setter, NODEFLAG_HIDE),
    JS_CGETSET_DEF("children", js_dndc_node_get_children, NULL),
    JS_CGETSET_DEF("location", js_dndc_node_get_location, NULL),
    JS_CFUNC_DEF("toString", 0, js_dndc_node_to_string),
    JS_CGETSET_DEF("header", js_dndc_node_get_header, js_dndc_node_set_header),
    JS_CGETSET_DEF("id", js_dndc_node_get_id, js_dndc_node_set_id),
    JS_CFUNC_DEF("parse", 1, js_dndc_node_parse),
    JS_CFUNC_DEF("detach", 0, js_dndc_node_detach),
    JS_CFUNC_DEF("make_child", 2, js_dndc_node_make_child),
    JS_CFUNC_DEF("add_child", 1, js_dndc_node_add_child),
    JS_CFUNC_DEF("replace_child", 2, js_dndc_node_replace_child),
    JS_CFUNC_DEF("insert_child", 2, js_dndc_node_insert_child),
    JS_CGETSET_DEF("attributes", js_dndc_node_get_attributes, NULL),
    JS_CGETSET_DEF("classes", js_dndc_node_get_classes, NULL),
    JS_CFUNC_DEF("err", 1, js_dndc_node_err),
    JS_CFUNC_DEF("has_class", 1, js_dndc_node_has_class),
    JS_CFUNC_DEF("clone", 0, js_dndc_node_clone),
    JS_CFUNC_DEF("set", 2, js_dndc_node_set),
    JS_CFUNC_DEF("get", 1, js_dndc_node_get),
    JS_CGETSET_DEF("internal_id", js_dndc_node_get_internal_id, NULL),
};

//
// DndcNodeLocation
//
//
static JSClassID JS_DNDC_LOCATION_CLASS_ID;

static
JSClassDef JS_DNDC_LOCATION_CLASS = {
    .class_name = "DndcNodeLocation",
};

JSMAGICGETTER(js_dndc_node_location_getter);
JSMETHOD(js_dndc_node_location_to_string);

static
const
JSCFunctionListEntry JS_DNDC_LOCATION_FUNCS[] = {
    JS_CGETSET_MAGIC_DEF("filename", js_dndc_node_location_getter, NULL, 0),
    JS_CGETSET_MAGIC_DEF("row", js_dndc_node_location_getter, NULL, 1),
    JS_CGETSET_MAGIC_DEF("column", js_dndc_node_location_getter, NULL, 2),
    JS_CFUNC_DEF("toString", 0, js_dndc_node_location_to_string),
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
JSMETHOD(js_dndc_attributes_has);
JSMETHOD(js_dndc_attributes_set);
JSMETHOD(js_dndc_attributes_to_string);
JSMETHOD(js_dndc_attributes_entries);

static
const
JSCFunctionListEntry JS_DNDC_ATTRIBUTES_FUNCS[] = {
    JS_CFUNC_DEF("get", 1, js_dndc_attributes_get),
    JS_CFUNC_DEF("has", 1, js_dndc_attributes_has),
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
QJSRuntime*_Nullable
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
        goto fail;
    }
    JS_NewClassID(&JS_DNDC_LOCATION_CLASS_ID);
    if(JS_NewClass(rt, JS_DNDC_LOCATION_CLASS_ID, &JS_DNDC_LOCATION_CLASS) < 0){
        goto fail;
    }
    JS_NewClassID(&JS_DNDC_ATTRIBUTES_CLASS_ID);
    if(JS_NewClass(rt, JS_DNDC_ATTRIBUTES_CLASS_ID, &JS_DNDC_ATTRIBUTES_CLASS) < 0){
        goto fail;
    }
    JS_NewClassID(&JS_DNDC_CLASSLIST_CLASS_ID);
    if(JS_NewClass(rt, JS_DNDC_CLASSLIST_CLASS_ID, &JS_DNDC_CLASSLIST_CLASS) < 0){
        goto fail;
    }
    JS_NewClassID(&JS_DNDC_NODE_CLASS_ID);
    if(JS_NewClass(rt, JS_DNDC_NODE_CLASS_ID, &JS_DNDC_NODE_CLASS) < 0){
        goto fail;
    }
    return rt;
    fail:
    ArenaAllocator_free_all(aa);
    return NULL;
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

static
QJSContext*_Nullable
new_qjs_ctx(QJSRuntime* rt, DndcContext* ctx, LongString jsargs){
    QJSContext* jsctx = NULL;
    jsctx = JS_NewContext(rt);
    if(!jsctx)
        goto fail;
    // setup DndcContext class
    {
        QJSValue proto = JS_NewObject(jsctx); // new ref
        JS_SetPropertyFunctionList(jsctx, proto, JS_DNDC_CONTEXT_FUNCS, arrlen(JS_DNDC_CONTEXT_FUNCS));
        JS_SetClassProto(jsctx, JS_DNDC_CONTEXT_CLASS_ID, proto); // steals ref
    }

    // setup DndcNodeLocation class
    {
        QJSValue proto = JS_NewObject(jsctx); // new ref
        JS_SetPropertyFunctionList(jsctx, proto, JS_DNDC_LOCATION_FUNCS, arrlen(JS_DNDC_LOCATION_FUNCS));
        JS_SetClassProto(jsctx, JS_DNDC_LOCATION_CLASS_ID, proto); // steals ref
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

    // Globals, console
    {
        QJSValue global_obj, console, dctx, node_types, args;
        global_obj = JS_GetGlobalObject(jsctx); // new ref
        dctx = js_make_dndc_context(jsctx, ctx); // new_ref
        node_types = JS_NewObject(jsctx); // new ref
        for(size_t i = 0; i < arrlen(NODENAMES)-1;i++){
            JS_SetPropertyStr(jsctx, node_types, NODENAMES[i].text, JS_NewUint32(jsctx, i));
            }
        JS_SetPropertyStr(jsctx, global_obj, "NodeType", node_types); // steals ref

        // console
        console = JS_NewObject(jsctx); // new ref
        JS_SetPropertyStr(jsctx, console, "log", JS_NewCFunction(jsctx, js_console_log, "log", 1)); // create and steal ref all in one go.
        JS_SetPropertyStr(jsctx, global_obj, "console", console); // steals ref

        // filesystem
        {
            QJSValue filesystem = JS_NewObject(jsctx); // new ref
            JS_SetPropertyStr(jsctx, filesystem, "load_file_as_base64", JS_NewCFunction(jsctx, js_load_file_as_base64, "load_file_as_base64", 1)); // create and steal in one go.
            JS_SetPropertyStr(jsctx, filesystem, "load_file", JS_NewCFunction(jsctx, js_load_file, "load_file", 1)); // create and steal in one go.
            JS_SetPropertyStr(jsctx, filesystem, "list_dnd_files", JS_NewCFunction(jsctx, js_list_dnd_files, "list_dnd_files", 1)); // create and steal in one go.
            JS_SetPropertyStr(jsctx, filesystem, "exists", JS_NewCFunction(jsctx, js_path_exists, "exists", 1)); // create and steal in one go.
            JS_SetPropertyStr(jsctx, filesystem, "write_file", JS_NewCFunction(jsctx, js_write_file, "write_file", 2)); // create and steal in one go.
            JS_SetPropertyStr(jsctx, global_obj, "FileSystem", filesystem); // Steals ref
        }

        if(!jsargs.length)
            jsargs = LS("null");
        args = JS_ParseJSON2(jsctx, jsargs.text, jsargs.length, "jsargs", JS_PARSE_JSON_EXT);
        if(JS_IsException(args)){
            report_system_error(ctx, SV("Failed to parse jsargs as JSON"));
            goto fail;
        }
        JS_SetPropertyStr(jsctx, global_obj, "Args", args); // steals ref
        JS_SetPropertyStr(jsctx, global_obj, "ctx", dctx); // steals ref
        JS_FreeValue(jsctx, global_obj); // decref


    }

    return jsctx;


    fail:
    if(jsctx)
        JS_FreeContext(jsctx);
    return NULL;
}

//
// The main execution function.
// str must be nul-terminated (underlying library). I should
// probably type it as a LongString
//
static
warn_unused
int
execute_qjs_string(QJSContext* jsctx, DndcContext* ctx, const char* str, size_t length, NodeHandle handle, NodeHandle firstline){
    int result = 0;


    {
        QJSValue global_obj, node;
        global_obj = JS_GetGlobalObject(jsctx); // new ref
        node = js_make_dndc_node(jsctx, handle); // new_ref

        JS_SetPropertyStr(jsctx, global_obj, "node", node); // steals ref
        JS_FreeValue(jsctx, global_obj); // decref
    }
    {
        const char* filename;
        {
            Node* node = get_node(ctx, firstline);
            StringView node_filename = ctx->filenames.data[node->filename_idx];
            filename = Allocator_strndup(string_allocator(ctx), node_filename.text, node_filename.length);
        }

        QJSValue err = JS_Eval(jsctx, str, length, filename, 1);
        if(JS_IsException(err)){
            log_js_traceback(ctx, jsctx, handle);
            result = DNDC_ERROR_JS;
        }
        JS_FreeValue(jsctx, err);
    }

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
        return (LongString){0};
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
LongString
jsstring_to_kebabed(QJSContext* jsctx, QJSValueConst v, Allocator a){
    size_t len;
    const char* str = JS_ToCStringLen(jsctx, &len, v);
    if(!str){
        return (LongString){0};
    }
    MStringBuilder msb = {.allocator=a};
    msb_write_kebab(&msb, str, len);
    return msb_detach_ls(&msb);
}

static inline
force_inline
StringView
jsstring_make_stringview_js_allocated(QJSContext* jsctx, QJSValueConst v){
    size_t len;
    const char* str = JS_ToCStringLen(jsctx, &len, v);
    if(!str){
        return (StringView){0};
    }
    return (StringView){.text=str, .length=len};
}


// returns false on error.
static
warn_unused
bool
js_dndc_get_node_handle(QJSContext* jsctx, QJSValueConst obj, NodeHandle* out){
    void* pointer = JS_GetOpaque2(jsctx, obj, JS_DNDC_NODE_CLASS_ID);
    uintptr_t p = (uintptr_t)pointer;
    if(!p){
        return false;
    }
    if(p == (uintptr_t)ZERO_NODE_VALUE)
        out->_value = 0;
    else
        out->_value = p;
    return true;
}

static
warn_unused
bool
js_dndc_get_attributes_handle(QJSContext* jsctx, QJSValueConst obj, NodeHandle* out){
    void* pointer = JS_GetOpaque2(jsctx, obj, JS_DNDC_ATTRIBUTES_CLASS_ID);
    uintptr_t p = (uintptr_t)pointer;
    if(!p){
        return false;
    }
    if(p == (uintptr_t)ZERO_NODE_VALUE)
        out->_value = 0;
    else
        out->_value = p;
    return true;
}

static
warn_unused
bool
js_dndc_get_classlist_handle(QJSContext* jsctx, QJSValueConst obj, NodeHandle* out){
    void* pointer = JS_GetOpaque2(jsctx, obj, JS_DNDC_CLASSLIST_CLASS_ID);
    uintptr_t p = (uintptr_t)pointer;
    if(!p){
        return false;
    }
    if(p == (uintptr_t)ZERO_NODE_VALUE)
        out->_value = 0;
    else
        out->_value = p;
    return true;
}

static
void
log_js_traceback(DndcContext* ctx, QJSContext* jsctx, NodeHandle handle){
    MStringBuilder msb = {.allocator = temp_allocator(ctx)};
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
    LongString msg = msb_borrow_ls(&msb);
    handle_log_error(ctx, handle, 1, (FormatArg[]){FMT(msg)});
    msb_destroy(&msb);
}

static
void
js_console_inner(QJSContext* jsctx, QJSValueConst v, MStringBuilder* sb){
    if(JS_IsArray(jsctx, v)){
        msb_write_char(sb, '[');
        QJSValue length_ = JS_GetPropertyStr(jsctx, v, "length");
        int32_t length;
        if(JS_ToInt32(jsctx, &length, length_)){
            msb_write_literal(sb, "(Error getting array length)");
        }
        else {
            for(int32_t i = 0; i < length; i++){
                if(i != 0) msb_write_literal(sb, ", ");
                js_console_inner(jsctx, JS_GetPropertyUint32(jsctx, v, i), sb);
            }
        }
        msb_write_char(sb, ']');
        return;
    }
    switch(JS_VALUE_GET_TAG(v)){
        case JS_TAG_STRING:{
            msb_write_char(sb, '"');

            size_t len;
            const char *str = JS_ToCStringLen(jsctx, &len, v);
            if(!str) msb_write_literal(sb, "(Error converting string to string)");
            else msb_write_str(sb, str, len);
            msb_write_char(sb, '"');
        }break;
        case JS_TAG_INT:{
            int i;
            if(JS_ToInt32(jsctx, &i, v)){
                msb_write_literal(sb, "(Error converting to integer)");
            }
            else {
                MSB_FORMAT(sb, i);
            }
        }break;
        case JS_TAG_OBJECT: {
            size_t len;
            const char *str = JS_ToCStringLen(jsctx, &len, v);
            if(!str) msb_write_literal(sb, "(Error converting object to string)");
            else msb_write_str(sb, str, len);
        }break;
        default:{
            size_t len;
            const char *str = JS_ToCStringLen(jsctx, &len, v);
            if(!str) msb_write_literal(sb, "(Error converting to string)");
            else msb_write_str(sb, str, len);
        }break;
    }
}

static
QJSValue
js_console_log(QJSContext *jsctx, QJSValueConst thisValue, int argc, QJSValueConst *argv){
    (void)thisValue;
    int line_num = -1;
    const char* filename = NULL;
    JS_get_caller_location(jsctx, &filename, NULL, &line_num);
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return JS_UNDEFINED;
    if(!ctx->log_func)
        return JS_UNDEFINED;
    MStringBuilder msb = {.allocator = temp_allocator(ctx)};

    for(int i = 0; i < argc; i++){
        if(i != 0) msb_write_char(&msb, ' ');
        js_console_inner(jsctx, argv[i], &msb);
    }
    LongString msg = msb_borrow_ls(&msb);
    ctx->log_func(ctx->log_user_data, DNDC_DEBUG_MESSAGE, filename?filename:"js", filename?strlen(filename):2, line_num-1, -1, msg.text, msg.length);
    msb_destroy(&msb);
    if(filename)
        JS_FreeCString(jsctx, filename);
    return JS_UNDEFINED;
}

//
// FileSystem stuff
//
static
QJSValue
js_load_file_as_base64(QJSContext *jsctx, QJSValueConst thisValue, int argc, QJSValueConst *argv){
    (void)thisValue;
    if(argc != 1){
        return JS_ThrowTypeError(jsctx, "Must be given a single path argument");
    }
    QJSValueConst str = argv[0];
    if(!JS_IsString(str)){
        return JS_ThrowTypeError(jsctx, "Must be given a single string argument");
    }
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    if(ctx->flags & DNDC_DONT_READ){
        return JS_ThrowTypeError(jsctx, "File loading is disabled");
    }

    StringView sv = jsstring_make_stringview_js_allocated(jsctx, str);
    StringResult e = FileCache_read_and_b64_file(ctx->b64cache, sv, false);
    JS_FreeCString(jsctx, sv.text);
    if(e.errored){
        return JS_ThrowTypeError(jsctx, "Error when loading file: '%s'", sv.text);
    }
    QJSValue result = JS_NewString(jsctx, e.result.text);
    return result;
}

static
QJSValue
js_load_file(QJSContext *jsctx, QJSValueConst thisValue, int argc, QJSValueConst *argv){
    (void)thisValue;
    if(argc != 1){
        return JS_ThrowTypeError(jsctx, "Must be given a single path argument");
    }
    QJSValueConst str = argv[0];
    if(!JS_IsString(str)){
        return JS_ThrowTypeError(jsctx, "load_file must be given a single string argument");
    }
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    if(ctx->flags & DNDC_DONT_READ){
        return JS_ThrowTypeError(jsctx, "File loading is disabled");
    }
    StringView sv = jsstring_make_stringview_js_allocated(jsctx, str);
    StringViewResult e = ctx_load_source_file(ctx, sv);
    JS_FreeCString(jsctx, sv.text);
    if(e.errored){
        return JS_ThrowTypeError(jsctx, "load_file: Error when loading '%.*s'", (int)sv.length, sv.text);
    }
    QJSValue result = JS_NewString(jsctx, e.result.text);
    return result;
}

static
QJSValue
js_write_file(QJSContext *jsctx, QJSValueConst thisValue, int argc, QJSValueConst *argv){
    (void)thisValue;
    if(argc != 2){
        return JS_ThrowTypeError(jsctx, "Must be given two args: filename and data to write");
    }
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    if(!(ctx->flags & DNDC_ENABLE_JS_WRITE)){
        return JS_ThrowTypeError(jsctx, "File writing is disabled");
    }
    QJSValueConst filename_s = argv[0];
    QJSValueConst text_s     = argv[1];
    if(!JS_IsString(filename_s) || !JS_IsString(text_s)){
        return JS_ThrowTypeError(jsctx, "Must be given two args: filename and data to write");
    }
    LongString filepath = jsstring_to_longstring(jsctx, filename_s, temp_allocator(ctx));
    StringView data = jsstring_make_stringview_js_allocated(jsctx, text_s);
    FileWriteResult err = write_file(filepath.text, data.text, data.length);
    Allocator_free(temp_allocator(ctx), filepath.text, filepath.length+1);
    JS_FreeCString(jsctx, data.text);
    if(err.errored){
        // Throwing a type error is the wrong way to report this error, but
        // I don't see a better way?
        #if !defined(_WIN32)
        return JS_ThrowTypeError(jsctx, "Error writing file '%s': %s", filepath.text, strerror(err.native_error));
        #else
        char errbuff[4192];
        DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM
                       // "when you are not in control of the message, you
                       // had better pass the FORMAT_MESSAGE_IGNORE_INSERTS
                       // flag"  - Raymond Chen
                       | FORMAT_MESSAGE_IGNORE_INSERTS
                       ;
        FormatMessageA(flags, NULL, err.native_error, 0, errbuff, sizeof errbuff, NULL);
        return JS_ThrowTypeError(jsctx, "Error writing file '%s': %s", filepath.text, errbuff);
        #endif
    }
    else
        return JS_UNDEFINED;
}


#if defined(_WIN32)
// Posix-likes provide an API that will do this for us.
// On Windows we are forced to roll our own.
static QJSValue js_list_dnd_files_inner(QJSContext* jsctx, DndcContext* ctx, QJSValue array, StringView directory, size_t base_length, int depth);
#endif

static
QJSValue
js_list_dnd_files(QJSContext *jsctx, QJSValueConst thisValue, int argc, QJSValueConst *argv){
    (void)thisValue;
    if(unlikely(argc > 1)){
        return JS_ThrowTypeError(jsctx, "Must be given 0 or no arguments");
    }
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    if(unlikely(ctx->flags & DNDC_DONT_READ)){
        return JS_ThrowTypeError(jsctx, "File system access is disabled.");
    }
    MStringBuilder sb = {.allocator = temp_allocator(ctx)};
    StringView base = ctx->base_directory;
    if(argc == 1){
        StringView dir = jsstring_make_stringview_js_allocated(jsctx, argv[0]);
        if(base.length && !path_is_abspath(dir)){
            msb_write_str_with_backslashes_as_forward_slashes(&sb, base.text, base.length);
            if(dir.length){
                msb_append_path(&sb, dir.text, dir.length);
            }
        }
        else {
            msb_write_str_with_backslashes_as_forward_slashes(&sb, dir.text, dir.length);
        }
        JS_FreeCString(jsctx, dir.text);
    }
    else {
        if(base.length){
            msb_write_str_with_backslashes_as_forward_slashes(&sb, base.text, base.length);
        }
        else {
            msb_write_str(&sb, ".", 1);
        }
    }
    if(unlikely(!sb.cursor)){
        return JS_ThrowTypeError(jsctx, "Invalid directory argument");
    }
    msb_nul_terminate(&sb);
#if defined(__APPLE__) || defined(__linux__)
    LongString dir = msb_borrow_ls(&sb);
    // On apple we could try using [NSFileManager
    //      enumeratorAtURL:includingPropertiesForKeys:options:errorHandler:]
    // instead, but whatever, this also works on linux. Main drawback is we
    // would have to compile as objective C.
    const char* dirs[] = {dir.text, NULL};
    PushDiagnostic();
    SuppressCastQual();
    FTS* handle = fts_open((char**)dirs, FTS_LOGICAL | FTS_NOCHDIR | FTS_NOSTAT, NULL);
    PopDiagnostic();
    if(unlikely(!handle)){
        msb_destroy(&sb);
        return JS_ThrowTypeError(jsctx, "Unable to open for recursion");
    }
    QJSValue result = JS_NewArray(jsctx);
    for(;;){
        FTSENT* ent = fts_read(handle);
        if(!ent) break;
        if(ent->fts_namelen > 1 && ent->fts_name[0] == '.'){
            fts_set(handle, ent, FTS_SKIP);
            continue;
        }
        if(ent->fts_info & (FTS_F | FTS_NSOK)){
            StringView name = {.text = ent->fts_name, .length=ent->fts_namelen};
            if(endswith(name, SV(".dnd"))){
                QJSValue item = JS_NewString(jsctx, ent->fts_path + dir.length+1);
                QJSValue v = JS_ArrayPush(jsctx, result, 1, &item);
                JS_FreeValue(jsctx, v);
                JS_FreeValue(jsctx, item);
            }
        }
    }
    fts_close(handle);
    msb_destroy(&sb);
    return result;
#elif defined(_WIN32)
    StringView dir = msb_borrow_sv(&sb);
    QJSValue result = JS_NewArray(jsctx);
    result = js_list_dnd_files_inner(jsctx, ctx, result, dir, dir.length, 0);
    msb_destroy(&sb);
    return result;
#else
    return JS_ThrowTypeError(jsctx, "Unsupported platform for recursive directory reading");
#endif

}

#if defined(_WIN32)
static
QJSValue
js_list_dnd_files_inner(QJSContext* jsctx, DndcContext* ctx, QJSValue array, StringView directory, size_t base_length, int depth){
    if(depth > 8){
        JS_FreeValue(jsctx, array);
        return JS_ThrowTypeError(jsctx, "Max Recursion depth exceeded: %d. Path was: '%s'", depth, directory.text);
    }
    // fprintf(stderr, "Entering with: '%s'\n", directory.text);
    MStringBuilder tempbuilder = {.allocator = temp_allocator(ctx)};
    MSB_FORMAT(&tempbuilder, directory, "/*.dnd");
    msb_nul_terminate(&tempbuilder);
    LongString dndwildcard = msb_borrow_ls(&tempbuilder);
    WIN32_FIND_DATAA findd;
    HANDLE handle = FindFirstFileExA(dndwildcard.text, FindExInfoBasic, &findd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    msb_erase(&tempbuilder, sizeof("/*.dnd")-1);
    if(handle == INVALID_HANDLE_VALUE){
    }
    else{
        do {
            size_t cursor = tempbuilder.cursor;
            MSB_FORMAT(&tempbuilder, SV("/"), findd.cFileName);
            StringView text = msb_borrow_sv(&tempbuilder);
            QJSValue s = JS_NewStringLen(jsctx, text.text+base_length+1, text.length-(base_length+1));
            QJSValue v = JS_ArrayPush(jsctx, array, 1, &s);
            JS_FreeValue(jsctx, s);
            JS_FreeValue(jsctx, v);
            tempbuilder.cursor = cursor;
        }while(FindNextFileA(handle, &findd));
        FindClose(handle);
    }
    msb_write_literal(&tempbuilder, "/*");
    msb_nul_terminate(&tempbuilder);
    LongString thisdir = msb_borrow_ls(&tempbuilder);
    handle = FindFirstFileExA(thisdir.text, FindExInfoBasic, &findd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if(handle == INVALID_HANDLE_VALUE){
        // fprintf(stderr, "Invalid handle: '%s'\n", thisdir.text);
        goto end;
    }
    msb_erase(&tempbuilder, sizeof("/*")-1);
    do {
        if(findd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN){
            continue;
        }
        if(!(findd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
            continue;
        }
        StringView fn = {.text = findd.cFileName, .length = strlen(findd.cFileName)};
        if(fn.text[0] == '.'){
            continue;
        }
        MSB_FORMAT(&tempbuilder, "/", fn);
        msb_nul_terminate(&tempbuilder);
        StringView nextdir = msb_borrow_sv(&tempbuilder);
        QJSValue e = js_list_dnd_files_inner(jsctx, ctx, array, nextdir, base_length, depth+1);
        msb_erase(&tempbuilder, 1+fn.length);
        if(JS_IsException(e)) {
            array = e;
            goto end;
        }
    }while(FindNextFileA(handle, &findd));
    end:
    msb_destroy(&tempbuilder);
    return array;
}
#endif


static
QJSValue
js_path_exists(QJSContext *jsctx, QJSValueConst thisValue, int argc, QJSValueConst *argv){
    (void)thisValue;
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "Need an argument!");

    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    if(ctx->flags & DNDC_DONT_READ){
        return JS_ThrowTypeError(jsctx, "File system access is disabled.");
    }
    MStringBuilder sb = {.allocator = temp_allocator(ctx)};
    StringView base = ctx->base_directory;
    StringView dir = jsstring_make_stringview_js_allocated(jsctx, argv[0]);
    if(base.length && !path_is_abspath(dir)){
        msb_write_str(&sb, base.text, base.length);
        if(dir.length){
            msb_append_path(&sb, dir.text, dir.length);
        }
    }
    else {
        msb_write_str(&sb, dir.text, dir.length);
    }
    JS_FreeCString(jsctx, dir.text);
    msb_nul_terminate(&sb);
    LongString path = msb_borrow_ls(&sb);
    struct stat s;
    bool exists = stat(path.text, &s) == 0;
    msb_destroy(&sb);
    if(exists)
        return JS_TRUE;
    else
        return JS_FALSE;
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
    LongString text = jsstring_to_longstring(jsctx, str, string_allocator(ctx));
    StringView old_filename = ctx->filename;
    int parse_e = dndc_parse(ctx, handle, SV("(generated string from script)"), text.text, text.length);
    if(parse_e){
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
    for(size_t i = 0; i < node_children_count(parent); i++){
        if(NodeHandle_eq(handle, node_children(parent)[i])){
            node_remove_child(parent, i, main_allocator(ctx));
            goto after;
        }
    }
    return JS_ThrowRangeError(jsctx, "Somehow a node was not a child of its parent");

    after:;
    return JS_UNDEFINED;
}

JSMETHOD(js_dndc_node_make_child){
    QJSValue child_js = js_dndc_context_make_node(jsctx, thisValue, argc, argv);
    if(JS_IsException(child_js)) return child_js;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle child;
    if(!js_dndc_get_node_handle(jsctx, child_js, &child)){
        // should never happen
        return JS_EXCEPTION;
    }
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    assert(!NodeHandle_eq(child, INVALID_NODE_HANDLE));
    append_child(ctx, handle, child);
    return child_js;
}

JSMETHOD(js_dndc_node_add_child){
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "need 1 argument to add_child");
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    QJSValueConst arg = argv[0];
    NodeHandle child;
    if(JS_IsString(arg)){
        StringView sv = jsstring_to_stringview(jsctx, arg, string_allocator(ctx));
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
    size_t count = node_children_count(parent_node);
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

JSMETHOD(js_dndc_node_insert_child){
    if(argc != 2)
        return JS_ThrowTypeError(jsctx, "need 2 arguments to insert_child");
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    int32_t index;
    if(JS_ToInt32(jsctx, &index, argv[0]))
        return JS_ThrowTypeError(jsctx, "Expected an integer index.");
    QJSValueConst newchild_arg = argv[1];
    NodeHandle new_child;
    if(!js_dndc_get_node_handle(jsctx, newchild_arg, &new_child))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(new_child, INVALID_NODE_HANDLE));
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* newchild_node = get_node(ctx, new_child);

    if(!NodeHandle_eq(newchild_node->parent, INVALID_NODE_HANDLE)){
        return JS_ThrowTypeError(jsctx, "Node needs to be an orphan to be added as a child of another node");
    }
    if(NodeHandle_eq(handle, new_child))
        return JS_ThrowTypeError(jsctx, "Node can't be a child of itself");
    node_insert_child(ctx, handle, index, new_child);
    return JS_UNDEFINED;
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
    if(type < 0 || type >= NODE_INVALID)
        return JS_ThrowTypeError(jsctx, "Integer out of range for valid node types.");
    switch(type){
        case NODE_TOC:
            ctx->tocnode = handle;
            break;
        case NODE_TITLE:
            ctx->titlenode = handle;
            break;
        case NODE_STYLESHEETS:
            Marray_push(NodeHandle)(&ctx->stylesheets_nodes, main_allocator(ctx), handle);
            break;
        case NODE_LINKS:
            Marray_push(NodeHandle)(&ctx->link_nodes, main_allocator(ctx), handle);
            break;
        case NODE_SCRIPTS:
            Marray_push(NodeHandle)(&ctx->script_nodes, main_allocator(ctx), handle);
            break;
        case NODE_META:
            Marray_push(NodeHandle)(&ctx->meta_nodes, main_allocator(ctx), handle);
            break;
        case NODE_JS:
            return JS_ThrowTypeError(jsctx, "Setting a node to JS is not supported");
        case NODE_IMPORT:
            return JS_ThrowTypeError(jsctx, "Setting a node to IMPORT not supported.");
        case NODE_DIV:
        case NODE_STRING:
        case NODE_PARA:
        case NODE_HEADING:
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

// TODO: look into magic variants
static
QJSValue
js_dndc_node_flag_setter(QJSContext* jsctx, QJSValueConst thisValue, QJSValueConst arg, int flag){
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);
    int b = JS_ToBool(jsctx, arg);
    if(b < 0){
        return JS_EXCEPTION;
    }
    if(b){
        node->flags |= flag;
    }
    else {
        node->flags &= ~flag;
    }
    return JS_UNDEFINED;
}

static
QJSValue
js_dndc_node_flag_getter(QJSContext* jsctx, QJSValueConst thisValue, int flag){
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    Node* node = get_node(ctx, handle);
    return JS_NewBool(jsctx, node->flags & flag);
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
    return array;
}

static
QJSValue
js_dndc_node_get_internal_id(QJSContext* jsctx, QJSValueConst thisValue){
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    assert(!NodeHandle_eq(handle, INVALID_NODE_HANDLE));
    return JS_NewUint32(jsctx, handle._value);
}

static
QJSValue
js_dndc_node_get_location(QJSContext* jsctx, QJSValueConst thisValue){
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    QJSValue obj = JS_NewObjectClass(jsctx, JS_DNDC_LOCATION_CLASS_ID);
    if(JS_IsException(obj))
        return obj;
    JS_SetOpaque(obj, NodeHandle_to_opaque(handle));
    return obj;
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
    StringView new_header = jsstring_to_stringview(jsctx, arg, string_allocator(ctx));
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
    StringView id = node_get_id(ctx, handle);
    if(!id.length){
        return JS_NewString(jsctx, "");
    }
    MStringBuilder msb = {.allocator = temp_allocator(ctx)};
    msb_write_kebab(&msb, id.text, id.length);
    StringView keb = msb_borrow_sv(&msb);
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
    if(!JS_IsString(arg)){
        return JS_ThrowTypeError(jsctx, "id must be a string");
    }
    StringView new_id = jsstring_to_stringview(jsctx, arg, string_allocator(ctx));
    node_set_id(ctx, handle, new_id);
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
    MStringBuilder msb = {.allocator=temp_allocator(ctx)};
    MSB_FORMAT(&msb, "Node(", NODENAMES[node->type].text);
    RARRAY_FOR_EACH(StringView, class, node->classes){
        MSB_FORMAT(&msb, ".", *class);
    }
    if(node->flags & NODEFLAG_HIDE)
        msb_write_literal(&msb, " #hide");
    if(node->flags & NODEFLAG_NOID)
        msb_write_literal(&msb, " #noid");
    if(node->flags & NODEFLAG_NOINLINE)
        msb_write_literal(&msb, " #noinline");
    MSB_FORMAT(&msb, ", '", node->header, "', [", (int)node_children_count(node), " children])");
    StringView text = msb_borrow_sv(&msb);
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
    RARRAY_FOR_EACH(StringView, c, node->classes){
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
    LongString msg = jsstring_to_longstring(jsctx, argv[0], temp_allocator(ctx));
    if(!msg.text)
        return JS_EXCEPTION;
    QJSValue jsmsg = JS_NewStringLen(jsctx, msg.text, msg.length);
    // HANDLE_LOG_ERROR(ctx, handle, msg);
    QJSValue result = JS_Throw(jsctx, jsmsg);
    Allocator_free(temp_allocator(ctx), msg.text, msg.length+1);
    return result;
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
    StringView msg = jsstring_to_stringview(jsctx, argv[0], temp_allocator(ctx));
    if(!msg.text)
        return JS_EXCEPTION;
    bool has_it = node_has_class(node, msg);
    Allocator_free(temp_allocator(ctx), msg.text, msg.length+1);
    return has_it? JS_TRUE : JS_FALSE;
}
JSMETHOD(js_dndc_node_clone){
    (void)argv;
    if(argc != 0)
        return JS_ThrowTypeError(jsctx, "clone must have no arguments");
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    NodeHandle newnode = node_clone(ctx, handle);
    return js_make_dndc_node(jsctx, newnode);
}
JSMETHOD(js_dndc_node_get){
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "get must be called with 1 string argument");
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    Node* node = get_node(ctx, handle);
    StringView key_arg = jsstring_to_stringview(jsctx, argv[0], temp_allocator(ctx));
    if(!key_arg.text)
        return JS_EXCEPTION;
    if(node->type != NODE_KEYVALUE)
        return JS_ThrowTypeError(jsctx, "Node is not a KEYVALUE node");
    NODE_CHILDREN_FOR_EACH(h, node){
        Node* ch = get_node(ctx, *h);
        if(ch->type != NODE_KEYVALUEPAIR) continue;
        if(node_children_count(ch) != 2) continue;
        NodeHandle* kv = node_children(ch);
        Node* k = get_node(ctx, kv[0]);
        if(k->type != NODE_STRING) continue;
        if(!SV_equals(k->header, key_arg)) continue;
        Node* v = get_node(ctx, kv[1]);
        if(v->type != NODE_STRING) continue;
        QJSValue result = JS_NewStringLen(jsctx, v->header.text, v->header.length);
        return result;
    }
    return JS_UNDEFINED;
}
JSMETHOD(js_dndc_node_set){
    if(argc != 2)
        return JS_ThrowTypeError(jsctx, "set must be called with 1 string argument");
    NodeHandle handle;
    if(!js_dndc_get_node_handle(jsctx, thisValue, &handle))
        return JS_EXCEPTION;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    Node* node = get_node(ctx, handle);
    if(node->type != NODE_KEYVALUE)
        return JS_ThrowTypeError(jsctx, "Node is not a KEYVALUE node");
    StringView key_arg = jsstring_to_stringview(jsctx, argv[0], temp_allocator(ctx));
    if(!key_arg.text)
        return JS_EXCEPTION;
    StringView value_arg = jsstring_to_stringview(jsctx, argv[1], string_allocator(ctx));
    if(!value_arg.text)
        return JS_EXCEPTION;
    NODE_CHILDREN_FOR_EACH(h, node){
        Node* ch = get_node(ctx, *h);
        if(ch->type != NODE_KEYVALUEPAIR) continue;
        if(node_children_count(ch) != 2) continue;
        NodeHandle* kv = node_children(ch);
        Node* k = get_node(ctx, kv[0]);
        if(k->type != NODE_STRING) continue;
        if(!SV_equals(k->header, key_arg)) continue;
        Node* v = get_node(ctx, kv[1]);
        if(v->type != NODE_STRING) continue;
        v->header = value_arg;
        return JS_UNDEFINED;
    }
    NodeHandle kvh = alloc_handle(ctx);
    {
        append_child(ctx, handle, kvh);
        Node* n = get_node(ctx, kvh);
        n->type = NODE_KEYVALUEPAIR;
    }
    {
        NodeHandle kh = alloc_handle(ctx);
        append_child(ctx, kvh, kh);
        Node* n = get_node(ctx, kh);
        n->type = NODE_STRING;
        StringView key_s = {.text=Allocator_strndup(string_allocator(ctx), key_arg.text, key_arg.length), .length=key_arg.length};
        n->header = key_s;
    }
    {
        NodeHandle vh = alloc_handle(ctx);
        append_child(ctx, kvh, vh);
        Node* n = get_node(ctx, vh);
        n->type = NODE_STRING;
        n->header = value_arg;
    }
    return JS_UNDEFINED;
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
    QJSValueConst arg = argv[0];
    if(!JS_IsString(arg))
        return JS_ThrowTypeError(jsctx, "Need 1 string arg to make_string");
    StringView sv = jsstring_to_stringview(jsctx, arg, string_allocator(ctx));
    NodeHandle new_handle = alloc_handle(ctx);
    {
        Node* node = get_node(ctx, new_handle);
        node->header = sv;
        node->type = NODE_STRING;
    }
    return js_make_dndc_node(jsctx, new_handle);
}

JSMETHOD(js_dndc_context_make_node){
    (void)thisValue;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    // DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    // if(!ctx)
        // return JS_EXCEPTION;
    if(argc == 0 || argc > 2)
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
        StringView sv = jsstring_to_stringview(jsctx, header, string_allocator(ctx));
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
            StringView sv = jsstring_to_stringview(jsctx, s, string_allocator(ctx));
            JS_FreeValue(jsctx, s);
            if(!sv.text){
                failure = JS_EXCEPTION;
                goto fail;
            }
            node->classes = Rarray_push(StringView)(node->classes, main_allocator(ctx), sv);
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
            StringView sv = jsstring_to_stringview(jsctx, s, string_allocator(ctx));
            JS_FreeValue(jsctx, s);
            if(!sv.text){
                failure = JS_EXCEPTION;
                goto fail;
            }
            node_set_attribute(node, main_allocator(ctx), sv, SV(""));
        }
    }
    Marray(NodeHandle)* node_store = NULL;
    switch(type){
        case NODE_IMPORT:
            failure = JS_ThrowTypeError(jsctx, "Creating import nodes from qjs is not supported");
            goto fail;
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
            node_store = &ctx->user_script_nodes;
            break;
        case NODE_META:
            node_store = &ctx->meta_nodes;
            break;
        case NODE_TITLE:
            ctx->titlenode = handle;
            break;
        case NODE_TOC:
            ctx->tocnode = handle;
            break;
        default:
            break;
    }
    if(node_store)
        Marray_push(NodeHandle)(node_store, main_allocator(ctx), handle);
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
    StringView sv = jsstring_to_stringview(jsctx, argv[0], string_allocator(ctx));
    if(!sv.text)
        return JS_EXCEPTION;
    Marray_push(StringView)(&ctx->dependencies, main_allocator(ctx), sv);
    return JS_UNDEFINED;
}

JSMETHOD(js_dndc_context_kebab){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "Need 1 string argument to kebab");
    StringView sv = jsstring_to_stringview(jsctx, argv[0], temp_allocator(ctx));
    if(!sv.text)
        return JS_EXCEPTION;
    MStringBuilder msb = {.allocator = temp_allocator(ctx)};
    msb_write_kebab(&msb, sv.text, sv.length);
    StringView keb = msb_borrow_sv(&msb);
    QJSValue result = JS_NewStringLen(jsctx, keb.text, keb.length);
    msb_destroy(&msb);
    Allocator_free(temp_allocator(ctx), sv.text, sv.length+1);
    return result;
}

JSMETHOD(js_dndc_context_html_escape){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "Need 1 string argument to html_escape");
    StringView sv = jsstring_to_stringview(jsctx, argv[0], temp_allocator(ctx));
    if(!sv.text)
        return JS_EXCEPTION;
    MStringBuilder msb = {.allocator = temp_allocator(ctx)};
    msb_write_tag_escaped_str(&msb, sv.text, sv.length);
    StringView esc = msb_borrow_sv(&msb);
    QJSValue result = JS_NewStringLen(jsctx, esc.text, esc.length);
    msb_destroy(&msb);
    Allocator_free(temp_allocator(ctx), sv.text, sv.length+1);
    return result;
}

JSMETHOD(js_dndc_context_select_nodes){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(argc != 1)
        return JS_ThrowTypeError(jsctx, "Need 1 obj argument to select_nodes");
    QJSValueConst arg = argv[0];
    if(!JS_IsObject(arg))
        return JS_ThrowTypeError(jsctx, "Need 1 obj argument to select_nodes");
    LinearAllocator la = new_linear_storage(1024*1024, "select_nodes allocator");
    Allocator tmp = allocator_from_la(&la);
    int32_t type = -1;
    Marray(StringView) attributes_array = {0};
    Marray(StringView) classes_array = {0};
    StringView node_id = {0};
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
    QJSValue id = JS_UNDEFINED;
    classes = JS_GetPropertyStr(jsctx, arg, "classes");
    attributes = JS_GetPropertyStr(jsctx, arg, "attributes");
    id = JS_GetPropertyStr(jsctx, arg, "id");
    if(!JS_IsUndefined(id)){
        if(!JS_IsString(id)){
            failure = JS_ThrowTypeError(jsctx, "id should be a string");
            goto fail;
        }
        node_id = jsstring_to_stringview(jsctx, id, tmp);
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
    if(node_id.text){
        for(size_t i = 0; i < ctx->nodes.count; i++){
            Node* node = &ctx->nodes.data[i];
            if(node->type == NODE_INVALID) continue;
            NodeHandle handle = {.index = i};
            StringView this_id = node_get_id(ctx, handle);
            if(SV_equals(this_id, node_id)){
                QJSValue nh = js_make_dndc_node(jsctx, handle);
                QJSValue v = JS_ArrayPush(jsctx, result, 1, &nh);
                JS_FreeValue(jsctx, v);
                JS_FreeValue(jsctx, nh);
                goto done;
            }

        }
    }
    // put them all in
    else if(!classes_array.count && !attributes_array.count && type < 0){
        for(size_t i = 0; i < ctx->nodes.count; i++){
            if(ctx->nodes.data[i].type == NODE_INVALID)
                continue;
            QJSValue nh = js_make_dndc_node(jsctx, (NodeHandle){.index = i});
            QJSValue v = JS_ArrayPush(jsctx, result, 1, &nh);
            JS_FreeValue(jsctx, v);
            JS_FreeValue(jsctx, nh);
        }
    }
    else {
        size_t idx = 0;
        for(size_t i = 0; i < ctx->nodes.count; i++){
            Node* node = &ctx->nodes.data[i];
            if(node->type == NODE_INVALID) continue;
            if(type >= 0){
                if(node->type != (NodeType)type)
                    goto Continue;
            }
            MARRAY_FOR_EACH(StringView, attr, attributes_array){
                if(!node_has_attribute(node, *attr))
                    goto Continue;
            }
            MARRAY_FOR_EACH(StringView, class_, classes_array){
                if(!node_has_class(node, *class_))
                    goto Continue;
            }
            JS_SetPropertyUint32(jsctx, result, idx++, js_make_dndc_node(jsctx, (NodeHandle){.index=i}));
            Continue:;
        }
    }
    done:
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
    MStringBuilder msb = {.allocator = temp_allocator(ctx)};
    msb_write_literal(&msb, "{\n");
    MSB_FORMAT(&msb, "  nodes: [", ctx->nodes.count, " nodes],\n");
    MSB_FORMAT(&msb, "  filename: \"", ctx->filename, "\",\n");
    MSB_FORMAT(&msb, "  base: \"", ctx->base_directory, "\",\n");
    MSB_FORMAT(&msb, "  dependencies: [", ctx->dependencies.count, " dependencies],\n");
    // TODO: hex format
    MSB_FORMAT(&msb, "  flags: ", ctx->flags, ",\n");
    msb_write_literal(&msb, "}");
    StringView text = msb_borrow_sv(&msb);
    QJSValue result = JS_NewStringLen(jsctx, text.text, text.length);
    msb_destroy(&msb);
    return result;
}

JSMETHOD(js_dndc_context_add_link){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    if(argc != 2)
        return JS_ThrowTypeError(jsctx, "Need 2 string argument to add_link");
    LongString kebabed_ = jsstring_to_kebabed(jsctx, argv[0], string_allocator(ctx));
    if(!kebabed_.text)
        return JS_EXCEPTION;
    StringView kebabed = LS_to_SV(kebabed_);
    StringView value = jsstring_to_stringview(jsctx, argv[1], string_allocator(ctx));
    if(!value.text)
        return JS_EXCEPTION;
    add_link_from_pair(ctx, kebabed, value);
    return JS_UNDEFINED;
}

static
DndcContext*_Nullable
js_get_dndc_context(QJSContext* ctx, QJSValueConst thisValue){
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
    StringView base = ctx->base_directory;
    if(!base.length)
        base = SV(".");
    return JS_NewStringLen(jsctx, base.text, base.length);
}

JSGETTER(js_dndc_context_get_all_nodes){
    DndcContext* ctx = js_get_dndc_context(jsctx, thisValue);
    if(!ctx)
        return JS_EXCEPTION;
    QJSValue result = JS_NewArray(jsctx);
    for(size_t i = 0; i < ctx->nodes.count; i++){
        if(ctx->nodes.data[i].type == NODE_INVALID) continue;
        QJSValue n = js_make_dndc_node(jsctx, (NodeHandle){._value=i});
        QJSValue v = JS_ArrayPush(jsctx, result, 1, &n);
        JS_FreeValue(jsctx, v);
        JS_FreeValue(jsctx, n);
    }
    return result;
}
//
// DndcNodeLocation
//
JSMAGICGETTER(js_dndc_node_location_getter){
    void* pointer = JS_GetOpaque2(jsctx, thisValue, JS_DNDC_LOCATION_CLASS_ID);
    uintptr_t p = (uintptr_t)pointer;
    if(!p){
        return JS_ThrowTypeError(jsctx, "Invalid NodeLocation");
    }
    NodeHandle handle;
    if(p == (uintptr_t)ZERO_NODE_VALUE)
        handle._value = 0;
    else
        handle._value = p;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    Node* node = get_node(ctx, handle);
    switch(magic){
        case 0:{
            StringView filename = ctx->filenames.data[node->filename_idx];
            return JS_NewStringLen(jsctx, filename.text, filename.length);
        }
        case 1:
            return JS_NewInt32(jsctx, node->row);
        case 2:
            return JS_NewInt32(jsctx, node->col);
        default:
            return JS_ThrowTypeError(jsctx, "wtf");
    }
}
JSMETHOD(js_dndc_node_location_to_string){
    (void)argv; (void)argc;
    void* pointer = JS_GetOpaque2(jsctx, thisValue, JS_DNDC_LOCATION_CLASS_ID);
    uintptr_t p = (uintptr_t)pointer;
    if(!p){
        return JS_ThrowTypeError(jsctx, "Invalid NodeLocation");
    }
    NodeHandle handle;
    if(p == (uintptr_t)ZERO_NODE_VALUE)
        handle._value = 0;
    else
        handle._value = p;
    DndcContext* ctx = JS_GetContextOpaque(jsctx);
    assert(ctx);
    Node* node = get_node(ctx, handle);
    MStringBuilder msb = {.allocator=temp_allocator(ctx)};
    StringView filename = ctx->filenames.data[node->filename_idx];
    MSB_FORMAT(&msb, "{filename:'", filename, "', row:", node->row, ", column:", node->col, "}");
    StringView text = msb_borrow_sv(&msb);
    QJSValue result = JS_NewStringLen(jsctx, text.text, text.length);
    msb_destroy(&msb);
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

JSMETHOD(js_dndc_attributes_has){
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
        return JS_FALSE;
    else
        return JS_TRUE;
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
    StringView key = jsstring_to_stringview(jsctx, key_arg, string_allocator(ctx)); // extra alloc if key already exists. Could fix this.
    if(!key.text)
        return JS_EXCEPTION;
    Node* node = get_node(ctx, handle);
    if(argc == 2){
        StringView value = jsstring_to_stringview(jsctx, argv[1], string_allocator(ctx));
        if(!value.text)
            return JS_EXCEPTION;
        node_set_attribute(node, main_allocator(ctx), key, value);
    }
    else {
        node_set_attribute(node, main_allocator(ctx), key, SV(""));
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
    MStringBuilder msb = {.allocator = temp_allocator(ctx)};
    msb_write_literal(&msb, "{ ");
    RARRAY_FOR_EACH(Attribute, kv, node->attributes){
        MSB_FORMAT(&msb, "\n  ", kv->key, ": \"", kv->value, "\",");
    }
    msb_erase(&msb, 1);
    msb_write_literal(&msb, "\n}");
    StringView text = msb_borrow_sv(&msb);
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
    RARRAY_FOR_EACH(Attribute, kv, node->attributes){
        QJSValue pair = JS_NewArray(jsctx);
        QJSValue js_kv[2] = {
            JS_NewStringLen(jsctx, kv->key.text, kv->key.length),
            JS_NewStringLen(jsctx, kv->value.text, kv->value.length),
        };
        QJSValue call = JS_ArrayPush(jsctx, pair, 2, js_kv);
        assert(!JS_IsException(call));
        JS_FreeValue(jsctx, js_kv[0]);
        JS_FreeValue(jsctx, js_kv[1]);
        QJSValue v = JS_ArrayPush(jsctx, result, 1, &pair);
        JS_FreeValue(jsctx, v);
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
    StringView c = jsstring_to_stringview(jsctx, arg, string_allocator(ctx));
    if(!c.text)
        return JS_EXCEPTION;
    node->classes = Rarray_push(StringView)(node->classes, main_allocator(ctx), c);
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
    MStringBuilder msb = {.allocator = temp_allocator(ctx)};
    msb_write_char(&msb, '[');
    RARRAY_FOR_EACH(StringView, c, node->classes){
        MSB_FORMAT(&msb, "\"", *c, "\", ");
    }
    if(msb.cursor != 1)
        msb_erase(&msb, 2);
    msb_write_char(&msb, ']');
    StringView text = msb_borrow_sv(&msb);
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
    RARRAY_FOR_EACH(StringView, c, node->classes){
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
