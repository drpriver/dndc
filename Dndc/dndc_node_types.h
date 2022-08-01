//
// Copyright © 2021-2022, David Priver
//
#ifndef DNDC_NODE_TYPES_H
#define DNDC_NODE_TYPES_H
#include "dndc_long_string.h"
#include "common_macros.h"

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
    apply(DEFLIST,        28)\
    apply(DEF,            29)\
    apply(INVALID,        30)\

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
    NODE_DEFLIST      = 28,
    NODE_DEF          = 29,
    NODE_INVALID      = 30, // maybe this should be 0.
};

typedef enum NodeType NodeType;

PushDiagnostic();
SuppressEnumCompare();
#define X(a, b) _Static_assert(NODE_##a == b, #a " != " #b);
NODETYPES(X)
#undef X
PopDiagnostic();

static const
LongString NODENAMES[] = {
    #define X(a, b) [NODE_##a] = LSINIT(#a),
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
    {SVINIT("md"),           NODE_MD},
    {SVINIT("div"),          NODE_DIV},
    {SVINIT("import"),       NODE_IMPORT},
    {SVINIT("js"),           NODE_JS},
    {SVINIT("title"),        NODE_TITLE},
    {SVINIT("h"),            NODE_HEADING},
    {SVINIT("table"),        NODE_TABLE},
    {SVINIT("css"),          NODE_STYLESHEETS},
    {SVINIT("links"),        NODE_LINKS},
    {SVINIT("script"),       NODE_SCRIPTS},
    {SVINIT("image"),        NODE_IMAGE},
    {SVINIT("img"),          NODE_IMAGE},
    {SVINIT("raw"),          NODE_RAW},
    {SVINIT("pre"),          NODE_PRE},
    {SVINIT("kv"),           NODE_KEYVALUE},
    {SVINIT("comment"),      NODE_COMMENT},
    {SVINIT("imglinks"),     NODE_IMGLINKS},
    {SVINIT("nav"),          NODE_TOC},
    {SVINIT("toc"),          NODE_TOC},
    {SVINIT("quote"),        NODE_QUOTE},
    {SVINIT("details"),      NODE_DETAILS},
    {SVINIT("meta"),         NODE_META},
    {SVINIT("dl"),           NODE_DEFLIST},
    {SVINIT("deflist"),      NODE_DEFLIST},
    {SVINIT("def"),          NODE_DEF},
};

enum {RAW_NODE_JS_INDEX = 5 };
enum {RAW_NODE_SCRIPT_INDEX = 4 };
static const
StringView RAW_NODES[] = {
    SVINIT("raw"),
    SVINIT("comment"),
    SVINIT("pre"),
    SVINIT("css"),
    [RAW_NODE_SCRIPT_INDEX] = SVINIT("script"),
    [RAW_NODE_JS_INDEX] = SVINIT("js"),
    SVINIT("meta"),
};
static const
StringViewUtf16 RAW_NODES_UTF16[] = {
    SV16INIT("raw"),
    SV16INIT("comment"),
    SV16INIT("pre"),
    SV16INIT("css"),
    [RAW_NODE_SCRIPT_INDEX] = SV16INIT("script"),
    [RAW_NODE_JS_INDEX] = SV16INIT("js"),
    SV16INIT("meta"),
};

static const
StringView
NODETYPE_TO_NODE_ALIASES[NODE_INVALID+1] = {
     [NODE_MD]          = SVINIT("md"),
     [NODE_DIV]         = SVINIT("div"),
     [NODE_IMPORT]      = SVINIT("import"),
     [NODE_TITLE]       = SVINIT("title"),
     [NODE_HEADING]     = SVINIT("h"),
     [NODE_TABLE]       = SVINIT("table"),
     [NODE_STYLESHEETS] = SVINIT("css"),
     [NODE_LINKS]       = SVINIT("links"),
     [NODE_SCRIPTS]     = SVINIT("script"),
     [NODE_IMAGE]       = SVINIT("img"),
     [NODE_BULLETS]     = SVINIT("bullets"),
     [NODE_RAW]         = SVINIT("raw"),
     [NODE_PRE]         = SVINIT("pre"),
     [NODE_KEYVALUE]    = SVINIT("kv"),
     [NODE_COMMENT]     = SVINIT("comment"),
     [NODE_IMGLINKS]    = SVINIT("imglinks"),
     [NODE_TOC]         = SVINIT("toc"),
     [NODE_QUOTE]       = SVINIT("quote"),
     [NODE_JS]          = SVINIT("js"),
     [NODE_DETAILS]     = SVINIT("details"),
     [NODE_META]        = SVINIT("meta"),
     [NODE_DEFLIST]     = SVINIT("deflist"),
     [NODE_DEF]         = SVINIT("def"),
};

enum {DNDC_MAX_NODE_DEPTH=100};

#endif
