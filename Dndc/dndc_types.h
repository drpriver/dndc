#ifndef DNDC_TYPES_H
#define DNDC_TYPES_H
#include <stdint.h>
#include "dndc.h"
#include "long_string.h"
#include "common_macros.h"
#include "dndc_node_types.h"
#include "dndc_file_cache.h"
#include "allocator.h"
#include "arena_allocator.h"
#include "linear_allocator.h"
#include "string_table.h"

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
#include "Marray.h"

#define MARRAY_T LongString
#include "Marray.h"

#define RARRAY_T StringView
#include "Rarray.h"


typedef struct Attribute {
    StringView key;
    StringView value; // often null
} Attribute;
#define RARRAY_T Attribute
#include "Rarray.h"

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
const NodeHandle INVALID_NODE_HANDLE = {._value=INVALID_NODE_HANDLE_VALUE};
// Shadow the above symbol intentionally.
#define INVALID_NODE_HANDLE ((NodeHandle){._value=INVALID_NODE_HANDLE_VALUE})

static inline
force_inline
bool
NodeHandle_eq(NodeHandle a, NodeHandle b){
    return a._value == b._value;
}

#define MARRAY_T NodeHandle
#include "Marray.h"

// For tracking what's the id of a node.
typedef struct IdItem {
    NodeHandle node;
    // text needs to be kebabed before use.
    StringView text;
} IdItem;
#define MARRAY_T IdItem
#include "Marray.h"

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
typedef struct Node {
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
    union {
        Marray(NodeHandle) children;   // 24 bytes
        struct {
            size_t children_count;
            NodeHandle inline_children[4];
        };
    };
    Rarray(Attribute)*_Nullable attributes;  // 8 bytes
    Rarray(StringView)*_Nullable classes;    // 8 bytes
    // Source filename (used for reporting errors)
    uint32_t filename_idx; // 4 bytes
    // Location of first character of where this node originated from.
    // These are 0-based. Functions that report errors add 1 to this number
    // for the general human-readable version.
    int row, col;         // 4 + 4 bytes.
    NodeFlags flags; // 4 bytes
} Node;

#if UINTPTR_MAX != 0xFFFFFFFF
_Static_assert(sizeof(Node) == 10*sizeof(size_t), "");
// Damn these are fat.
// As a huge number of nodes are string nodes, we need a different scheme
// for storing children attributes and classes.
_Static_assert(sizeof(Node) == 80, "");
#endif

#define MARRAY_T Node
#include "Marray.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static inline
force_inline
NodeHandle*
node_children(Node* node){
    if(node->children.count > 4)
        return node->children.data;
    return node->inline_children;
}
static inline
force_inline
size_t
node_children_count(Node* node){
    return node->children.count;
}

static inline
force_inline
void
node_remove_child(Node* node, size_t i, const Allocator a){
    assert(i < node->children.count);
    if(node->children.count > 4){
        Marray_remove__NodeHandle(&node->children, i);
        if(node->children.count <= 4){
            NodeHandle children[4];
            memcpy(children, node->children.data, sizeof(children));
            Allocator_free(a, node->children.data, node->children.capacity*sizeof(NodeHandle));
            memcpy(node->inline_children, children, sizeof(children));
        }
    }
    else {
        node->children.count--;
        // early out for last one.
        if(i == node->children.count)
            return;
        // Need to preserve order of children.
        size_t n_moved = node->children.count - i;
        size_t size_moved = sizeof(node->inline_children[0]) * n_moved;
        NodeHandle* hole = &node->inline_children[i];
        memmove(hole, hole+1, size_moved);
    }
}


#define NODE_CHILDREN_FOR_EACH(iter, n) for(NodeHandle *iter = node_children(n), *iter##end__=node_children(n)+node_children_count(n);iter != iter##end__;++iter)

typedef struct DndcContext {
    // The actual storage for all the nodes.
    Marray(Node) nodes;
    // Handle to the root node. Scripts can change this.
    NodeHandle root_handle;

    // General purpose allocator.
    ArenaAllocator main_arena;
    // Allocator for strings (strings are almost never freed)
    ArenaAllocator string_arena;
    // Allocator for scratch allocations
    LinearAllocator temp;

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
} DndcContext;

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
    return allocator_from_la(&ctx->temp);
}




typedef union DndcDependsArg DependsArg;
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
