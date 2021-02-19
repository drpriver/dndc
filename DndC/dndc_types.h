#ifndef DNDC_TYPES_H
#define DNDC_TYPES_H
#include "long_string.h"
// Type definitions for dndc, shared across components.

#define NODETYPES(apply) \
    apply(ROOT,           0) \
    apply(TEXT,           1) \
    apply(DIV,            2)\
    apply(STRING,         3)\
    apply(PARA,           4)\
    apply(TITLE,          5)\
    apply(HEADING,        6)\
    apply(TABLE,          7)\
    apply(TABLE_ROW,      8)\
    apply(STYLESHEETS,    9)\
    apply(DEPENDENCIES,   10)\
    apply(LINKS,          11)\
    apply(SCRIPTS,        12)\
    apply(IMPORT,         13)\
    apply(IMAGE,          14)\
    apply(BULLETS,        15)\
    apply(BULLET,         16)\
    apply(PYTHON,         17)\
    apply(RAW,            18)\
    apply(PRE,            19)\
    apply(LIST,           20)\
    apply(LIST_ITEM,      21)\
    apply(KEYVALUE,       22)\
    apply(KEYVALUEPAIR,   23)\
    apply(IMGLINKS,       24)\
    apply(NAV,            25)\
    apply(DATA,           26)\
    apply(COMMENT,        27)\
    apply(MD,             28)\
    apply(CONTAINER,      29)\
    apply(QUOTE,          30)\
    apply(INVALID,        31)\

typedef enum NodeType {
    #define X(a, b) NODE_##a = b,
    NODETYPES(X)
    #undef X
    } NodeType;

static const
LongString nodenames[] = {
    #define X(a, b) [NODE_##a] = LS(#a),
    NODETYPES(X)
    #undef X
    };
//
// These strings are how to refer to a kind of node from within a document.
// It is intentional that not all nodes have an alias. Many nodes can not
// be directly created.
//
static const
struct {
    StringView name;
    NodeType type;
} nodealiases[] = {
    {SV("md"),           NODE_MD},
    {SV("div"),          NODE_DIV},
    {SV("text"),         NODE_TEXT},
    {SV("import"),       NODE_IMPORT},
    {SV("python"),       NODE_PYTHON},
    {SV("title"),        NODE_TITLE},
    {SV("h"),            NODE_HEADING},
    {SV("table"),        NODE_TABLE},
    {SV("stylesheets"),  NODE_STYLESHEETS},
    {SV("dependencies"), NODE_DEPENDENCIES},
    {SV("links"),        NODE_LINKS},
    {SV("scripts"),      NODE_SCRIPTS},
    {SV("script"),       NODE_SCRIPTS},
    {SV("image"),        NODE_IMAGE},
    {SV("img"),          NODE_IMAGE},
    {SV("bullets"),      NODE_BULLETS},
    {SV("raw"),          NODE_RAW},
    {SV("pre"),          NODE_PRE},
    {SV("list"),         NODE_LIST},
    {SV("kv"),           NODE_KEYVALUE},
    {SV("comment"),      NODE_COMMENT},
    {SV("imglinks"),     NODE_IMGLINKS},
    {SV("nav"),          NODE_NAV},
    {SV("data"),         NODE_DATA},
    {SV("quote"),        NODE_QUOTE},
    };


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

_Static_assert(sizeof(Node) == 15*sizeof(size_t), "");
// Damn these are fat.
// As a huge number of nodes are string nodes, we need a different scheme
// for storing children attributes and classes.
_Static_assert(sizeof(Node) == 120, "");

#define MARRAY_T Node
#include "Marray.h"

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
    // Only set if an error has occurred.
    LongString error_message;
    // current file we are parsing. When not parsing, it is the entry point.
    StringView filename;

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
        Marray(LoadedSource) processed_binary_files;
        // Strings that are from strings generated by scripts.
        // We make and keep a copy because our nodes will point into this string.
        // TODO: since we use a recording allocator maybe we don't need this?
        Marray(LongString) loaded_strings;
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
    // See flags below.
    uint64_t flags;
} DndcContext;

#ifndef WINDOWS
FlagEnum DndcFlags {
    DNDC_FLAGS_NONE        = 0x0,
    // Don't error on bad links.
    DNDC_ALLOW_BAD_LINKS   = 0x001,
    // Don't report any non-fatal errors.
    DNDC_SUPPRESS_WARNINGS = 0x002,
    // Log stats during execution of timings and counts.
    DNDC_PRINT_STATS       = 0x004,
    // Log orphaned nodes.
    DNDC_REPORT_ORPHANS    = 0x008,
    // Don't execute python blocks.
    DNDC_NO_PYTHON         = 0x010,
    // The python interpreter and docparser types have already been initialized.
    DNDC_PYTHON_IS_INIT    = 0x020,
    // Print out the document tree
    DNDC_PRINT_TREE        = 0x040,
    // Print out all links and what they resolve to
    DNDC_PRINT_LINKS       = 0x080,
    // Don't spawn any worker threads. No parallelism.
    DNDC_NO_THREADS        = 0x100,
    // Don't write out the final result.
    DNDC_DONT_WRITE        = 0x200,
    // Don't cleanup allocations or anything
    DNDC_NO_CLEANUP        = 0x400,
    // The source_path argument is actually a string containing the data, not a path.
    DNDC_SOURCE_PATH_IS_DATA_NOT_PATH = 0x800,
    // Don't print errors to stderr
    DNDC_DONT_PRINT_ERRORS = 0x1000,
    // Don't bother isolating python from site
    // Greatly slows startup, but allows importing user-installed
    // libraries.
    DNDC_PYTHON_UNISOLATED = 0x2000,
};
#else
#define DNDC_FLAGS_NONE                   0x0000
#define DNDC_ALLOW_BAD_LINKS              0x0001
#define DNDC_SUPPRESS_WARNINGS            0x0002
#define DNDC_PRINT_STATS                  0x0004
#define DNDC_REPORT_ORPHANS               0x0008
#define DNDC_NO_PYTHON                    0x0010
#define DNDC_PYTHON_IS_INIT               0x0020
#define DNDC_PRINT_TREE                   0x0040
#define DNDC_PRINT_LINKS                  0x0080
#define DNDC_NO_THREADS                   0x0100
#define DNDC_DONT_WRITE                   0x0200
#define DNDC_NO_CLEANUP                   0x0400
#define DNDC_SOURCE_PATH_IS_DATA_NOT_PATH 0x0800
#define DNDC_DONT_PRINT_ERRORS            0x1000
#define DNDC_PYTHON_UNISOLATED            0x2000
#endif

#endif
