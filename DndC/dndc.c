#define DOCPARSER_VERSION "0.1.0"
#ifdef LOG_LEVEL
#undef LOG_LEVEL
#endif
#define LOG_LEVEL LOG_LEVEL_INFO
#include <stdarg.h>
#include "file_util.h"
#include "str_util.h"
#include "path_util.h"
#include "linear_allocator.h"
#include "long_string.h"
// #include "init_python.h"
#include "MStringBuilder.h"
#include "frozenstdlib.h"
#include "measure_time.h"
#include "argument_parsing.h"
#include "base64.h"
#include "json_util.h"
#include "recording_allocator.h"

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

typedef struct LoadedBin {
    LongString path; // doesn't have to be a filename
    ByteBuffer bytes;
    } LoadedBin;
#define MARRAY_T LoadedBin
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


//TODO: move to a thread utils?
#if defined(LINUX) || defined(DARWIN)
#include <pthread.h>
// opaque thread type to ease porting to Windows later.
typedef struct ThreadHandle {
    pthread_t thread;
}ThreadHandle;

typedef Nullable(void*) (thread_func)(Nullable(void*));
static
void
create_thread(Nonnull(ThreadHandle*)handle, Nonnull(thread_func*) func, Nullable(void*)thread_arg){
    pthread_create(&handle->thread, NULL, func, thread_arg);
    }

static
void
join_thread(ThreadHandle handle, Nullable(void*)result){
    pthread_join(handle.thread, result);
    }
#elif defined(WINDOWS)
#error "didn't actually do windows"

#else
#error "Unhandled threading platform."
#endif


static
Errorable_f(LongString)
read_and_base64_bin_file(Nonnull(ByteBuilder*)bb, Nonnull(const Allocator*)a, Nonnull(const char*) filepath){
    Errorable(LongString) result = {};
    assert(bb->allocator);
    assert(bb->cursor == 0);
#ifdef USE_C_STDIO
    auto fp = fopen(filepath, "rb");
    if(not fp)
        Raise(FILE_NOT_OPENED);
    auto size_e = file_size_from_fp(fp);
    if(size_e.errored){
        fclose(fp);
        Raise(FILE_ERROR);
        }
    auto nbytes = unwrap(size_e);
    bb_reserve(bb, nbytes);
    auto fread_result = fread(bb->data, 1, nbytes, fp);
    fclose(fp);
    if(fread_result != nbytes){
        Raise(FILE_ERROR);
        }
#else
    int fd = open(filepath, O_RDONLY);
    if(fd < 0)
        Raise(FILE_NOT_OPENED);
    auto size_e = file_size_from_fd(fd);
    if(size_e.errored){
        close(fd);
        Raise(FILE_ERROR);
        }
    auto nbytes = unwrap(size_e);
    bb_reserve(bb, nbytes);
    auto read_result = read(fd, bb->data, nbytes);
    close(fd);
    if(read_result != nbytes){
        Raise(FILE_ERROR);
        }
#endif
    bb->cursor = nbytes;
    auto buff = bb_borrow(bb);
    MStringBuilder sb = {};
    msb_write_b64(&sb, a, buff.buff, buff.n_bytes);
    result.result = msb_detach(&sb, a);
    bb_reset(bb);
    return result;
    }

typedef struct BinaryJob{
    Marray(StringView) sourcepaths;
    Nonnull(const Allocator*) a;
    Marray(LoadedSource) loaded;
    bool report_time;
    } BinaryJob;

static
Nullable(void*)
binary_worker(Nonnull(void*) j){
    auto before = get_t();
    BinaryJob* jobp = j;
    auto job = *jobp;
    auto count = job.sourcepaths.count;
    auto data = job.sourcepaths.data;
    MStringBuilder sb = {};
    ByteBuilder bb = {};
    bb.allocator = job.a;
    for(size_t i = 0; i < count; i++){
        auto sv = data[i];
        msb_write_str(&sb, job.a, sv.text, sv.length);
        msb_nul_terminate(&sb, job.a);
        auto path = msb_borrow(&sb, job.a);
        auto e = read_and_base64_bin_file(&bb, job.a, path.text);
        if(e.errored){
            // We'll let the renderer report the error when it tries
            // to load it.
            msb_reset(&sb);
            continue;
            }
        auto s = Marray_alloc(LoadedSource)(&job.loaded, job.a);
        s->sourcepath = msb_detach(&sb, job.a);
        s->sourcetext = unwrap(e);
        }
    msb_destroy(&sb, job.a);
    bb_destroy(&bb);
    jobp->loaded = job.loaded;
    auto after = get_t();
    if(job.report_time)
        fprintf(stderr, "Info: Binary worker took %.3fms\n", (after-before)/1000.);
    return NULL;
    }


static inline
int
msb_write_kebab(Nonnull(MStringBuilder*)msb, Nonnull(const Allocator*)a, Nonnull(const char*)text, size_t length){
    int n_written = 0;
    bool want_write_hyphen = false;
    for(size_t i = 0; i < length; i++){
        char c = text[i];
        switch(c){
            case 'A' ... 'Z':
                c |= 0x20; // tolower
                // fall-through
            case 'a' ... 'z':
            case '0' ... '9':
                if(want_write_hyphen){
                    msb_write_char(msb, a, '-');
                    want_write_hyphen = false;
                    }
                msb_write_char(msb, a, c);
                n_written += 1;
                continue;
            case ' ': case '\t': case '-':
                if(n_written)
                    want_write_hyphen = true;
                continue;
            default:
                continue;
            }
        }
    return n_written;
    }

static inline
void
msb_write_title(Nonnull(MStringBuilder*) restrict msb, Nonnull(const Allocator*)a, Nonnull(const char*) restrict str, size_t len){
    if(not len)
        return;
    _check_msb_size(msb, a, len);
    bool wants_cap = true;
    for(size_t i = 0; i < len; i++){
        char c = str[i];
        switch(c){
            case 'a' ... 'z':
                if(wants_cap){
                    c &= ~0x20;
                    wants_cap = false;
                    }
                break;
            case 'A' ... 'Z':
                wants_cap = false;
                break;
            default:
                c = ' ';
                wants_cap = true;
                break;
            }
        msb->data[msb->cursor++] = c;
        }
    }
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


// OBSERVATION:
//      Most nodes do not have attributes
//      A large number of nodes are string nodes.
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
_Static_assert(sizeof(Node) == 120, "");
#define MARRAY_T Node
#include "Marray.h"

#if 0
// I believe this design would be faster and more efficient,
// but my profiling shows that we spend all of our time in python + outputting
// to disk. So yeah, it's low priority over just adding features.
//
// Alternative Node design
typedef struct SourceLocation {
    Nonnull(const char*) filename;
    int row, col;
    } SourceLocation;
typedef struct Node {
    NodeType type;
    uint32_t header_length;
    NullUnspec(const char*) header_text;
    NodeHandle parent; // can we pack this into 4 bytes?
    uint32_t children; // index into children array, handle
    uint32_t attributes; // index into attributes array, handle
    uint32_t classes; // index into classes array, handle
    uint32_t source_location; // these are rarely neded, so afford the level
    } Node;
_Static_assert(sizeof(Node) == 36, "");
// unfortunately, an 8 byte alignment means an array of these takes 40 bytes per.
_Static_assert(_Alignof(Node) == 8, "");
// Don't bother sharing children or attributes or whatever.
// 0 means unallocated. (leak the first one)

// Yet another design:
// Everything is parallel arrays
// Everything is getters/setters.
#endif

static inline
bool
node_has_attribute(Nonnull(const Node*) node, StringView attr){
    // TODO: maybe use a dict? Idk how many attributes we actually use.
    // Maybe if count is greater than some N we sort and do a binary search?
    auto attrs = node->attributes.data;
    auto count = node->attributes.count;
    for(size_t i = 0; i < count; i++){
        if(SV_equals(attrs[i].key, attr))
            return true;
        }
    return false;
    }
static inline
Nullable(StringView*)
node_get_attribute(Nonnull(const Node*) node, StringView attr){
    // TODO: maybe use a dict? Idk how many attributes we actually use.
    // Maybe if count is greater than some N we sort and do a binary search?
    auto attrs = node->attributes.data;
    auto count = node->attributes.count;
    for(size_t i = 0; i < count; i++){
        if(SV_equals(attrs[i].key, attr))
            return &attrs[i].value;
        }
    return NULL;
    }


typedef struct ParseContext {
    Marray(Node) nodes;
    NodeHandle root_handle;
    Nonnull(const Allocator*) allocator;
    Nonnull(const Allocator*) temp_allocator;
    // current parsing locating
    struct {
        const char*_Nonnull cursor;
        const char*_Null_unspecified linestart;
        const char*_Nullable doublecolon;
        const char*_Null_unspecified lineend;
        int nspaces;
        int lineno;
    };
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
        Marray(LoadedBin) loaded_binary_files;
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
#endif

printf_func(3, 4)
static
void
set_err(Nonnull(ParseContext*)ctx, NullUnspec(const char*) errchar, Nonnull(const char*) fmt, ...){
    MStringBuilder msb = {};
    int col = (int)(errchar - ctx->linestart);
    msb_sprintf(&msb, ctx->allocator, "%.*s:%d:%d: ", (int)ctx->filename.length, ctx->filename.text, ctx->lineno+1, col+1);
    va_list args;
    va_start(args, fmt);
    msb_vsprintf(&msb, ctx->allocator, fmt, args);
    va_end(args);
    ctx->error_message = msb_detach(&msb, ctx->allocator);
    }

printf_func(3, 4)
static
void
node_set_err(Nonnull(ParseContext*)ctx, Nonnull(const Node*)node, Nonnull(const char*) fmt, ...){
    MStringBuilder msb = {};
    auto filename = node->filename;
    auto lineno = node->row;
    int col = node->col;
    msb_sprintf(&msb, ctx->allocator, "%.*s:%d:%d: ", (int)filename.length, filename.text, lineno+1, col+1);
    va_list args;
    va_start(args, fmt);
    msb_vsprintf(&msb, ctx->allocator, fmt, args);
    va_end(args);
    ctx->error_message = msb_detach(&msb, ctx->allocator);
    }

printf_func(3, 4)
static
void
node_print_err(Nonnull(ParseContext*)ctx, Nonnull(const Node*)node, Nonnull(const char*) fmt, ...){
    if(ctx->flags & PARSE_DONT_PRINT_ERRORS)
        return;
    MStringBuilder msb = {};
    auto filename = node->filename;
    auto lineno = node->row;
    int col = node->col;
    msb_sprintf(&msb, ctx->temp_allocator, "%.*s:%d:%d: ", (int)filename.length, filename.text, lineno+1, col+1);
    va_list args;
    va_start(args, fmt);
    msb_vsprintf(&msb, ctx->temp_allocator, fmt, args);
    va_end(args);
    auto msg = msb_borrow(&msb, ctx->temp_allocator);
    fprintf(stderr, "%s\n", msg.text);
    msb_destroy(&msb, ctx->temp_allocator);
    }

printf_func(3, 4)
static
void
node_print_warning(Nonnull(ParseContext*)ctx, Nonnull(const Node*)node, Nonnull(const char*) fmt, ...){
    if(ctx->flags & PARSE_SUPPRESS_WARNINGS)
        return;
    if(ctx->flags & PARSE_DONT_PRINT_ERRORS)
        return;
    MStringBuilder msb = {};
    auto filename = node->filename;
    auto lineno = node->row;
    int col = node->col;
    msb_sprintf(&msb, ctx->temp_allocator, "%.*s:%d:%d: ", (int)filename.length, filename.text, lineno+1, col+1);
    va_list args;
    va_start(args, fmt);
    msb_vsprintf(&msb, ctx->temp_allocator, fmt, args);
    va_end(args);
    auto msg = msb_borrow(&msb, ctx->temp_allocator);
    fprintf(stderr, "%s\n", msg.text);
    msb_destroy(&msb, ctx->temp_allocator);
    }

printf_func(2, 3)
static
void
report_stat(uint64_t flags, Nonnull(const char*) fmt, ...){
    if(!(flags & PARSE_PRINT_STATS))
        return;
    fprintf(stderr, "Info: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    }

printf_func(2, 3)
static
void
report_error(uint64_t flags, Nonnull(const char*)fmt, ...){
    if(flags & PARSE_DONT_PRINT_ERRORS)
        return;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    }

static
Errorable_f(LongString)
load_source_file(Nonnull(ParseContext*)ctx, StringView sourcepath){
    // check if we already have it.
    for(size_t i = 0; i < ctx->loaded_files.count; i++){
        auto loaded = &ctx->loaded_files.data[i];
        if(LS_SV_equals(loaded->sourcepath, sourcepath)){
            // DBG("Returning cached copy of '%.*s'", (int)sourcepath.length, sourcepath.text);
            return (Errorable(LongString)){.result=loaded->sourcetext};
            }
        }
    char* path = Allocator_strndup(ctx->allocator, sourcepath.text, sourcepath.length);
    // DBG("Loading '%s'", path);
    auto before = get_t();
    auto load_err = read_file_a(ctx->allocator, path);
    auto after = get_t();
    if(!load_err.errored){
        report_stat(ctx->flags, "Loading '%.*s' took %.3fms", (int)sourcepath.length, sourcepath.text, (after-before)/1000.);
        auto loaded = Marray_alloc(LoadedSource)(&ctx->loaded_files, ctx->allocator);
        loaded->sourcepath.text = path;
        loaded->sourcepath.length = sourcepath.length;
        loaded->sourcetext = unwrap(load_err);
        }
    else {
        Allocator_free(ctx->allocator, path, sourcepath.length+1);
        }
    return load_err;
    }

static
Errorable_f(LongString)
load_processed_binary_file(Nonnull(ParseContext*)ctx, StringView binarypath){
    // check if we already have it.
    for(size_t i = 0; i < ctx->processed_binary_files.count; i++){
        auto loaded = &ctx->processed_binary_files.data[i];
        if(LS_SV_equals(loaded->sourcepath, binarypath)){
            // DBG("Returning cached copy of '%.*s'", (int)sourcepath.length, sourcepath.text);
            return (Errorable(LongString)){.result=loaded->sourcetext};
            }
        }
    return (Errorable(LongString)){.errored = NOT_FOUND};
    }

static
Errorable_f(ByteBuffer)
load_binary_file(Nonnull(ParseContext*)ctx, StringView binarypath){
    // check if we already have it.
    for(size_t i = 0; i < ctx->loaded_binary_files.count; i++){
        auto loaded = &ctx->loaded_binary_files.data[i];
        if(LS_SV_equals(loaded->path, binarypath)){
            // DBG("Returning cached copy of '%.*s'", (int)sourcepath.length, sourcepath.text);
            return (Errorable(ByteBuffer)){.result=loaded->bytes};
            }
        }
    char* path = Allocator_strndup(ctx->allocator, binarypath.text, binarypath.length);
    // DBG("Loading '%s'", path);
    auto before = get_t();
    auto load_err = read_bin_file_a(ctx->allocator, path);
    auto after = get_t();
    if(!load_err.errored){
        report_stat(ctx->flags, "Loading '%.*s' took %.3fms", (int)binarypath.length, binarypath.text, (after-before)/1000.);
        auto loaded = Marray_alloc(LoadedBin)(&ctx->loaded_binary_files, ctx->allocator);
        loaded->path.text = path;
        loaded->path.length = binarypath.length;
        loaded->bytes = unwrap(load_err);
        }
    else {
        Allocator_free(ctx->allocator, path, binarypath.length+1);
        }
    return load_err;
    }

static
void
set_context_source(Nonnull(ParseContext*)ctx, StringView filename, Nonnull(const char*) text){
    ctx->cursor = text;
    ctx->linestart = NULL;
    ctx->doublecolon = NULL;
    ctx->lineend = NULL;
    ctx->nspaces = 0;
    ctx->lineno = 0;
    ctx->filename = filename;
    }
printf_func(3, 4)
static void set_err(Nonnull(ParseContext*)ctx, NullUnspec(const char*) errchar, Nonnull(const char*) fmt, ...);
printf_func(3, 4)
static void node_set_err(Nonnull(ParseContext*)ctx, Nonnull(const Node*), Nonnull(const char*) fmt, ...);
printf_func(3, 4)
static void node_print_err(Nonnull(ParseContext*)ctx, Nonnull(const Node*), Nonnull(const char*) fmt, ...);
printf_func(2, 3)
static void report_stat(uint64_t flags, Nonnull(const char*) fmt, ...);
static void print_node_and_children(Nonnull(ParseContext*), NodeHandle handle, int depth);
static Errorable_f(void) parse(Nonnull(ParseContext*), NodeHandle root);
#define PARSEFUNC(name) static Errorable_f(void) name(Nonnull(ParseContext*)ctx, NodeHandle parent_handle, int indentation)
PARSEFUNC(parse_node);
PARSEFUNC(parse_text_node);
PARSEFUNC(parse_table_node);
PARSEFUNC(parse_keyvalue_node);
PARSEFUNC(parse_bullets_node);
PARSEFUNC(parse_bullet_node);
PARSEFUNC(parse_raw_node);
PARSEFUNC(parse_list_node);
PARSEFUNC(parse_list_item);
PARSEFUNC(parse_md_node);

static Errorable_f(void) render_tree(Nonnull(ParseContext*), Nonnull(MStringBuilder*));
static inline force_inline Errorable_f(void) render_node(Nonnull(ParseContext*), Nonnull(MStringBuilder*) restrict, Nonnull(const Node*), int header_depth);
static void gather_anchors(Nonnull(ParseContext*));
static Errorable_f(void)check_depth(Nonnull(ParseContext*));
static void build_nav_block(Nonnull(ParseContext*));
#define RENDERFUNCNAME(nt) render_##nt
#define RENDERFUNC(nt) static Errorable_f(void) RENDERFUNCNAME(nt)(Nonnull(ParseContext*)ctx, Nonnull(MStringBuilder*)sb, Nonnull(const Node*) node, int header_depth)

#define X(a, b) RENDERFUNC(a);
NODETYPES(X)
#undef X

typedef Errorable_f(void)(*_Nonnull renderfunc)(Nonnull(ParseContext*), Nonnull(MStringBuilder*), Nonnull(const Node*), int);
static const renderfunc renderfuncs[] = {
#define X(a,b) [NODE_##a] = &RENDERFUNCNAME(a),
    NODETYPES(X)
#undef X
    };

static Errorable_f(void) parse_post_colon(Nonnull(ParseContext*)ctx, StringView postcolon, NodeHandle node_handle);
static void analyze_line(Nonnull(ParseContext*));
static void advance_row(Nonnull(ParseContext*));
static inline NodeHandle alloc_handle(Nonnull(ParseContext*));
static Errorable_f(void) execute_python_string(Nonnull(ParseContext*), Nonnull(const char*), NodeHandle);
static Errorable_f(void) docparse_init_types(void);
static Errorable_f(void) init_python_docparser(void);

static inline Nonnull(Node*) get_node(Nonnull(ParseContext*), NodeHandle);
extern Nonnull(Node*) get_node_e(Nonnull(ParseContext*), NodeHandle);

static void append_child(Nonnull(ParseContext*), NodeHandle parent, NodeHandle child);

static inline
Nullable(StringView*)
find_link_target(Nonnull(ParseContext*)ctx, StringView kebabed){
    // HERE("kebabed = '%.*s'", (int)kebabed.length, kebabed.text);
    if(!ctx->links.count)
        return NULL;
#if 1
    auto data = ctx->links.data;
    size_t low = 0, high = ctx->links.count-1;
    // This can't realistically overflow.
    size_t mid = (high+low)/2;
    if(SV_equals(data[low].key, kebabed))
        return &data[low].value;
    if(SV_equals(data[high].key, kebabed))
        return &data[high].value;
    while(low < high){
        mid = (low+high)/2;
        auto c = StringView_cmp(&data[mid].key, &kebabed);
        if(c == 0)
            return &data[mid].value;
        if(c > 0){
            high = mid;
            continue;
            }
        // c < 0
        low = mid + 1;
        }
    return NULL;
#else
    for(size_t i = 0; i < ctx->links.count; i++){
        if(SV_equals(ctx->links.data[i].key, kebabed))
            return &ctx->links.data[i].value;
        }
    return NULL;
#endif
    }

static inline
Errorable_f(void)
add_link_from_sv(Nonnull(ParseContext*)ctx, StringView str, bool check_valid){
    Errorable(void) result = {};
    const char* equals = memchr(str.text, '=', str.length);
    if(!equals){
        // TODO: print error from this node
        ctx->error_message = LS("no '=' in a link node");
        Raise(PARSE_ERROR);
        }
    MStringBuilder sb = {};
    msb_write_kebab(&sb, ctx->allocator, str.text, equals - str.text);
    if(!sb.cursor){
        ctx->error_message = LS("key is empty");
        Raise(PARSE_ERROR);
        }
    auto key = LS_to_SV(msb_detach(&sb, ctx->allocator));
    StringView value = {.text = equals + 1, .length = (str.text+str.length)-(equals+1)};
    value = strip_sv_tabspace(value);
    if(!value.length){
        ctx->error_message = LS("link target is empty");
        Raise(PARSE_ERROR);
        }
    if(check_valid and value.text[0] == '#'){
        StringView target = {.text = value.text+1, .length = value.length-1};
        if(!target.length){
            ctx->error_message = LS("link target is empty after the '#'");
            Raise(PARSE_ERROR);
            }
        // TODO: keep a binary tree or something?
        for(size_t i = 0; i < ctx->links.count; i++){
            if(SV_equals(ctx->links.data[i].value, value))
                goto foundit;
            }
        ctx->error_message = LS("Anchor does not correspond to any link");
        Raise(PARSE_ERROR);
        foundit:;
        }
    auto li = Marray_alloc(LinkItem)(&ctx->links, ctx->allocator);
    li->key = key;
    li->value = value;
    return result;
    }

static inline
void
add_link_from_header(Nonnull(ParseContext*)ctx, StringView str){
    MStringBuilder sb = {};
    msb_write_kebab(&sb, ctx->allocator, str.text, str.length);
    if(!sb.cursor)
        return;
    auto kebabed = LS_to_SV(msb_detach(&sb, ctx->allocator));

    const char* anchor = mprintf(ctx->allocator, "#%.*s", (int)kebabed.length, kebabed.text);
    StringView value = {.text=anchor, .length = kebabed.length+1};
    auto li = Marray_alloc(LinkItem)(&ctx->links, ctx->allocator);
    li->key = kebabed;
    li->value = value;
    return;
    }

static Errorable_f(void) run_the_parser(uint64_t flags, LongString source_path, LongString output_path, LongString depends_dir);

#ifndef NOMAIN
int main(int argc, char**argv){
    auto t0 = get_t();
    LongString source_path = {};
    LongString output_path = {};
    LongString depends_dir = {};
    bool report_orphans = false;
    bool no_python = false;
    bool print_tree = false;
    bool print_links = false;
    bool print_stats = false;
    bool allow_bad_links = false;
    bool suppress_warnings = false;
    bool dont_write = false;
    bool no_threads = false;
    bool cleanup = false;
    {
    ArgToParse pos_args[] = {
        [0] = {
            .name = LS("source"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&source_path),
            .help = "Source file (.dnd file) to read from.",
            },
        [1] = {
            .name = LS("output"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&output_path),
            .help = "output path (.html file) to write to.",
            .hide_default = true,
            },
        };
    ArgToParse kw_args[] = {
        {
            .name = LS("-d"),
            .altname1 = LS("--depends-dir"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&depends_dir),
            .help = "If given, what directory to write a corresponding make-style .dep file.",
            .hide_default = true,
        },
        {
            .name = LS("--report-orphans"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&report_orphans),
            .help = "Report orphaned nodes (for debugging scripts).",
        },
        {
            .name = LS("--no-python"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&no_python),
            .help = "Don't execute python nodes.",
        },
        {
            .name = LS("--print-tree"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&print_tree),
            .help = "Print out the entire document tree.",
        },
        {
            .name = LS("--print-links"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&print_links),
            .help = "Print out all links (and what they target) known by the system.",
        },
        {
            .name = LS("--print-stats"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&print_stats),
            .help = "Log some informative statistics.",
        },
        {
            .name = LS("--allow-bad-links"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&allow_bad_links),
            .help = "Warn instead of erroring if a link can't be resolved.",
        },
        {
            .name = LS("--suppress-warnings"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&suppress_warnings),
            .help = "Don't report non-fatal errors.",
        },
        {
            .name = LS("--dont-write"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&dont_write),
            .help = "Don't write out the document.\n"
                "    Outputfile is exposed to scripts so that must still be given.",
        },
        {
            .name = LS("--no-threads"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&no_threads),
            .help = "Do not create worker threads, do everything in the same thread.",
        },
        {
            .name = LS("--cleanup"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&cleanup),
            .help = "Cleanup all resources (memory allocations, etc.).\n"
                "    Development debugging tool, useless in regular cli use."
        },
        };
    ArgParser argparser = {
        .name = argv[0],
        .description = "A .dnd to .html parser and compiler.",
        .version = "Docparser version " DOCPARSER_VERSION "",
        .positional.args = pos_args,
        .positional.count = arrlen(pos_args),
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
        .option_char = '-',
        };
    Args args = argc?(Args){argc-1, argv+1}: (Args){0, 0};
    if(check_for_help(&args)){
        print_help(&argparser);
        return 0;
        }
    if(check_for_version(&args)){
        print_version(&argparser);
        return 0;
        }
    auto e = parse_args(&argparser, &args);
    if(e.errored){
        fprintf(stderr, "Error when parsing arguments.\n");
        print_help(&argparser);
        return e.errored;
        }
    auto after_parse_args = get_t();
    // this one has to be done manually as we don't have a ctx yet.
    report_stat(print_stats?PARSE_PRINT_STATS:0, "Parsing args took: %.3fms", (after_parse_args-t0)/1000.);
    }

    uint64_t flags = PARSE_FLAGS_NONE;
    if(allow_bad_links)
        flags |= PARSE_ALLOW_BAD_LINKS;
    if(suppress_warnings)
        flags |= PARSE_SUPPRESS_WARNINGS;
    if(print_stats)
        flags |= PARSE_PRINT_STATS;
    if(report_orphans)
        flags |= PARSE_REPORT_ORPHANS;
    if(no_python)
        flags |= PARSE_NO_PYTHON;
    if(print_tree)
        flags |= PARSE_PRINT_TREE;
    if(print_links)
        flags |= PARSE_PRINT_LINKS;
    if(no_threads)
        flags |= PARSE_NO_THREADS;
    if(dont_write)
        flags |= PARSE_DONT_WRITE;
    if(not cleanup)
        flags |= PARSE_NO_CLEANUP;

#ifdef BENCHMARKING
    if(!source_path.length){
        source_path = LS(BENCHMARKINPUTPATH);
        }
    if(!output_path.length){
        output_path = LS(BENCHMARKOUTPUTPATH);
        }
    auto chdir_e = chdir(BENCHMARKDIRECTORY);
    assert(chdir_e == 0);
    flags &= ~PARSE_NO_CLEANUP;
    auto e = run_the_parser(flags, source_path, output_path, depends_dir);
    assert(!e.errored);
    flags |= PARSE_PYTHON_IS_INIT;
    for(int i = 0; i < BENCHMARKITERS;i++){
        e = run_the_parser(flags, source_path, output_path, depends_dir);
        assert(!e.errored);
        }
    return 0;
#else
    auto e = run_the_parser(flags, source_path, output_path, depends_dir);
    return e.errored;
#endif
    }
#endif

static
Errorable_f(void)
run_the_parser(uint64_t flags, LongString source_path, LongString output_path, LongString depends_dir){
    auto t0 = get_t();
    Errorable(void) result = {};
    StringView path;
    if(flags & PARSE_SOURCE_PATH_IS_DATA_NOT_PATH)
        path = SV("(string input)");
    else
        path = LS_to_SV(source_path);
    const Allocator* allocator;
    if(flags & PARSE_NO_CLEANUP){
        allocator = get_mallocator();
        }
    else {
        allocator = make_recorded_mallocator();
        }
    auto la_ = new_linear_storage(1024*1024, "temp storage");
    auto la = Allocator_from_linear_allocator(&la_);
    ParseContext ctx = {
        .flags = flags,
        .allocator = allocator,
        // .allocator = mallocator,
        // .allocator = &recorded,
        .temp_allocator = &la,
        .titlenode = INVALID_NODE_HANDLE,
        .navnode = INVALID_NODE_HANDLE,
        .outputfile = output_path,
        };
    LongString source;
    if(flags & PARSE_SOURCE_PATH_IS_DATA_NOT_PATH){
        source = source_path;
        }
    else if(!path.length){
        // read from stdin
        MStringBuilder sb = {};
        for(;;){
            enum {N = 4096};
            msb_reserve(&sb, ctx.allocator, N);
            char* buff = sb.data + sb.cursor;
            auto numread = fread(buff, 1, N, stdin);
            sb.cursor += numread;
            if(numread != N)
                break;
            }
        source = msb_detach(&sb, ctx.allocator);
        }
    else {
        auto source_err = load_source_file(&ctx, path);
        if(source_err.errored){
            report_error(flags, "Unable to open %.*s", (int)path.length, path.text);
            Raise(source_err.errored);
            }
        source = unwrap(source_err);
        }
    set_context_source(&ctx, path, source.text);
    Marray_reserve(Node)(&ctx.nodes, ctx.allocator, source.length/10+1);

    {
    auto root_handle = alloc_handle(&ctx);
    ctx.root_handle = root_handle;
    auto root = get_node(&ctx, root_handle);
    root->col = 0;
    root->row = 0;
    root->filename = ctx.filename;
    root->type = NODE_ROOT;
    root->parent = root_handle;
    }
    {
    auto before_parse = get_t();
    auto e = parse(&ctx, ctx.root_handle);
    auto after_parse = get_t();
    report_stat(ctx.flags, "Initial parsing took: %.3fms", (after_parse-before_parse)/1000.);
    if(e.errored){
        report_error(flags, "%s", ctx.error_message.text);
        Raise(e.errored);
        }
    }
    MStringBuilder msb = {};
    auto before_imports = get_t();
    for(size_t i = 0; i < ctx.imports.count; i++){
        auto handle = ctx.imports.data[i];
        auto node = get_node(&ctx, handle);
        for(size_t j = 0; j < node->children.count; j++, node=get_node(&ctx, handle)){
            auto child_handle = node->children.data[j];
            StringView filename;
            {
            auto child = get_node(&ctx, child_handle);
            if(child->type != NODE_STRING){
                node_print_err(&ctx, child, "import child is not a string");
                Raise(PARSE_ERROR);
                }
            filename = child->header;
            child->type = NODE_CONTAINER;
            child->header = SV("");
            auto imp_e = load_source_file(&ctx, filename);
            if(imp_e.errored){
                node_print_err(&ctx, child, "Unable to open '%.*s'", (int)filename.length, filename.text);
                Raise(imp_e.errored);
                }
            auto imp_text = unwrap(imp_e);
            set_context_source(&ctx, filename, imp_text.text);
            }
            auto parse_e = parse(&ctx, child_handle);
            if(parse_e.errored){
                report_error(flags, "%s", ctx.error_message.text);
                Raise(parse_e.errored);
                }
            }
        }
    auto after_imports = get_t();
    report_stat(ctx.flags, "Resolving imports took: %.3fms", (after_imports-before_imports)/1000.);

    {
    auto worker_allocator = (flags & PARSE_NO_CLEANUP)?get_mallocator():make_recorded_mallocator();
    // auto worker_recorded_ = RecordingAllocator_from_mallocator();
    // auto worker_recorded = Allocator_from_recorded_allocator(&worker_recorded_);
    BinaryJob job = {
        // .a = get_mallocator(), // maybe do a threadlocal one later?
        // .a = &worker_recorded,
        .a = worker_allocator,
        .report_time = !!(ctx.flags & PARSE_PRINT_STATS),
        };
    {
        Marray(NodeHandle)* img_nodes[] = {
            &ctx.img_nodes,
            &ctx.imglinks_nodes,
            };
        for(size_t n = 0; n < arrlen(img_nodes); n++){
            auto nodes = img_nodes[n];
            for(size_t i = 0; i < nodes->count; i++){
                auto node = get_node(&ctx, nodes->data[i]);
                if(!node->children.count)
                    continue;
                auto child = get_node(&ctx, node->children.data[0]);
                if(!child->header.length)
                    continue;
                // FIXME: this makes it O(N^2)
                // In practice, we only have a few images though.
                // But also in practice, we don't have duplicates and
                // this just speeds up benchmarks. *shrug*.
                for(size_t j = 0; j < job.sourcepaths.count; j++){
                    if(SV_equals(child->header, job.sourcepaths.data[j]))
                        goto Continue;
                    }
                Marray_push(StringView)(&job.sourcepaths, job.a, child->header);
                Continue:;
                }
            }
    }
    ThreadHandle worker;
    bool binary_work_to_be_done = !!job.sourcepaths.count;
    if(binary_work_to_be_done){
        if(flags & PARSE_NO_THREADS){
            binary_worker(&job);
            }
        else{
            auto before = get_t();
            create_thread(&worker, &binary_worker, &job);
            auto after = get_t();
            report_stat(ctx.flags, "Launching binary data processing took: %.3fms", (after-before)/1000.);
            }
        }

    bool init_python = !(flags & PARSE_PYTHON_IS_INIT);
    if(!(flags & PARSE_NO_PYTHON) and ctx.python_nodes.count){
        auto before = get_t();
        if(init_python){
            auto e = init_python_docparser();
            if(e.errored) {
                report_error(flags, "Failed to initialize python\n");
                Raise(e.errored);
                }
        }
        auto after = get_t();
        report_stat(ctx.flags, "Python startup took: %.3fms", (after-before)/1000.);
        for(size_t i = 0; i < ctx.python_nodes.count; i++){
            auto handle = ctx.python_nodes.data[i];
            {
            auto node = get_node(&ctx, handle);
            if(node->type != NODE_PYTHON)
                continue;
            for(auto j = 0; j < node->children.count; j++){
                auto child = node->children.data[j];
                auto child_node = get_node(&ctx, child);
                msb_write_str(&msb, ctx.allocator, child_node->header.text, child_node->header.length);
                msb_write_char(&msb, ctx.allocator, '\n');
                }
            if(!msb.cursor)
                continue;
            auto str = msb_detach(&msb, ctx.allocator);
            auto py_err = execute_python_string(&ctx, str.text, handle);
            if(py_err.errored){
                report_error(flags, "%s", ctx.error_message.text);
                Raise(py_err.errored);
                }
            }
            auto node = get_node(&ctx, handle);
            // unsure if this is right, but doing it for now.
            auto parent = get_node(&ctx, node->parent);
            node->parent = INVALID_NODE_HANDLE;
            for(size_t j = 0; j < parent->children.count; j++){
                if(NodeHandle_eq(handle, parent->children.data[j])){
                    Marray_remove__NodeHandle(&parent->children, j);
                    goto after;
                    }
                }
            // don't both warning here, but leave the scaffolding in case I want to.
            after:;
            }
        auto after_python = get_t();
        report_stat(ctx.flags, "Python scripts took: %.3fms", (after_python-after)/1000.);
        report_stat(ctx.flags, "Python total took: %.3fms", (after_python-before)/1000.);
        }
    if(binary_work_to_be_done){
        if(!(flags & PARSE_NO_THREADS)){
            auto before = get_t();
            join_thread(worker, NULL);
            auto after = get_t();
            report_stat(ctx.flags, "Joining took : %.3fms", (after-before)/1000.);
            }
        Allocator_free(job.a, job.sourcepaths.data, sizeof(*job.sourcepaths.data)*job.sourcepaths.capacity);
        // for(size_t i = 0; i < job.loaded.count; i++){
            // Marray_push(LoadedSource)(&ctx.processed_binary_files, ctx.allocator, job.loaded.data[i]);
            // }

        if(job.loaded.count)
            Marray_extend(LoadedSource)(&ctx.processed_binary_files, ctx.allocator, job.loaded.data, job.loaded.count);
        Allocator_free(job.a, job.loaded.data, sizeof(*job.loaded.data)*job.loaded.capacity);
        if(!(flags & PARSE_NO_CLEANUP)){
            merge_recorded_mallocators_and_destroy_src(allocator, worker_allocator);
            }
        // recording_merge(&recorded_, &worker_recorded_);
        // recording_cleanup_tracking(&worker_recorded_);
        }
    }
    report_stat(ctx.flags, "ctx.nodes.count = %zu", ctx.nodes.count);
    report_stat(ctx.flags, "ctx.python_nodes.count = %zu", ctx.python_nodes.count);
    report_stat(ctx.flags, "ctx.imports.count = %zu", ctx.imports.count);
    report_stat(ctx.flags, "ctx.script_nodes.count = %zu", ctx.script_nodes.count);
    report_stat(ctx.flags, "ctx.dependencies.count = %zu", ctx.dependencies_nodes.count);
    report_stat(ctx.flags, "ctx.link_nodes.count = %zu", ctx.link_nodes.count);
    if(flags & PARSE_REPORT_ORPHANS){
        for(size_t i = 0; i < ctx.nodes.count; i++){
            auto node = &ctx.nodes.data[i];
            // python nodes get orphaned after execution
            if(node->type == NODE_PYTHON)
                continue;
            if(NodeHandle_eq(node->parent, INVALID_NODE_HANDLE)){
                node_print_warning(&ctx, node, "Orphaned node (invalid parent node handle)");
                }
            }
        }
    if(NodeHandle_eq(ctx.root_handle, INVALID_NODE_HANDLE)){
        report_error(flags, "ctx has no root Node.");
        Raise(PARSE_ERROR);
        }
    // check that the tree is not too deep!
    {
    auto before = get_t();
    auto e = check_depth(&ctx);
    if(e.errored){
        report_error(flags, "%s", ctx.error_message.text);
        Raise(PARSE_ERROR);
        }
    auto after = get_t();
    report_stat(ctx.flags, "Checking depth took %.3fms", (after-before)/1000.);
    }
    // resolve links
    {
        auto before = get_t();
        gather_anchors(&ctx);
        auto after = get_t();
        report_stat(ctx.flags, "Link resolving took: %.3fms", (after-before)/1000.);
        if(ctx.error_message.length){
            report_error(flags, "%s", ctx.error_message.text);
            Raise(PARSE_ERROR);
            }
    }

    if(flags & PARSE_PRINT_TREE)
        print_node_and_children(&ctx, ctx.root_handle, 0);
    {
        auto before = get_t();
        if(!NodeHandle_eq(ctx.navnode, INVALID_NODE_HANDLE))
            build_nav_block(&ctx);
        auto after =  get_t();
        report_stat(ctx.flags, "Nav block building took: %.3fms", (after-before)/1000.);
    }
    {
    auto link_node_count = ctx.link_nodes.count;
    auto link_handles = ctx.link_nodes.data;
    for(size_t ln = 0; ln < link_node_count; ln++){
        auto link_node_handle = link_handles[ln];
        auto link_node = get_node(&ctx, link_node_handle);
        for(size_t i = 0; i < link_node->children.count; i++){
            auto link_str_handle = link_node->children.data[i];
            auto link_str_node = get_node(&ctx, link_str_handle);
            auto str = link_str_node->header;
            auto e = add_link_from_sv(&ctx, str, true);
            if(e.errored){
                // This looks weird, but I am formatting the error.
                node_set_err(&ctx, link_str_node, "%s", ctx.error_message.text);
                report_error(flags, "%s", ctx.error_message.text);
                Raise(e.errored);
                }
            }
        }
    if(ctx.links.count)
        qsort(ctx.links.data, ctx.links.count, sizeof(ctx.links.data[0]), StringView_cmp);
    if(flags & PARSE_PRINT_LINKS){
        for(size_t i = 0; i < ctx.links.count; i++){
            auto li = &ctx.links.data[i];
            printf("[%zu] key: '%.*s', value: '%.*s'\n", i, (int)li->key.length, li->key.text, (int)li->value.length, li->value.text);
            }
        }
    }
    report_stat(ctx.flags, "ctx.links.count = %zu", ctx.links.count);
    if(unlikely(flags & PARSE_DONT_WRITE))
        goto success;

    auto before_data = get_t();
    {
    MStringBuilder sb = {};
    for(size_t i = 0; i < ctx.data_nodes.count; i++){
        auto handle = ctx.data_nodes.data[i];
        auto data_node = get_node(&ctx, handle);
        // Node could've been mutated after being registered.
        if(data_node->type != NODE_DATA)
            continue;
        for(size_t j = 0; j < data_node->children.count; j++){
            auto child = get_node(&ctx, data_node->children.data[j]);
            if(!child->header.length){
                node_print_warning(&ctx, child, "Missing header from data child?");
                }
            // FIXME:
            // A maliciously crafted python block could bypass our depth check
            // up above by detaching the data node and making one too deep,
            // thus making us vulnerable to stack exhaustion during this
            // recursive call.
            //
            // However, our code execution is totally unsandboxed right now and
            // a malicious python block can just do anything. Crashing this
            // main program is the least of your worries when it can just do an
            // os.system('rm -rf /'). We bother guarding at all as you could
            // run this in no-python mode.
            {
            msb_reset(&sb);
            auto e = render_node(&ctx, &sb, child, 1);
            if(e.errored){
                report_error(flags, "%s", ctx.error_message.text);
                Raise(e.errored);
                }
            }
            if(!sb.cursor){
                node_print_warning(&ctx, child, "Rendered a data node with no data. Not outputting it.");
                continue;
                }
            auto text = msb_detach(&sb, ctx.allocator);
            auto di = Marray_alloc(DataItem)(&ctx.rendered_data, ctx.allocator);
            di->key = child->header;
            di->value = text;
            }
        }
    auto after_data = get_t();
    report_stat(ctx.flags, "Data blob rendering took: %.3fms", (after_data-before_data)/1000.);
    report_stat(ctx.flags, "ctx.rendered_data.count = %zu", ctx.rendered_data.count);
    }
    {
    msb_reset(&msb);
    auto before_render = get_t();
    auto e = render_tree(&ctx, &msb);
    auto after_render = get_t();
    report_stat(ctx.flags, "Rendering took: %.3fms", (after_render-before_render)/1000.);

    if(e.errored){
        report_error(flags, "%s", ctx.error_message.text);
        Raise(e.errored);
        }
    auto str = msb_borrow(&msb, ctx.allocator);
    if(!output_path.length){
        fputs(str.text, stdout);
        goto success;
        }
    auto before_write = get_t();
    auto write_err = write_file(output_path.text, str.text, str.length);
    auto after_write = get_t();
    report_stat(ctx.flags, "Writing took: %.3fms", (after_write-before_write)/1000.);
    report_stat(ctx.flags, "Total output size: %zu bytes", str.length);
    if(write_err.errored){
        ERROR("Error on write: %s", get_error_name(write_err));
        perror("Error on write");
        Raise(write_err.errored);
        }
    }
    if(depends_dir.length){
        for(size_t i = 0; i < ctx.loaded_files.count; i++){
            Marray_push(StringView)(&ctx.dependencies, ctx.allocator, LS_to_SV(ctx.loaded_files.data[i].sourcepath));
            }
        for(size_t i = 0; i < ctx.loaded_binary_files.count; i++){
            Marray_push(StringView)(&ctx.dependencies, ctx.allocator, LS_to_SV(ctx.loaded_binary_files.data[i].path));
            }
        for(size_t i = 0; i < ctx.dependencies_nodes.count; i++){
            auto handle = ctx.dependencies_nodes.data[i];
            auto node = get_node(&ctx, handle);
            for(size_t j = 0; j < node->children.count; j++){
                auto child_handle = node->children.data[j];
                auto child = get_node(&ctx, child_handle);
                if(child->type != NODE_STRING){
                    // just warn, don't want to fail the build
                    node_print_warning(&ctx, child, "Non-string node found as a child node: %s", nodenames[child->type].text);
                    continue;
                    }
                Marray_push(StringView)(&ctx.dependencies, ctx.allocator, child->header);
                }
            }
        msb_reset(&msb);
        MStringBuilder depb = {};
        msb_write_str(&depb, ctx.temp_allocator, depends_dir.text, depends_dir.length);
        auto out = LS_to_SV(output_path);
        auto basename = path_basename(out);
        auto stripped = path_strip_extension(basename);
        msb_append_path(&depb, ctx.temp_allocator, stripped.text, stripped.length);
        msb_write_literal(&depb, ctx.temp_allocator, ".dep");
        auto depfilename = msb_borrow(&depb, ctx.temp_allocator);
        msb_write_str(&msb, ctx.allocator, output_path.text, output_path.length);
        msb_write_char(&msb, ctx.allocator, ':');
        for(size_t i = 0; i < ctx.dependencies.count; i++){
            auto dep = &ctx.dependencies.data[i];
            msb_write_char(&msb, ctx.allocator, ' ');
            msb_write_str(&msb, ctx.allocator, dep->text, dep->length);
            }
        msb_write_char(&msb, ctx.allocator, '\n');
        // generate empty rules so deleted files don't fail the build
        for(size_t i = 0; i < ctx.dependencies.count; i++){
            auto dep = &ctx.dependencies.data[i];
            msb_write_str(&msb, ctx.allocator, dep->text, dep->length);
            msb_write_literal(&msb, ctx.allocator, ":\n");
            }
        auto deptext = msb_borrow(&msb, ctx.allocator);
        auto write_err = write_file(depfilename.text, deptext.text, deptext.length);
        msb_destroy(&depb, ctx.temp_allocator);
        if(write_err.errored){
            ERROR("Error on write: %s", get_error_name(write_err));
            perror("Error on write");
            Raise(write_err.errored);
            }
        }
    success:
    msb_destroy(&msb, ctx.allocator);
    DBGPrint(la_.high_water);
    if(!(flags & PARSE_NO_CLEANUP)){
        auto before = get_t();
        if(ctx.flags & PARSE_PRINT_STATS){
            RecordingAllocator* recorder = allocator->_allocator_data;
            report_stat(ctx.flags, "There were %zu allocations.", recorder->recorded.count);
            size_t total = 0;
            for(size_t i = 0; i < recorder->recorded.count; i++){
                total += recorder->recorded.allocation_sizes[i];
                }
            report_stat(ctx.flags, "Allocations outstanding total: %zu", total);
            }
        Allocator_free_all(allocator);
        shallow_free_recorded_mallocator(allocator);
        destroy_linear_storage(&la_);
        auto after = get_t();
        report_stat(ctx.flags, "Cleaning up memory took: %.3fms", (after-before)/1000.);
        }
    // recording_free_all(&recorded_);
    // recording_cleanup_tracking(&recorded_);
    auto t1 = get_t();
    report_stat(ctx.flags, "Execution took: %.3fms", (t1-t0)/1000.);
    return result;
    }


static inline
force_inline
NodeHandle
alloc_handle(Nonnull(ParseContext*)ctx){
    auto index = Marray_alloc_index(Node)(&ctx->nodes, ctx->allocator);
    ctx->nodes.data[index] = (Node){};
    // debug to help find nodes without parents
    ctx->nodes.data[index].parent = INVALID_NODE_HANDLE;
    return (NodeHandle){.index=index};
    }

static inline
Nonnull(Node*)
force_inline
get_node(Nonnull(ParseContext*)ctx, NodeHandle handle){
    assert(handle.index < ctx->nodes.count);
    auto result = &ctx->nodes.data[handle.index];
    return result;
    }

// for debugging
extern
Nonnull(Node*)
get_node_e(Nonnull(ParseContext*)ctx, NodeHandle handle){
    return get_node(ctx, handle);
    }

static inline
void
force_inline
append_child(Nonnull(ParseContext*)ctx, NodeHandle parent_handle, NodeHandle child_handle){
    auto parent = get_node(ctx, parent_handle);
    auto child = get_node(ctx, child_handle);
    child->parent = parent_handle;
    Marray_push(NodeHandle)(&parent->children, ctx->allocator, child_handle);
    }

static inline
void
analyze_line(Nonnull(ParseContext*)ctx){
    if(ctx->cursor == ctx->linestart)
        return;
    const char* doublecolon = NULL;
    const char* endline = NULL;
    const char* cursor = ctx->cursor;
    bool nonspace = false;
    int nspace = 0;
    for(;;){
        if(!doublecolon){
            if(unlikely(*cursor == ':')){
                if(cursor[1] == ':'){
                    doublecolon = cursor;
                    }
                }
            }
        if(!nonspace){
            if(*cursor == '\t'){
                if(!(ctx->flags & PARSE_SUPPRESS_WARNINGS))
                    fprintf(stderr, "Encountered a tab. Counting as 1 space.\n");
                nspace++;
                }
            else if(*cursor == ' '){
                nspace++;
                }
            else
                nonspace = true;
            }
        if(unlikely(*cursor == '\n' || *cursor == '\0')){
            endline = cursor;
            break;
            }
        cursor++;
        }
    ctx->doublecolon = doublecolon;
    ctx->lineend = endline;
    ctx->linestart = ctx->cursor;
    ctx->nspaces = nspace;
    }
static inline
void
force_inline
advance_row(Nonnull(ParseContext*)ctx){
    if(!unlikely(ctx->lineend[0]))
        ctx->cursor = ctx->lineend;
    else
        ctx->cursor = ctx->lineend+1;
    ctx->lineno++;
    }

static inline
void
force_inline
init_node(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(const char*) src_char, NodeType type){
    auto node = get_node(ctx, handle);
    int col = (int)(src_char - ctx->linestart);
    node->col = col;
    assert(node->col >= 0);
    node->filename = ctx->filename;
    node->row = ctx->lineno;
    node->type = type;
    }
static inline
void
force_inline
init_string_node(Nonnull(ParseContext*)ctx, NodeHandle handle, StringView sv){
    auto node = get_node(ctx, handle);
    int col = (int)(sv.text - ctx->linestart);
    node->col = col;
    node->filename = ctx->filename;
    node->row = ctx->lineno;
    node->type = NODE_STRING;
    node->header = sv;
    }
static
Errorable_f(void)
parse(Nonnull(ParseContext*)ctx, NodeHandle root_handle){
    Errorable(void) result = {};
    auto e = parse_node(ctx, root_handle, -1);
    if(e.errored) return e;
    return result;
    }

static
Errorable_f(void)
parse_double_colon(Nonnull(ParseContext*)ctx, NodeHandle parent_handle){
    Errorable(void) result = {};
    // parse the node header
    const char* starttext = ctx->doublecolon + 2;
    size_t length = ctx->lineend - starttext;
    StringView postcolon = {.text = starttext, .length=length};
    auto new_node_handle = alloc_handle(ctx);
    init_node(ctx, new_node_handle, ctx->linestart+ctx->nspaces, NODE_INVALID);
    {
    auto e = parse_post_colon(ctx, postcolon, new_node_handle);
    if(e.errored) return e;
    }
    append_child(ctx, parent_handle, new_node_handle);
    {
    auto node = get_node(ctx, new_node_handle);
    const char* header = ctx->linestart + ctx->nspaces;
    node->header = (StringView){.text=header, .length=ctx->doublecolon-header};
    node->header = strip_sv_tabspace(node->header);
    }
    auto new_indent = ctx->nspaces;
    advance_row(ctx);
    auto e = parse_node(ctx, new_node_handle, new_indent);
    if(e.errored) return e;
    return result;
    }
// generic parsing function
PARSEFUNC(parse_node){
    {
    auto parent = get_node(ctx, parent_handle);
    if(unlikely(indentation > 64)){
        node_set_err(ctx, parent, "Too deep! Indentation greater than 64 is unsupported.");

        return (Errorable(void)){.errored=PARSE_ERROR};
        }
    switch((NodeType)parent->type){
        case NODE_PRE:
        case NODE_RAW:
        case NODE_PYTHON:
            return parse_raw_node(ctx, parent_handle, indentation);
        case NODE_LIST:
            return parse_list_node(ctx, parent_handle, indentation);
        case NODE_TABLE:
            return parse_table_node(ctx, parent_handle, indentation);
        case NODE_KEYVALUE:
            return parse_keyvalue_node(ctx, parent_handle, indentation);
        case NODE_MD:
            return parse_md_node(ctx, parent_handle, indentation);
        case NODE_IMGLINKS:
        case NODE_DATA:
        case NODE_COMMENT:
        case NODE_NAV:
        case NODE_STYLESHEETS:
        case NODE_DEPENDENCIES:
        case NODE_LINKS:
        case NODE_SCRIPTS:
        case NODE_IMPORT:
        case NODE_IMAGE:
        case NODE_ROOT:
        case NODE_DIV:
        case NODE_HEADING:
        case NODE_TITLE:
        case NODE_CONTAINER:
        case NODE_QUOTE:
            break;
        case NODE_TEXT:
            return parse_text_node(ctx, parent_handle, indentation);
        case NODE_BULLETS:
            return parse_bullets_node(ctx, parent_handle, indentation);
        case NODE_LIST_ITEM:
        case NODE_BULLET:
        case NODE_TABLE_ROW:
        case NODE_PARA:
        case NODE_STRING:
        case NODE_KEYVALUEPAIR:
        case NODE_INVALID:
            unreachable();
        }
    }
    Errorable(void) result = {};
    for(;ctx->cursor[0];){
        analyze_line(ctx);
        if(ctx->linestart+ctx->nspaces == ctx->lineend){
            advance_row(ctx);
            continue;
            }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            auto e = parse_double_colon(ctx, parent_handle);
            if(e.errored) return e;
            continue;
            }
        // default: string node
        StringView content = {.length = (ctx->lineend - ctx->linestart)-ctx->nspaces, .text = ctx->linestart + ctx->nspaces};
        auto new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
        append_child(ctx, parent_handle, new_node_handle);
        advance_row(ctx);
        }
    return result;
    }
PARSEFUNC(parse_list_node){
    {
        auto parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_LIST);
    }
    Errorable(void) result = {};
    for(;ctx->cursor[0];){
        analyze_line(ctx);
        // skip blanks
        if(ctx->linestart+ctx->nspaces == ctx->lineend){
            advance_row(ctx);
            continue;
            }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            // This looks weird, but we allow double colon nodes in the table
            // so that things like ::links and ::python nodes work properly.
            // Those will be removed at render time, so it's not invalid to
            // parse them.
            auto e = parse_double_colon(ctx, parent_handle);
            if(e.errored) return e;
            continue;
            }
        const char* firstchar = ctx->linestart + ctx->nspaces;
        for(;;firstchar++){
            switch(*firstchar){
                case '0' ... '9':
                    continue;
                case '.':
                    firstchar++;
                    goto after;
                    break;
                default:
                    set_err(ctx, firstchar, "Non numeric found when parsing list: '%c'", *firstchar);
                    Raise(PARSE_ERROR);
                }
            }
        after:;
        auto li_handle = alloc_handle(ctx);
        init_node(ctx, li_handle, ctx->linestart+ctx->nspaces, NODE_LIST_ITEM);
        append_child(ctx, parent_handle, li_handle);
        StringView text = {.text=firstchar, .length=ctx->lineend - firstchar};
        auto first_child = alloc_handle(ctx);
        init_string_node(ctx, first_child, text);
        advance_row(ctx);
        auto e = parse_list_item(ctx, li_handle, ctx->nspaces);
        if(e.errored) return e;
        }
    return result;
    }
PARSEFUNC(parse_list_item){
    Errorable(void) result = {};
    {
        auto parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_LIST_ITEM);
    }
    for(;ctx->cursor[0];){
        top:;
        analyze_line(ctx);
        if(ctx->linestart+ctx->nspaces == ctx->lineend){
            advance_row(ctx);
            continue;
            }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            set_err(ctx, ctx->doublecolon, "This node type cannot contain subnodes, only strings");
            Raise(PARSE_ERROR);
            }
        const char* firstchar = ctx->linestart + ctx->nspaces;
        for(;;firstchar++){
            switch(*firstchar){
                case '0' ... '9':
                    continue;
                case '.':{
                    auto new_handle = alloc_handle(ctx);
                    init_node(ctx, new_handle, ctx->linestart + ctx->nspaces, NODE_LIST);
                    append_child(ctx, parent_handle, new_handle);
                    auto e = parse_list_node(ctx, new_handle, indentation);
                    if(e.errored) return e;
                    goto top;
                    }break;
                default:
                    goto after;
                }
            }
        after:;
        // default: string node
        StringView content = {.length = (ctx->lineend - ctx->linestart)-ctx->nspaces, .text = ctx->linestart + ctx->nspaces};
        auto new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, strip_sv_tabspace(content));
        append_child(ctx, parent_handle, new_node_handle);
        advance_row(ctx);
        }
    return result;
    }
PARSEFUNC(parse_raw_node){
    Errorable(void) result = {};
    // In order to avoid needing to scan all of the lines in the text
    // to figure out what the minimum leading indent is, we use the indent
    // of the first non-blank line. However, this can be greater than the indentation
    // of subsequent lines (which are indented less than what would cause us to
    // break out of this node). So for those, we'll pretend like they are indented at
    // the same level as our leading indent, which means their indentation will be
    // off in the output.
    bool have_leading_indent = false;
    int leading_indent = 0;
    for(;ctx->cursor[0];){
        analyze_line(ctx);
        if(!have_leading_indent and ctx->linestart+ctx->nspaces != ctx->lineend){
            leading_indent = ctx->nspaces;
            have_leading_indent = true;
            }
        size_t length;
        const char* text;
        if(ctx->linestart + ctx->nspaces != ctx->lineend){
            if(ctx->nspaces <= indentation)
                break;
            length = ctx->lineend - ctx->linestart;
            auto effective_indent = Min(leading_indent, ctx->nspaces);
            length -= effective_indent;
            text = ctx->linestart + effective_indent;
            }
        else {
            length = Max(ctx->nspaces - leading_indent, 0);
            text = ctx->linestart + ctx->nspaces - length;
            }
        // default: string node
        StringView content = {.length = length, .text = text};
        auto new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
        append_child(ctx, parent_handle, new_node_handle);
        advance_row(ctx);
        }
    return result;
    }

PARSEFUNC(parse_table_node){
    {
        auto parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_TABLE);
    }
    Errorable(void) result = {};
    for(;ctx->cursor[0];){
        analyze_line(ctx);
        // skip blanks
        if(ctx->linestart+ctx->nspaces == ctx->lineend){
            advance_row(ctx);
            continue;
            }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            // This looks weird, but we allow double colon nodes in the table
            // so that things like ::links and ::python nodes work properly.
            // Those will be removed at render time, so it's not invalid to
            // parse them.
            auto e = parse_double_colon(ctx, parent_handle);
            if(e.errored) return e;
            continue;
            }
        auto new_node_handle = alloc_handle(ctx);
        init_node(ctx, new_node_handle, ctx->linestart+ctx->nspaces, NODE_TABLE_ROW);
        append_child(ctx, parent_handle, new_node_handle);
        const char* cursor = ctx->linestart+ctx->nspaces;
        const char* pipe = memchr(cursor, '|', ctx->lineend - cursor);
        while(pipe){
            auto cell_index = alloc_handle(ctx);
            size_t length = pipe - cursor;
            StringView content = {.text = cursor, .length = length};
            content = strip_sv_tabspace(content);
            init_string_node(ctx, cell_index, content);
            append_child(ctx, new_node_handle, cell_index);
            cursor = pipe+1;
            pipe = memchr(cursor, '|', ctx->lineend - cursor);
            }
        auto cell_index = alloc_handle(ctx);
        StringView content = {.text=cursor, .length = ctx->lineend-cursor};
        content = strip_sv_tabspace(content);
        init_string_node(ctx, cell_index, content);
        append_child(ctx, new_node_handle, cell_index);
        advance_row(ctx);
        }
    return result;
    }
PARSEFUNC(parse_keyvalue_node){
    {
        auto parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_KEYVALUE);
    }
    Errorable(void) result = {};
    for(;ctx->cursor[0];){
        analyze_line(ctx);
        // skip blanks
        if(ctx->linestart+ctx->nspaces == ctx->lineend){
            advance_row(ctx);
            continue;
            }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            auto e = parse_double_colon(ctx, parent_handle);
            if(e.errored) return e;
            continue;
            }
        auto new_node_handle = alloc_handle(ctx);
        init_node(ctx, new_node_handle, ctx->linestart + ctx->nspaces, NODE_KEYVALUEPAIR);
        append_child(ctx, parent_handle, new_node_handle);
        const char* cursor = ctx->linestart+ctx->nspaces;
        const char* colon = memchr(cursor, ':', ctx->lineend - cursor);
        if(!colon){
            set_err(ctx, cursor, "Expected a colon for key value pairs");
            Raise(PARSE_ERROR);
            }
        const char* pre_text = ctx->linestart+ctx->nspaces;

        StringView pre = {.text = pre_text, .length = colon - pre_text};
        StringView post = {.text = colon+1, .length = (ctx->lineend-colon)-1};
        auto key_idx = alloc_handle(ctx);
        init_string_node(ctx, key_idx, strip_sv_tabspace(pre));
        auto val_idx = alloc_handle(ctx);
        init_string_node(ctx, val_idx, strip_sv_tabspace(post));
        append_child(ctx, new_node_handle, key_idx);
        append_child(ctx, new_node_handle, val_idx);
        advance_row(ctx);
        }
    return result;
    }

PARSEFUNC(parse_bullets_node){
    Errorable(void) result = {};
    {
        auto parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_BULLETS);
    }
    for(;ctx->cursor[0];){
        analyze_line(ctx);
        // skip blanks
        if(ctx->linestart+ctx->nspaces == ctx->lineend){
            advance_row(ctx);
            continue;
            }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            // same comment as the table parser. Makes ::links and such work
            auto e = parse_double_colon(ctx, parent_handle);
            if(e.errored) return e;
            continue;
            }
        const char* firstchar = ctx->linestart+ctx->nspaces;
        char first = *firstchar;
        if(first != '*' and first != '+' and first != '-'){
            set_err(ctx, firstchar, "Bullets must begin with one of *-+, got '%c'", first);
            Raise(PARSE_ERROR);
            }
        firstchar++;
        StringView bullet_text = {.text = firstchar, .length = ctx->lineend - firstchar};
        bullet_text = strip_sv_tabspace(bullet_text);

        auto bullet_node_handle = alloc_handle(ctx);
        init_node(ctx, bullet_node_handle, ctx->linestart+ctx->nspaces, NODE_BULLET);
        append_child(ctx, parent_handle, bullet_node_handle);
        auto first_child_index = alloc_handle(ctx);
        init_string_node(ctx, first_child_index, bullet_text);
        append_child(ctx, bullet_node_handle, first_child_index);
        advance_row(ctx);
        auto e = parse_bullet_node(ctx, bullet_node_handle, ctx->nspaces);
        if(e.errored) return e;
        }
    return result;
    }

PARSEFUNC(parse_bullet_node){
    Errorable(void) result = {};
    {
        auto parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_BULLET);
    }
    for(;ctx->cursor[0];){
        analyze_line(ctx);
        if(ctx->linestart+ctx->nspaces == ctx->lineend){
            advance_row(ctx);
            continue;
            }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            set_err(ctx, ctx->doublecolon,"This node type cannot contain subnodes, only strings");
            Raise(PARSE_ERROR);
            }
        const char* firstchar = ctx->linestart + ctx->nspaces;
        char first = *firstchar;
        if(first == '*' or first == '+' or first == '-'){
            auto new_index = alloc_handle(ctx);
            init_node(ctx, new_index, firstchar, NODE_BULLETS);
            append_child(ctx, parent_handle, new_index);
            auto e = parse_bullets_node(ctx, new_index, indentation);
            if(e.errored) return e;
            continue;
            }
        // default: string node
        StringView content = {.length = (ctx->lineend - ctx->linestart)-ctx->nspaces, .text = ctx->linestart + ctx->nspaces};
        auto new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, strip_sv_tabspace(content));
        append_child(ctx, parent_handle, new_node_handle);
        advance_row(ctx);
        }
    return result;
    }
PARSEFUNC(parse_text_node){
    {
    auto parent = get_node(ctx, parent_handle);
    assert(parent->type == NODE_TEXT);
    }
    bool in_para_node = 0;
    NodeHandle para_handle;
    Errorable(void) result = {};
    for(;ctx->cursor[0];){
        analyze_line(ctx);
        if(ctx->linestart+ctx->nspaces == ctx->lineend){
            in_para_node = false;
            advance_row(ctx);
            continue;
            }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            // same comment as the table parser. Makes ::links and such work
            // We'll flag those as errors in a later analysis
            auto e = parse_double_colon(ctx, parent_handle);
            if(e.errored) return e;
            continue;
            }
        if(!in_para_node){
            para_handle = alloc_handle(ctx);
            init_node(ctx, para_handle, ctx->linestart+ctx->nspaces, NODE_PARA);
            append_child(ctx, parent_handle, para_handle);
            }
        in_para_node = true;
        // default: new paragraph node
        StringView content = {.length = (ctx->lineend - ctx->linestart)-ctx->nspaces, .text = ctx->linestart + ctx->nspaces};
        auto new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, strip_sv_tabspace(content));
        append_child(ctx, para_handle, new_node_handle);
        advance_row(ctx);
        }
    return result;
    }
PARSEFUNC(parse_md_node){
    {
    auto parent = get_node(ctx, parent_handle);
    assert(parent->type == NODE_MD);
    }
    enum MDSTATE {
        NONE = 0,
        PARA = 1,
        BULLET = 2,
        LIST = 3,
        };
    enum MDSTATE state = NONE;
    NodeHandle para_handle      = INVALID_NODE_HANDLE;
    NodeHandle bullets_handle   = INVALID_NODE_HANDLE;
    NodeHandle bullet_handle    = INVALID_NODE_HANDLE;
    NodeHandle list_handle      = INVALID_NODE_HANDLE;
    NodeHandle list_item_handle = INVALID_NODE_HANDLE;
    int normal_indent = 0;
    Errorable(void) result = {};
    for(;ctx->cursor[0];){
        analyze_line(ctx);
        // skip_blanks
        if(ctx->linestart+ctx->nspaces == ctx->lineend){
            state = NONE;
            advance_row(ctx);
            continue;
            }
        if(!normal_indent){
            normal_indent = ctx->nspaces;
            }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            state = NONE;
            // same comment as the table parser. Makes ::links and such work
            // We'll flag those as errors in a later analysis
            auto e = parse_double_colon(ctx, parent_handle);
            if(e.errored) return e;
            continue;
            }
        enum MDSTATE newstate = NONE;
        const char* firstchar = ctx->linestart + ctx->nspaces;
        switch(*firstchar){
            case '*':
                newstate = BULLET;
                break;
            case '0' ... '9':{
                for(const char* c = firstchar+1;;c++){
                    switch(*c){
                        case '0' ... '9':
                            continue;
                        case '.':
                            newstate = LIST;
                            goto after;
                        default:
                            newstate = PARA;
                            goto after;
                        }
                    }
                }break;
            default:
                newstate = PARA;
                goto after;
            }
        after:;
        assert(newstate != NONE);
        if(newstate == BULLET){
            if(state != BULLET){
                bullets_handle = alloc_handle(ctx);
                init_node(ctx, bullets_handle, ctx->linestart+ctx->nspaces, NODE_BULLETS);
                append_child(ctx, parent_handle, bullets_handle);
                }
            bullet_handle = alloc_handle(ctx);
            init_node(ctx, bullet_handle, ctx->linestart+ctx->nspaces, NODE_BULLET);
            append_child(ctx, bullets_handle, bullet_handle);

            StringView content = {.length = (ctx->lineend - ctx->linestart)-ctx->nspaces-1, .text = ctx->linestart + ctx->nspaces+1};
            auto new_node_handle = alloc_handle(ctx);
            init_string_node(ctx, new_node_handle, strip_sv_tabspace(content));
            append_child(ctx, bullet_handle, new_node_handle);
            advance_row(ctx);
            state = newstate;
            continue;
            }
        if(newstate == LIST){
            if(state != LIST){
                list_handle = alloc_handle(ctx);
                init_node(ctx, list_handle, ctx->linestart+ctx->nspaces, NODE_LIST);
                append_child(ctx, parent_handle, list_handle);
                }
            list_item_handle = alloc_handle(ctx);
            init_node(ctx, list_item_handle, ctx->linestart+ctx->nspaces, NODE_LIST_ITEM);
            append_child(ctx, list_handle, list_item_handle);

            const char* dot = memchr(ctx->linestart, '.', ctx->lineend-ctx->linestart);
            assert(dot);
            dot++;
            StringView content = {.length = ctx->lineend - dot, .text = dot};
            auto new_node_handle = alloc_handle(ctx);
            init_string_node(ctx, new_node_handle, strip_sv_tabspace(content));
            append_child(ctx, list_item_handle, new_node_handle);
            advance_row(ctx);
            state = newstate;
            continue;
            }
        assert(newstate == PARA);
        if(state == PARA or state == NONE or ctx->nspaces == normal_indent){
            if(state != PARA){
                para_handle = alloc_handle(ctx);
                init_node(ctx, para_handle, ctx->linestart+ctx->nspaces, NODE_PARA);
                append_child(ctx, parent_handle, para_handle);
                }
            StringView content = {.length = (ctx->lineend - ctx->linestart)-ctx->nspaces, .text = ctx->linestart + ctx->nspaces};
            auto new_node_handle = alloc_handle(ctx);
            init_string_node(ctx, new_node_handle, strip_sv_tabspace(content));
            append_child(ctx, para_handle, new_node_handle);
            advance_row(ctx);
            state = newstate;
            continue;
            }
        // don't change state for these
        if(state == BULLET){
            StringView content = {.length = (ctx->lineend - ctx->linestart)-ctx->nspaces, .text = ctx->linestart + ctx->nspaces};
            auto new_node_handle = alloc_handle(ctx);
            init_string_node(ctx, new_node_handle, strip_sv_tabspace(content));
            append_child(ctx, bullet_handle, new_node_handle);
            advance_row(ctx);
            continue;
            }
        if(state == LIST){
            StringView content = {.length = (ctx->lineend - ctx->linestart)-ctx->nspaces, .text = ctx->linestart + ctx->nspaces};
            auto new_node_handle = alloc_handle(ctx);
            init_string_node(ctx, new_node_handle, strip_sv_tabspace(content));
            append_child(ctx, list_item_handle, new_node_handle);
            advance_row(ctx);
            continue;
            }
        unreachable();
        }
    return result;
    }
static
void
eat_leading_tabspaces(Nonnull(StringView*)sv){
    while(sv->length){
        char first = sv->text[0];
        if(first != ' ' and first != '\t')
            break;
        sv->length--;
        sv->text++;
        }
    return;
    }

static inline
void
advance_sv(Nonnull(StringView*)sv){
    assert(sv->length);
    sv->text++;
    sv->length--;
    }

static
Errorable_f(void)
parse_post_colon(Nonnull(ParseContext*)ctx, StringView postcolon, NodeHandle node_handle){
    Errorable(void) result = {};
    auto node = get_node(ctx, node_handle);
    eat_leading_tabspaces(&postcolon);
    size_t boundary = postcolon.length;
    for(size_t i = 0; i < postcolon.length;i++){
        switch(postcolon.text[i]){
            case 'a' ... 'z':
                continue;
            default:
                boundary = i;
                break;
            }
        break;
        }
    if(!boundary){
        set_err(ctx, postcolon.text, "no node type found after '::'");
        Raise(PARSE_ERROR);
        }
    for(size_t i = 0; i < arrlen(nodealiases); i++){
        if(nodealiases[i].name.length == boundary){
            if(memcmp(nodealiases[i].name.text, postcolon.text, boundary)==0){
                auto type = nodealiases[i].type;
                switch(type){
                    case NODE_PYTHON:
                        Marray_push(NodeHandle)(&ctx->python_nodes, ctx->allocator, node_handle);
                        break;
                    case NODE_IMPORT:
                        Marray_push(NodeHandle)(&ctx->imports, ctx->allocator, node_handle);
                        break;
                    case NODE_STYLESHEETS:
                        Marray_push(NodeHandle)(&ctx->stylesheets_nodes, ctx->allocator, node_handle);
                        break;
                    case NODE_DEPENDENCIES:
                        Marray_push(NodeHandle)(&ctx->dependencies_nodes, ctx->allocator, node_handle);
                        break;
                    case NODE_LINKS:
                        Marray_push(NodeHandle)(&ctx->link_nodes, ctx->allocator, node_handle);
                        break;
                    case NODE_SCRIPTS:
                        Marray_push(NodeHandle)(&ctx->script_nodes, ctx->allocator, node_handle);
                        break;
                    case NODE_DATA:
                        Marray_push(NodeHandle)(&ctx->data_nodes, ctx->allocator, node_handle);
                        break;
                    case NODE_IMAGE:
                        Marray_push(NodeHandle)(&ctx->img_nodes, ctx->allocator, node_handle);
                        break;
                    case NODE_IMGLINKS:
                        Marray_push(NodeHandle)(&ctx->imglinks_nodes, ctx->allocator, node_handle);
                        break;
                    case NODE_TITLE:
                        ctx->titlenode = node_handle;
                        break;
                    case NODE_NAV:
                        ctx->navnode = node_handle;
                        break;
                    default: break;
                    }
                node->type = type;
                goto foundit;
                }
            }
        }
    set_err(ctx, postcolon.text, "Unrecognized node name: '%.*s'", (int)boundary, postcolon.text);
    Raise(PARSE_ERROR);
    foundit:;
    StringView aftertype = {.text=postcolon.text + boundary, .length=postcolon.length-boundary};
    for(;;){
        eat_leading_tabspaces(&aftertype);
        if(!aftertype.length)
            break;
        switch(aftertype.text[0]){
            case '.':{
                advance_sv(&aftertype);
                eat_leading_tabspaces(&aftertype);
                const char* class_start = aftertype.text;
                while(aftertype.length){
                    char first = aftertype.text[0];
                    if(first == ' ' or first == '\t' or first == '@' or first == '.')
                        break;
                    advance_sv(&aftertype);
                    }
                size_t class_length = aftertype.text - class_start;
                if(!class_length){
                    set_err(ctx, aftertype.text, "Empty class name after a '.'");
                    Raise(PARSE_ERROR);
                    }
                auto class_ = Marray_alloc(StringView)(&node->classes, ctx->allocator);
                class_->length = class_length;
                class_->text = class_start;
                }break;
            case '@':{
                advance_sv(&aftertype);
                eat_leading_tabspaces(&aftertype);
                const char* attribute_start = aftertype.text;
                while(aftertype.length){
                    char first = aftertype.text[0];
                    if(first == ' ' or first == '\t' or first == '@' or first == '.' or first == '(')
                        break;
                    advance_sv(&aftertype);
                    }
                size_t attribute_length = aftertype.text - attribute_start;
                if(!attribute_length){
                    set_err(ctx, aftertype.text, "Empty attribute name after a '@'");
                    Raise(PARSE_ERROR);
                    }
                auto attr = Marray_alloc(Attribute)(&node->attributes, ctx->allocator);
                attr->key.length = attribute_length;
                attr->key.text = attribute_start;
                attr->value = SV("");
                if(aftertype.length){
                    eat_leading_tabspaces(&aftertype);
                    if(aftertype.length and aftertype.text[0] == '('){
                        size_t n_parens = 1;
                        advance_sv(&aftertype);
                        const char* valstart = aftertype.text;
                        for(;;){
                            if(!aftertype.length){
                                set_err(ctx, aftertype.text, "End of line when expecting a closing ')'");
                                Raise(PARSE_ERROR);
                                }
                            char first = aftertype.text[0];
                            if(first == '(')
                                n_parens++;
                            else if(first == ')')
                                n_parens--;
                            if(n_parens == 0)
                                break;
                            advance_sv(&aftertype);
                            }
                        size_t vallength = aftertype.text - valstart;
                        assert(aftertype.length);
                        advance_sv(&aftertype);
                        attr->value.text = valstart;
                        attr->value.length = vallength;
                        }
                    }
                }break;
            default:
                set_err(ctx, aftertype.text, "illegal character when parsing type, classes and attributes: '%c'", aftertype.text[0]);
                Raise(PARSE_ERROR);
            }
        }
    return result;
    }

static
void
print_node_and_children(Nonnull(ParseContext*)ctx, NodeHandle handle, int depth){
    auto node = get_node(ctx, handle);
    for(int i = 0 ; i < depth*2; i++){
        putchar(' ');
        }
    printf("[%-8s]", nodenames[node->type].text);
    switch((NodeType)node->type){
        case NODE_ROOT:
        case NODE_PARA:
        case NODE_TABLE_ROW:
        case NODE_BULLET:
        case NODE_LIST_ITEM:
        case NODE_KEYVALUEPAIR:
            break;
        case NODE_RAW:
        case NODE_PRE:
        case NODE_PYTHON:
        case NODE_BULLETS:
        case NODE_STYLESHEETS:
        case NODE_DEPENDENCIES:
        case NODE_LINKS:
        case NODE_SCRIPTS:
        case NODE_IMPORT:
        case NODE_IMAGE:
        case NODE_TABLE:
        case NODE_TEXT:
        case NODE_TITLE:
        case NODE_HEADING:
        case NODE_LIST:
        case NODE_COMMENT:
        case NODE_DATA:
        case NODE_NAV:
        case NODE_KEYVALUE:
        case NODE_IMGLINKS:
        case NODE_MD:
        case NODE_CONTAINER:
        case NODE_INVALID:
        case NODE_QUOTE:
        case NODE_DIV:{
            printf(" '%.*s' ", (int)node->header.length, node->header.text);
            for(size_t i = 0; i < node->classes.count; i++){
                auto c = &node->classes.data[i];
                printf(".%.*s ", (int)c->length, c->text);
                }
            for(size_t i = 0; i < node->attributes.count;i++){
                auto a = &node->attributes.data[i];
                printf("@%.*s", (int)a->key.length, a->key.text);
                if(a->value.length)
                    printf("(%.*s) ", (int)a->value.length, a->value.text);
                else
                    putchar(' ');
                }
            }break;
        case NODE_STRING:{
            printf(" '%.*s'", (int)node->header.length, node->header.text);
            }break;
        }
    putchar('\n');
    for(size_t i = 0; i < node->children.count; i++){
        print_node_and_children(ctx, node->children.data[i], depth+1);
        }
    }

/* Rendering */
static inline
force_inline
Errorable_f(void)
render_node(Nonnull(ParseContext*)ctx, Nonnull(MStringBuilder*) restrict sb, Nonnull(const Node*)node, int header_depth){
    bool hide = node_has_attribute(node, SV("hide"));
    if(hide) return (Errorable(void)){};
    return renderfuncs[node->type](ctx, sb, node, header_depth);
    }
static
Errorable_f(void)
render_tree(Nonnull(ParseContext*)ctx, Nonnull(MStringBuilder*)msb){
    Errorable(void) result = {};
    // estimate memory usage as 10 characters per node.
    // TODO: actually measure this.
    auto a = ctx->allocator;
    msb_reserve(msb, a, ctx->nodes.count*10);
    msb_write_literal(msb, a,
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">\n"
        );
    if(!ctx->rendered_data.count){
        msb_write_literal(msb, a, "<script>\nconst data_blob = {};\n</script>\n");
        }
    else{
        msb_write_literal(msb, a, "<script>\nconst data_blob = {");
        for(size_t i = 0; i < ctx->rendered_data.count; i++){
            auto data = &ctx->rendered_data.data[i];
            msb_write_char(msb, a, '"');
            msb_write_str(msb, a, data->key.text, data->key.length);
            msb_write_literal(msb, a, "\": \"");
            msb_write_json_escaped_str(msb, a, data->value.text, data->value.length);
            msb_write_literal(msb, a, "\",\n");
            }
        msb_write_literal(msb, a, "};\n</script>\n");
        }
    if(!NodeHandle_eq(ctx->titlenode, INVALID_NODE_HANDLE)){
        auto n = get_node(ctx, ctx->titlenode);
        msb_sprintf(msb, a, "<title>%.*s</title>\n", (int)n->header.length, n->header.text);
        }
    else {
        auto filename = path_basename(path_strip_extension(LS_to_SV(ctx->outputfile)));
        msb_write_literal(msb, a, "<title>");
        msb_write_title(msb, a, filename.text, filename.length);
        msb_write_literal(msb, a, "</title>\n");
        }
    if(ctx->stylesheets_nodes.count){
        msb_write_literal(msb, a, "<style>\n");
        for(size_t i = 0; i < ctx->stylesheets_nodes.count; i++){
            auto node = get_node(ctx, ctx->stylesheets_nodes.data[i]);
            // python nodes can change node types after they are registered
            if(unlikely(node->type != NODE_STYLESHEETS))
                continue;
            if(node_has_attribute(node, SV("inline"))){
                for(size_t j = 0; j < node->children.count; j++){
                    auto child = get_node(ctx, node->children.data[j]);
                    if(unlikely(child->type != NODE_STRING)){
                        node_print_warning(ctx, child, "Non-string child of a style sheet is being ignored.");
                        continue;
                        }
                    msb_write_str(msb, a, child->header.text, child->header.length);
                    msb_write_char(msb, a, '\n');
                    }
                }
            else{
                for(size_t j = 0; j < node->children.count; j++){
                    auto child = get_node(ctx, node->children.data[j]);
                    if(unlikely(child->type != NODE_STRING)){
                        node_print_warning(ctx, child, "Non-string child of a style sheet.");
                        continue;
                        }
                    if(!child->header.length)
                        continue;
                    auto style_e = load_source_file(ctx, child->header);
                    if(style_e.errored){
                        node_set_err(ctx, child, "Unable to load %.*s\n", (int)child->header.length, child->header.text);
                        Raise(style_e.errored);
                        }
                    auto style = unwrap(style_e);
                    msb_write_str(msb, a, style.text, style.length);
                    }
                }
            }
        msb_write_literal(msb, a, "</style>\n");
        }
    if(ctx->script_nodes.count){
        for(size_t i = 0; i < ctx->script_nodes.count; i++){
            msb_write_literal(msb, a, "<script>\n");
            auto node = get_node(ctx, ctx->script_nodes.data[i]);
            // python nodes can change node types after they are registered
            if(unlikely(node->type != NODE_SCRIPTS))
                continue;
            if(node_has_attribute(node, SV("inline"))){
                for(size_t j = 0; j < node->children.count; j++){
                    auto child = get_node(ctx, node->children.data[j]);
                    if(unlikely(child->type != NODE_STRING)){
                        node_print_warning(ctx, child, "script with a non-string child is being ignored");
                        continue;
                        }
                    auto header = child->header;
                    if(header.length)
                        msb_write_str(msb, a, header.text, header.length);
                    msb_write_char(msb, a, '\n');
                    }
                msb_write_literal(msb, a, "</script>\n");
                continue;
                }
            for(size_t j = 0; j < node->children.count; j++){
                auto child = get_node(ctx, node->children.data[j]);
                if(unlikely(child->type != NODE_STRING)){
                    node_print_warning(ctx, child, "script with a non-string child is being ignored");
                    continue;
                    }
                if(!child->header.length)
                    continue;
                auto script_e = load_source_file(ctx, child->header);
                if(script_e.errored){
                    node_set_err(ctx, child, "Unable to load %.*s\n", (int)child->header.length, child->header.text);
                    Raise(script_e.errored);
                    }
                auto script = unwrap(script_e);
                msb_write_str(msb, a, script.text, script.length);
                }
            msb_write_literal(msb, a, "</script>\n");
            }
        }
    msb_write_literal(msb, a, "</head>\n");
    msb_write_literal(msb, a, "<body>\n");
    auto root_node = get_node(ctx, ctx->root_handle);
    auto e = render_node(ctx, msb, root_node, 1);
    if(e.errored) return e;
    msb_write_literal(msb, a,
        "</body>\n"
        "</html>\n"
        );
    return result;
    }

static Errorable_f(void) check_node_depth(Nonnull(ParseContext*)ctx, NodeHandle handle, int depth);

static
Errorable_f(void)
check_depth(Nonnull(ParseContext*)ctx){
    return check_node_depth(ctx, ctx->root_handle, 0);
    }

static
Errorable_f(void)
check_node_depth(Nonnull(ParseContext*)ctx, NodeHandle handle, int depth){
    auto node = get_node(ctx, handle);
    enum {MAX_DEPTH=64};
    if(unlikely(depth > MAX_DEPTH)){
        node_set_err(ctx, node, "Tree depth exceeded: %d > %d", depth, MAX_DEPTH);
        return (Errorable(void)){.errored=PARSE_ERROR};
        }
    for(size_t i = 0; i < node->children.count; i++){
        auto e = check_node_depth(ctx, node->children.data[i], depth+1);
        if(e.errored) return e;
        }
    return (Errorable(void)){.errored=NO_ERROR};
    }

static void gather_anchor(Nonnull(ParseContext*)ctx, NodeHandle handle);
static
void
gather_anchors(Nonnull(ParseContext*)ctx){
    auto root = ctx->root_handle;
    return gather_anchor(ctx, root);
    }

static
void
gather_anchor_children(Nonnull(ParseContext*)ctx, Nonnull(Node*)node){
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        gather_anchor(ctx, children[i]);
        }
    }

static
void
gather_anchor(Nonnull(ParseContext*)ctx, NodeHandle handle){
    auto node = get_node(ctx, handle);
    switch(node->type){
        case NODE_BULLETS:
        case NODE_TABLE:
        case NODE_HEADING:
        case NODE_TITLE:
        case NODE_PARA:
        case NODE_DIV:
        case NODE_IMAGE:
        case NODE_TEXT:
        case NODE_LIST:
        case NODE_KEYVALUE:
        case NODE_IMGLINKS:
        case NODE_MD:
        case NODE_QUOTE:
        case NODE_CONTAINER:
            if(node->header.length and !node_has_attribute(node, SV("noid"))){
                auto id = node_get_attribute(node, SV("id"));
                if(unlikely(id)){
                    add_link_from_header(ctx, *id);
                    }
                else{
                    add_link_from_header(ctx, node->header);
                    }
                }
            // fall-through
        case NODE_DATA: // this is a little sketchy
        case NODE_ROOT:
        case NODE_IMPORT:
        case NODE_BULLET:
        case NODE_LIST_ITEM:
        case NODE_KEYVALUEPAIR:{
            gather_anchor_children(ctx, node);
            }
        case NODE_TABLE_ROW:
        case NODE_STYLESHEETS:
        case NODE_DEPENDENCIES:
        case NODE_LINKS:
        case NODE_SCRIPTS:
        case NODE_PYTHON:
        case NODE_STRING:
        case NODE_NAV:
        case NODE_COMMENT:
        case NODE_INVALID:
            break;
        case NODE_PRE:
        case NODE_RAW:
            if(node->header.length and !node_has_attribute(node, SV("noid"))){
                auto id = node_get_attribute(node, SV("id"));
                if(unlikely(id)){
                    add_link_from_header(ctx, *id);
                    }
                else{
                    add_link_from_header(ctx, node->header);
                    }
                }
            break;
        }
    }

static void build_nav_block_node(Nonnull(ParseContext*), NodeHandle, Nonnull(MStringBuilder*), int);
static void build_nav_block_children(Nonnull(ParseContext*), NodeHandle, Nonnull(MStringBuilder*), int);

static
void
build_nav_block(Nonnull(ParseContext*)ctx){
    MStringBuilder sb = {};
    auto a = ctx->allocator;
    msb_write_literal(&sb, a, "<nav>\n<ul>\n");
    build_nav_block_node(ctx, ctx->root_handle, &sb, 1);
    msb_write_literal(&sb, a, "</ul>\n</nav>");
    ctx->renderednav = msb_detach(&sb, ctx->allocator);
    }

static
void
build_nav_block_node(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(MStringBuilder*)sb, int depth){
    auto node = get_node(ctx, handle);
    switch(node->type){
        case NODE_BULLETS:
        case NODE_TABLE:
        case NODE_HEADING:
        case NODE_PARA:
        case NODE_DIV:
        case NODE_IMAGE:
        case NODE_TEXT:
        case NODE_LIST:
        case NODE_KEYVALUE:
        case NODE_IMGLINKS:
        case NODE_MD:
        case NODE_QUOTE:
        case NODE_CONTAINER:
            if(node->header.length and !node_has_attribute(node, SV("noid"))){
                auto a = ctx->allocator;
                auto id = node_get_attribute(node, SV("id"));
                if(likely(!id)){
                    id = &node->header;
                    }
                msb_write_literal(sb, a, "<li><a href=\"#");
                msb_write_kebab(sb, a, id->text, id->length);
                msb_sprintf(sb, a, "\">%.*s</a>\n<ul>\n", (int)node->header.length, node->header.text);
                // kind of a hack
                auto cursor = sb->cursor;
                build_nav_block_children(ctx, handle, sb, depth+1);
                if(cursor != sb->cursor){
                    msb_write_literal(sb, a, "</ul>\n");
                    }
                else{
                    msb_erase(sb, sizeof("\n<ul>\n")-1);
                    }
                msb_write_literal(sb, a, "</li>\n");
                break;
                }
            // fall-through
        case NODE_DATA: // this is a little sketchy
        case NODE_ROOT:
        case NODE_IMPORT:
        case NODE_BULLET:
        case NODE_LIST_ITEM:
        case NODE_KEYVALUEPAIR:{
            build_nav_block_children(ctx, handle, sb, depth);
            }break;
        case NODE_TITLE: // skip title as everything would be a child of it
        case NODE_TABLE_ROW:
        case NODE_STYLESHEETS:
        case NODE_DEPENDENCIES:
        case NODE_LINKS:
        case NODE_SCRIPTS:
        case NODE_PYTHON:
        case NODE_STRING:
        case NODE_NAV:
        case NODE_COMMENT:
        case NODE_INVALID:
            break;
        case NODE_PRE:
        case NODE_RAW:
            if(node->header.length and !node_has_attribute(node, SV("noid"))){
                auto a = ctx->allocator;
                auto id = node_get_attribute(node, SV("id"));
                if(likely(!id)){
                    id = &node->header;
                    }
                msb_write_literal(sb, a, "<li><a href=\"#");
                msb_write_kebab(sb, a, id->text, id->length);
                msb_sprintf(sb, a, "\">%.*s</a>", (int)node->header.length, node->header.text);
                msb_write_literal(sb, a, "</li>\n");
                }
            break;
        }
    }

static
void
build_nav_block_children(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(MStringBuilder*)sb, int depth){
    if(depth > 2)
        return;
    auto node = get_node(ctx, handle);
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        build_nav_block_node(ctx, children[i], sb, depth);
        }
    }

static inline
void
write_tag_escaped_str(Nonnull(ParseContext*) ctx, Nonnull(MStringBuilder*)sb, Nonnull(const char*)text, size_t length){
    for(size_t i = 0; i < length; i++){
        char c = text[i];
        if(unlikely(c == '&')){
            msb_write_literal(sb, ctx->allocator, "&amp;");
            }
        else if(unlikely(c == '<')){
            msb_write_literal(sb, ctx->allocator, "&lt;");
            }
        else if(unlikely(c == '>')){
            msb_write_literal(sb, ctx->allocator, "&gt;");
            }
        else {
            msb_write_char(sb, ctx->allocator, c);
            }
        }
    }


static inline
Errorable_f(void)
write_link_escaped_str(Nonnull(ParseContext*) ctx, Nonnull(MStringBuilder*)sb, Nonnull(const char*)text, size_t length, Nonnull(const Node*)node){
    Errorable(void) result = {};
    for(size_t i = 0; i < length; i++){
        char c = text[i];
        if(unlikely(c == '[')){
            msb_write_literal(sb, ctx->allocator, "<a href=\"");
            const char* closing_brace = memchr(text+i, ']', length-i);
            if(!closing_brace){
                MStringBuilder eb = {};
                msb_sprintf(&eb, ctx->allocator, "%.*s:%d:%d: Unterminated '['", (int)node->filename.length, node->filename.text, node->row+1, node->col+1+(int)i);
                ctx->error_message = msb_detach(&eb, ctx->allocator);
                Raise(PARSE_ERROR);
                }
            size_t link_length = closing_brace - (text+i);
            {
            MStringBuilder temp = {};
            msb_write_kebab(&temp, ctx->temp_allocator, text+i+1, link_length-1);
            auto temp_str = msb_borrow(&temp, ctx->temp_allocator);
            auto value = find_link_target(ctx, temp_str);
            if(!value){
                if(ctx->flags & PARSE_ALLOW_BAD_LINKS){
                    node_print_warning(ctx, node, "Unable to resolve link '%.*s'", (int)temp_str.length, temp_str.text);
                    msb_write_str(sb, ctx->allocator, temp_str.text, temp_str.length);
                    }
                else {
                    node_set_err(ctx, node, "Unable to resolve link '%.*s'", (int)temp_str.length, temp_str.text);
                    msb_destroy(&temp, ctx->temp_allocator);
                    Raise(PARSE_ERROR);
                    }
                }
            else {
                StringView* val = value;
                msb_write_str(sb, ctx->allocator, val->text, val->length);
                }
            msb_destroy(&temp, ctx->temp_allocator);
            }
            msb_write_literal(sb, ctx->allocator, "\">");
            msb_write_str(sb, ctx->allocator, text+i+1, link_length-1);
            msb_write_literal(sb, ctx->allocator, "</a>");
            i += link_length;
            continue;
            }
        else if(unlikely(c == '-')){
            if(i < length - 1){
                auto peek1 = text[i+1];
                if(peek1 == '-'){
                    if(i < length - 2){
                        auto peek2 = text[i+2];
                        if(peek2 == '-'){
                            msb_write_literal(sb, ctx->allocator, "&mdash;");
                            i += 2;
                            continue;
                            }
                        }
                        msb_write_literal(sb, ctx->allocator, "&ndash;");
                        i += 1;
                        continue;
                    }
                }
            msb_write_char(sb, ctx->allocator, c);
            }
        else if(unlikely(c == '&')){
            msb_write_literal(sb, ctx->allocator, "&amp;");
            }
        else if(unlikely(c == '<')){
            // we allow inline <b>, <s>, <i>, </b>, </s>, </i>
            if(i < length - 1){
                auto peek1 = text[i+1];
                switch(peek1){
                    case 'b':
                    case 's':
                    case 'i':
                    case '/':
                        break;
                    default:
                        msb_write_literal(sb, ctx->allocator, "&lt;");
                        continue;
                    }
                if(i < length - 2){
                    auto peek2 = text[i+2];
                    if(peek1 != '/'){
                        if(peek2 == '>'){
                            msb_write_char(sb, ctx->allocator, c);
                            msb_write_char(sb, ctx->allocator, peek1);
                            msb_write_char(sb, ctx->allocator, peek2);
                            i += 2;
                            continue;
                            }
                        msb_write_literal(sb, ctx->allocator, "&lt;");
                        continue;
                        }
                    switch(peek2){
                        case 'b':
                        case 's':
                        case 'i':
                            break;
                        default:
                            msb_write_literal(sb, ctx->allocator, "&lt;");
                            continue;
                        }
                    if(i < length - 3){
                        auto peek3 = text[i+3];
                        if(peek3 == '>'){
                            msb_write_char(sb, ctx->allocator, c);
                            msb_write_char(sb, ctx->allocator, peek1);
                            msb_write_char(sb, ctx->allocator, peek2);
                            msb_write_char(sb, ctx->allocator, peek3);
                            i += 3;
                            continue;
                            }
                        }
                    }
                }
            msb_write_literal(sb, ctx->allocator, "&lt;");
            }
        else if(unlikely(c == '>')){
            msb_write_literal(sb, ctx->allocator, "&gt;");
            }
        else {
            msb_write_char(sb, ctx->allocator, c);
            }
        }
    return result;
    }

static inline
Errorable_f(void)
write_header(Nonnull(ParseContext*)ctx, Nonnull(MStringBuilder*)sb, Nonnull(const char*)text, size_t length, Nonnull(const Node*)node, int header_level){
    bool no_id = node_has_attribute(node, SV("noid"));
    if(no_id)
        msb_sprintf(sb, ctx->allocator, "<h%d>", header_level);
    else{
        auto id = node_get_attribute(node, SV("id"));
        const char* id_text = id?id->text:text;
        size_t id_length = id?id->length:length;
        msb_sprintf(sb, ctx->allocator, "<h%d id=\"", header_level);
        msb_write_kebab(sb, ctx->allocator, id_text, id_length);
        msb_write_literal(sb, ctx->allocator, "\">");
        }
    auto e = write_link_escaped_str(ctx, sb, text, length, node);
    if(e.errored) return e;
    msb_sprintf(sb, ctx->allocator, "</h%d>", header_level);
    return (Errorable(void)){};
    }

static inline
void
write_classes(Nonnull(ParseContext*)ctx, Nonnull(MStringBuilder*)sb, Nonnull(const Node*)node){
    auto count = node->classes.count;
    if(!count) return;
    auto classes = node->classes.data;
    msb_write_literal(sb, ctx->allocator, " class=\"");
    for(size_t i = 0; i < count; i++){
        if(i != 0){
            msb_write_char(sb, ctx->allocator, ' ');
            }
        auto c = &classes[i];
        msb_write_str(sb, ctx->allocator, c->text, c->length);
        }
    msb_write_char(sb, ctx->allocator, '"');
    return;
    }

RENDERFUNC(ROOT){
    auto childs = &node->children;
    auto count = childs->count;
    for(size_t i = 0; i < count; i++){
        auto child_handle = childs->data[i];
        auto child = get_node(ctx, child_handle);
        auto e = render_node(ctx, sb, child, header_depth);
        if(unlikely(e.errored))
            return e;
        }
    return (Errorable(void)){};
    }

RENDERFUNC(STRING){
    if(unlikely(node->classes.count))
        node_print_warning(ctx, node, "Ignoring classes on string node");
    if(unlikely(node->children.count))
        node_print_warning(ctx, node, "Ignoring children of string node");
    auto e = write_link_escaped_str(ctx, sb, node->header.text, node->header.length, node);
    if(e.errored) return e;
    msb_write_char(sb, ctx->allocator, '\n');
    (void)header_depth;
    return (Errorable(void)){};
    }

RENDERFUNC(TEXT){
    msb_write_literal(sb, ctx->allocator, "<div");
    write_classes(ctx, sb, node);
    msb_write_literal(sb, ctx->allocator, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
        if(e.errored) return e;
        msb_write_char(sb, ctx->allocator, '\n');
        }
    auto children = &node->children;
    auto count = children->count;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children->data[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(DIV){
    msb_write_literal(sb, ctx->allocator, "<div");
    write_classes(ctx, sb, node);
    msb_write_literal(sb, ctx->allocator, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
        if(e.errored) return e;
        msb_write_char(sb, ctx->allocator, '\n');
        }
    auto children = &node->children;
    auto count = children->count;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children->data[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(NAV){
    (void)header_depth;
    if(node->header.length){
        node_print_warning(ctx, node, "Headers on navs unsupported");
        }
    if(node->children.count){
        node_print_warning(ctx, node, "Children on navs unsupported");
        }
    msb_write_str(sb, ctx->allocator, ctx->renderednav.text, ctx->renderednav.length);
    return (Errorable(void)){};
    }
RENDERFUNC(PARA){
    if(node->classes.count){
        // maybe we should allow classes though?
        node_print_warning(ctx, node, "Ignoring classes on paragraph node");
        }
    if(node->header.length){
        node_print_warning(ctx, node, "Ignoring header on paragraph node");
        }
    msb_write_literal(sb, ctx->allocator, "<p>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child_handle = children[i];
        auto child = get_node(ctx, child_handle);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "</p>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(TITLE){
    auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
    if(e.errored) return e;
    msb_write_char(sb, ctx->allocator, '\n');
    if(node->children.count){
        node_print_warning(ctx, node, "Ignoring children of title");
        }
    if(node->classes.count){
        node_print_warning(ctx, node, "UNIMPLEMENTED: classes on the title");
        }
    return (Errorable(void)){};
    }
RENDERFUNC(HEADING){
    auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
    if(e.errored) return e;
    msb_write_char(sb, ctx->allocator, '\n');
    if(node->children.count){
        node_print_warning(ctx, node, "Ignoring children of heading");
        }
    if(node->classes.count){
        node_print_warning(ctx, node, "UNIMPLEMENTED: classes on the heading");
        }
    return (Errorable(void)){};
    }
RENDERFUNC(TABLE){
    msb_write_literal(sb, ctx->allocator, "<div");
    write_classes(ctx, sb, node);
    msb_write_literal(sb, ctx->allocator, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "<table>\n<thead>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    if(count){
        auto child = get_node(ctx, children[0]);
        if(child->type != NODE_TABLE_ROW){
            node_set_err(ctx, child, "children of a table ought to be table rows...");
            return (Errorable(void)){.errored=GENERIC_ERROR};
            }
        // inline rendering table row here so we can do heads
        msb_write_literal(sb, ctx->allocator, "<tr>\n");
        auto child_count = child->children.count;
        auto child_children = child->children.data;
        for(size_t i = 0; i < child_count; i++){
            auto child_child = get_node(ctx, child_children[i]);
            msb_write_literal(sb, ctx->allocator, "<th>");
            auto e = render_node(ctx, sb, child_child, header_depth);
            if(e.errored) return e;
            msb_write_literal(sb, ctx->allocator, "</th>\n");
            }
        msb_write_literal(sb, ctx->allocator, "</tr>\n");
        }
    msb_write_literal(sb, ctx->allocator, "</thead>\n<tbody>\n");
    for(size_t i = 1; i < count; i++){
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "</tbody></table>\n</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(TABLE_ROW){
    // TODO: odd even class?
    msb_write_literal(sb, ctx->allocator, "<tr>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        msb_write_literal(sb, ctx->allocator, "<td>");
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        msb_write_literal(sb, ctx->allocator, "</td>\n");
        }
    msb_write_literal(sb, ctx->allocator, "</tr>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(STYLESHEETS){
    // intentionally do not render stylesheets
    (void)ctx;
    (void)sb;
    (void)node;
    (void)header_depth;
    return (Errorable(void)){};
    }
RENDERFUNC(DEPENDENCIES){
    // intentionally do not render dependencies
    (void)ctx;
    (void)sb;
    (void)node;
    (void)header_depth;
    return (Errorable(void)){};
    }
RENDERFUNC(LINKS){
    // intentionally do not render links
    (void)ctx;
    (void)sb;
    (void)node;
    (void)header_depth;
    return (Errorable(void)){};
    }
RENDERFUNC(SCRIPTS){
    // intentionally do not render scripts
    (void)ctx;
    (void)sb;
    (void)node;
    (void)header_depth;
    return (Errorable(void)){};
    }
RENDERFUNC(IMPORT){
    // An imports members are replaced with containers that were the things
    // they imported.
    // Don't render the import itself though.
    if(node->header.length){
        node_print_warning(ctx, node, "Ignoring import header");
        }
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    return (Errorable(void)){};
    }
RENDERFUNC(IMAGE){
    Errorable(void) result = {};
    msb_write_literal(sb, ctx->allocator, "<div");
    write_classes(ctx, sb, node);
    msb_write_literal(sb, ctx->allocator, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
        if(e.errored) return e;
        msb_write_char(sb, ctx->allocator, '\n');
        }
    if(!node->children.count){
        node_set_err(ctx, node, "Image node missing any children (first should be a string that is path to the image");
        Raise(PARSE_ERROR);
        }
    auto children = &node->children;
    {
        auto first_child = get_node(ctx, children->data[0]);
        if(first_child->type != NODE_STRING){
            node_set_err(ctx, first_child, "First child of an imagee node should be a string that is path to the image.");
            Raise(PARSE_ERROR);
            }
        auto imgpath_node = get_node(ctx, node->children.data[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, "First should be a string and be the path to the image.");
            Raise(PARSE_ERROR);
            }
        auto header = imgpath_node->header;
        auto processed_e = load_processed_binary_file(ctx, header);
        if(processed_e.errored){
            // HERE("Falling back to old loading");
            auto load_e = load_binary_file(ctx, header);
            if(load_e.errored){
                node_set_err(ctx, imgpath_node, "Unable to read '%.*s'", (int)header.length, header.text);
                Raise(load_e.errored);
                }
            ByteBuffer imgdata = unwrap(load_e);
            msb_write_literal(sb, ctx->allocator, "<img src=\"data:image/png;base64,");
            auto before = get_t();
            msb_write_b64(sb, ctx->allocator, imgdata.buff, imgdata.n_bytes);
            auto after = get_t();

            report_stat(ctx->flags, "Base64ing '%.*s' took %.3fms", (int)header.length, header.text, (after-before)/1000.);
            }
        else {
            msb_write_literal(sb, ctx->allocator, "<img src=\"data:image/png;base64,");
            auto b64 = unwrap(processed_e);
            msb_write_str(sb, ctx->allocator, b64.text, b64.length);
            }
        msb_write_literal(sb, ctx->allocator, "\">");
    }
    auto count = children->count;
    for(size_t i = 1; i < count; i++){
        auto child = get_node(ctx, children->data[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "</div>\n");
    return result;
    }
RENDERFUNC(BULLETS){
    // I should probably be checking if the parent of this node is a bullet
    // so that I don't output these divs unnnecessarily.
    // But maybe I should do that in the parse phase (distinguish between bullets
    // and nested bullets?).
    msb_write_literal(sb, ctx->allocator, "<div");
    write_classes(ctx, sb, node);
    msb_write_literal(sb, ctx->allocator, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "<ul>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "</ul>\n</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(QUOTE){
    msb_write_literal(sb, ctx->allocator, "<div");
    write_classes(ctx, sb, node);
    msb_write_literal(sb, ctx->allocator, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "<blockquote>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "</blockquote>\n</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(BULLET){
    msb_write_literal(sb, ctx->allocator, "<li>\n");
    if(unlikely(node->header.length))
        node_print_warning(ctx, node, "ignoring header on bullet");
    if(unlikely(node->classes.count))
        node_print_warning(ctx, node, "Ignoring classes on bullet");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        if(i != 0)
            msb_write_char(sb, ctx->allocator, ' ');
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "</li>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(PYTHON){
    // intentionally not outputting this
    (void)ctx;
    (void)sb;
    (void)node;
    (void)header_depth;
    return (Errorable(void)){};
    }
RENDERFUNC(RAW){
    // ignoring the header for now. Idk what the semantics are supposed to be.
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        if(unlikely(child->type != NODE_STRING))
            node_print_warning(ctx, child, "Raw node with a non-string child");
        msb_write_str(sb, ctx->allocator, child->header.text, child->header.length);
        msb_write_char(sb, ctx->allocator, '\n');
        }
    (void)header_depth;
    return (Errorable(void)){};
    }
RENDERFUNC(PRE){
    msb_write_literal(sb, ctx->allocator, "<div");
    write_classes(ctx, sb, node);
    msb_write_literal(sb, ctx->allocator, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "<pre>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        if(unlikely(child->type != NODE_STRING))
            node_print_warning(ctx, child, "pre node with a non-string child");
        write_tag_escaped_str(ctx, sb, child->header.text, child->header.length);
        msb_write_char(sb, ctx->allocator, '\n');
        }
    msb_write_literal(sb, ctx->allocator, "</pre>\n</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(LIST){
    // I should probably be checking if the parent of this node is a list item
    // so that I don't output these divs unnnecessarily.
    // But maybe I should do that in the parse phase (distinguish between lists
    // and nested lists?).
    msb_write_literal(sb, ctx->allocator, "<div");
    write_classes(ctx, sb, node);
    msb_write_literal(sb, ctx->allocator, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "<ol>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "</ol>\n</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(LIST_ITEM){
    msb_write_literal(sb, ctx->allocator, "<li>");
    if(unlikely(node->header.length))
        node_print_warning(ctx, node, "ignoring header on list item");
    if(unlikely(node->classes.count))
        node_print_warning(ctx, node, "Ignoring classes on list item");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        if(i != 0)
            msb_write_char(sb, ctx->allocator, ' ');
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "</li>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(KEYVALUE){
    msb_write_literal(sb, ctx->allocator, "<div");
    write_classes(ctx, sb, node);
    msb_write_literal(sb, ctx->allocator, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "<table><tbody>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "</tbody></table>\n</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(KEYVALUEPAIR){
    // TODO: maybe this should be lowered into a table row node?
    // TODO: odd even class?
    msb_write_literal(sb, ctx->allocator, "<tr>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        msb_write_literal(sb, ctx->allocator, "<td>");
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        msb_write_literal(sb, ctx->allocator, "</td>\n");
        }
    msb_write_literal(sb, ctx->allocator, "</tr>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(IMGLINKS){
    Errorable(void) result = {};
    msb_write_literal(sb, ctx->allocator, "<div");
    write_classes(ctx, sb, node);
    msb_write_literal(sb, ctx->allocator, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
        if(e.errored) return e;
        }
    if(node->children.count < 4){
        node_set_err(ctx, node, "Too few children of an imglinks node (expected path to the image, width, height, viewBox in that order)");
        Raise(PARSE_ERROR);
        }

    LongString imgdatab64 = {};
    ByteBuffer imgdata;
    {
        auto imgpath_node = get_node(ctx, node->children.data[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, "First should be a string and be the path to the image");
            Raise(PARSE_ERROR);
            }
        auto header = imgpath_node->header;
        auto processed_e = load_processed_binary_file(ctx, header);
        if(processed_e.errored){
            // HERE("Falling back to old process");
            auto e = load_binary_file(ctx, header);
            if(e.errored){
                node_set_err(ctx, imgpath_node, "Unable to read '%.*s'", (int)header.length, header.text);
                Raise(e.errored);
                }
            imgdata = unwrap(e);
            }
        else {
            imgdatab64 = unwrap(processed_e);
            }
    }
    int width;
    {
        auto width_node = get_node(ctx, node->children.data[1]);
        if(width_node->type != NODE_STRING){
            node_set_err(ctx, width_node, "Second should be a string and be 'width = WIDTH'");
            Raise(PARSE_ERROR);
            }
        auto header = width_node->header;
        auto pair = stripped_split(header.text, header.length, '=');
        if(pair.head.length == header.length){
            node_set_err(ctx, width_node, "Missing a '='");
            Raise(PARSE_ERROR);
            }
        if(!SV_equals(pair.head, SV("width"))){
            node_set_err(ctx, width_node, "Expected 'width', got '%.*s'", (int)pair.head.length, pair.head.text);
            Raise(PARSE_ERROR);
            }
        auto e = parse_int(pair.tail.text, pair.tail.length);
        if(e.errored){
            node_set_err(ctx, width_node, "Unable to parse an int from '%.*s'", (int)pair.tail.length, pair.tail.text);
            Raise(PARSE_ERROR);
            }
        width = unwrap(e);
    }
    int height;
    {
        auto height_node  = get_node(ctx, node->children.data[2]);
        if(height_node->type != NODE_STRING){
            node_set_err(ctx, height_node, "Third should be a string and be 'height = HEIGHT'");
            Raise(PARSE_ERROR);
            }
        auto header = height_node->header;
        auto pair = stripped_split(header.text, header.length, '=');
        if(pair.head.length == header.length){
            node_set_err(ctx, height_node, "Missing a '='");
            Raise(PARSE_ERROR);
            }
        if(!SV_equals(pair.head, SV("height"))){
            node_set_err(ctx, height_node, "Expected 'height', got '%.*s'", (int)pair.head.length, pair.head.text);
            Raise(PARSE_ERROR);
            }
        auto e = parse_int(pair.tail.text, pair.tail.length);
        if(e.errored){
            node_set_err(ctx, height_node, "Unable to parse an int from '%.*s'", (int)pair.tail.length, pair.tail.text);
            Raise(PARSE_ERROR);
            }
        height = unwrap(e);
    }
    int viewbox[4];
    {
        auto viewBox_node = get_node(ctx, node->children.data[3]);
        if(viewBox_node->type != NODE_STRING){
            node_set_err(ctx, viewBox_node, "Fourth should be a string and be 'viewbox = x0 y0 x1 y1'");
            Raise(PARSE_ERROR);
            }
        auto header = viewBox_node->header;
        const char* equals = memchr(header.text, '=', header.length);
        if(!equals){
            node_set_err(ctx, viewBox_node, "Missing a '='");
            Raise(PARSE_ERROR);
            }
        StringView lead = stripped_view(header.text, equals - header.text);
        if(!SV_equals(lead, SV("viewBox"))){
            node_set_err(ctx, viewBox_node, "Expected 'viewBox', got '%.*s'", (int)lead.length, lead.text);
            Raise(PARSE_ERROR);
            }
        const char* cursor = equals+1;
        int which = 0;
        const char* end = header.text + header.length;
        for(;;){
            if(cursor == end){
                node_set_err(ctx, viewBox_node, "Unexpected end of line before we finished parsing the ints");
                Raise(PARSE_ERROR);
                }
            switch(*cursor){
                case ' ': case '\t': case '\r': case '\n':
                    cursor++;
                    continue;
                case '0' ... '9':
                    break;
                default:
                    node_set_err(ctx, viewBox_node, "Found non-numeric when trying to parse the viewBox");
                    Raise(PARSE_ERROR);
                }
            const char* after_number = cursor+1;
            for(;;){
                if(after_number == end)
                    break;
                switch(*after_number){
                    case '0' ... '9':
                        after_number++;
                        continue;
                    default:
                        break;
                    }
                break;
                }
            auto num_length = after_number - cursor;
            auto e = parse_int(cursor, num_length);
            if(e.errored){
                node_set_err(ctx, viewBox_node, "Failed to parse an int from '%.*s'", (int)num_length, cursor);
                Raise(PARSE_ERROR);
                }
            viewbox[which++] = unwrap(e);
            cursor = after_number;
            if(which == 4)
                break;
            }
        // at this point we should have 4 ints and only trailing whitespace
        assert(which == 4);
        while(cursor != end){
            switch(*cursor){
                case ' ': case '\t': case '\r': case '\n':
                    cursor++;
                    continue;
                default:
                    node_set_err(ctx, viewBox_node, "Found trailing text after successfully parsing 4 ints: '%.*s'", (int)(end - cursor), cursor);
                    Raise(PARSE_ERROR);
                }
            }
    }
    msb_sprintf(sb, ctx->allocator, "<svg width=\"%d\" height=\"%d\" viewbox=\"%d %d %d %d\" style=\"background-image: url('data:image/png;base64,", width, height, viewbox[0], viewbox[1], viewbox[2], viewbox[3]);
    auto before = get_t();
    if(imgdatab64.length){
        msb_write_str(sb, ctx->allocator, imgdatab64.text, imgdatab64.length);
        }
    else{
        msb_write_b64(sb, ctx->allocator, imgdata.buff, imgdata.n_bytes);
        Allocator_free(ctx->temp_allocator, imgdata.buff, imgdata.n_bytes);
        }
    auto after = get_t();
    report_stat(ctx->flags, "Base64ing an imglinks took %.3fms", (after-before)/1000.);
    msb_write_literal(sb, ctx->allocator, "');\">\n");
    for(size_t i = 4; i < node->children.count; i++){
        auto child = get_node(ctx, node->children.data[i]);
        if(child->type != NODE_STRING){
            // TODO: this lets us skip embedded python nodes, but we should
            // error on other nodes probably.
            if(child->type == NODE_PYTHON)
                continue;
            node_print_warning(ctx, child, "Non-string node child of imglinks node: '%s'", nodenames[child->type].text);
            continue;
            }
        auto header = child->header;
        auto end = header.text + header.length;
        const char* equals = memchr(header.text, '=', header.length);
        if(!equals){
            node_set_err(ctx, child, "No '=' found in an imglinks line");
            Raise(PARSE_ERROR);
            }
        const char* at = memchr(equals, '@', end - equals);
        if(!at){
            node_set_err(ctx, child, "No '@' found in an imglinks line");
            Raise(PARSE_ERROR);
            }
        const char* comma = memchr(at, ',', end - at);
        if(!comma){
            node_set_err(ctx, child, "No ',' found in an imglinks line separating the coordinates");
            Raise(PARSE_ERROR);
            }
        auto first = stripped_view(header.text, equals - header.text);
        auto second = stripped_view(equals+1, at - (equals + 1));
        auto third = stripped_view(at+1, comma - (at+1));
        auto fourth = stripped_view(comma+1, end - (comma+1));
        auto x_err = parse_int(third.text, third.length);
        if(x_err.errored){
            node_set_err(ctx, child, "Unable to parse an int from '%.*s'", (int)third.length, third.text);
            Raise(x_err.errored);
            }
        auto x = unwrap(x_err);
        auto y_err = parse_int(fourth.text, fourth.length);
        if(y_err.errored){
            node_set_err(ctx, child, "Unable to parse an int from '%.*s'", (int)third.length, third.text);
            Raise(y_err.errored);
            }
        auto y = unwrap(y_err);
        msb_sprintf(sb, ctx->allocator,
                "<a href=\"%.*s\"><text transform=\"translate(%d,%d)\">\n"
                "%.*s\n"
                "</text></a>\n", (int)second.length, second.text, x, y, (int)first.length, first.text);
        }
    msb_write_literal(sb, ctx->allocator, "</svg>\n");
    msb_write_literal(sb, ctx->allocator, "</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(DATA){
    // intentionally not rendering this
    (void)ctx;
    (void)sb;
    (void)node;
    (void)header_depth;
    return (Errorable(void)){};
    }
RENDERFUNC(COMMENT){
    // intentionally not rendering a comment
    (void)ctx;
    (void)sb;
    (void)node;
    (void)header_depth;
    return (Errorable(void)){};
    }
RENDERFUNC(MD){
    msb_write_literal(sb, ctx->allocator, "<div");
    write_classes(ctx, sb, node);
    msb_write_literal(sb, ctx->allocator, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node->header.text, node->header.length, node, header_depth);
        if(e.errored) return e;
        msb_write_char(sb, ctx->allocator, '\n');
        }
    auto children = &node->children;
    auto count = children->count;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children->data[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, ctx->allocator, "</div>\n");
    return (Errorable(void)){};
    return (Errorable(void)){};
    }
RENDERFUNC(CONTAINER){
    if(node->header.length){
        node_print_warning(ctx, node, "Ignoring container header.");
        }
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    return (Errorable(void)){};
    }
RENDERFUNC(INVALID){
    node_set_err(ctx, node, "Invalid node when rendering.");
    (void)ctx;
    (void)sb;
    (void)node;
    (void)header_depth;
    return (Errorable(void)){.errored=GENERIC_ERROR};
    }



/* Python */

#include "pyhead.h"
// not a fan that the includes are so generic (why not <Python/code.h>?)
#include <frameobject.h>
#include <code.h>

PushDiagnostic();
SuppressUnusedFunction();
static inline
LongString
pystring_to_longstring(Nonnull(PyObject*)pyobj, Nonnull(const Allocator*)a){
    const char* text;
    Py_ssize_t length;
    text = PyUnicode_AsUTF8AndSize(pyobj, &length);
    assert(text);
    if(!length){
        return (LongString){};
        }
    char* copy = Allocator_dupe(a, text, length+1);
    return (LongString){
        .text = copy,
        .length = length,
        };
    }
PopDiagnostic();

static inline
StringView
pystring_to_stringview(Nonnull(PyObject*)pyobj, Nonnull(const Allocator*)a){
    const char* text;
    Py_ssize_t length;
    text = PyUnicode_AsUTF8AndSize(pyobj, &length);
    assert(text);
    if(!length){
        return (StringView){};
        }
    char* copy = Allocator_dupe(a, text, length);
    return (StringView){
        .text = copy,
        .length = length,
        };
    }
static inline
StringView
pystring_borrow_stringview(Nonnull(PyObject*)pyobj){
    const char* text;
    Py_ssize_t length;
    text = PyUnicode_AsUTF8AndSize(pyobj, &length);
    assert(text);
    return (StringView){.text=text, .length=length};
    }


typedef struct NodeTypeEnum {
    PyObject_HEAD
    NodeType type;
    }NodeTypeEnum;

static
Nullable(PyObject*)
NodeTypeEnum_repr(Nonnull(NodeTypeEnum*)e){
    if(e->type > NODE_INVALID or e->type < 0){
        PyErr_Format(PyExc_RuntimeError, "Somehow we have an enum with an invalid value: %d", (int)e->type);
        return NULL;
        }
    auto name = nodenames[e->type];
    return PyUnicode_FromFormat("NodeType.%s", name.text);
    }

static
PyObject* _Nullable
NodeTypeEnum_getattr(Nonnull(NodeTypeEnum*)e, Nonnull(const char*)name){
    if(e->type > NODE_INVALID or e->type < 0){
        PyErr_Format(PyExc_RuntimeError, "Somehow we have an enum with an invalid value: %d", (int)e->type);
        return NULL;
        }
    if(strcmp(name, "name")==0){
        auto enu_name = nodenames[e->type];
        return PyUnicode_FromStringAndSize(enu_name.text, enu_name.length);
        }
    if(strcmp(name, "value")==0){
        return PyLong_FromLong(e->type);
        }
    PyErr_Format(PyExc_AttributeError, "Unknown attribute on NodeTypeEnum: %s", name);
    return NULL;
    }

// decl
static PyTypeObject NodeTypeEnumType;

static
PyObject* _Nullable
NodeTypeEnum_richcmp(Nonnull(PyObject*)a, Nonnull(PyObject*)b, int cmp){
    auto check = PyObject_IsInstance(b, (PyObject*)&NodeTypeEnumType);
    if(check == -1)
        return NULL;
    if(check == 0){
        Py_RETURN_NOTIMPLEMENTED;
        }
    auto lhs = (NodeTypeEnum*)a;
    auto rhs = (NodeTypeEnum*)b;
    if(cmp == Py_EQ){
        if(lhs->type == rhs->type)
            Py_RETURN_TRUE;
        else
            Py_RETURN_FALSE;
        }
    if(cmp == Py_NE){
        if(lhs->type != rhs->type)
            Py_RETURN_TRUE;
        else
            Py_RETURN_FALSE;
        }
    Py_RETURN_NOTIMPLEMENTED;
    }

// definition
static PyTypeObject NodeTypeEnumType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "docparser.NodeType",
    .tp_basicsize = sizeof(NodeTypeEnum),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "NodeType Enum",
    .tp_repr = (reprfunc)NodeTypeEnum_repr,
    .tp_getattr = (getattrfunc)&NodeTypeEnum_getattr,
    .tp_richcompare = &NodeTypeEnum_richcmp,
    };

static
Nonnull(PyObject*)
make_node_type_enum(NodeType t){
    NodeTypeEnum* self = (NodeTypeEnum*)NodeTypeEnumType.tp_alloc(&NodeTypeEnumType, 0);
    assert(self);
    self->type = t;
    return (PyObject*)self;
    }

typedef Nullable(PyObject*) (*_Nonnull NodeMethod)(Nonnull(ParseContext*), NodeHandle, Nonnull(PyObject*), Nullable(PyObject*));

typedef struct NodeBoundMethod {
    PyObject_HEAD
    Nonnull(ParseContext*)ctx;
    NodeHandle handle;
    NodeMethod func;
    } NodeBoundMethod;

static
Nullable(PyObject*)
NodeBound_call(Nonnull(PyObject*)self, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    auto meth = (NodeBoundMethod*)self;
    return meth->func(meth->ctx, meth->handle, args, kwargs);
    }

static PyTypeObject NodeBoundMethodType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "docparser.NodeBoundMethod",
    .tp_basicsize = sizeof(NodeBoundMethod),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Node bound method",
    .tp_call = &NodeBound_call,
    };

static
Nonnull(PyObject*)
make_node_bound_method(Nonnull(ParseContext*)ctx, NodeHandle handle, NodeMethod func){
    NodeBoundMethod* self = (NodeBoundMethod*)NodeBoundMethodType.tp_alloc(&NodeBoundMethodType, 0);
    assert(self);
    self->ctx = ctx;
    self->handle = handle;
    self->func = func;
    return (PyObject*)self;
    }

Nullable(PyObject*)
py_parse_and_append_children(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    const char* text;
    Py_ssize_t length;
    const char* const keywords[] = { "text", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "s#:parse_and_append_children", (char**)keywords, &text, &length)){
        return NULL;
        }
    PopDiagnostic();
    // We dupe this as we have no guarantee that the python
    // string will last beyond this execution and we store pointers
    // into the original source string.
    char* copy = Allocator_strndup(ctx->allocator, text, length);
    auto old_filename = ctx->filename;
    set_context_source(ctx, SV("(generated string from script)"), copy);
    // We store it in case we need to be able to clean up ourselves later.
    auto string_store = Marray_alloc(LongString)(&ctx->loaded_strings, ctx->allocator);
    *string_store = (LongString){.text=copy, .length=length};

    auto parse_e = parse(ctx, handle);
    if(parse_e.errored){
        PyErr_SetString(PyExc_ValueError, ctx->error_message.text);
        return NULL;
        }
    ctx->filename = old_filename;
    Py_RETURN_NONE;
    }

typedef struct DndClassesList {
    PyObject_HEAD
    Nonnull(ParseContext*)ctx;
    NodeHandle handle;
    } DndClassesList;

static
Py_ssize_t
DndClasses_length(Nonnull(DndClassesList*)list){
    auto node = get_node(list->ctx, list->handle);
    return (Py_ssize_t)node->classes.count;
    }

static
Nullable(PyObject*)
DndClasses_getitem(Nonnull(DndClassesList*)list, Py_ssize_t index){
    auto node = get_node(list->ctx, list->handle);
    auto length = node->classes.count;
    if(index < 0){
        index += length;
        }
    if(index >= length){
        PyErr_SetString(PyExc_IndexError, "Index out of bounds");
        return NULL;
        }
    auto sv = node->classes.data[index];
    return PyUnicode_FromStringAndSize(sv.text, sv.length);
    }

static
int
DndClasses_contains(Nonnull(DndClassesList*)list, PyObject*_Nonnull query){
    if(!PyUnicode_Check(query)){
        PyErr_SetString(PyExc_TypeError, "Only strings can be in classes lists");
        return -1;
        }
    auto node = get_node(list->ctx, list->handle);
    size_t n_classes = node->classes.count;
    if(!n_classes)
        return 0;

    auto key_sv = pystring_borrow_stringview(query);
    for(size_t i = 0; i < n_classes; i++){
        auto class_string = node->classes.data[i];
        if(SV_equals(class_string, key_sv))
            return 1;
        }
    return 0;
    }

static
int
DndClasses_setitem(Nonnull(DndClassesList*)list, Py_ssize_t index, Nullable(PyObject*) value){
    if(!value){
        PyErr_SetString(PyExc_NotImplementedError, "Deletion is unsupported");
        return -1;
        }
    auto node = get_node(list->ctx, list->handle);
    auto nclasses = node->classes.count;
    if(index < 0){
        index += nclasses;
        }
    if(index >= nclasses){
        PyErr_SetString(PyExc_IndexError, "Index out of bounds");
        return -1;
        }
    node->classes.data[index] = pystring_to_stringview((Nonnull(PyObject*))value, list->ctx->allocator);
    return 0;
    }

static
Nullable(PyObject*)
DndClasses_append(Nonnull(DndClassesList*)list, Nonnull(PyObject*)args){
    const char* text;
    Py_ssize_t length;
    if(!PyArg_ParseTuple(args, "s#:append", &text, &length))
        return NULL;
    auto node = get_node(list->ctx, list->handle);
    StringView sv = {.text = Allocator_dupe(list->ctx->allocator, text, length), .length=length};
    Marray_push(StringView)(&node->classes, list->ctx->allocator, sv);
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
DndClasses_repr(Nonnull(DndClassesList*)list){
    auto node = get_node(list->ctx, list->handle);
    auto allocator = list->ctx->temp_allocator;
    MStringBuilder msb = {};
    msb_write_char(&msb, allocator, '[');
    for(size_t i = 0; i < node->classes.count; i++){
        if(i != 0)
            msb_write_str(&msb, allocator, ", ", 2);
        msb_write_char(&msb, allocator, '\'');
        auto sv = node->classes.data[i];
        msb_write_str(&msb, allocator, sv.text, sv.length);
        msb_write_char(&msb, allocator, '\'');
        }
    msb_write_char(&msb, allocator, ']');
    auto str = msb_borrow(&msb, allocator);
    auto result = PyUnicode_FromStringAndSize(str.text, str.length);
    msb_destroy(&msb, allocator);
    return result;
    }


static PyMethodDef DndClassesList_methods[] = {
    {"append", (PyCFunction)&DndClasses_append, METH_VARARGS, "add a class string"},
    {NULL, NULL, 0, NULL}, // Sentinel
    };

static PySequenceMethods DndClasses_sq_methods = {
    .sq_length = (lenfunc)DndClasses_length,
    .sq_concat = NULL,
    .sq_repeat = NULL,
    .sq_item = (ssizeargfunc)DndClasses_getitem,
    .was_sq_slice = NULL,
    .sq_ass_item = (ssizeobjargproc)DndClasses_setitem,
    .was_sq_ass_slice = NULL,
    .sq_contains = (objobjproc)DndClasses_contains,
    .sq_inplace_concat = NULL,
    .sq_inplace_repeat = NULL,
};
static PyTypeObject DndClassesListType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "docparser.ClassList",
    .tp_basicsize = sizeof(DndClassesList),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Classes List Wrapper",
    .tp_methods = DndClassesList_methods,
    .tp_getattro = PyObject_GenericGetAttr,
    .tp_as_sequence = &DndClasses_sq_methods,
    .tp_repr = (reprfunc)&DndClasses_repr,
    };

static
Nonnull(PyObject*)
make_classes_list(Nonnull(ParseContext*)ctx, NodeHandle handle){
    DndClassesList* self = (DndClassesList*)DndClassesListType.tp_alloc(&DndClassesListType, 0);
    assert(self);
    self->ctx = ctx;
    self->handle = handle;
    return (PyObject*)self;
    }

typedef struct DndAttributesMap {
    PyObject_HEAD
    Nonnull(ParseContext*)ctx;
    NodeHandle handle;
    } DndAttributesMap;

static
Nonnull(PyObject*)
DndAttributesMap_items(Nonnull(DndAttributesMap*)map, Nonnull(PyObject*)unused){
    (void)unused;
    auto node = get_node(map->ctx, map->handle);
    auto attributes = &node->attributes;
    size_t count = attributes->count;
    PyObject* result = PyList_New(count);
    for(size_t i = 0; i < count; i++){
        auto attr = &attributes->data[i];
        // new ref
        auto item = Py_BuildValue("s#s#", attr->key.text, attr->key.length, attr->value.text, attr->value.length);
        // but then steals the ref
        PyList_SET_ITEM(result, i, item);
        }
    return result;
    }

static
Py_ssize_t
DndAttributesMap_length(Nonnull(DndAttributesMap*)list){
    auto node = get_node(list->ctx, list->handle);
    return (Py_ssize_t)node->attributes.count;
    }

static
Nullable(PyObject*)
DndAttributesMap_getitem(Nonnull(DndAttributesMap*)map, Nonnull(PyObject*) key){
    if(!PyUnicode_Check(key)){
        PyErr_SetString(PyExc_TypeError, "Attribute maps must be indexed by strings");
        return NULL;
        }
    auto key_sv = pystring_borrow_stringview(key);
    auto node = get_node(map->ctx, map->handle);
    auto attributes = &node->attributes;
    auto count = attributes->count;
    for(size_t i = 0; i < count; i++){
        auto attr = &attributes->data[i];
        if(SV_equals(attr->key, key_sv))
            return PyUnicode_FromStringAndSize(attr->value.text, attr->value.length);
        }
    PyErr_Format(PyExc_KeyError, "Unknown attribute: '%s'", key_sv.text);
    return NULL;
    }

static
int
DndAttributesMap_contains(Nonnull(DndAttributesMap*)map, Nonnull(PyObject*) key){
    if(!PyUnicode_Check(key)){
        PyErr_SetString(PyExc_TypeError, "Attribute maps must be indexed by strings");
        return -1;
        }
    auto key_sv = pystring_borrow_stringview(key);
    auto node = get_node(map->ctx, map->handle);
    auto attributes = &node->attributes;
    auto count = attributes->count;
    for(size_t i = 0; i < count; i++){
        auto attr = &attributes->data[i];
        if(SV_equals(attr->key, key_sv))
            return 1;
        }
    return 0;
    }

static
int
DndAttributesMap_setitem(Nonnull(DndAttributesMap*)map, Nonnull(PyObject*) key, Nullable(PyObject*) value){
    if(!PyUnicode_Check(key)){
        PyErr_SetString(PyExc_TypeError, "Attribute maps must be indexed by strings");
        return -1;
        }
    if(value and !PyUnicode_Check(value)){
        PyErr_SetString(PyExc_TypeError, "Attribute maps can only have string values");
        return -1;
        }
    auto key_sv = pystring_borrow_stringview(key);
    StringView value_sv;
    if(value)
        value_sv = pystring_to_stringview((Nonnull(PyObject*))value, map->ctx->allocator);
    auto node = get_node(map->ctx, map->handle);
    auto attributes = &node->attributes;
    auto count = attributes->count;
    for(size_t i = 0; i < count; i++){
        auto attr = &attributes->data[i];
        if(SV_equals(attr->key, key_sv)){
            if(value){
                attr->value = value_sv;
                return 0;
                }
            else {
                Marray_remove(Attribute)(attributes, i);
                return 0;
                }
            }
        }
    if(!value){
        PyErr_Format(PyExc_KeyError, "Unknown attribute: '%s'", key_sv.text);
        return -1;
        }
    const char* key_copy = Allocator_dupe(map->ctx->allocator, key_sv.text, key_sv.length);
    auto attr = Marray_alloc(Attribute)(&node->attributes, map->ctx->allocator);
    attr->key.length = key_sv.length;
    attr->key.text = key_copy;
    attr->value = value_sv;
    return 0;
    }

static
Nullable(PyObject*)
DndAttributesMap_repr(Nonnull(DndAttributesMap*)map){
    auto node = get_node(map->ctx, map->handle);
    auto allocator = map->ctx->temp_allocator;
    auto attributes = &node->attributes;
    auto count = attributes->count;
    MStringBuilder msb = {};
    msb_write_char(&msb, allocator, '{');
    for(size_t i = 0; i < count; i++){
        auto attr = &attributes->data[i];
        if(i != 0)
            msb_write_str(&msb, allocator, ", ", 2);
        msb_write_char(&msb, allocator, '\'');
        auto key = attr->key;
        msb_write_str(&msb, allocator, key.text, key.length);
        msb_write_char(&msb, allocator, '\'');
        msb_write_char(&msb, allocator, ':');
        msb_write_char(&msb, allocator, ' ');
        msb_write_char(&msb, allocator, '\'');
        auto val = attr->value;
        msb_write_str(&msb, allocator, val.text, val.length);
        msb_write_char(&msb, allocator, '\'');
        }
    msb_write_char(&msb, allocator, '}');
    auto str = msb_borrow(&msb, allocator);
    auto result = PyUnicode_FromStringAndSize(str.text, str.length);
    msb_destroy(&msb, allocator);
    return result;
    }


static
Nullable(PyObject*)
DndAttributesMap_add(Nonnull(DndAttributesMap*)map, Nonnull(PyObject*)arg){
    if(!PyUnicode_Check(arg)){
        PyErr_SetString(PyExc_TypeError, "Argument to add must be a string");
        return NULL;
        }
    auto ctx = map->ctx;
    auto key = pystring_to_stringview(arg, ctx->allocator);
    auto node = get_node(ctx, map->handle);
    auto attributes = &node->attributes;
    auto attr = Marray_alloc(Attribute)(attributes, ctx->allocator);
    attr->key = key;
    attr->value = SV("");
    Py_RETURN_NONE;
    }
static PyMethodDef DndAttributesMap_methods[] = {
    {"items", (PyCFunction)&DndAttributesMap_items, METH_NOARGS, "returns a list of (key, value) tuples"},
    {"add", (PyCFunction)&DndAttributesMap_add, METH_O, "Add a single string item to the attributes. It's corresponding value will be the empty string."},
    {NULL, NULL, 0, NULL}, // Sentinel
    };

static PySequenceMethods DndAttributesMap_sq_methods = {
    .sq_length = (lenfunc)&DndAttributesMap_length,
    .sq_concat = NULL,
    .sq_repeat = NULL,
    .sq_item = NULL,
    .was_sq_slice = NULL,
    .sq_ass_item = NULL,
    .was_sq_ass_slice = NULL,
    .sq_contains = (objobjproc)&DndAttributesMap_contains,
    .sq_inplace_concat = NULL,
    .sq_inplace_repeat = NULL,
};

static PyMappingMethods DndAttributesMap_map_methods = {
    .mp_length = (lenfunc)&DndAttributesMap_length,
    .mp_subscript = (binaryfunc)&DndAttributesMap_getitem,
    .mp_ass_subscript = (objobjargproc)&DndAttributesMap_setitem,
};

static PyTypeObject DndAttributesMapType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "docparser.AttributesMap",
    .tp_basicsize = sizeof(DndAttributesMap),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Attributes Map Wrapper",
    .tp_methods = DndAttributesMap_methods,
    .tp_getattro = PyObject_GenericGetAttr,
    .tp_setattro = PyObject_GenericSetAttr,
    .tp_as_mapping = &DndAttributesMap_map_methods,
    .tp_as_sequence = &DndAttributesMap_sq_methods,
    .tp_repr = (reprfunc)&DndAttributesMap_repr,
    };

static
Nonnull(PyObject*)
make_attributes_map(Nonnull(ParseContext*)ctx, NodeHandle handle){
    DndAttributesMap* self = (DndAttributesMap*)DndAttributesMapType.tp_alloc(&DndAttributesMapType, 0);
    assert(self);
    self->ctx = ctx;
    self->handle = handle;
    return (PyObject*)self;
    }

typedef struct DndNode {
    PyObject_HEAD
    Nonnull(ParseContext*)ctx;
    NodeHandle handle;
    } DndNode;

static PyMethodDef DndNode_methods[] = {
    {NULL, NULL, 0, NULL}, // Sentinel
    };
static PyObject* _Nullable DndNode_getattr(Nonnull(DndNode*), Nonnull(const char*));
static int DndNode_setattr(Nonnull(DndNode*), Nonnull(const char*), Nullable(PyObject *));
static Nullable(PyObject*) DndNode_repr(Nonnull(DndNode*));


static PyTypeObject DndNodeType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "docparser.Node",
    .tp_basicsize = sizeof(DndNode),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Node Wrapper",
    .tp_methods = DndNode_methods,
    .tp_getattr = (getattrfunc)&DndNode_getattr,
    .tp_setattr = (setattrfunc)&DndNode_setattr,
    .tp_repr = (reprfunc)&DndNode_repr,
    };

static
Nullable(PyObject*)
DndNode_repr(Nonnull(DndNode*)self){
    auto node = get_node(self->ctx, self->handle);
    // format a buffer as python apparently doesn't support %.*s
    auto allocator = self->ctx->temp_allocator;
    MStringBuilder msb = {};
    if(not node->classes.count)
        msb_sprintf(&msb, allocator, "Node(%s, '%.*s', [%d children])",  nodenames[node->type].text, (int)node->header.length, node->header.text, (int)node->children.count);
    else {
        msb_sprintf(&msb, allocator, "Node(%s", nodenames[node->type].text);
        for(size_t i = 0; i < node->classes.count;i++){
            auto class = &node->classes.data[i];
            msb_sprintf(&msb, allocator, ".%.*s", (int)class->length, class->text);
            }
        msb_sprintf(&msb, allocator, ", '%.*s', [%d children])",   (int)node->header.length, node->header.text, (int)node->children.count);
    }
    auto text = msb_borrow(&msb, allocator);
    auto result = PyUnicode_FromStringAndSize(text.text, text.length);
    msb_destroy(&msb, allocator);
    return result;
    }

static
Nullable(PyObject*)
py_node_set_err(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE)){
        PyErr_SetString(PyExc_ValueError, "Method called with invalid handle: 'err'");
        return NULL;
        }
    auto node = get_node(ctx, handle);
    const char* msg;
    Py_ssize_t length;
    const char* const keywords[] = { "msg", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "s#:err", (char**)keywords, &msg, &length)){
        return NULL;
        }
    PopDiagnostic();
    node_set_err(ctx, node, "%.*s", (int)length, msg);
    PyErr_SetString(PyExc_Exception, "Node threw error.");
    return NULL;
    }


static
Nullable(PyObject*)
make_py_node(Nonnull(ParseContext*)ctx, NodeHandle handle){
    DndNode* self = (DndNode*)DndNodeType.tp_alloc(&DndNodeType, 0);
    assert(self);
    self->ctx = ctx;
    self->handle = handle;
    return (PyObject*)self;
    }

static
Nullable(PyObject*)
py_change_root_node(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle; // we're abusing my pyboundmethod machinery
    DndNode* new_root;
    const char* const keywords[] = { "new_root", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!:set_root_node", (char**)keywords, &DndNodeType, &new_root)){
        return NULL;
        }
    PopDiagnostic();
    assert(new_root->ctx == ctx);
    ctx->root_handle = new_root->handle;
    auto node = get_node(new_root->ctx, new_root->handle);
    node->parent = new_root->handle;
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
py_make_string_node(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle;
    PyObject* arg;
    const char* const keywords[] = { "text", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!:make_string", (char**)keywords, &PyUnicode_Type, &arg)){
        return NULL;
        }
    PopDiagnostic();
    auto sv = pystring_to_stringview(arg, ctx->allocator);
    auto new_handle = alloc_handle(ctx);
    {
    auto node = get_node(ctx, new_handle);
    node->header = sv;
    node->type = NODE_STRING;
    }
    return make_py_node(ctx, new_handle);
    }

static
Nullable(PyObject*)
py_kebab(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle;
    const char* text;
    Py_ssize_t length;
    const char* const keywords[] = { "text", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "s#:kebab", (char**)keywords, &text, &length)){
        return NULL;
        }
    PopDiagnostic();
    MStringBuilder sb = {};
    msb_write_kebab(&sb, ctx->temp_allocator, text, length);
    auto kebabed = msb_borrow(&sb, ctx->temp_allocator);
    PyObject* result = PyUnicode_FromStringAndSize(kebabed.text, kebabed.length);
    msb_destroy(&sb, ctx->temp_allocator);
    return result;
    }

static
Nullable(PyObject*)
py_add_dependency(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle;
    const char* text;
    Py_ssize_t length;
    const char* const keywords[] = { "text", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "s#:add_dependency", (char**)keywords, &text, &length)){
        return NULL;
        }
    PopDiagnostic();
    char* copy = Allocator_dupe(ctx->allocator, text, length);
    StringView sv = {.text=copy, .length=length};
    Marray_push(StringView)(&ctx->dependencies, ctx->allocator, sv);
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
py_make_node(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle;
    NodeTypeEnum* type;
    const char* text = NULL;
    Py_ssize_t length;
    PyObject* classes = NULL;
    PyObject* class_sq = NULL;
    PyObject* attributes = NULL;
    PyObject* attributes_sq = NULL;
    const char* const keywords[] = { "type", "header", "classes", "attributes", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O!|s#OO:make_node", (char**)keywords, &NodeTypeEnumType, &type, &text, &length, &classes, &attributes)){
        return NULL;
        }
    PopDiagnostic();
    if(classes){
        class_sq = PySequence_Fast(classes, "make_node needs 'classes' to be a sequence of strings");
        if(class_sq == NULL){
            goto err;
            }
        auto sq_length = PySequence_Fast_GET_SIZE(class_sq);
        for(Py_ssize_t i = 0; i < sq_length; i++){
            auto item = PySequence_Fast_GET_ITEM(class_sq, i);
            if(!PyUnicode_Check(item)){
                PyErr_SetString(PyExc_TypeError, "make node needs 'classes' to be a sequence of strings. Non-string found.");
                goto err;
                }
            }
        }
    if(attributes){
        attributes_sq = PySequence_Fast(attributes, "make_node needs 'attributes' to be a sequence of strings");
        if(attributes_sq == NULL){
            goto err;
            }
        auto sq_length = PySequence_Fast_GET_SIZE(attributes_sq);
        for(Py_ssize_t i = 0; i < sq_length; i++){
            auto item = PySequence_Fast_GET_ITEM(attributes_sq, i);
            if(!PyUnicode_Check(item)){
                PyErr_SetString(PyExc_TypeError, "make node needs 'attributes' to be a sequence of strings. Non-string found.");
                goto err;
                }
            }
        }
    {
    auto new_handle = alloc_handle(ctx);
    {
    auto node = get_node(ctx, new_handle);
    {
    auto frame = PyEval_GetFrame(); // borrowed ref
    if(frame){
        node->row = PyFrame_GetLineNumber(frame) - 1;
        auto code = frame->f_code;
        node->filename = pystring_to_stringview(code->co_filename, ctx->allocator);
        }
    }
    if(text){
        char* copy = Allocator_dupe(ctx->allocator, text, length);
        StringView sv = {.text=copy, .length=length};
        node->header = sv;
        }
    if(class_sq){
        auto sq_length = PySequence_Fast_GET_SIZE(class_sq);
        for(Py_ssize_t i = 0; i < sq_length; i++){
            auto item = PySequence_Fast_GET_ITEM(class_sq, i);
            auto c = Marray_alloc(StringView)(&node->classes, ctx->allocator);
            *c = pystring_to_stringview(item, ctx->allocator);
            }
        }
    if(attributes_sq){
        auto sq_length = PySequence_Fast_GET_SIZE(attributes_sq);
        for(Py_ssize_t i = 0; i < sq_length; i++){
            auto item = PySequence_Fast_GET_ITEM(attributes_sq, i);
            auto a = Marray_alloc(Attribute)(&node->attributes, ctx->allocator);
            a->key = pystring_to_stringview(item, ctx->allocator);
            a->value = SV("");
            }
        }
    node->type = type->type;
    Marray(NodeHandle)* node_store = NULL;;
    switch(node->type){
        case NODE_IMPORT:
            PyErr_SetString(PyExc_ValueError, "Creating import nodes from python is not supported");
            return NULL;
        case NODE_DEPENDENCIES:
            node_store = &ctx->dependencies_nodes;
            break;
        case NODE_STYLESHEETS:
            node_store = &ctx->stylesheets_nodes;
            break;
        case NODE_LINKS:
            node_store = &ctx->link_nodes;
            break;
        case NODE_SCRIPTS:
            node_store = &ctx->script_nodes;
            break;
        case NODE_PYTHON:
            node_store = &ctx->python_nodes;
            break;
        case NODE_DATA:
            node_store = &ctx->data_nodes;
            break;
        case NODE_TITLE:
            ctx->titlenode = new_handle;
            break;
        case NODE_NAV:
            ctx->navnode = new_handle;
            break;
        default:
            break;
        }
    if(node_store)
        Marray_push(NodeHandle)(node_store, ctx->allocator, new_handle);
    }
    return make_py_node(ctx, new_handle);
    }
    err:
    Py_XDECREF(class_sq);
    Py_XDECREF(attributes_sq);
    return NULL;
    }

static
Nullable(PyObject*)
py_set_data(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    (void)handle;
    const char* key_text = NULL;
    Py_ssize_t key_length;
    const char* value_text = NULL;
    Py_ssize_t value_length;
    const char* const keywords[] = { "key", "value", NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "s#s#:set_data", (char**)keywords, &key_text, &key_length, &value_text, &value_length)){
        return NULL;
        }
    PopDiagnostic();
    char* key_copy = Allocator_dupe(ctx->allocator, key_text, key_length);
    auto new_data = Marray_alloc(DataItem)(&ctx->rendered_data, ctx->allocator);
    new_data->key.text = key_copy;
    new_data->key.length = key_length;
    char* value_copy = Allocator_dupe(ctx->allocator, value_text, value_length+1);
    new_data->value.text = value_copy;
    new_data->value.length = value_length;
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
py_detach_node(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    const char* const keywords[] = { NULL, };
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, ":detach_node", (char**)keywords)){
        return NULL;
        }
    PopDiagnostic();
    auto node = get_node(ctx, handle);
    if(NodeHandle_eq(node->parent, INVALID_NODE_HANDLE)){
        Py_RETURN_NONE;
        }
    if(NodeHandle_eq(handle, ctx->root_handle)){
        ctx->root_handle = INVALID_NODE_HANDLE;
        node->parent = INVALID_NODE_HANDLE;
        Py_RETURN_NONE;
        }
    auto parent = get_node(ctx, node->parent);
    node->parent = INVALID_NODE_HANDLE;
    for(size_t i = 0; i < parent->children.count; i++){
        if(NodeHandle_eq(handle, parent->children.data[i])){
            Marray_remove__NodeHandle(&parent->children, i);
            goto after;
            }
        }
    PyErr_SetString(PyExc_RuntimeError, "Somehow a node was not a child of its parents");
    return NULL;
    after:;
    Py_RETURN_NONE;
    }

static
Nullable(PyObject*)
py_add_child_node(Nonnull(ParseContext*)ctx, NodeHandle handle, Nonnull(PyObject*)args, Nullable(PyObject*)kwargs){
    const char* const keywords[] = { "new_root", NULL, };
    NodeHandle new_handle;
    PyObject* arg;
    PushDiagnostic();
    SuppressCastQual();
    // This call is guaranteed to not modify keywords, but it's declared as char**
    // as const in C is kind of broken.
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "O:add_child", (char**)keywords, &arg)){
        return NULL;
        }
    PopDiagnostic();
    if(PyObject_IsInstance(arg, (PyObject*)&DndNodeType)){
        auto new_child = (DndNode*)arg;
        new_handle = new_child->handle;
        }
    else if(PyUnicode_Check(arg)){
        auto sv = pystring_to_stringview(arg, ctx->allocator);
        new_handle = alloc_handle(ctx);
        auto node = get_node(ctx, new_handle);
        node->header = sv;
        node->type = NODE_STRING;
        }
    else {
        PyErr_SetString(PyExc_TypeError, "Argument 'to_add' child must be a node or a string");
        return NULL;
        }
    auto child_node = get_node(ctx, new_handle);
    if(!NodeHandle_eq(child_node->parent, INVALID_NODE_HANDLE)){
        PyErr_SetString(PyExc_ValueError, "Node needs to be an orphan to be added as a child of another node.");
        return NULL;
        }
    if(NodeHandle_eq(handle, new_handle)){
        PyErr_SetString(PyExc_ValueError, "Node can't be a child of itself");
        return NULL;
        }
    append_child(ctx, handle, new_handle);
    Py_RETURN_NONE;
    }

static Nonnull(PyObject*) make_py_ctx(Nonnull(ParseContext*));

static
Errorable_f(void)
execute_python_string(Nonnull(ParseContext*)ctx, Nonnull(const char*)text, NodeHandle handle){
    PyCompilerFlags flags = {
        .cf_flags = PyCF_SOURCE_IS_UTF8,
        .cf_feature_version = PY_MINOR_VERSION,
        };
    PyObject* glbl = PyDict_New();
    PyObject* nodetypes = PyDict_New();
    for(size_t i = 0; i < arrlen(nodenames); i++){
        auto enu = make_node_type_enum(i);
        PyDict_SetItemString(nodetypes, nodenames[i].text, enu);
        Py_XDECREF(enu);
        }
    auto nt = _PyNamespace_New(nodetypes);
    assert(nt);
    Py_XDECREF(nodetypes);
    PyDict_SetItemString(glbl, "NodeType", nt);
    Py_XDECREF(nt);
    PyObject* pynode = make_py_node(ctx, handle);
    PyDict_SetItemString(glbl, "node", pynode);
    Py_XDECREF(pynode);
    PyObject* pyctx = make_py_ctx(ctx);
    PyDict_SetItemString(glbl, "ctx", pyctx);
    Py_XDECREF(pyctx);

    PyDict_SetItemString(glbl, "__builtins__", PyEval_GetBuiltins());

    auto node = get_node(ctx, handle);
    char buff[1024];
    snprintf(buff, sizeof(buff), "%.*s", (int)node->filename.length, node->filename.text);
    PyObject* code = Py_CompileStringExFlags(text, buff, Py_file_input, &flags, 0);
    auto c = (PyCodeObject*)code;
    PyObject* result;
    if(!code){
        result = NULL;
        }
    else {
        auto old_co_name = c->co_name;
        c->co_name = PyUnicode_FromString(":python");
        Py_XDECREF(old_co_name);
        c->co_firstlineno+= node->row +1;
        result = PyEval_EvalCode(code, glbl, glbl);
        }
    // result = PyRun_StringFlags(text, Py_file_input, glbl, glbl, &flags);
    Py_XDECREF(glbl);
    if(!result){
        PyObject *type, *value, *traceback;
        PyErr_Fetch(&type, &value, &traceback);
        PyErr_NormalizeException(&type, &value, &traceback);
        if(ctx->error_message.length){
            }
        else{
            PyObject* exc_str = PyObject_Str(value);
            assert(exc_str);
            const char* exc_text = PyUnicode_AsUTF8(exc_str);
            assert(exc_text);
            auto python_block = get_node(ctx, handle);
            auto old_row = python_block->row;
            auto new_row = old_row;
            if(traceback){
                auto tb = (PyTracebackObject*)traceback;
                // since we mucked with the code up above, the lineno is actually
                // accurate.
                auto lineno = tb->tb_lineno;
                new_row = lineno-1; // the error reporting adds 1
                }
            else if(type == PyExc_SyntaxError){
                auto se = (PySyntaxErrorObject*)value;
                auto lineno = PyLong_AsLong(se->lineno);
                new_row += lineno;
                }
            // kind of hacky, but meh;
            const char* type_text = ((PyTypeObject*)type)->tp_name;
            assert(type_text);
            // NASTY: modding the line number
            python_block->row = new_row;
            node_set_err(ctx, python_block, "%s: %s", type_text, exc_text);
            python_block->row = old_row;
            Py_XDECREF(exc_str);
            }
        Py_XDECREF(type);
        Py_XDECREF(value);
        Py_XDECREF(traceback);
        Py_XDECREF(result);
        Py_XDECREF(code);
        return (Errorable(void)){PARSE_ERROR};
        }
    Py_XDECREF(result);
    Py_XDECREF(code);
    return (Errorable(void)){};
    }
#if 0
static PyMethodDef DndParserMethods[] = {
    // parse
    // make_raw
    // make_string
    // err
    {NULL,NULL, 0, NULL}, // terminator
    };
static PyModuleDef DndParseModule = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "docparser",
    .m_doc = "Extension module for the docparser",
    .m_size = -1,
    .m_methods = DndParserMethods,
    };
#endif

static
Nullable(PyObject*)
DndNode_getattr(Nonnull(DndNode*)obj, Nonnull(const char*)name){
    auto len = strlen(name);
#define CHECK(lit) (len == sizeof(""lit)-1 and memcmp(name, ""lit, sizeof(""lit)-1)==0)
    if(CHECK("parent")){
        auto node = get_node(obj->ctx, obj->handle);
        return make_py_node(obj->ctx, node->parent);
        }
    if(CHECK("type")){
        auto node = get_node(obj->ctx, obj->handle);
        return make_node_type_enum(node->type);
        }
    if(CHECK("children")){
        auto node = get_node(obj->ctx, obj->handle);
        auto result = PyTuple_New(node->children.count);
        if(!result)
            return result;
        for(size_t i = 0; i < node->children.count; i++){
            auto child = node->children.data[i];
            auto pynode = make_py_node(obj->ctx, child);
            auto fail = PyTuple_SetItem(result, i, pynode);
            //meh
            assert(fail == 0);
            }
        return result;
        }
    if(CHECK("header")){
        auto node = get_node(obj->ctx, obj->handle);
        return PyUnicode_FromStringAndSize(node->header.text, node->header.length);
        }
    if(CHECK("attributes")){
        return make_attributes_map(obj->ctx, obj->handle);
        }
    if(CHECK("classes")){
        return make_classes_list(obj->ctx, obj->handle);
        }
    if(CHECK("parse")){
        return make_node_bound_method(obj->ctx, obj->handle, &py_parse_and_append_children);
        }
    if(CHECK("detach")){
        return make_node_bound_method(obj->ctx, obj->handle, &py_detach_node);
        }
    if(CHECK("add_child")){
        return make_node_bound_method(obj->ctx, obj->handle, &py_add_child_node);
        }
    if(CHECK("err")){
        return make_node_bound_method(obj->ctx, obj->handle, &py_node_set_err);
        }
    PyErr_Format(PyExc_AttributeError, "Unknown attribute: %s", name);
    return NULL;
    }

static
int
DndNode_setattr(Nonnull(DndNode*)obj, Nonnull(const char*)name, Nullable(PyObject*) value){
    auto len = strlen(name);
    if(!value){
        PyErr_SetString(PyExc_TypeError, "deletion of attributes is not supported");
        return -1;
        }
    if(CHECK("parent")){
        PyErr_SetString(PyExc_TypeError, "parent cannot be reassigned");
        return -1;
        }
    if(CHECK("attributes")){
        PyErr_SetString(PyExc_TypeError, "attributes cannot be reassigned");
        return -1;
        }
    if(CHECK("classes")){
        PyErr_SetString(PyExc_TypeError, "classes cannot be reassigned");
        return -1;
        }
    if(CHECK("type")){
        if(PyObject_IsInstance(value, (PyObject *)&NodeTypeEnumType)){
            auto ty = (NodeTypeEnum*)value;
            auto node = get_node(obj->ctx, obj->handle);
            switch(ty->type){
                case NODE_NAV:
                    obj->ctx->navnode = obj->handle;
                    break;
                case NODE_TITLE:
                    obj->ctx->titlenode = obj->handle;
                    break;
                case NODE_STYLESHEETS:
                    Marray_push(NodeHandle)(&obj->ctx->stylesheets_nodes, obj->ctx->allocator, obj->handle);
                    break;
                case NODE_DEPENDENCIES:
                    Marray_push(NodeHandle)(&obj->ctx->dependencies_nodes, obj->ctx->allocator, obj->handle);
                    break;
                case NODE_LINKS:
                    Marray_push(NodeHandle)(&obj->ctx->link_nodes, obj->ctx->allocator, obj->handle);
                    break;
                case NODE_SCRIPTS:
                    Marray_push(NodeHandle)(&obj->ctx->script_nodes, obj->ctx->allocator, obj->handle);
                    break;
                case NODE_DATA:
                    Marray_push(NodeHandle)(&obj->ctx->data_nodes, obj->ctx->allocator, obj->handle);
                    break;
                case NODE_PYTHON:
                    PyErr_SetString(PyExc_ValueError, "Setting a node to PYTHON not supported.");
                    return -1;
                case NODE_IMPORT:
                    PyErr_SetString(PyExc_ValueError, "Setting a node to IMPORT not supported.");
                    return -1;
                case NODE_ROOT:
                case NODE_TEXT:
                case NODE_DIV:
                case NODE_STRING:
                case NODE_PARA:
                case NODE_HEADING:
                case NODE_TABLE:
                case NODE_TABLE_ROW:
                case NODE_IMAGE:
                case NODE_BULLETS:
                case NODE_BULLET:
                case NODE_RAW:
                case NODE_PRE:
                case NODE_LIST:
                case NODE_LIST_ITEM:
                case NODE_KEYVALUE:
                case NODE_KEYVALUEPAIR:
                case NODE_IMGLINKS:
                case NODE_COMMENT:
                case NODE_MD:
                case NODE_CONTAINER:
                case NODE_INVALID:
                case NODE_QUOTE:
                    break;
                }
            node->type = ty->type;
            return 0;
            }
        else {
            PyErr_SetString(PyExc_TypeError, "node type must be a NodeType");
            return -1;
            }
        }
    if(CHECK("header")){
        if(!PyUnicode_Check(value)){
            PyErr_SetString(PyExc_TypeError, "Header must be a string");
            return -1;
            }
        auto node = get_node(obj->ctx, obj->handle);
        node->header = pystring_to_stringview((Nonnull(PyObject*))value, obj->ctx->allocator);
        return 0;
        }
#undef CHECK
    PyErr_Format(PyExc_AttributeError, "Unknown attribute: %s", name);
    return -1;
    }

// TODO:
//  Possibly all these ParseContext* should actually be ParseContext** so they can get nulled.
//
// just a bare wrapper around the parse context
typedef struct DndContext {
    PyObject_HEAD
    Nonnull(ParseContext*) ctx;
    } DndContext;

// static PyMethodDef DndContext_methods[] = {
    // {NULL, NULL, 0, NULL}, // sentinel
    // };

static PyObject* _Nullable DndContext_getattr(Nonnull(DndContext*), Nonnull(const char*));

static PyTypeObject DndContextType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "docparse.ParseContext",
    .tp_basicsize = sizeof(DndContext),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "ParseContext",
    // can you just leave this out?
    // .tp_methods = DndContext_methods,
    .tp_getattr = (getattrfunc)&DndContext_getattr,
    };

static
PyObject* _Nullable
DndContext_getattr(Nonnull(DndContext*)pyctx, Nonnull(const char*)attr){
    auto ctx = pyctx->ctx;
    auto len = strlen(attr);
#define CHECK(lit) (len == sizeof(""lit)-1 and memcmp(attr, ""lit, sizeof(""lit)-1)==0)
    if(CHECK("root")){
        if(NodeHandle_eq(ctx->root_handle, INVALID_NODE_HANDLE)){
            PyErr_SetString(PyExc_AttributeError, "There is currently no root node");
            return NULL;
            }
        return make_py_node(ctx, ctx->root_handle);
        }
    if(CHECK("set_root")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_change_root_node);
        }
    if(CHECK("make_string")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_make_string_node);
        }
    if(CHECK("make_node")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_make_node);
        }
    if(CHECK("add_dependency")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_add_dependency);
        }
    if(CHECK("kebab")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_kebab);
        }
    if(CHECK("set_data")){
        return make_node_bound_method(ctx, INVALID_NODE_HANDLE, &py_set_data);
        }
    if(CHECK("outfile")){
        auto filename = path_basename(LS_to_SV(ctx->outputfile));
        return PyUnicode_FromStringAndSize(filename.text, filename.length);
        }
    if(CHECK("outdir")){
        auto outdir = path_dirname(LS_to_SV(ctx->outputfile));
        if(!outdir.length)
            outdir = SV(".");
        return PyUnicode_FromStringAndSize(outdir.text, outdir.length);
        }
    if(CHECK("outpath")){
        return PyUnicode_FromStringAndSize(ctx->outputfile.text, ctx->outputfile.length);
        }
    if(CHECK("sourcepath")){
        return PyUnicode_FromStringAndSize(ctx->filename.text, ctx->filename.length);
        }
#undef CHECK
    PyErr_Format(PyExc_AttributeError, "Unknown attribute: '%s'", attr);
    return NULL;
    }

static
Nonnull(PyObject*)
make_py_ctx(Nonnull(ParseContext*)ctx){
    DndContext* self = (DndContext*)DndContextType.tp_alloc(&DndContextType, 0);
    assert(self);
    self->ctx = ctx;
    return (PyObject*)self;
    }

static
Errorable_f(void)
docparse_init_types(void){
    Errorable(void) result = {};
    if(PyType_Ready(&DndNodeType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&DndClassesListType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&DndAttributesMapType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&NodeBoundMethodType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&NodeTypeEnumType) < 0)
        Raise(GENERIC_ERROR);
    if(PyType_Ready(&DndContextType) < 0)
        Raise(GENERIC_ERROR);
    return result;
    }

#include "terminal_logger.c"
#define SUPPRESS_BUILTIN_MODS
#define NO_AUX

static struct _inittab mods[] = {
    {NULL, 0}, // sentinel
    };

static
int
init_python_interpreter(void){
    PyStatus status;
    PyPreConfig preconfig;
    PyPreConfig_InitIsolatedConfig(&preconfig);
    preconfig.use_environment = 0;
    preconfig.utf8_mode = 1;
    status = Py_PreInitialize(&preconfig);
    if(PyStatus_Exception(status)){
        // ERROR("pre init fail");
        goto fail;
        }

    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);
    config.buffered_stdio = 0;
    config.write_bytecode = 0;
    config.quiet = 1;
    config.install_signal_handlers = 1;
    config.module_search_paths_set = 1;
    config.site_import = 0;
    config.isolated = 1;
    config.use_hash_seed = 1;
    config.hash_seed = 358780669; // very random
    // config.import_time = 1;
    // Initialize all paths otherwise python tries to do it itself in silly ways.
    PyConfig_SetString(&config, &config.program_name, L"");
    PyConfig_SetString(&config, &config.pythonpath_env, L"");
    PyConfig_SetString(&config, &config.home, L"");
    PyConfig_SetString(&config, &config.executable, L"");
    PyConfig_SetString(&config, &config.base_executable, L"");
    PyConfig_SetString(&config, &config.prefix, L"");
    PyConfig_SetString(&config, &config.base_prefix, L"");
    PyConfig_SetString(&config, &config.exec_prefix, L"");
    PyConfig_SetString(&config, &config.base_exec_prefix, L"");
    int import_fail = PyImport_ExtendInittab(&mods[0]);
    if(import_fail < 0){
        // ERROR("import fail");
        goto fail;
        }
    status = Py_InitializeFromConfig(&config);
    if(PyStatus_Exception(status)){
        // ERROR("init fail");
        goto fail;
        }
    PyConfig_Clear(&config);
    return 0;

    fail:
    PyConfig_Clear(&config);
    if(PyStatus_IsExit(status)){
        return status.exitcode;
        }
    return 1;
    }

static
Errorable_f(void)
init_python_docparser(void){
    Errorable(void) result = {};
    if(Py_IsInitialized())
        return result;
    set_frozen_modules();
    auto fail = init_python_interpreter();
    if(fail){
        ERROR("Failed to init python interpreter");
        Raise(GENERIC_ERROR);
        }
    {
        auto e = docparse_init_types();
        if(e.errored)
            return e;
    }
    return result;
    }

