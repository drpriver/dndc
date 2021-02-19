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

static const LongString nodenames[] = {
#define X(a, b) [NODE_##a] = LS(#a),
    NODETYPES(X)
#undef X
    };
static const struct {
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



#define MARRAY_T StringView
#include "Marray.h"
#define MARRAY_T LongString
#include "Marray.h"

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

typedef union NodeHandle {
    struct{uint32_t index; };
    uint32_t _value;
    } NodeHandle;

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
    NodeType type;
    NodeHandle parent; // 0 is the root node, who will have itself as its own parent
    StringView header;
    Marray(NodeHandle) children;
    Marray(Attribute) attributes;
    Marray(StringView) classes;
    StringView filename;
    int row, col; // 0-based
}Node;
_Static_assert(sizeof(Node) == 15*sizeof(size_t), "");
// Damn these are fat.
_Static_assert(sizeof(Node) == 120, "");
#define MARRAY_T Node
#include "Marray.h"

typedef struct ParseContext {
    Marray(Node) nodes;
    NodeHandle root_handle;
    const Allocator  allocator;
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
    struct {
        // file/source string cache.

        // Cached strings that are from loaded files.
        // We also copy the filename as we need those on our nodes.
        Marray(LoadedSource) loaded_files;
        Marray(LoadedSource) processed_binary_files;
        // strings that are from strings generated by scripts.
        // We make and keep a copy because our nodes will point into this string.
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
    LongString renderednav;
    LongString outputfile;
    uint64_t flags;
} ParseContext;

#ifndef WINDOWS
FlagEnum ParseFlags {
    PARSE_FLAGS_NONE        = 0x0,
    // Don't error on bad links.
    PARSE_ALLOW_BAD_LINKS   = 0x001,
    // Don't report any non-fatal errors.
    PARSE_SUPPRESS_WARNINGS = 0x002,
    // Log stats during execution of timings and counts.
    PARSE_PRINT_STATS       = 0x004,
    // Log orphaned nodes.
    PARSE_REPORT_ORPHANS    = 0x008,
    // Don't execute python blocks.
    PARSE_NO_PYTHON         = 0x010,
    // The python interpreter and docparser types have already been initialized.
    PARSE_PYTHON_IS_INIT    = 0x020,
    // Print out the document tree
    PARSE_PRINT_TREE        = 0x040,
    // Print out all links and what they resolve to
    PARSE_PRINT_LINKS       = 0x080,
    // Don't spawn any worker threads. No parallelism.
    PARSE_NO_THREADS        = 0x100,
    // Don't write out the final result.
    PARSE_DONT_WRITE        = 0x200,
    // Don't cleanup allocations or anything
    PARSE_NO_CLEANUP        = 0x400,
    // The source_path argument is actually a string containing the data, not a path.
    PARSE_SOURCE_PATH_IS_DATA_NOT_PATH = 0x800,
    // Don't print errors to stderr
    PARSE_DONT_PRINT_ERRORS = 0x1000,
    // Don't bother isolating python from site
    // Greatly slows startup, but allows importing user-installed
    // libraries.
    PARSE_PYTHON_UNISOLATED = 0x2000,
};
#else
#define PARSE_FLAGS_NONE                   0x0000
#define PARSE_ALLOW_BAD_LINKS              0x0001
#define PARSE_SUPPRESS_WARNINGS            0x0002
#define PARSE_PRINT_STATS                  0x0004
#define PARSE_REPORT_ORPHANS               0x0008
#define PARSE_NO_PYTHON                    0x0010
#define PARSE_PYTHON_IS_INIT               0x0020
#define PARSE_PRINT_TREE                   0x0040
#define PARSE_PRINT_LINKS                  0x0080
#define PARSE_NO_THREADS                   0x0100
#define PARSE_DONT_WRITE                   0x0200
#define PARSE_NO_CLEANUP                   0x0400
#define PARSE_SOURCE_PATH_IS_DATA_NOT_PATH 0x0800
#define PARSE_DONT_PRINT_ERRORS            0x1000
#define PARSE_PYTHON_UNISOLATED            0x2000
#endif

#endif
