//
// Copyright © 2021-2022, David Priver
//
#ifndef DNDC_TYPES_H
#define DNDC_TYPES_H
#include <stdint.h>
#include <stdbool.h>
#include "dndc_long_string.h"
#include "dndc.h"
#include "common_macros.h"
#include "dndc_node_types.h"
#include "dndc_file_cache.h"
#include "string_table.h"
#include "AttrTable.h"
#include "Allocators/allocator.h"
#include "Allocators/arena_allocator.h"
// #include "Allocators/linear_allocator.h"
#include "Utils/long_string.h"

//
// dndc_types.h
// ------------
// Type definitions for dndc, shared across components.
//

#ifndef __clang__
#ifndef _Nonnull
#define _Nonnull
#endif
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

//
// Janky pseudo-template shenanigans
//

#define MARRAY_T StringView
#include "Utils/Marray.h"

#define MARRAY_T LongString
#include "Utils/Marray.h"

#define RARRAY_T StringView
#include "Utils/Rarray.h"


#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// In the pre-alpha, this was in the public API. It's still
// used in a few internal spots.
// dndc_cli uses it.
typedef int DndcDependencyFunc(DNDC_NULLABLE(void*) dependency_user_data,
        size_t dependency_paths_count,
        DndcStringView* dependency_paths);
// ------------------
//
// A function type for reporting dependencies. For use with
// `run_the_dndc`.
//
// Arguments:
// ----------
// dependency_user_data:
//    A pointer to user-defined data. The pointer will be the same one provided
//    to `dndc_compile_dnd_file`.
//
// dependency_paths_count:
//    The length of the array dependency_paths points to.
//
// dependency_paths:
//    A pointer to an array of string views of the paths to the files that the
//    file depends on. Note these are string views and so not guaranteed to be
//    nul-terminated. Files that were loaded in the usual way will have the
//    base dir prepended, but javascript blocks can introduce arbitrary strings
//    as dependencies, which may or may not be absolute paths, or valid paths
//    at all.
//
// Returns:
// --------
// 0 on success and non-zero on failure. The value you return will be returned
// from `dndc_compile_dnd_file` if non-zero.
//
#ifdef __clang__
#pragma clang assume_nonnull end
#endif


//
// NodeHandle
// ----------
// Opaque handle to a node
// Provides strong typing instead of using raw indexes.
// Possibly we can do some trickery with generations and multi-threading the parser,
// but then it turned out that was one of the fastest parts and the actual slow parts
// are inherently serial.
//
// A thing we could actually do is tag if the node is a fat or skinny node
// (skinny nodes being things like string nodes), which could potentially result
// in some good savings.
//
// The issue with that is many things assume these node handles are stable
// and we support turning string nodes into other nodes.
//
typedef union NodeHandle {
    struct { uint32_t index; };
    uint32_t _value;
} NodeHandle;

// As 0 is the root node, we use this as the invalid value instead.
enum {INVALID_NODE_HANDLE_VALUE = -1};
// INVALID_NODE_HANDLE
// -------------------
// NOTE: not static so it is visible in the debugger.
// const NodeHandle INVALID_NODE_HANDLE = {._value=INVALID_NODE_HANDLE_VALUE};
// Shadow the above symbol intentionally.
#define INVALID_NODE_HANDLE ((NodeHandle){._value=INVALID_NODE_HANDLE_VALUE})

static inline
force_inline
bool
NodeHandle_eq(NodeHandle a, NodeHandle b){
    return a._value == b._value;
}

#define RARRAY_T NodeHandle
#include "Utils/Rarray.h"

#define MARRAY_T NodeHandle
#include "Utils/Marray.h"

// For tracking what's the id of a node.
typedef struct IdItem IdItem;
struct IdItem{
    NodeHandle node;
    // text needs to be kebabed before use.
    StringView text;
};
#define MARRAY_T IdItem
#include "Utils/Marray.h"

typedef enum
#ifdef __clang__
__attribute__((flag_enum))
#endif
NodeFlags {
    // -----
    // Public flags
    NODEFLAG_NONE = 0,
    // Import children
    NODEFLAG_IMPORT = 0x1,

    // Don't give an id to this node
    NODEFLAG_NOID = 0x2,

    // Hide this node from final output
    NODEFLAG_HIDE = 0x4,

    // Don't inline the images from this node
    NODEFLAG_NOINLINE = 0x8,

    // -----
    // Implementation detail flags

    // ID is not derived from header -> it's stored elsewhere.
    // Look it up via the NodeHandle of the node.
    NODEFLAG_ID = 0x10,
} NodeFlags;

enum {
    PUBLIC_NODE_FLAGS = NODEFLAG_NONE
        | NODEFLAG_IMPORT
        | NODEFLAG_NOID
        | NODEFLAG_HIDE
        | NODEFLAG_NOINLINE
};

// Node
// ----
typedef struct Node Node;
struct Node{
    // The type of the node
    NodeType type;                // 4 bytes
    // Handle to this node's parent node.
    // It is possible for this to be INVALID_NODE_HANDLE, which indicates it
    // has no parent.
    NodeHandle parent;            // 4 bytes
    // The header text for a node.
    // For NODE_STRING, this is instead the contents of that node
    StringView header;            // 16 bytes
    // Handles to child nodes.
    Rarray(NodeHandle)*_Nullable children; // 8 bytes
    AttrTable*_Nullable attributes; // 8 bytes
    Rarray(StringView)*_Nullable classes;    // 8 bytes
    // Source filename (used for reporting errors)
    uint32_t filename_idx; // 4 bytes
    // Location of first character of where this node originated from.
    // These are 0-based. Functions that report errors add 1 to this number
    // for the general human-readable version.
    int row, col;         // 4 + 4 bytes.
    NodeFlags flags; // 4 bytes
};

#if UINTPTR_MAX != 0xFFFFFFFF
_Static_assert(sizeof(Node) == 8*sizeof(size_t), "");
// Damn these are fat.
// As a huge number of nodes are string nodes, we need a different scheme
// for storing children attributes and classes.
_Static_assert(sizeof(Node) == 64, "");
#endif

#define MARRAY_T Node
#include "Utils/Marray.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static inline
force_inline
NodeHandle*_Nullable
node_children(Node* node){
    if(node->children)
        return node->children->data;
    else
        return NULL;
}
static inline
force_inline
size_t
node_children_count(Node* node){
    return node->children?node->children->count:0;
}

static inline
force_inline
void
node_remove_child(Node* node, size_t i){
    if(!node->children) return;
    if(i >= node->children->count) return;
    PushDiagnostic();
    SuppressNullableConversion();
    Rarray_remove(NodeHandle)(node->children, i);
    PopDiagnostic();
}


#define NODE_CHILDREN_FOR_EACH(iter, n) RARRAY_FOR_EACH(NodeHandle, iter, (n)->children)

typedef struct DndcContext DndcContext;
struct DndcContext {
    // The actual storage for all the nodes.
    Marray(Node) nodes;
    // Handle to the root node. Scripts can change this.
    NodeHandle root_handle;

    // General purpose allocator.
    ArenaAllocator main_arena;
    // Allocator for strings (strings are almost never freed)
    ArenaAllocator string_arena;
    // Allocator for scratch allocations
    ArenaAllocator temp;

    // current parsing location
    struct {
        const char*_Nonnull cursor;
        const char*_Nonnull end;
        const char*_Null_unspecified linestart;
        const char*_Nullable doublecolon;
        const char*_Null_unspecified line_end;
        int nspaces;
        int lineno;
    };
    Marray(StringView) filenames;
    // current file we are parsing. When not parsing, it is the entry point.
    StringView filename;
    // Base directory. All filepaths are relative to this directory.
    // If it is the empty string, filepaths are unaltered.
    StringView base_directory;

    // Special nodes we need to track. Store them here
    // so we don't have to scan for them later.
    struct {
        Marray(NodeHandle) user_script_nodes;
        Marray(NodeHandle) imports;
        Marray(NodeHandle) stylesheets_nodes;
        Marray(NodeHandle) link_nodes;
        Marray(NodeHandle) script_nodes;
        Marray(NodeHandle) meta_nodes;
        // NOTE: we only grab these during parsing right now,
        // we don't add to them from user scripts.
        // These are used for speculatively pre-loading images.
        Marray(NodeHandle) img_nodes;
        Marray(NodeHandle) imglinks_nodes;
        NodeHandle titlenode;
        NodeHandle tocnode;
    };

    // file/source string cache.
    struct {
        FileCache* textcache;
        FileCache* b64cache;
        FileCache* jsb64cache; // For the rare cases that js is reading
                               // files as base64. Needs to be its own
                               // cache for thread safety.
    };
    Marray(StringView) dependencies;
    // Mapping of shorthand for a link to its actual link.
    StringTable links;
    // TODO: use an adaptive table (linear at small N, hashmap
    //       at large N).
    Marray(IdItem) explicit_node_ids;
    // If a toc block exists, this string holds the html fragment
    // that is the toc.
    // TODO: make this a string view?
    LongString renderedtoc;
    // See DndcFlags.
    uint64_t flags;
    // See dndc.h
    DndcLogFunc*_Nullable log_func;
    void*_Nullable log_user_data;

    // book keeping info for the ast api
    // -----

    // Whether the context and such are heap allocated.
    uint32_t heap_allocated: 1;
    uint32_t textcache_allocated: 1;
    uint32_t b64cache_allocated: 1;
};

static inline force_inline
Allocator
main_allocator(DndcContext* ctx){
    return allocator_from_arena(&ctx->main_arena);
}
static inline force_inline
Allocator
string_allocator(DndcContext* ctx){
    return allocator_from_arena(&ctx->string_arena);
}

static inline force_inline
Allocator
temp_allocator(DndcContext* ctx){
    return allocator_from_arena(&ctx->temp);
}

static inline
warn_unused
int
ctx_add_filename(DndcContext* ctx, StringView filename, int copy, uint32_t* result){
    for(size_t i = 0; i < ctx->filenames.count; i++){
        if(SV_equals(filename, ctx->filenames.data[i])){
            *result =  (uint32_t)i;
            return 0;
        }
    }
    if(unlikely(ctx->filenames.count >= UINT32_MAX))
        return DNDC_ERROR_OOM;
    if(copy && filename.length){
        filename.text = Allocator_dupe(string_allocator(ctx), filename.text, filename.length);
    }
    int err = Marray_push(StringView)(&ctx->filenames, main_allocator(ctx), filename);
    if(unlikely(err)) return DNDC_ERROR_OOM;
    *result = (uint32_t)(ctx->filenames.count-1);
    return 0;
}

typedef union DndcDependsArg DependsArg;
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
