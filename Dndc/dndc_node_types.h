//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#ifndef DNDC_NODE_TYPES_H
#define DNDC_NODE_TYPES_H
#include "dndc_long_string.h"
#include "common_macros.h"

#define NODETYPES(apply) \
    apply(INVALID,         0)\
    apply(MD,              1)\
    apply(DIV,             2)\
    apply(STRING,          3)\
    apply(PARA,            4)\
    apply(TITLE,           5)\
    apply(HEADING,         6)\
    apply(TABLE,           7)\
    apply(TABLE_ROW,       8)\
    apply(STYLESHEETS,     9)\
    apply(LINKS,          10)\
    apply(SCRIPTS,        11)\
    apply(IMPORT,         12)\
    apply(IMAGE,          13)\
    apply(BULLETS,        14)\
    apply(RAW,            15)\
    apply(PRE,            16)\
    apply(LIST,           17)\
    apply(LIST_ITEM,      18)\
    apply(KEYVALUE,       19)\
    apply(KEYVALUEPAIR,   20)\
    apply(IMGLINKS,       21)\
    apply(TOC,            22)\
    apply(COMMENT,        23)\
    apply(CONTAINER,      24)\
    apply(QUOTE,          25)\
    apply(JS,             26)\
    apply(DETAILS,        27)\
    apply(META,           28)\
    apply(DEFLIST,        29)\
    apply(DEF,            30)\
    apply(HEAD,           31)\
    apply(SHEBANG,        32)\

enum NodeType{
    NODE_INVALID      =  0,
    NODE_MD           =  1,
    NODE_DIV          =  2,
    NODE_STRING       =  3,
    NODE_PARA         =  4,
    NODE_TITLE        =  5,
    NODE_HEADING      =  6,
    NODE_TABLE        =  7,
    NODE_TABLE_ROW    =  8,
    NODE_STYLESHEETS  =  9,
    NODE_LINKS        = 10,
    NODE_SCRIPTS      = 11,
    NODE_IMPORT       = 12,
    NODE_IMAGE        = 13,
    NODE_BULLETS      = 14,
    NODE_RAW          = 15,
    NODE_PRE          = 16,
    NODE_LIST         = 17,
    NODE_LIST_ITEM    = 18,
    NODE_KEYVALUE     = 19,
    NODE_KEYVALUEPAIR = 20,
    NODE_IMGLINKS     = 21,
    NODE_TOC          = 22,
    NODE_COMMENT      = 23,
    NODE_CONTAINER    = 24,
    NODE_QUOTE        = 25,
    NODE_JS           = 26,
    NODE_DETAILS      = 27,
    NODE_META         = 28,
    NODE_DEFLIST      = 29,
    NODE_DEF          = 30,
    NODE_HEAD         = 31,
    NODE_SHEBANG      = 32,
};
enum {NODE_MAX = NODE_SHEBANG+1};

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
    {SVINIT("head"),         NODE_HEAD},
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
    SVINIT("head"),
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
    SV16INIT("head"),
};

static const
StringView
NODETYPE_TO_NODE_ALIASES[NODE_MAX] = {
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
     [NODE_HEAD]        = SVINIT("head"),
};

enum {DNDC_MAX_NODE_DEPTH=100};

#endif
