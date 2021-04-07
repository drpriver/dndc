#ifndef DNDC_NODE_TYPES_H
#define DNDC_NODE_TYPES_H
#include "long_string.h"

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
    apply(PYTHON,         16)\
    apply(RAW,            17)\
    apply(PRE,            18)\
    apply(LIST,           19)\
    apply(LIST_ITEM,      20)\
    apply(KEYVALUE,       21)\
    apply(KEYVALUEPAIR,   22)\
    apply(IMGLINKS,       23)\
    apply(NAV,            24)\
    apply(DATA,           25)\
    apply(COMMENT,        26)\
    apply(MD,             27)\
    apply(CONTAINER,      28)\
    apply(QUOTE,          29)\
    apply(INVALID,        30)\

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
};

static const
StringView raw_nodes[] = {
    SV("python"),
    SV("raw"),
    SV("comment"),
    SV("pre"),
};
static const
StringViewUtf16 raw_nodes_utf16[] = {
    {.text=u"python", .length=6},
    {.text=u"raw", .length=3},
    {.text=u"comment", .length=7},
    {.text=u"pre", .length=3},
};

static const
StringView
nodetype_to_node_aliases[NODE_INVALID+1] = {
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
};

#endif
