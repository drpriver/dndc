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
// --------------
// Handle to a node.
//


#ifdef __clang__
enum: DndcNodeHandle {DNDC_NODE_HANDLE_INVALID = (DndcNodeHandle)-1};
#else
enum {DNDC_NODE_HANDLE_INVALID = -1};
#endif
// ------------------------
// This value represents the absence of a node.
//

typedef struct DndcContext DndcContext;
// -----------
// Opaque type representing a parsing/ast context. Manages the memory, file
// caches, etc. of a parsing context.
//

DNDC_API
DndcStringView
dndc_ctx_dup_sv(DndcContext* ctx, DndcStringView text);
// ------------
// Functions taking string views potentially hold onto them for
// the lifetime of ctx. If you can't guarantee they last that long,
// then call this function to get a copy that will last as long
// as the ctx.
//

DNDC_API
DndcContext*
dndc_create_ctx(unsigned long long flags,
        DNDC_NULLABLE(DndcErrorFunc*) error_func, DNDC_NULLABLE(void*) error_func_data,
        DNDC_NULLABLE(DndcFileCache*) base64cache, DNDC_NULLABLE(DndcFileCache*) textcache);
// ------------
// Creates an ast context.
//
// Arguments:
// ---------
// flags:
//     same as the ones to `dndc_compile_dnd_file`. Note that some of those
//     flags won't make sense.
//
// error_func:
//    A function for reporting errors. See `DndcErrorFunc`. If NULL,
//    errors will not be printed. Use `dndc_stderr_error_func` for a function
//    that just prints to stderr.
//
// error_user_data:
//    A pointer that will be passed to the error_func. For
//    `dndc_stderr_error_func`, this should be NULL. For a function you've
//    defined, pass an appropriate pointer!
//
// base64cache:
//    A pointer to a filecache (created with `dndc_create_filecache`) that is
//    used to cache files across invocations of this function. This may be
//    null, in which case no caching is done.
//
//    This cache is used to cache the results of loading and base64-ing binary
//    files.
//
// textcache:
//    A pointer to a filecache (created with `dndc_create_filecache`) that is
//    used to cache files across invocations of this function. This may be
//    null, in which case no caching is done.
//
//    This cache is used to cache the results of loading text files.
//
// Returns:
// --------
// A valid context on success, NULL on failure.

DNDC_API
void
dndc_ctx_destroy(DndcContext*);
// ----------------
// Call this when done with the context to free all resources.
//

DNDC_API
DndcContext*
dndc_ctx_clone(DndcContext*);
// --------------
// Performs a deep copy of the given context. This copies everything, including
// strings that may have been borrowed from calls to this ast api. The one thing that
// is not deep copied are the file caches if you passed your own file caches into
// `dndc_create_ctx` instead of passing NULL.
//

DNDC_API
int
dndc_ctx_set_base(DndcContext*, DndcStringView);
// -----------------
// Sets the base directory used to resolve relative filepaths.
//
// Returns:
// --------
// 0 on success, non-zero on error.
//

DNDC_API
int
dndc_ctx_get_base(DndcContext*, DndcStringView*);
// -----------------
// Borrows the base directory used to resolve relative filepaths.
//
// Returns:
// --------
// 0 on success, non-zero on error.
//

// DELETEME
DNDC_API
int
dndc_ctx_store_builtin_file(DndcContext* ctx, DndcStringView filename, DndcStringView contents);

DNDC_API
int
dndc_ctx_parse_file(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView sourcepath);
// -------------------
// Loads and then parses the file into the context.
//
// Arguments:
// ----------
// ctx:
//     The parsing context.
//
// dnh:
//     Handle to the node that the parsed file will be the child of.
//     Must be a valid handle.
//
// sourcepath:
//     The path to the file to load and parse. If it is a relative path, then
//     it will be adjusted by the context's base directory.
//
// Returns:
// --------
// 0 on success, non-zero on error.
//

DNDC_API
int
dndc_ctx_parse_string(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView filename, DndcStringView contents);
// ---------------------
// Parses the given contents into the context.
//
// Arguments:
// ----------
// ctx:
//     The parsing context.
//
// dnh:
//     Handle to the node that the parsed string will be the child of.
//     Must be a valid handle.
//
// filename:
//     The name of the file the text came from. This is used for reporting
//     errors and is visible in the node's location. This can be whatever.
//     Borrowed by the context.
//
// contents:
//     The contents to parse into the context. Borrowed by the context.
//
// Returns:
// --------
// 0 on success, non-zero on error.
//

DNDC_API
DndcNodeHandle
dndc_ctx_make_root(DndcContext* ctx, DndcStringView filename);
// ------------------
// Creates the root node of the context, with the given filename.
// The root node will be a MD node.
//
// If the root already exists, returns DNDC_NODE_HANDLE_INVALID.
// Otherwise, returns a handle to the new node.
//

DNDC_API
DndcNodeHandle
dndc_ctx_get_root(DndcContext*);
// -----------------
// Returns the handle to the root node. Returns DNDC_NODE_HANDLE_INVALID if
// there is no root node (as can happen if the root gets detached).
//

DNDC_API
int
dndc_ctx_set_root(DndcContext*, DndcNodeHandle);
// -----------------
// Sets the given node as the new root of the context.
// If there already is a root node, it detaches that node first then attaches
// this one.
//
// The given node must be an orphan.
//
// Returns 0 on success, non-zero on error.
//


DNDC_API
int
dndc_node_get_attribute(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView key, DndcStringView* value);
// -----------------------
// Retrieves the value of an attribute for a given node.
//
// If the attribute is not set on the node or the handle is invalid, non-zero is returned.
// Otherwise, 0 is returned and value is filled with the value of the given attribute.
// NOTE: this may be an empty string view.
//
// Returns 0 on success, non-zero on error.
//


DNDC_API
int
dndc_node_has_attribute(DndcContext*, DndcNodeHandle, DndcStringView key);
// -----------------------
// Checks if a specific attribute is set on a node.
//
// Returns 1 if it does, 0 otherwise.
//
// This function does not distinguish between not having an attribute and an error.
//

DNDC_API
int
dndc_node_set_attribute(DndcContext*, DndcNodeHandle, DndcStringView key, DndcStringView value);
// -----------------------
// Sets a specific attribute for a given node.
//
// Note that in this API, each attribute has a value, even if it is the empty string.
// The empty string represents the lack of a value for that attribute.
//
// Returns 0 on success, non-zero on error.
//

DNDC_API
size_t
dndc_node_attributes_count(DndcContext*, DndcNodeHandle);
// --------------------------
// Returns the number of attributes set on a given node.
//
// This function does not distinguish between a node with no attributes and an error.
//

typedef struct DndcAttributePair {
    DndcStringView key;
    DndcStringView value;
} DndcAttributePair;
// -----------------
// Key-Value pair for the attributes set on a node.
//

DNDC_API
size_t
dndc_node_attributes(DndcContext* ctx, DndcNodeHandle dnh, size_t* cookie, DndcAttributePair* buff, size_t bufflen);
// --------------------
// Copies the set attributes and their values into the given buffer.
//
// Arguments:
// ----------
// ctx:
//     The parsing context.
//
// dnh:
//     Handle to the node that the parsed file will be the child of.
//     Must be a valid handle.
//
// cookie:
//     A pointer to an opaque value used for remembering where in the attributes map
//     this function is. Initialize the cookie to 0 before calling this function.
//
// buff:
//     The buffer to copy pairs into.
//
// bufflen:
//     The length (in items, not bytes) of buff.
//
// Returns:
// --------
// The number of items copied into buff. If 0 is returned, no items were copied
// into the buff and there are no more items to copy.
//
// Either loop until this function returns 0 or until the total number of items
// copied is equal to the result of `dnd_node_attributes_count`.
//
// Example:
// --------
#ifdef DNDC_AST_EXAMPLE
void print_attributes(DndcContext* ctx, DndcNodeHandle dnh){
    enum {buff_len=32};
    DndcAttributePair buff[buff_len];
    size_t n_copied = 0;
    size_t print_idx = 0;
    size_t cookie = 0;
    while((n_copied = dndc_node_attributes(ctx, dnh, &cookie, buff, buff_len))){
        for(size_t buf_idx = 0; buf_idx < n_copied; buf_idx++, print_idx++){
            DndcStringView k = buff[buf_idx].key;
            DndcStringView v = buff[buf_idx].value;
            if(v.length)
                printf("[%zu]: @%.*s(%.*s)\n", print_idx,
                        (int)k.length, k.text,
                        (int)v.length, v.text);
            else
                printf("[%zu]: @%.*s\n", print_idx,
                        (int)k.length, k.text);
        }
    }
}
#endif

DNDC_API
int
dndc_node_get_id(DndcContext*, DndcNodeHandle, DndcStringView* id);
// ---------------
// Retrieves the string id associated with the given node.
//
// FIXME: the returned string id is unnormalized (kebabed) and so
// may be confusing.
//

DNDC_API
int
dndc_node_set_id(DndcContext*, DndcNodeHandle, DndcStringView id);
// ----------------
// Sets the string id associated with the given node.
//
// Returns 0 on success, a non-zero error code otherwise.
//

DNDC_API
int
dndc_node_append_child(DndcContext* ctx, DndcNodeHandle parent, DndcNodeHandle child);
// ----------------------
// Appends the child as a child to parent.
//
// Arguments:
// ----------
// ctx:
//     The parsing context.
//
// parent:
//     The (valid) node to append to.
//
// child:
//     The child node to append to parent. This node must be an orphan (no parent node)
//     and must not be equal to the parent.
//
//     Call `dndc_node_detach` to make a node an orphan.
//
// Returns:
// --------
// Returns 0 on success, a non-zero error code otherwise.
//

DNDC_API
int
dndc_node_insert_child(DndcContext*, DndcNodeHandle, size_t, DndcNodeHandle);
// ---------------------

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


#ifdef __clang__
enum __attribute__((flag_enum)) {
#else
enum {
#endif
    DNDC_NODEFLAG_NONE     = 0x0,
    DNDC_NODEFLAG_IMPORT   = 0x1,
    DNDC_NODEFLAG_NOID     = 0x2,
    DNDC_NODEFLAG_HIDE     = 0x4,
    DNDC_NODEFLAG_NOINLINE = 0x8,
};


DNDC_API
int
dndc_node_get_flags(DndcContext*, DndcNodeHandle);


DNDC_API
int
dndc_node_set_flags(DndcContext*, DndcNodeHandle, int);

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

DNDC_API
size_t
dndc_node_classes(DndcContext*, DndcNodeHandle, size_t* cookie, DndcStringView* buff, size_t buff_len);

DNDC_API
size_t
dndc_node_classes_count(DndcContext*, DndcNodeHandle);

DNDC_API
int
dndc_node_add_class(DndcContext*, DndcNodeHandle, DndcStringView);

DNDC_API
int
dndc_node_remove_class(DndcContext*, DndcNodeHandle, DndcStringView);


DNDC_API
int
dndc_ctx_expand_to_dnd(DndcContext*, DndcLongString*);

DNDC_API
int
dndc_ctx_render_to_html(DndcContext*, DndcLongString*);

DNDC_API
int
dndc_node_render_to_html(DndcContext*, DndcNodeHandle, DndcLongString*);

DNDC_API
int
dndc_ctx_format_tree(DndcContext*, DndcLongString*);

DNDC_API
int
dndc_node_format(DndcContext*, DndcNodeHandle, int indent, DndcLongString*);

DNDC_API
int
dndc_node_execute_js(DndcContext*, DndcNodeHandle, DndcLongString);

DNDC_API
int
dndc_ctx_execute_js(DndcContext*, DndcLongString jsargs);

typedef struct DndcNodeLocation {
    DndcStringView filename;
    int row, column;
} DndcNodeLocation;

DNDC_API
int
dndc_node_location(DndcContext*, DndcNodeHandle, DndcNodeLocation*);


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

DNDC_API
int
dndc_node_tree_repr(DndcContext* ctx, DndcNodeHandle dnh, DndcLongString*);

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
    DndcContext* ctx = dndc_create_ctx(flags, errfunc, errarg, b64cache, textcache);
    dndc_ctx_set_base(ctx, basedir);
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
    DndcContext* ctx = dndc_create_ctx(flags, errfunc, errarg, b64cache, textcache);
    dndc_ctx_set_base(ctx, basedir);
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
