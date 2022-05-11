#ifndef DNDC_NODE_TYPES_H
#define DNDC_NODE_TYPES_H
#include "dndc_long_string.h"

#define NODETYPES(apply) \
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
    apply(INVALID,        28)\

enum NodeType{
    NODE_MD           =  0,
    NODE_DIV          =  1,
    NODE_STRING       =  2,
    NODE_PARA         =  3,
    NODE_TITLE        =  4,
    NODE_HEADING      =  5,
    NODE_TABLE        =  6,
    NODE_TABLE_ROW    =  7,
    NODE_STYLESHEETS  =  8,
    NODE_LINKS        =  9,
    NODE_SCRIPTS      = 10,
    NODE_IMPORT       = 11,
    NODE_IMAGE        = 12,
    NODE_BULLETS      = 13,
    NODE_RAW          = 14,
    NODE_PRE          = 15,
    NODE_LIST         = 16,
    NODE_LIST_ITEM    = 17,
    NODE_KEYVALUE     = 18,
    NODE_KEYVALUEPAIR = 19,
    NODE_IMGLINKS     = 20,
    NODE_TOC          = 21,
    NODE_COMMENT      = 22,
    NODE_CONTAINER    = 23,
    NODE_QUOTE        = 24,
    NODE_JS           = 25,
    NODE_DETAILS      = 26,
    NODE_META         = 27,
    NODE_INVALID      = 28,
};
typedef enum NodeType NodeType;

#define X(a, b) _Static_assert(NODE_##a == b, #a " != " #b);
NODETYPES(X)
#undef X

static const
LongString NODENAMES[] = {
    #define X(a, b) [NODE_##a] = LS(#a),
    NODETYPES(X)
    #undef X
};
//
// These strings are how to refer to a kind of node from within a document.
// It is intentional that not all nodes have an alias. Many nodes can not
// be directly created.
//
typedef struct NodeAlias NodeAlias;
struct NodeAlias {
    StringView name;
    NodeType type;
};

static const
NodeAlias
NODEALIASES[] = {
    {SV("md"),           NODE_MD},
    {SV("div"),          NODE_DIV},
    {SV("import"),       NODE_IMPORT},
    {SV("js"),           NODE_JS},
    {SV("title"),        NODE_TITLE},
    {SV("h"),            NODE_HEADING},
    {SV("table"),        NODE_TABLE},
    {SV("css"),          NODE_STYLESHEETS},
    {SV("links"),        NODE_LINKS},
    {SV("script"),       NODE_SCRIPTS},
    {SV("image"),        NODE_IMAGE},
    {SV("img"),          NODE_IMAGE},
    {SV("raw"),          NODE_RAW},
    {SV("pre"),          NODE_PRE},
    {SV("kv"),           NODE_KEYVALUE},
    {SV("comment"),      NODE_COMMENT},
    {SV("imglinks"),     NODE_IMGLINKS},
    {SV("nav"),          NODE_TOC},
    {SV("toc"),          NODE_TOC},
    {SV("quote"),        NODE_QUOTE},
    {SV("details"),      NODE_DETAILS},
    {SV("meta"),         NODE_META},
};

enum {RAW_NODE_JS_INDEX = 5 };
enum {RAW_NODE_SCRIPT_INDEX = 4 };
static const
StringView RAW_NODES[] = {
    SV("raw"),
    SV("comment"),
    SV("pre"),
    SV("css"),
    [RAW_NODE_SCRIPT_INDEX] = SV("script"),
    [RAW_NODE_JS_INDEX] = SV("js"),
    SV("meta"),
};
static const
StringViewUtf16 RAW_NODES_UTF16[] = {
    SV16("raw"),
    SV16("comment"),
    SV16("pre"),
    SV16("css"),
    [RAW_NODE_SCRIPT_INDEX] = SV16("script"),
    [RAW_NODE_JS_INDEX] = SV16("js"),
    SV16("meta"),
};

static const
StringView
NODETYPE_TO_NODE_ALIASES[NODE_INVALID+1] = {
     [NODE_MD]          = SV("md"),
     [NODE_DIV]         = SV("div"),
     [NODE_IMPORT]      = SV("import"),
     [NODE_TITLE]       = SV("title"),
     [NODE_HEADING]     = SV("h"),
     [NODE_TABLE]       = SV("table"),
     [NODE_STYLESHEETS] = SV("css"),
     [NODE_LINKS]       = SV("links"),
     [NODE_SCRIPTS]     = SV("script"),
     [NODE_IMAGE]       = SV("img"),
     [NODE_BULLETS]     = SV("bullets"),
     [NODE_RAW]         = SV("raw"),
     [NODE_PRE]         = SV("pre"),
     [NODE_KEYVALUE]    = SV("kv"),
     [NODE_COMMENT]     = SV("comment"),
     [NODE_IMGLINKS]    = SV("imglinks"),
     [NODE_TOC]         = SV("toc"),
     [NODE_QUOTE]       = SV("quote"),
     [NODE_JS]          = SV("js"),
     [NODE_DETAILS]     = SV("details"),
     [NODE_META]        = SV("meta"),
};

enum {DNDC_MAX_NODE_DEPTH=100};

#endif
