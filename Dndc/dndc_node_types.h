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
    apply(NAV,            21)\
    apply(DATA,           22)\
    apply(COMMENT,        23)\
    apply(TEXT,           24)\
    apply(CONTAINER,      25)\
    apply(QUOTE,          26)\
    apply(HR,             27)\
    apply(JS,             28)\
    apply(DETAILS,        29)\
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
    {SV("nav"),          NODE_NAV},
    {SV("data"),         NODE_DATA},
    {SV("quote"),        NODE_QUOTE},
    {SV("hr"),           NODE_HR},
    {SV("details"),      NODE_DETAILS},
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
};
static const
StringViewUtf16 RAW_NODES_UTF16[] = {
    SV16("raw"),
    SV16("comment"),
    SV16("pre"),
    SV16("css"),
    [RAW_NODE_SCRIPT_INDEX] = SV16("script"),
    [RAW_NODE_JS_INDEX] = SV16("js"),
};

static const
StringView
NODETYPE_TO_NODE_ALIASES[NODE_INVALID+1] = {
     [NODE_MD]           = SV("md"),
     [NODE_DIV]          = SV("div"),
     [NODE_TEXT]         = SV("text"),
     [NODE_IMPORT]       = SV("import"),
     [NODE_TITLE]        = SV("title"),
     [NODE_HEADING]      = SV("h"),
     [NODE_TABLE]        = SV("table"),
     [NODE_STYLESHEETS]  = SV("css"),
     [NODE_LINKS]        = SV("links"),
     [NODE_SCRIPTS]      = SV("script"),
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
     [NODE_JS]           = SV("js"),
     [NODE_DETAILS]      = SV("details"),
};

#endif
