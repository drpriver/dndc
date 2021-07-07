#ifndef DNDC_NODE_TYPES_H
#define DNDC_NODE_TYPES_H
#include "long_string.h"

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
    apply(DEPENDENCIES,    9)\
    apply(LINKS,          10)\
    apply(SCRIPTS,        11)\
    apply(IMPORT,         12)\
    apply(IMAGE,          13)\
    apply(BULLETS,        14)\
    apply(PYTHON,         15)\
    apply(RAW,            16)\
    apply(PRE,            17)\
    apply(LIST,           18)\
    apply(LIST_ITEM,      19)\
    apply(KEYVALUE,       20)\
    apply(KEYVALUEPAIR,   21)\
    apply(IMGLINKS,       22)\
    apply(NAV,            23)\
    apply(DATA,           24)\
    apply(COMMENT,        25)\
    apply(TEXT,           26) \
    apply(CONTAINER,      27)\
    apply(QUOTE,          28)\
    apply(HR,             29)\
    apply(INVALID,        30)\

typedef enum NodeType {
    #define X(a, b) NODE_##a = b,
    NODETYPES(X)
    #undef X
} NodeType;

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
static const
struct {
    StringView name;
    NodeType type;
} NODEALIASES[] = {
    {SV("md"),           NODE_MD},
    {SV("div"),          NODE_DIV},
    {SV("text"),         NODE_TEXT},
    {SV("import"),       NODE_IMPORT},
    {SV("python"),       NODE_PYTHON},
    {SV("title"),        NODE_TITLE},
    {SV("h"),            NODE_HEADING},
    {SV("table"),        NODE_TABLE},
    {SV("css"),          NODE_STYLESHEETS},
    {SV("dependencies"), NODE_DEPENDENCIES},
    {SV("links"),        NODE_LINKS},
    {SV("js"),           NODE_SCRIPTS},
    {SV("image"),        NODE_IMAGE},
    {SV("img"),          NODE_IMAGE},
    {SV("raw"),          NODE_RAW},
    {SV("pre"),          NODE_PRE},
    {SV("kv"),           NODE_KEYVALUE},
    {SV("comment"),      NODE_COMMENT},
    {SV("imglinks"),     NODE_IMGLINKS},
    {SV("nav"),          NODE_NAV},
    {SV("data"),         NODE_DATA},
    {SV("quote"),        NODE_QUOTE},
    {SV("hr"),           NODE_HR},
};

static const
StringView RAW_NODES[] = {
    SV("python"),
    SV("raw"),
    SV("comment"),
    SV("pre"),
};
static const
StringViewUtf16 RAW_NODES_UTF16[] = {
    {.text=u"python", .length=6},
    {.text=u"raw", .length=3},
    {.text=u"comment", .length=7},
    {.text=u"pre", .length=3},
};

static const
StringView
NODETYPE_TO_NODE_ALIASES[NODE_INVALID+1] = {
     [NODE_MD]           = SV("md"),
     [NODE_DIV]          = SV("div"),
     [NODE_TEXT]         = SV("text"),
     [NODE_IMPORT]       = SV("import"),
     [NODE_PYTHON]       = SV("python"),
     [NODE_TITLE]        = SV("title"),
     [NODE_HEADING]      = SV("h"),
     [NODE_TABLE]        = SV("table"),
     [NODE_STYLESHEETS]  = SV("css"),
     [NODE_DEPENDENCIES] = SV("dependencies"),
     [NODE_LINKS]        = SV("links"),
     [NODE_SCRIPTS]      = SV("js"),
     [NODE_IMAGE]        = SV("img"),
     [NODE_BULLETS]      = SV("bullets"),
     [NODE_RAW]          = SV("raw"),
     [NODE_PRE]          = SV("pre"),
     [NODE_KEYVALUE]     = SV("kv"),
     [NODE_COMMENT]      = SV("comment"),
     [NODE_IMGLINKS]     = SV("imglinks"),
     [NODE_NAV]          = SV("nav"),
     [NODE_DATA]         = SV("data"),
     [NODE_QUOTE]        = SV("quote"),
     [NODE_HR]           = SV("hr"),
};

#endif
