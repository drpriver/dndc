#ifndef DNDC_TYPES_H
#define DNDC_TYPES_H
#include <stdint.h>
#include "dndc.h"
#include "long_string.h"
#include "dndc_node_types.h"
// Type definitions for dndc, shared across components.

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


// A cached loaded source file.
// This is used both for actual sourcefiles loaded from physical storage
// and for manufactured strings from python.
typedef struct LoadedSource {
    LongString sourcepath; // doesn't have to be a filename
    LongString sourcetext; // the actual source text
} LoadedSource;

#define MARRAY_T LoadedSource
#include "Marray.h"

typedef struct LinkItem {
    StringView key;
    StringView value;
} LinkItem;
#define MARRAY_T LinkItem
#include "Marray.h"

//
// For things that will go in the generated js data_blob.
//
typedef struct DataItem {
    StringView key;
    LongString value;
} DataItem;
#define MARRAY_T DataItem
#include "Marray.h"

typedef struct Attribute {
    StringView key;
    StringView value; // often null
} Attribute;
#define RARRAY_T Attribute
#include "Rarray.h"

//
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

//
// As 0 is the root node, we use this as the invalid value instead.
//
enum {INVALID_NODE_HANDLE_VALUE = (uint32_t)-1};
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
    unsigned filename_idx; // 4 bytes
    // Location of first character of where this node originated from.
    // These are 0-based. Functions that report errors add 1 to this number
    // for the general human-readable version.
    int row, col;                  // 4 + 4 bytes.
    // 4 bytes of padding in this struct
    // no padding in this struct
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
        if(i == node->children.count)
            return;
        node->inline_children[i]= node->inline_children[node->children.count];
        }
    }

#define NODE_CHILDREN_FOR_EACH(iter, n) for(NodeHandle *iter = node_children(n), *iter##end__=node_children(n)+n->children.count;iter != iter##end__;++iter)



struct DndcFileCache {
    Allocator allocator;
    Marray(LoadedSource) files;
};
typedef struct DndcFileCache FileCache;

static inline
void
FileCache_clear(FileCache* cache){
    Allocator al = cache->allocator;
    MARRAY_FOR_EACH(src, cache->files){
        Allocator_free(al, src->sourcepath.text, src->sourcepath.length+1);
        Allocator_free(al, src->sourcetext.text, src->sourcetext.length+1);
        }
    Marray_cleanup(LoadedSource)(&cache->files, al);
    }

static inline
int
FileCache_maybe_remove(FileCache* cache, StringView path){
    Allocator al = cache->allocator;
    for(size_t i = 0; i < cache->files.count; i++){
        auto src = cache->files.data[i];
        if(LS_SV_equals(src.sourcepath, path)){
            Marray_remove(LoadedSource)(&cache->files, i);
            Allocator_free(al, src.sourcepath.text, src.sourcepath.length+1);
            Allocator_free(al, src.sourcetext.text, src.sourcetext.length+1);
            return 1;
            }
        }
    return 0;
    }

static inline
bool
FileCache_has_file(FileCache* cache, StringView path){
    MARRAY_FOR_EACH(src, cache->files){
        if(LS_SV_equals(src->sourcepath, path)){
            return true;
            }
        }
    return false;
    }

typedef struct DndcContext {
    // The actual storage for all the nodes.
    Marray(Node) nodes;
    // Handle to the root node. Python blocks can change this.
    NodeHandle root_handle;
    // General purpose allocator.
    const Allocator  allocator;
    // Allocator for scratch allocations
    const Allocator  temp_allocator;
    const Allocator  string_allocator;
    // current parsing location
    struct {
        const char*_Nonnull cursor;
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
    LongString base_directory;

    // Special nodes we need to track. Store them here
    // so we don't have to scan for them later.
    struct {
        Marray(NodeHandle) user_script_nodes;
        Marray(NodeHandle) imports;
        Marray(NodeHandle) stylesheets_nodes;
        Marray(NodeHandle) dependencies_nodes;
        Marray(NodeHandle) link_nodes;
        Marray(NodeHandle) script_nodes;
        Marray(NodeHandle) data_nodes;
        // NOTE: we only grab these during parsing right now,
        // we don't add to them from user scripts.
        // These are used for speculatively pre-loading images.
        Marray(NodeHandle) img_nodes;
        Marray(NodeHandle) imglinks_nodes;
        NodeHandle titlenode;
        NodeHandle navnode;
    };

    // file/source string cache.
    struct {
        // Cached strings that are from loaded files.
        // We also copy the filename as we need those on our nodes.
        Marray(LoadedSource) builtin_files;
        FileCache textcache;
        FileCache b64cache;
    };
    Marray(StringView) dependencies;
    // Mapping of shorthand for a link to its actual link.
    // Actually an array of pairs, we sort this and then do binary searches
    // for lookup.
    Marray(LinkItem) links;
    // Mapping of key to string (will be outputted as "data_blob").
    // Not sure if we actually need this as the python scripting
    // is pretty powerful.
    // This made more sense when we didn't have internal scripting.
    Marray(DataItem) rendered_data;
    // If a nav block exists, this string holds the html fragment
    // that is the nav.
    LongString renderednav;
    // Where the output will go to.
    // Python gets read-only access to this.
    LongString outputfile;
    // See DndcFlags.
    uint64_t flags;
    // See dndc.h
    DndcErrorFunc*_Nullable error_func;
    void*_Nullable error_user_data;
    struct {
        StringView filename;
        int line; // 0-based
        int col; // 0-based
        LongString message;
    } error;
} DndcContext;

typedef union DndcDependsArg DependsArg;
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
