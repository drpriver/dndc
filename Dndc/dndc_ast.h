#ifndef DNDC_AST_H
#define DNDC_AST_H
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
dndc_dup_sv(DndcContext* ctx, DndcStringView text);

DNDC_API
DndcContext*
dndc_create_ctx(unsigned long long flags, DndcErrorFunc*_Nullable error_func, void*_Nullable error_func_data, DndcFileCache*_Nullable base64cache, DndcFileCache*_Nullable textcache, DndcStringView base_directory, DndcStringView outpath, int copy_paths);

DNDC_API
void
dndc_ctx_destroy(DndcContext*);

DNDC_API
int
dndc_ctx_store_builtin_file(DndcContext*, DndcStringView, DndcStringView);

#if 0
DNDC_API
int
dndc_ctx_parse_file(DndcContext*, DndcNodeHandle, DndcLongString);
#endif

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
dndc_expand_to_dnd(DndcContext*, DndcLongString*);

DNDC_API
int
dndc_render_to_html(DndcContext*, DndcLongString*);

#if 0
DNDC_API
int
dndc_render_node(DndcContext*, DndcNodeHandle, DndcLongString*);

#endif
DNDC_API
int
dndc_format_tree(DndcContext*, DndcLongString*);

DNDC_API
int
dndc_format_node(DndcContext*, DndcNodeHandle, int indent, DndcLongString*);
#if 0

DNDC_API
int
dndc_execute_js_in_node(DndcContext*, DndcNodeHandle, DndcLongString);

DNDC_API
int
dndc_execute_js_blocks(DndcContext*, LongString jsargs);

DNDC_API
DndcNodeLocation
dndc_node_location(DndcContext*, DndcNodeHandle);

#endif

DNDC_API
DndcNodeHandle
dndc_node_by_id(DndcContext*, DndcStringView);

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
dndc_make_node(DndcContext*, int type, DndcStringView header, DndcNodeHandle parent);

DNDC_API
int
dndc_resolve_imports(DndcContext*);

#ifdef __cplusplus
}
#endif


#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
