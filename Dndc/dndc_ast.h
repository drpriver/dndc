#ifndef DNDC_AST_H
#define DNDC_AST_H
#ifndef NO_DNDC_AST_API
#include "dndc.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifdef __cplusplus
extern "C" {
#endif

//
// DndcAst API
// ---------------
// This is the api for creating and intereacting with an ast directly.
// The functions provided in dndc.h are easier to use and have
// a stable API. This API is unstable by necessity.

typedef unsigned DndcNodeHandle;
#ifdef __clang__
enum: DndcNodeHandle {DNDC_NODE_HANDLE_INVALID = (DndcNodeHandle)-1};
#else
enum {DNDC_NODE_HANDLE_INVALID = -1};
#endif
typedef struct DndcContext DndcContext;

typedef struct DndcNodeLocation {
    DndcStringView filename;
    int row, column;
} DndcNodeLocation;

// Functions taking string views generally hold onto them for
// the lifetime of ctx. If you can't guarantee they last that long,
// then call this function to get a copy that will last as long
// as the ctx.
DNDC_API
DndcStringView
dndc_ctx_dup_sv(DndcContext* ctx, DndcStringView text);

DNDC_API
DndcContext*
dndc_create_ctx(unsigned long long flags, DNDC_NULLABLE(DndcErrorFunc*) error_func, DNDC_NULLABLE(void*) error_func_data, DNDC_NULLABLE(DndcFileCache*) base64cache, DNDC_NULLABLE(DndcFileCache*) textcache, DndcStringView base_directory, DndcStringView outpath, int copy_paths);

DNDC_API
void
dndc_ctx_destroy(DndcContext*);

DNDC_API
int
dndc_ctx_store_builtin_file(DndcContext*, DndcStringView, DndcStringView);

DNDC_API
int
dndc_ctx_parse_file(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView sourcepath);

DNDC_API
int
dndc_ctx_parse_string(DndcContext*, DndcNodeHandle, DndcStringView, DndcStringView);


DNDC_API
DndcNodeHandle
dndc_ctx_get_root(DndcContext*);

DNDC_API
DndcNodeHandle
dndc_ctx_make_root(DndcContext* ctx, DndcStringView);

DNDC_API
int
dndc_ctx_set_root(DndcContext*, DndcNodeHandle);


#if 0
DNDC_API
int
dndc_node_get_attribute(DndcContext*, DndcNodeHandle, DndcStringView, DndcStringView*_Nonnull*_Nonnull);

DNDC_API
int
dndc_node_has_attribute(DndcContext*, DndcNodeHandle, DndcStringView);

#endif

DNDC_API
int
dndc_node_set_attribute(DndcContext*, DndcNodeHandle, DndcStringView, DndcStringView);

DNDC_API
int
dndc_node_get_id(DndcContext*, DndcNodeHandle, DndcStringView*);

DNDC_API
int
dndc_node_set_id(DndcContext*, DndcNodeHandle, DndcStringView);

DNDC_API
int
dndc_node_append_child(DndcContext*, DndcNodeHandle, DndcNodeHandle);

DNDC_API
int
dndc_node_insert_child(DndcContext*, DndcNodeHandle, size_t, DndcNodeHandle);

DNDC_API
void
dndc_node_detach(DndcContext*, DndcNodeHandle);

DNDC_API
int
dndc_node_remove_child(DndcContext*, DndcNodeHandle, size_t);

DNDC_API
int
dndc_node_has_class(DndcContext*, DndcNodeHandle, DndcStringView);

DNDC_API
int
dndc_node_has_attribute(DndcContext*, DndcNodeHandle, DndcStringView);



DNDC_API
DndcNodeHandle
dndc_node_get_parent(DndcContext*, DndcNodeHandle);

DNDC_API
int
dndc_node_get_type(DndcContext*, DndcNodeHandle);

DNDC_API
int
dndc_node_set_type(DndcContext*, DndcNodeHandle, int);

DNDC_API
int
dndc_node_get_flags(DndcContext*, DndcNodeHandle);


#if 0
DNDC_API
int
dndc_node_set_flags(DndcContext*, DndcNodeHandle, int);
#endif

DNDC_API
int
dndc_node_get_header(DndcContext*, DndcNodeHandle, DndcStringView*);

DNDC_API
int
dndc_node_set_header(DndcContext*, DndcNodeHandle, DndcStringView);

DNDC_API
size_t
dndc_node_get_children(DndcContext*, DndcNodeHandle, DndcNodeHandle* buff, size_t buff_len, size_t* cookie);

DNDC_API
size_t
dndc_node_children_count(DndcContext*, DndcNodeHandle);

//
// Convenience function. Iterates over the children of a node and concatenates
// their strings together.
// This could be useful for executing custom scripting languages.
//
DNDC_API
int
dndc_node_cat_string_children(DndcContext*, DndcNodeHandle, DndcLongString* out);

#if 0
DNDC_API
int
dndc_node_get_classes(DndcContext*, DndcNodeHandle, DndcStringView* buff, size_t buff_len, size_t* cookie);

DNDC_API
size_t
dndc_node_classes_count(DndcContext*, DndcNodeHandle);

DNDC_API
int
dndc_node_add_class(DndcContext*, DndcNodeHandle, DndcStringView);

DNDC_API
int
dndc_node_remove_class(DndcContext*, DndcNodeHandle, DndcStringView);

#endif

DNDC_API
int
dndc_ctx_expand_to_dnd(DndcContext*, DndcLongString*);

DNDC_API
int
dndc_ctx_render_to_html(DndcContext*, DndcLongString*);

#if 0
DNDC_API
int
dndc_node_render_to_html(DndcContext*, DndcNodeHandle, DndcLongString*);

#endif
DNDC_API
int
dndc_ctx_format_tree(DndcContext*, DndcLongString*);

DNDC_API
int
dndc_node_format(DndcContext*, DndcNodeHandle, int indent, DndcLongString*);
#if 0

DNDC_API
int
dndc_node_execute_js(DndcContext*, DndcNodeHandle, DndcLongString);

#endif

DNDC_API
int
dndc_ctx_execute_js(DndcContext*, DndcLongString jsargs);

#if 0
DNDC_API
DndcNodeLocation
dndc_node_location(DndcContext*, DndcNodeHandle);

#endif

DNDC_API
DndcNodeHandle
dndc_ctx_node_by_id(DndcContext*, DndcStringView);

DNDC_API
int
dndc_ctx_node_invalid(DndcContext* ctx, DndcNodeHandle);

// Usable in an X macro for convenient wrapping.
#define DNDCNODETYPES(apply) \
    apply(MD,              0)\
    apply(DIV,             1)\
    apply(STRING,          2)\
    apply(PARA,            3)\
    apply(TITLE,           4)\
    apply(HEADING,         5)\
    apply(TABLE,           6)\
    apply(TABLE_ROW,       7)\
    apply(STYLESHEETS,     8)\
    apply(LINKS,           9)\
    apply(SCRIPTS,        10)\
    apply(IMPORT,         11)\
    apply(IMAGE,          12)\
    apply(BULLETS,        13)\
    apply(RAW,            14)\
    apply(PRE,            15)\
    apply(LIST,           16)\
    apply(LIST_ITEM,      17)\
    apply(KEYVALUE,       18)\
    apply(KEYVALUEPAIR,   19)\
    apply(IMGLINKS,       20)\
    apply(NAV,            21)\
    apply(DATA,           22)\
    apply(COMMENT,        23)\
    apply(CONTAINER,      24)\
    apply(QUOTE,          25)\
    apply(HR,             26)\
    apply(JS,             27)\
    apply(DETAILS,        28)\
    apply(META,           29)\
    apply(INVALID,        30)\

enum DndcNodeType {
#define X(a, b) DNDC_NODE_TYPE_##a = b,
    DNDCNODETYPES(X)
#undef X
};

// header may be empty and parent may be DNDC_NODE_HANDLE_INVALID
DNDC_API
DndcNodeHandle
dndc_ctx_make_node(DndcContext*, int type, DndcStringView header, DndcNodeHandle parent);

DNDC_API
int
dndc_ctx_resolve_imports(DndcContext*);

DNDC_API
int
dndc_ctx_gather_links(DndcContext*);

DNDC_API
int
dndc_ctx_build_nav(DndcContext*);

DNDC_API
int
dndc_ctx_resolve_links(DndcContext*);

DNDC_API
int
dndc_ctx_resolve_data_blocks(DndcContext*);

DNDC_API
size_t
dndc_ctx_select_nodes(DndcContext* ctx, size_t* cookie, int type, DNDC_NULLABLE(DndcStringView*) attributes, size_t attribute_count, DNDC_NULLABLE(DndcStringView*) classes, size_t class_count,  DndcNodeHandle* outbuf, size_t buflen);

#ifdef __cplusplus
}
#endif


#ifdef DNDC_AST_EXAMPLE
//
// This is an example of how to recreate compiling a dnd string. You can insert
// custom manipulations between these steps (which is the entire point of the
// ast api over just calling `dndc_compile_dnd_file`).
//
static inline
int
compile_dnd_to_html(DndcStringView basedir, DndcStringView filename, DndcStringView text, DndcLongString* outhtml){
    unsigned long long flags = 0;
    DndcFileCache* b64cache = NULL;
    DndcFileCache* textcache = NULL;
    DndcErrorFunc* errfunc = dndc_stderr_error_func;
    void* errarg = NULL;
    int copy_paths = 0;
    DndcStringView outpath = {.length=sizeof("example.html")-1, "example.html"};
    DndcContext* ctx = dndc_create_ctx(flags, errfunc, errarg, b64cache, textcache, basedir, outpath, copy_paths);
    DndcNodeHandle root = dndc_ctx_make_root(ctx, filename);
    int err = 0;
    err = dndc_ctx_parse_string(ctx, root, filename, text);
    if(err) goto fail;

    err = dndc_ctx_resolve_imports(ctx);
    if(err) goto fail;

    {
        DndcLongString jsargs = {4, "null"};
        err = dndc_ctx_execute_js(ctx, jsargs);
    }
    if(err) goto fail;

    err = dndc_ctx_gather_links(ctx);
    if(err) goto fail;

    err = dndc_ctx_resolve_links(ctx);
    if(err) goto fail;

    err = dndc_ctx_build_nav(ctx);
    if(err) goto fail;

    err = dndc_ctx_resolve_data_blocks(ctx);
    if(err) goto fail;

    err = dndc_ctx_render_to_html(ctx, outhtml);
    if(err) goto fail;

    fail:
    dndc_ctx_destroy(ctx);
    return err;
}


// This is the same as above, but it additionally injects some javascript
static inline
int
compile_dnd_to_html_with_extra_script(DndcStringView basedir,DndcStringView filename, DndcStringView text, DndcLongString* outhtml){
    unsigned long long flags = 0;
    DndcFileCache* b64cache = NULL;
    DndcFileCache* textcache = NULL;
    DndcErrorFunc* errfunc = dndc_stderr_error_func;
    void* errarg = NULL;
    int copy_paths = 0;
    DndcStringView outpath = {.length=sizeof("example.html")-1, "example.html"};
    DndcContext* ctx = dndc_create_ctx(flags, errfunc, errarg, b64cache, textcache, basedir, outpath, copy_paths);
    DndcNodeHandle root = dndc_ctx_make_root(ctx, filename);
    int err = 0;
    err = dndc_ctx_parse_string(ctx, root, filename, text);
    if(err) goto fail;

    err = dndc_ctx_resolve_imports(ctx);
    if(err) goto fail;

    // inject a script
    DndcNodeHandle jsnode = dndc_ctx_make_node(ctx, DNDC_NODE_TYPE_SCRIPTS, (DndcStringView){0}, root);
#define MYSCRIPT "window.alert('pwned');"
    dndc_ctx_make_node(ctx, DNDC_NODE_TYPE_STRING, (DndcStringView){sizeof(MYSCRIPT)-1, MYSCRIPT}, jsnode);
#undef MYSCRIPT

    {
        DndcLongString jsargs = {4, "null"};
        err = dndc_ctx_execute_js(ctx, jsargs);
    }
    if(err) goto fail;

    err = dndc_ctx_gather_links(ctx);
    if(err) goto fail;

    err = dndc_ctx_resolve_links(ctx);
    if(err) goto fail;

    err = dndc_ctx_build_nav(ctx);
    if(err) goto fail;

    err = dndc_ctx_resolve_data_blocks(ctx);
    if(err) goto fail;

    err = dndc_ctx_render_to_html(ctx, outhtml);
    if(err) goto fail;

    fail:
    dndc_ctx_destroy(ctx);
    return err;
}
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
#endif
