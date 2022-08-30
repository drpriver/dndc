//
// Copyright © 2021-2022, David Priver
//
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
// This is the api for creating and intereacting with an ast directly.  The
// functions provided in dndc.h are easier to use and have a stable API. This
// API is unstable by necessity.
//
// Some of these functions are documented to invoke the logger.

typedef unsigned DndcNodeHandle;
// --------------
// Handle to a node.
//


#ifdef __clang__
enum: DndcNodeHandle {DNDC_NODE_HANDLE_INVALID = (DndcNodeHandle)-1};
#else
enum {DNDC_NODE_HANDLE_INVALID = (DndcNodeHandle)-1};
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
DNDC_WARN_UNUSED
int
dndc_ctx_dup_sv(DndcContext* ctx, DndcStringView text, DndcStringView* result);
// ------------
// Functions taking string views potentially hold onto them for the lifetime of
// ctx. If you can't guarantee they last that long, then call this function to
// get a copy that will last as long as the ctx.
//
// Returns DNDC_ERROR_OOM on oom, else 0.

DNDC_API
DNDC_NULLABLE(DndcContext*)
dndc_create_ctx(unsigned long long flags, DNDC_NULLABLE(DndcFileCache*) base64cache, DNDC_NULLABLE(DndcFileCache*) textcache);
// ------------
// Creates an ast context.
//
// Arguments:
// ---------
// flags:
//     same as the ones to `dndc_compile_dnd_file`. Note that some of those
//     flags won't make sense.
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
unsigned long long
dndc_ctx_get_flags(DndcContext*);

DNDC_API
void
dndc_ctx_set_logger(DndcContext*, DNDC_NULLABLE(DndcLogFunc*), DNDC_NULLABLE(void*));
// ------------------
// Sets the logger on the context. Pass NULL to disable logging.
//
// Arguments:
// ---------
// ctx:
//     The parsing context.
//
// log_func:
//    A function for reporting errors. See `DndcLogFunc`. If NULL, errors
//    will not be printed. Use `dndc_stderr_log_func` for a function that
//    just prints to stderr.
//
// log_user_data:
//    A pointer that will be passed to the log_func. For
//    `dndc_stderr_log_func`, this should be NULL. For a function you've
//    defined, pass an appropriate pointer!
//

DNDC_API
void
dndc_ctx_destroy(DndcContext*);
// ----------------
// Call this when done with the context to free all resources.
//

DNDC_API
DNDC_NULLABLE(DndcContext*)
dndc_ctx_clone(DndcContext*);
// --------------
// Performs a deep copy of the given context. This copies everything, including
// strings that may have been borrowed from calls to this ast api. The one
// thing that is not deep copied are the file caches if you passed your own
// file caches into `dndc_create_ctx` instead of passing NULL.
//
// When done, pass it to `dndc_ctx_destroy`.
//
// Returns NULL on oom.
DNDC_API
DNDC_NULLABLE(DndcContext*)
dndc_ctx_shallow_clone(DndcContext*);
// --------------
// This is similar to `dndc_ctx_clone`, but it does not copy things that
// are safe to share, like strings. This means that the context returned
// from this function should notoutlive the context it was cloned from.
// This still has independent nodes, etc. so you can use it to fork a context,
// make some modifications and render the results, etc. which can have
// significant performance benefits if you need to dynamically render off
// a shared base.
//
// When done, pass it to `dndc_ctx_destroy`.
//
// Returns NULL on oom.
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
DNDC_WARN_UNUSED
int
dndc_ctx_get_base(DndcContext*, DndcStringView*);
// -----------------
// Borrows the base directory used to resolve relative filepaths.
//
// Returns:
// --------
// 0 on success, non-zero on error.
//

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
// This function can invoke the logger.
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
// This function can invoke the logger.
//

DNDC_API
DndcNodeHandle
dndc_ctx_make_root(DndcContext* ctx, DndcStringView filename);
// ------------------
// Creates the root node of the context, with the given filename.  The root
// node will be a MD node.
//
// If the root already exists, returns DNDC_NODE_HANDLE_INVALID.  Otherwise,
// returns a handle to the new node.
//
// This can also return DNDC_NODE_HANDLE_INVALID in an oom situtation.
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
// Sets the given node as the new root of the context.  If there already is a
// root node, it detaches that node first then attaches this one.
//
// The given node must be an orphan.
//
// Returns 0 on success, non-zero on error.
//


DNDC_API
DNDC_WARN_UNUSED
int
dndc_node_get_attribute(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView key, DndcStringView* value);
// -----------------------
// Retrieves the value of an attribute for a given node.
//
// If the attribute is not set on the node or the handle is invalid, non-zero
// is returned.  Otherwise, 0 is returned and value is filled with the value of
// the given attribute.  NOTE: this may be an empty string view.
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
// Note that in this API, each attribute has a value, even if it is the empty
// string.  The empty string represents the lack of a value for that attribute.
//
// Returns 0 on success, non-zero on error.
//
// This does not copy key nor value.
//

DNDC_API
int
dndc_node_del_attribute(DndcContext*, DndcNodeHandle, DndcStringView key);
// -----------------------
// Deletes a specific attribute set on a node.
//
// Returns 1 if it does, 0 otherwise.
//
// This function does not distinguish between not having an attribute and an error.
//

DNDC_API
size_t
dndc_node_attributes_count(DndcContext*, DndcNodeHandle);
// --------------------------
// Returns the number of attributes set on a given node.
//
// This function does not distinguish between a node with no attributes and an
// error.
//

typedef struct DndcAttributePair DndcAttributePair;
struct DndcAttributePair {
    DndcStringView key;
    DndcStringView value;
};
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
//     A pointer to an opaque value used for remembering where in the
//     attributes map this function is. Initialize the cookie to 0 before
//     calling this function.
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
// copied is equal to the result of `dndc_node_attributes_count`.
//
// Example:
// --------
#ifdef DNDC_AST_EXAMPLE
void print_attributes(FILE* fp, DndcContext* ctx, DndcNodeHandle dnh){
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
                fprintf(fp, "[%zu]: @%.*s(%.*s)\n", print_idx,
                        (int)k.length, k.text,
                        (int)v.length, v.text);
            else
                fprintf(fp, "[%zu]: @%.*s\n", print_idx,
                        (int)k.length, k.text);
        }
    }
}
#endif

DNDC_API
int
dndc_node_has_id(DndcContext*, DndcNodeHandle);
// --------------
// Returns 1 if the node has an id (explicit or implicit), otherwise 0.

DNDC_API
DNDC_WARN_UNUSED
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
//     The child node to append to parent. This node must be an orphan (no
//     parent node) and must not be equal to the parent.
//
//     Call `dndc_node_detach` to make a node an orphan.
//
// Returns:
// --------
// Returns 0 on success, a non-zero error code otherwise.
//
//

DNDC_API
int
dndc_node_append_string(DndcContext* ctx, DndcNodeHandle parent, DndcStringView sv);
// ----------------------
// Creates a string node with the given string and immediately appends that new
// node as a child to parent.
//
// This is a convenience function, to spare creating a node, setting its header
// and appending it.
//
// Arguments:
// ----------
// ctx:
//     The parsing context.
//
// parent:
//     The (valid) node to append to.
//
// sv:
//     The string to set as the header of the new STRING node.
//
//     NOTE: the string view needs to live as long as the node or ctx. Call
//     `dndc_ctx_dup_sv` if that cannot be guaranteed.
//
// Returns:
// --------
// Returns 0 on success, a non-zero error code otherwise.
//

DNDC_API
int
dndc_node_insert_child(DndcContext* ctx, DndcNodeHandle parent, size_t i, DndcNodeHandle child);
// ---------------------
// Inserts the child as the `i`th child of the parent.
//
// Arguments:
// ----------
// ctx:
//     The parsing context.
//
// parent:
//     The (valid) node to insert into.
//
// i:
//     The index to insert the child at. If this is greater than the number of
//     nodes that are children of the parent, then the child is inserted at the
//     end, making this function like `dndc_node_append_child` instead.
//
// child:
//     The child node to insert into the children of the parent. This node must
//     be an orphan (no parent node) and must not be equal to the parent.
//
//     Call `dndc_node_detach` to make a node an orphan.
//
// Returns:
// --------
// Returns 0 on success, a non-zero error code otherwise.
//

DNDC_API
int
dndc_node_insert_string(DndcContext* ctx, DndcNodeHandle parent, size_t i, DndcStringView sv);
// ---------------------
// Creates a new STRING node, with the header set to `sv`. Inserts the new node
// as the `i`th child of the parent.
//
// This is a convenience function, to spare creating a node, setting its header
// and inserting it.
//
// Arguments:
// ----------
// ctx:
//     The parsing context.
//
// parent:
//     The (valid) node to insert into.
//
// i:
//     The index to insert the child at. If this is greater than the number of
//     nodes that are children of the parent, then the child is inserted at the
//     end, making this function like `dndc_node_append_child` instead.
//
// sv:
//     The string to set as the header of the new STRING node.
//
//     NOTE: the string view needs to live as long as the node or ctx. Call
//     `dndc_ctx_dup_sv` if that cannot be guaranteed.
//
// Returns:
// --------
// Returns 0 on success, a non-zero error code otherwise.
//

DNDC_API
void
dndc_node_detach(DndcContext* ctx, DndcNodeHandle dnh);
// ----------------
// Detaches the given node from its parent. The node will be removed from the
// parent's children array and the node will have its parent set to the invalid
// node handle (representing not having a parent). This is sometimes referred
// to as an orphan node.
//

DNDC_API
int
dndc_node_remove_child(DndcContext* ctx, DndcNodeHandle parent, size_t i);
// ----------------------
// Removes the `i`th child from parent's children array, and makes the child an
// orphan node.
//
// If `i` is out of bounds, a non-zero error code is returned.
//
// Returns 0 on success, a non-zero error code otherwise.
//

DNDC_API
DndcNodeHandle
dndc_node_get_parent(DndcContext*, DndcNodeHandle);
// -------------------
// Returns the handle to the parent node associated with the given node.
//
// Note: this function does not distinguish between an error and a node without
// a parent.
//
// If the node does not have a parent or if an error occurs, returns
// DNDC_NODE_HANDLE_INVALID.
//

// DNDCNODETYPES
// -------------
// This macro is setup so that you can use it in an X macro.  It is all of the
// node types and their integer value.
//
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
    apply(TOC,            21)\
    apply(COMMENT,        22)\
    apply(CONTAINER,      23)\
    apply(QUOTE,          24)\
    apply(JS,             25)\
    apply(DETAILS,        26)\
    apply(META,           27)\
    apply(DEFLIST,        28)\
    apply(DEF,            29)\
    apply(INVALID,        30)\

// Manually expand for better documentation
enum DndcNodeType {
//----------------------
// The type of the node.
//
    DNDC_NODE_TYPE_MD           =  0,
    DNDC_NODE_TYPE_DIV          =  1,
    DNDC_NODE_TYPE_STRING       =  2,
    DNDC_NODE_TYPE_PARA         =  3,
    DNDC_NODE_TYPE_TITLE        =  4,
    DNDC_NODE_TYPE_HEADING      =  5,
    DNDC_NODE_TYPE_TABLE        =  6,
    DNDC_NODE_TYPE_TABLE_ROW    =  7,
    DNDC_NODE_TYPE_STYLESHEETS  =  8,
    DNDC_NODE_TYPE_LINKS        =  9,
    DNDC_NODE_TYPE_SCRIPTS      = 10, // NOTE: This is for <script> tags in the rendered doc
    DNDC_NODE_TYPE_IMPORT       = 11,
    DNDC_NODE_TYPE_IMAGE        = 12,
    DNDC_NODE_TYPE_BULLETS      = 13,
    DNDC_NODE_TYPE_RAW          = 14,
    DNDC_NODE_TYPE_PRE          = 15,
    DNDC_NODE_TYPE_LIST         = 16,
    DNDC_NODE_TYPE_LIST_ITEM    = 17,
    DNDC_NODE_TYPE_KEYVALUE     = 18,
    DNDC_NODE_TYPE_KEYVALUEPAIR = 19,
    DNDC_NODE_TYPE_IMGLINKS     = 20,
    DNDC_NODE_TYPE_TOC          = 21,
    DNDC_NODE_TYPE_COMMENT      = 22,
    DNDC_NODE_TYPE_CONTAINER    = 23,
    DNDC_NODE_TYPE_QUOTE        = 24,
    DNDC_NODE_TYPE_JS           = 25, // NOTE: This is for compiletime scripting.
    DNDC_NODE_TYPE_DETAILS      = 26,
    DNDC_NODE_TYPE_META         = 27,
    DNDC_NODE_TYPE_DEFLIST      = 28,
    DNDC_NODE_TYPE_DEF          = 29,
    DNDC_NODE_TYPE_INVALID      = 30,
};

// Check that the above is correct.
#define X(a, b) _Static_assert(DNDC_NODE_TYPE_##a == b, #a " has incorrect value, not equal to " #b);
    DNDCNODETYPES(X)
#undef X

DNDC_API
int
dndc_node_get_type(DndcContext*, DndcNodeHandle);
// ------------------
// Returns the type of the node (as an integer).
//
// Note: this function does not distinguish between an error and a node with
// the type DNDC_NODE_TYPE_INVALID.
//

DNDC_API
int
dndc_node_set_type(DndcContext* ctx, DndcNodeHandle dnh, int node_type);
// -----------------
// Sets the type of the node.
//
// `node_type` must be a valid value of `DndcNodeType`.
//
// Returns 0 on success and non-zero on error.
//


#ifdef __clang__
enum __attribute__((flag_enum)) DNDC_NODEFLAG {
#else
enum DNDC_NODEFLAG{
#endif
    DNDC_NODEFLAG_NONE     = 0x0,
    DNDC_NODEFLAG_IMPORT   = 0x1,
    DNDC_NODEFLAG_NOID     = 0x2,
    DNDC_NODEFLAG_HIDE     = 0x4,
    DNDC_NODEFLAG_NOINLINE = 0x8,
};

// DNDC_NODEFLAG
// -------------
// Flags that can be set or cleared on nodes providing specific behaviors.
//
// DNDC_NODEFLAG_NONE:
//     Does nothing.
//
// DNDC_NODEFLAG_IMPORT:
//     Instead of interpreting the node's children as strings, they are paths
//     to files that should be loaded and parsed themselves, with the given
//     node as the parent container. This will get imported if you call
//     `dndc_ctx_resolve_imports`.
//
// DNDC_NODEFLAG_NOID:
//     Suppresses the generation of the default id (which is derived from the
//     node's header).
//
// DNDC_NODEFLAG_HIDE:
//     Skip this node when rendering to html.
//
// DNDC_NODEFLAG_NOINLINE:
//     Some nodes (such as IMG and IMGLINKS) are inlined by default, putting
//     the contents in the document itself instead of generating a link.  This
//     flag suppressed that behavior and causes a link to be generated instead.
//     This will mean the resulting document is no longer self-contained.
//


DNDC_API
int
dndc_node_get_flags(DndcContext*, DndcNodeHandle);
// -------------------
// Retrieves the flags set on a node. This will be the bitwise-or of the
// `DNDC_NODEFLAG`s that are set.
//
// Note: this function does not distinguish between an error and a node with no
// flags set.
//


DNDC_API
int
dndc_node_set_flags(DndcContext*, DndcNodeHandle, int);
// -------------------
// Sets the flags on a node to the given value. This should be the bitwise-or
// of the `DNDC_NODEFLAG`s you wish to set. Note that this will override all
// of the flags already set on the node. Call `dndc_node_get_flags` first and
// bit-twiddle them if you don't want to leave the values of those flags
// undisturbed.
//
// Returns 0 on success and non-zero on error.
//

DNDC_API
int
dndc_node_get_header(DndcContext*, DndcNodeHandle, DndcStringView*);
// --------------------
// Gets the "header" associated with a node. The header is the stuff to the
// left of a double colon in a dnd document and is usually used to generate
// headings.  As a special case, the header is actually the value of a STRING
// node.
//
// Returns 0 on success and non-zero on error.
//

DNDC_API
int
dndc_node_set_header(DndcContext*, DndcNodeHandle, DndcStringView);
// --------------------
// Sets the "header" associated with a node. Note that for a STRING node this
// is actually that node's value. For most nodes, this is the heading that will
// be generated at the start of that content in the html. For some nodes this
// is ignored.
//
// NOTE: the string view needs to live as long as the node or ctx. Call
// `dndc_ctx_dup_sv` if that cannot be guaranteed.
//
// Returns 0 on success and non-zero on error.
//


DNDC_API
size_t
dndc_node_children_count(DndcContext*, DndcNodeHandle);
// ----------------------
// Returns how many nodes are children of this node.
//
// Note: this function does not distinguish between an error and a node with no
// children.
//

DNDC_API
size_t
dndc_node_get_children(DndcContext* ctx, DndcNodeHandle dnh, size_t* cookie, DndcNodeHandle* buff, size_t buff_len);
// -----------------------
// Copies the handles to the child nodes of a node into a buffer.
//
// Arguments:
// ----------
// ctx:
//     The parsing context.
//
// dnh:
//     Handle to the node that the parsed file will be the child of.  Must be a
//     valid handle.
//
// cookie:
//     A pointer to an opaque value used for remembering where in the children
//     array this function is. Initialize the cookie to 0 before calling this
//     function.
//
// buff:
//     The buffer to copy handles into.
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
// copied is equal to the result of `dndc_node_children_count`.

DNDC_API
DndcNodeHandle
dndc_node_get_child(DndcContext* ctx, DndcNodeHandle dnh, long i);
// ---------------------
// Retrieves the handle of the `i`th child of the given node.
//
// If this is out of bounds, DNDC_NODE_HANDLE_INVALID is returned.
//
// Arguments:
// ----------
// ctx:
//     The parsing context.
//
// dnh:
//     Handle to the node that is the parent of the return value.
//
// i:
//     Index of the child. This can be negative, which means to index from the back.
//
//
// Returns:
// --------
// The handle of the `i`th child or DNDC_NODE_HANDLE_INVALID if out of bounds.
//
// DNDC_NODE_HANDLE_INVALID can also be return if `dnh` is an invalid handle.

DNDC_API
int
dndc_node_cat_string_children(DndcContext*, DndcNodeHandle, DndcLongString* out);
// -------------------------------
// Convenience function. Iterates over the children of a node and concatenates
// their strings together.  This could be useful for executing custom scripting
// languages.
//
// When you are done with the the string, pass it to `dndc_free_string`.
//
// Returns 0 on success and non-zero on error.
//
// On error, nothing is written to the string argument and you do not need to
// call `dndc_free_string`.
//


DNDC_API
size_t
dndc_node_classes_count(DndcContext*, DndcNodeHandle);
// -----------------------
// Returns how many classes this node has.
//
// Note: this function does not distinguish between an error and a node with no
// classes.
//

DNDC_API
size_t
dndc_node_classes(DndcContext*, DndcNodeHandle, size_t* cookie, DndcStringView* buff, size_t buff_len);
// -----------------------
// Copies the classes (string views) of a node into a buffer.
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
//     A pointer to an opaque value used for remembering where in the classes array
//     this function is. Initialize the cookie to 0 before calling this function.
//
// buff:
//     The buffer to copy classes into.
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
// copied is equal to the result of `dndc_node_classes_count`.
//

DNDC_API
int
dndc_node_add_class(DndcContext*, DndcNodeHandle, DndcStringView);
// ------------------
// Adds a class to the class array of a node.
//
// Returns 0 on success and non-zero on error.
//
// Note: this does not copy the class.
//

DNDC_API
int
dndc_node_has_class(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView cls);
// -------------------
// Queries if the node has the given class in its classes array.
//
// Note: this function does not distinguish between an error and a node not
// having a class.
//
// Returns 0 if it does not or if an error occurs. 1 if it does have that
// class.
//



DNDC_API
int
dndc_node_remove_class(DndcContext*, DndcNodeHandle, DndcStringView);
// Removes a class to the class array of a node.
//
// Returns 0 on success and non-zero on error.
//
// Note: this returns 0 even if the node does not have this class.
//


DNDC_API
DNDC_WARN_UNUSED
int
dndc_ctx_expand_to_dnd(DndcContext*, DndcLongString*);
// ---------------------
// Serializes the context starting at the root into a string that will parse
// back into the same tree.
//
// Some trees cannot be serialized to a string in this way.
//
// When you are done with the the string, pass it to `dndc_free_string`.
//
// Returns 0 on success and non-zero on error.
//
// On error, nothing is written to the string argument and you do not need to
// call `dndc_free_string`.
//
// This function can call the logger.
//

DNDC_API
DNDC_WARN_UNUSED
int
dndc_ctx_render_to_md(DndcContext*, DndcLongString*);
// ---------------------
// Generates a markdown document from the context, starting at the root.
// This is a best effort attempt and can fail.
//
// When you are done with the the string, pass it to `dndc_free_string`.
//
// Returns 0 on success and non-zero on error.
//
// On error, nothing is written to the string argument and you do not need to
// call `dndc_free_string`.
//
// This function can call the logger.
//

DNDC_API
DNDC_WARN_UNUSED
int
dndc_ctx_render_to_html(DndcContext*, DndcLongString*);
// -----------------------
// Generates an html document from the context, starting at the root.
//
// This will be either a complete document or a fragment if the
// DNDC_FRAGMENT_ONLY flag was passed to `dndc_create_ctx`.
//
// When you are done with the the string, pass it to `dndc_free_string`.
//
// Returns 0 on success and non-zero on error.
//
// On error, nothing is written to the string argument and you do not need to
// call `dndc_free_string`.
//
// This function can call the logger.
//

DNDC_API
DNDC_WARN_UNUSED
int
dndc_node_render_to_html(DndcContext*, DndcNodeHandle, DndcLongString*);
// ------------------------
// Generates an html fragment starting at the given node. Note this will not
// have any SCRIPTS nodes or CSS nodes as thsoe are only output in a complete
// document from `dndc_ctx_render_to_html`.
//
// When you are done with the string, pass it to `dndc_free_string`.
//
// Returns 0 on success and non-zero on error.
//
// On error, nothing is written to the string argument and you do not need to
// call `dndc_free_string`.
//
// This function can call the logger.
//

DNDC_API
DNDC_WARN_UNUSED
int
dndc_ctx_format_tree(DndcContext*, DndcLongString*);
// ----------------------
// Serializes the context starting at the root into a string that is a valid
// .dnd file and will parse back into the same tree. Unlike
// `dndc_ctx_expand_to_dnd`, this will be formatted to a specific width, tables
// aligned, etc.
//
// Some trees can't be serialized back into a .dnd string.
//
// When you are done with the the string, pass it to `dndc_free_string`.
//
// Returns 0 on success and non-zero on error.
//
// On error, nothing is written to the string argument and you do not need to
// call `dndc_free_string`.
//
// This function can call the logger.
//

DNDC_API
DNDC_WARN_UNUSED
int
dndc_node_format(DndcContext*, DndcNodeHandle, int indent, DndcLongString*);
// ----------------
// Serializes the node and its children into a string that is valid .dnd and
// will parse back into the same node + children. Additionally, this will be
// formatted to a specific width, tables aligned, etc.
//
// Some trees can't be serialized back into a .dnd string.
//
// When you are done with the the string, pass it to `dndc_free_string`.
//
// Returns 0 on success and non-zero on error.
//
// On error, nothing is written to the string argument and you do not need to
// call `dndc_free_string`.
//
// This function can call the logger.
//

DNDC_API
int
dndc_node_execute_js(DndcContext*, DndcNodeHandle, DndcLongString script);
// --------------------
// Execute the given javascript script in the context of the given node.  The
// node and ctx will be placed into the scope of the script as per usual, etc.
// `Args` will be null.
//
// Returns 0 on success and non-zero on error.
//
// Note that the tree may be in an unexpected state after an error in
// javascript. Likely the only safe thing to do is to call `dndc_ctx_destroy`
// on the context to cleanup resources.
//
// This function can call the logger.
//

DNDC_API
int
dndc_ctx_execute_js(DndcContext* ctx, DndcLongString jsargs);
// -------------------
// Executes all of the javascript nodes in the context.  The node and ctx will
// be placed into the scope of the script as per usual, etc.
//
// After execution, the javascript nodes will have their types changed to
// DNDC_NODE_TYPE_INVALID and are removed from the tree. This means this
// function can be safely called multiple times.
//
// Note that the tree may be in an unexpected state after an error in
// javascript. Likely the only safe thing to do is to call `dndc_ctx_destroy`
// on the context to cleanup resources.
//
// Arguments:
// ----------
// ctx:
//     The parsing context.
//
// jsargs:
//     A string that will be parsed as json and be present in the javascript
//     execution as the special `Args` global. For convenience, an empty string
//     will be treated as "null".
//
// Returns:
// --------
// Returns 0 on success and non-zero on error.
//
// This function can call the logger.
//

typedef struct DndcNodeLocation DndcNodeLocation;
struct DndcNodeLocation {
    DndcStringView filename;
    int row, column;
};
// ----------------
// Where in the source files the current node comes from.  Note that since
// nodes can be genereated programatically (this api and from js), that the
// location might not correspond to a real file or might correspond to a
// non-sensical part of a real file.
//
// Members:
// --------
// filename: name of the file where the node came from
// row:      1-based, which line of the file this node is from.
// column:   1-based, which column of the line
//

DNDC_API
DNDC_WARN_UNUSED
int
dndc_node_location(DndcContext*, DndcNodeHandle, DndcNodeLocation*);
// --------------------
// Returns the location of a node in its source file. See the discussion of the
// caveats of this information above in `DndcNodeLocation`.
//
// Returns 0 on success and non-zero on error.
//

DNDC_API
DndcNodeHandle
dndc_ctx_node_by_approximate_location(DndcContext*, DndcStringView filename, int row, int column);
// ---------------------
// Retrieves the node closest to the given location.
// Closest is only in regards to row and column - filename has to be exact.
//
// Note: the current implementation of this function just scans every node
// in the context, so it is O(N) with the number of nodes.
//
// Note that not every line of a file has a corresponding node.  This can come
// up for blank lines. In this case, this function will return the closest node
// before the blank line.
//
// Arguments:
// ----------
// ctx:
//      The parsing context.
//
// filename:
//      Which file the node originated from.
//
// row:
//      Which line in the file the node is from. 1-based.
//
// column:
//      While column in the row the node is from. 1-based, but 0 is allowed.
//
// Returns:
// --------
// Returns the node's handle on successful lookup and `DNDC_NODE_HANDLE_INVALID`
// if the node cannot be found.
//

DNDC_API
DndcNodeHandle
dndc_ctx_node_by_id(DndcContext*, DndcStringView);
// --------------------
// Retrieves the node with the given string id.
//
// Note that the id will be normalized (kebabed) before lookup.
// "Hello World" becomes "hello-world". You do not need to call `dndc_kebab`
// beforehand.
//
// Returns the node's handle on successful lookup and `DNDC_NODE_HANDLE_INVALID`
// if the node cannot be found.
//
// Note: in some circumstances, two nodes can have the same string id. Which
// handle is returned is not specified.
//

DNDC_API
int
dndc_ctx_node_invalid(DndcContext* ctx, DndcNodeHandle);
// ---------------------
// Returns whether the node handle is invalid.
//
// The obvious case is `DNDC_NODE_HANDLE_INVALID`, but also this allows you to
// the check the validity of a handle that you deserialized from disk, stuffed
// into an integer somewhere, taken from user input, etc.
//
// Returns 1 if the handle is invalid and 0 otherwise.

DNDC_API
DndcNodeHandle
dndc_ctx_make_node(DndcContext*, int type, DndcStringView header, DndcNodeHandle parent);
// -------------------
// Creates a new node in the tree.
//
// Arguments:
// ----------
// ctx:
//      The parsing context.
//
//  type:
//      The type of the new node. Must be a valid value of DndcNodeType.
//
//  header:
//      The "header" of the new node. This may be an emptry string.
//
//  parent:
//      The parent of the new node. If valid, this node will be appended as a
//      child to that parent node. If invalid, then this new node will be an
//      orphan.
//
// Returns:
// --------
// The handle to the new node on success and `DNDC_NODE_HANDLE_INVALID` on error.
//

DNDC_API
int
dndc_ctx_resolve_imports(DndcContext*);
// ------------------------
// Iterates through all IMPORT nodes and all nodes with the #import flag and
// imports their children.  These nodes will have the #import flag removed and
// IMPORT nodes will be changed to a non-IMPORT container node. This means this
// function is safe to call multiple times as a given node will only be
// imported once.
//
// Returns 0 on success, non-zero on error.
//
// This function can call the logger.
//

typedef struct DndcPreloadImageJob DndcPreloadImageJob;
DNDC_API
DNDC_NULLABLE(DndcPreloadImageJob*)
dndc_ctx_create_preload_img_job(DndcContext*);
// ------------------------------------
// Preps the preload job so it can be safely performed in parallel with things
// like user scripts that can add more img nodes.
//
// Returns NULL if there is no work to do.
//

DNDC_API
void
dndc_preload_imgs(DNDC_NULLABLE(DndcPreloadImageJob*));
// ---------------------------------
// Iterates the IMG nodes and the IMGLINKS node and popluates the loaded image
// chage. This can speed up html generation as this work would otherwise have
// to be done during html generation.
//
// The intended use is to call this in parallel with executing user scripts.
//
// This function does not report errors.
//
DNDC_API
void
dndc_ctx_preload_img_job_join(DndcContext*, DNDC_NULLABLE(DndcPreloadImageJob*));
// ----------------------------
// Merges the result of preloading into the ctx. Call this after user scripts have
// finished.

DNDC_API
int
dndc_ctx_resolve_links(DndcContext*);
// ------------------
// Populates the internal link database based on the ids of the nodes in the
// Also, adds links from LINKS nodes and prepares the internal link database.
// Call this before rendering to html.
//
// Returns 0 on success, non-zero on error.
//
// This function can call the logger.
//

DNDC_API
int
dndc_ctx_build_toc(DndcContext*);
// ------------------
// Populates the TOC node, if there is one in the context.
//
// Returns 0 on success, non-zero on error.
//


DNDC_API
size_t
dndc_ctx_select_nodes(DndcContext* ctx, size_t* cookie,
        int type,
        DNDC_NULLABLE(DndcStringView*) attributes, size_t attribute_count,
        DNDC_NULLABLE(DndcStringView*) classes, size_t class_count,
        DndcNodeHandle* buff, size_t buflen);
// ---------------------
// Copies the handles of nodes in the context that meet certain criteria.
//
// Note: if type is DNDC_NODE_TYPE_INVALID, attributes is null and classes is
// null, then every node in the context will be copied into the buffer.
//
// Arguments:
// ----------
// ctx:
//     The parsing context.
//
// cookie:
//     A pointer to an opaque value used for remembering where in the nodes
//     this function is. Initialize the cookie to 0 before calling this
//     function.
//
// type:
//     The type of the selected nodes. If this is DNDC_NODE_TYPE_INVALID, then
//     this argument is ignored.  Otherwise, all nodes will be of this type.
//     Note that there is no way to select nodes with type
//     DNDC_NODE_TYPE_INVALID as they are supposed to be ignored.
//
// attributes:
//     Pointer to an array of attributes that the selected nodes must have. If
//     this is length 0, then this argument is ignored. Otherwise, the nodes
//     must have all of these attributes.
//
// attribute_count:
//     The length of the array pointed to by `attributes` (in elements, not
//     bytes).
//
// classes:
//     Pointer to an array of classes that the selected nodes must have. If
//     this is length 0, then this argument is ignored. Otherwise, the nodes
//     must have all of these classes.
//
// class_count:
//      The length of the array pointed to by `classes` (in elements, not
//      bytes).
//
// buff:
//      Pointer to an array to copy the handles into.
//
// buflen:
//      The length of `buff` (in elements, not bytes).
//
// Returns:
// --------
// The number of items copied into buff. If 0 is returned, no items were copied
// into the buff and there are no more items to copy.
//
// To gather all of the nodes meeting this criteria, loop until this returns 0.
//
// There is no way to get the total number that meet the criteria ahead of time
// (maybe in the future).
//

DNDC_API
DNDC_WARN_UNUSED
int
dndc_node_tree_repr(DndcContext* ctx, DndcNodeHandle dnh, DndcLongString*);
// --------------------
// Outputs a string representation of the tree starting from the given node.
// Useful for debugging.
//
// When you are done with the the string, pass it to `dndc_free_string`.
//
// Returns 0 on success and non-zero on error.
//
// On error, nothing is written to the string argument and you do not need to
// call `dndc_free_string`.
//

DNDC_API
int
dndc_ctx_add_link(DndcContext* ctx, DndcStringView k, DndcStringView v);
// -----------------
// Adds an explicit link to the link table.

DNDC_API
size_t
dndc_ctx_get_dependencies(DndcContext*, DndcStringView* buff, size_t bufflen, size_t* cookie);
// ---------------------
// Copies the filepaths that are the files the context depends on into a buffer.
// This always you to do things like watch these files and re-create and
// recalculate the outputted html whenever those files change.
//
// Arguments:
// ----------
// ctx:
//      The parsing context.
// buff:
//      The buffer to copy the paths into.
// bufflen:
//      How long the buffer is (in items).
// cookie:
//      A pointer to an opaque value used for remembering where in the
//      dependencies this function is. Initialize the cookie to 0 before
//      calling this function.
//
// Returns:
// --------
// The number of items copied into buff. If 0 is returned, no items were copied
// into buff and there are no more items to copy.
//
// Example:
// --------
#ifdef DNDC_AST_EXAMPLE
// This example writes a make-style dependency file, which is the de-facto
// standard format used by most tools.
void
write_mk_escaped_char(char c, FILE* fp){
    // some characters need to be escaped in makefiles
    // Not all paths can be represented properly in a makefile.
    switch(c){
        case ' ': case '#':
            fputc('\\', fp);
            break;
        case '$':
            fputc('$', fp);
            break;
        default: break;
    }
    fputc(c, fp);
}
void
write_dependencies(DndcContext* ctx, FILE* fp, const char* targetname){
    for(const char* p = targetname; *p; p++)
        write_mk_escaped_char(*p, fp);
    fputc(':', fp);
    size_t cookie = 0;
    size_t n;
    enum {bufflen=32};
    DndcStringView buff[bufflen];
    while((n=dndc_ctx_get_dependencies(ctx, buff, bufflen, &cookie))){
        for(size_t i = 0; i < n; i++){
            DndcStringView sv = buff[i];
            fputc(' ', fp);
            for(size_t j = 0; j < sv.length; j++)
                write_mk_escaped_char(sv.text[j], fp);
        }
    }
    fputc('\n', fp);
}

#endif


DNDC_API
DNDC_WARN_UNUSED
int
dndc_kebab(DndcStringView sv, char* buff, size_t bufflen, size_t* used);
// ------------------
// "Kebabs" a string, which is what is used for the generated ids in the html
// document. Use this function if you need the id of a node in the generated
// html document
//
// Note that a nul terminator is not written into the buffer.
//
// Arguments:
// ----------
// sv:
//      The string to be kebabed.
//
// buff:
//      Pointer to a char array to write the kebabed string into.
//      The character array must be at least of length sv.length+2.
//      I don't know why this is and it might be reduced in the future,
//      but that is a safe value.
//
//      Note that a nul terminator is not written into the buffer.
//
// bufflen:
//      The size of the char array pointed to by buff.
//
// used:
//      The actual number of characters written into buff will be stored
//      here.
//
// Returns:
// --------
// 0 on success, non-zero if there is an error.
// Errors occur due to zero length sv or insufficiently sized buff.

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
compile_dnd_to_html(
        DndcStringView basedir,
        DndcStringView filename,
        DndcStringView text,
        DndcLongString* outhtml
        ){
    unsigned long long flags = 0;
    DndcFileCache* b64cache = NULL;
    DndcFileCache* textcache = NULL;
    DndcContext* ctx = dndc_create_ctx(flags, b64cache, textcache);
    dndc_ctx_set_logger(ctx, dndc_stderr_log_func, NULL);
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

    err = dndc_ctx_resolve_links(ctx);
    if(err) goto fail;

    err = dndc_ctx_build_toc(ctx);
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
compile_dnd_to_html_with_extra_script(
        DndcStringView basedir,
        DndcStringView filename,
        DndcStringView text,
        DndcLongString* outhtml
        ){
    unsigned long long flags = 0;
    DndcFileCache* b64cache = NULL;
    DndcFileCache* textcache = NULL;
    DndcContext* ctx = dndc_create_ctx(flags, b64cache, textcache);
    dndc_ctx_set_logger(ctx, dndc_stderr_log_func, NULL);
    dndc_ctx_set_base(ctx, basedir);
    DndcNodeHandle root = dndc_ctx_make_root(ctx, filename);
    int err = 0;
    err = dndc_ctx_parse_string(ctx, root, filename, text);
    if(err) goto fail;

    err = dndc_ctx_resolve_imports(ctx);
    if(err) goto fail;

    // inject a script
    DndcStringView empty = {0};
    DndcNodeHandle jsnode = dndc_ctx_make_node(ctx, DNDC_NODE_TYPE_SCRIPTS, empty, root);
    #define MYSCRIPT "window.alert('pwned');"
    DndcStringView myscript = {sizeof(MYSCRIPT)-1, MYSCRIPT};
    #undef MYSCRIPT
    dndc_ctx_make_node(ctx, DNDC_NODE_TYPE_STRING, myscript, jsnode);

    {
        DndcLongString jsargs = {4, "null"};
        err = dndc_ctx_execute_js(ctx, jsargs);
    }
    if(err) goto fail;

    err = dndc_ctx_resolve_links(ctx);
    if(err) goto fail;

    err = dndc_ctx_build_toc(ctx);
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
