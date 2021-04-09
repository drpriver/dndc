#ifndef DNDC_TYPES_H
#define DNDC_TYPES_H
#include "dndc.h"
#include "long_string.h"
#include "dndc_flags.h"
#include "dndc_node_types.h"
// Type definitions for dndc, shared across components.

//
// Janky pseudo-template shenanigans
//

#define MARRAY_T StringView
#include "Marray.h"

#define MARRAY_T LongString
#include "Marray.h"

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
#define MARRAY_T Attribute
#include "Marray.h"

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
typedef union NodeHandle {
    struct { uint32_t index; };
    uint32_t _value;
} NodeHandle;

//
// As 0 is the root node, we use this as the invalid value instead.
//
#define INVALID_NODE_HANDLE ((NodeHandle){._value=-1})

static inline
force_inline
bool
NodeHandle_eq(NodeHandle a, NodeHandle b){
    return a._value == b._value;
    }

#define MARRAY_T NodeHandle
#include "Marray.h"

// OBSERVATION:
//      Most nodes do not have attributes
//      A large number of nodes are string nodes.
//      We could get some gains from redesigning this.
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
    Marray(NodeHandle) children;   // 24 bytes
    Marray(Attribute) attributes;  // 24 bytes
    Marray(StringView) classes;    // 24 bytes
    // Source filename (used for reporting errors)
    StringView filename;           // 16 bytes
    // Location of first character of where this node originated from.
    // These are 0-based. Functions that report errors add 1 to this number
    // for the general human-readable version.
    int row, col;                  // 4 + 4 bytes.
    // no padding in this struct
}Node;


#if UINTPTR_MAX != 0xFFFFFFFF
_Static_assert(sizeof(Node) == 15*sizeof(size_t), "");
// Damn these are fat.
// As a huge number of nodes are string nodes, we need a different scheme
// for storing children attributes and classes.
_Static_assert(sizeof(Node) == 120, "");
#endif

#define MARRAY_T Node
#include "Marray.h"

typedef struct FileCache {
    const Allocator allocator;
    Marray(LoadedSource) files;
} FileCache;

typedef struct DndcContext {
    // The actual storage for all the nodes.
    Marray(Node) nodes;
    // Handle to the root node. Python blocks can change this.
    NodeHandle root_handle;
    // General purpose allocator.
    const Allocator  allocator;
    // Allocator for scratch allocations
    const Allocator  temp_allocator;
    // current parsing location
    struct {
        const char*_Nonnull cursor;
        const char*_Null_unspecified linestart;
        const char*_Nullable doublecolon;
        const char*_Null_unspecified lineend;
        int nspaces;
        int lineno;
    };
    // current file we are parsing. When not parsing, it is the entry point.
    StringView filename;
    // Base directory. All filepaths are relative to this directory.
    // If it is the empty string, filepaths are unaltered.
    StringView base_directory;

    // Special nodes we need to track. Store them here
    // so we don't have to scan for them later.
    struct {
        Marray(NodeHandle) python_nodes;
        Marray(NodeHandle) imports;
        Marray(NodeHandle) stylesheets_nodes;
        Marray(NodeHandle) dependencies_nodes;
        Marray(NodeHandle) link_nodes;
        Marray(NodeHandle) script_nodes;
        Marray(NodeHandle) data_nodes;
        // TODO: we only grab these during parsing right now,
        // we don't add to them from user scripts.
        Marray(NodeHandle) img_nodes;
        Marray(NodeHandle) imglinks_nodes;
        NodeHandle titlenode;
        NodeHandle navnode;
    };

    // file/source string cache.
    struct {
        // Cached strings that are from loaded files.
        // We also copy the filename as we need those on our nodes.
        Marray(LoadedSource) loaded_files;
        Marray(LoadedSource) builtin_files;
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
    Nullable(ErrorFunc*) error_func;
    Nullable(void*) error_user_data;
    struct {
        StringView filename;
        int line; // 0-based
        int col; // 0-based
        LongString message;
    } error;
} DndcContext;

typedef union DependsArg {
    LongString path;
    struct {
        void (*_Nullable callback)(void*_Nullable, StringView);
        void*_Nullable user_data;
    };
}DependsArg;

#endif
