#ifndef DNDC_CONTEXT_C
#define DNDC_CONTEXT_C
// This file is kind of a grab bag of random functionality
// that directly interacts with nodes or with the context.
#include "MStringBuilder.h"
#include "dndc_flags.h"
#include "dndc_types.h"
#include "dndc_funcs.h"
#include "measure_time.h"
#include "file_util.h"
#include "ByteBuilder.h"
#include "bb_extensions.h"
#include "msb_extensions.h"
#include "str_util.h"
#include "path_util.h"

static inline
bool
node_has_attribute(Nonnull(const Node*) node, StringView attr){
    // TODO: maybe use a dict? Idk how many attributes we actually use.
    // Maybe if count is greater than some N we sort and do a binary search?
    // In using this program, I don't think I've ever exceeded 2 attributes.
    auto attrs = node->attributes.data;
    auto count = node->attributes.count;
    for(size_t i = 0; i < count; i++){
        if(SV_equals(attrs[i].key, attr))
            return true;
        }
    return false;
    }

static inline
bool
node_has_class(Nonnull(const Node*) node, StringView c){
    auto classes = node->classes.data;
    auto count = node->classes.count;
    for(size_t i = 0; i < count; i++){
        if(SV_equals(classes[i], c))
            return true;
        }
    return false;
    }
static inline
Nullable(StringView*)
node_get_attribute(Nonnull(const Node*) node, StringView attr){
    // TODO: maybe use a dict? Idk how many attributes we actually use.
    // Maybe if count is greater than some N we sort and do a binary search?
    // In using this program, I don't think I've ever exceeded 2 attributes.
    auto attrs = node->attributes.data;
    auto count = node->attributes.count;
    for(size_t i = 0; i < count; i++){
        if(SV_equals(attrs[i].key, attr))
            return &attrs[i].value;
        }
    return NULL;
    }

static inline
Nullable(const StringView*)
node_get_id(Nonnull(const Node*) node){
    if(node_has_attribute(node, SV("noid")))
        return NULL;
    const StringView* id = node_get_attribute(node, SV("id"));
    if(likely(!id)){
        if(node->header.length)
            id = &node->header;
        }
    return id;
    }
printf_func(3, 4)
static
void
parse_set_err(Nonnull(DndcContext*)ctx, NullUnspec(const char*) errchar, Nonnull(const char*) fmt, ...){
    MStringBuilder msb = {.allocator = ctx->allocator};
    int col = (int)(errchar - ctx->linestart);
    ctx->error.filename = ctx->filename;
    ctx->error.line = ctx->lineno;
    ctx->error.col = col;
    va_list args;
    va_start(args, fmt);
    msb_vsprintf(&msb, fmt, args);
    va_end(args);
    ctx->error.message = msb_detach(&msb);
    }

printf_func(3, 4)
static
void
node_set_err(Nonnull(DndcContext*)ctx, Nonnull(const Node*)node, Nonnull(const char*) fmt, ...){
    MStringBuilder msb = {.allocator=ctx->allocator};
    ctx->error.filename = node->filename;
    ctx->error.line = node->row;
    ctx->error.col = node->col;
    va_list args;
    va_start(args, fmt);
    msb_vsprintf(&msb, fmt, args);
    va_end(args);
    ctx->error.message = msb_detach(&msb);
    }

printf_func(4, 5)
static
void
node_set_err_offset(Nonnull(DndcContext*)ctx, Nonnull(const Node*)node, int offset, Nonnull(const char*) fmt, ...){
    MStringBuilder msb = {.allocator=ctx->allocator};
    ctx->error.filename = node->filename;
    ctx->error.line = node->row;
    ctx->error.col = node->col+offset;
    va_list args;
    va_start(args, fmt);
    msb_vsprintf(&msb, fmt, args);
    va_end(args);
    ctx->error.message = msb_detach(&msb);
    }

printf_func(3, 4)
static
void
node_print_err(Nonnull(DndcContext*)ctx, Nonnull(const Node*)node, Nonnull(const char*) fmt, ...){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(not ctx->error_func)
        return;
    MStringBuilder msb = {.allocator=ctx->temp_allocator};
    auto filename = node->filename;
    auto lineno = node->row;
    int col = node->col;
    va_list args;
    va_start(args, fmt);
    msb_vsprintf(&msb, fmt, args);
    va_end(args);
    auto msg = msb_borrow(&msb);
    ctx->error_func(ctx->error_user_data, DNDC_WARNING_MESSAGE, filename.text, filename.length, lineno, col, msg.text, msg.length);
    msb_destroy(&msb);
    }

printf_func(3, 4)
static
void
node_print_warning(Nonnull(DndcContext*)ctx, Nonnull(const Node*)node, Nonnull(const char*) fmt, ...){
    if(ctx->flags & DNDC_SUPPRESS_WARNINGS)
        return;
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(not ctx->error_func)
        return;
    MStringBuilder msb = {.allocator=ctx->temp_allocator};
    auto filename = node->filename;
    auto lineno = node->row;
    int col = node->col;
    va_list args;
    va_start(args, fmt);
    msb_vsprintf(&msb, fmt, args);
    va_end(args);
    auto msg = msb_borrow(&msb);
    ctx->error_func(ctx->error_user_data, DNDC_WARNING_MESSAGE, filename.text, filename.length, lineno, col, msg.text, msg.length);
    msb_destroy(&msb);
    }

printf_func(4, 5)
static inline
void
report_stat_raw(uint64_t flags, Nullable(ErrorFunc*) error_func, Nullable(void*) error_user_data, Nonnull(const char*) fmt, ...){
    if(not (flags & DNDC_PRINT_STATS))
        return;
    if(not error_func)
        return;
    char buff[256];
    va_list args;
    va_start(args, fmt);
    int printed = vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);
    if(printed >= sizeof(buff))
        printed = sizeof(buff)-1;
    error_func(error_user_data, DNDC_STATISTIC_MESSAGE, "", 0, 0, 0, buff, printed);
    }

printf_func(2, 3)
static
void
report_stat(Nonnull(DndcContext*)ctx, Nonnull(const char*) fmt, ...){
    if(not (ctx->flags & DNDC_PRINT_STATS))
        return;
    if(not ctx->error_func)
        return;

    MStringBuilder temp = {.allocator = ctx->temp_allocator};
    va_list args;
    va_start(args, fmt);
    msb_vsprintf(&temp, fmt, args);
    va_end(args);
    auto msg = msb_borrow(&temp);
    ctx->error_func(ctx->error_user_data, DNDC_STATISTIC_MESSAGE, "", 0, 0, 0, msg.text, msg.length);
    msb_destroy(&temp);
    }

static
void
report_set_error(Nonnull(DndcContext*)ctx){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(not ctx->error_func)
        return;
    ctx->error_func(ctx->error_user_data, DNDC_ERROR_MESSAGE, ctx->error.filename.text, ctx->error.filename.length, ctx->error.line, ctx->error.col, ctx->error.message.text, ctx->error.message.length);
    }

static
printf_func(2, 3)
void
report_system_error(Nonnull(DndcContext*)ctx, Nonnull(const char*)fmt, ...){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(not ctx->error_func)
        return;
    MStringBuilder temp = {.allocator = ctx->temp_allocator};
    va_list args;
    va_start(args, fmt);
    msb_vsprintf(&temp, fmt, args);
    va_end(args);
    auto msg = msb_borrow(&temp);
    ctx->error_func(ctx->error_user_data, DNDC_SYSTEM_MESSAGE, "", 0, 0, 0, msg.text, msg.length);
    msb_destroy(&temp);
    }

static inline
void
ctx_store_builtin_file(Nonnull(DndcContext*)ctx, LongString sourcepath, LongString text){
    auto loaded = Marray_alloc(LoadedSource)(&ctx->builtin_files, ctx->allocator);
    loaded->sourcepath = sourcepath;
    loaded->sourcetext = text;
    }

static
Errorable_f(LongString)
ctx_load_source_file(Nonnull(DndcContext*)ctx, StringView sourcepath){
    // check if we already have it as a builtin
    for(size_t i = 0; i < ctx->builtin_files.count; i++){
        auto builtin = &ctx->builtin_files.data[i];
        if(LS_SV_equals(builtin->sourcepath, sourcepath)){
            return (Errorable(LongString)){.result=builtin->sourcetext};
            }
        }
    MStringBuilder temp_builder = {.allocator=ctx->temp_allocator};
    if(!sourcepath.length){
        return (Errorable(LongString)){.errored=UNEXPECTED_END};
        }
    assert(sourcepath.length);

    if(not path_is_abspath(sourcepath) && ctx->base_directory.length){
        msb_write_str(&temp_builder, ctx->base_directory.text, ctx->base_directory.length);
        msb_append_path(&temp_builder, sourcepath.text, sourcepath.length);
        sourcepath = msb_borrow(&temp_builder);
        }
    // check if we already have it.
    for(size_t i = 0; i < ctx->loaded_files.count; i++){
        auto loaded = &ctx->loaded_files.data[i];
        if(LS_SV_equals(loaded->sourcepath, sourcepath)){
            msb_destroy(&temp_builder);
            return (Errorable(LongString)){.result=loaded->sourcetext};
            }
        }
    char* path = Allocator_strndup(ctx->allocator, sourcepath.text, sourcepath.length);
    msb_destroy(&temp_builder);

    auto before = get_t();
    auto load_err = read_file(ctx->allocator, path);
    auto after = get_t();
    if(!load_err.errored){
        report_stat(ctx, "Loading '%.*s' took %.3fms", (int)sourcepath.length, sourcepath.text, (after-before)/1000.);
        auto loaded = Marray_alloc(LoadedSource)(&ctx->loaded_files, ctx->allocator);
        loaded->sourcepath.text = path;
        loaded->sourcepath.length = sourcepath.length;
        loaded->sourcetext = load_err.result;
        }
    else {
        Allocator_free(ctx->allocator, path, sourcepath.length+1);
        }
    return load_err;
    }

static
Errorable_f(LongString)
ctx_load_processed_binary_file(Nonnull(DndcContext*)ctx, StringView binarypath){
    MStringBuilder path_builder = {.allocator=ctx->temp_allocator};
    if(not path_is_abspath(binarypath) && ctx->base_directory.length){
        msb_write_str(&path_builder, ctx->base_directory.text, ctx->base_directory.length);
        msb_append_path(&path_builder, binarypath.text, binarypath.length);
        binarypath = LS_to_SV(msb_detach(&path_builder));
        }
    ByteBuilder bb = {.allocator = ctx->allocator};
    auto result = load_processed_binary_file(&ctx->b64cache, binarypath, &bb);
    bb_destroy(&bb);
    msb_destroy(&path_builder);
    return result;
    }

static
Errorable_f(LongString)
load_processed_binary_file(Nonnull(Base64Cache*)cache, StringView binarypath, Nonnull(ByteBuilder*)bb){
    // check if we already have it.
    for(size_t i = 0; i < cache->processed_binary_files.count; i++){
        auto loaded = &cache->processed_binary_files.data[i];
        if(LS_SV_equals(loaded->sourcepath, binarypath)){
            DBG("Returning cached b64: '%.*s'", (int)binarypath.length, binarypath.text);
            return (Errorable(LongString)){.result=loaded->sourcetext};
            }
        }
    DBG("Not cached, processing b64: '%.*s'", (int)binarypath.length, binarypath.text);

    // We don't have it, try to load it ourselves.
    auto a = cache->allocator;
    MStringBuilder sb = {.allocator = a};
    msb_write_str(&sb, binarypath.text, binarypath.length);
    auto path = msb_borrow(&sb);

    auto base64ed_e = read_and_base64_bin_file(bb, a, path.text);
    if(unlikely(base64ed_e.errored)){
        msb_destroy(&sb);
        return (Errorable(LongString)){.errored = base64ed_e.errored};
        }
    auto base64ed = base64ed_e.result;
    auto sourcepath = msb_detach(&sb);
    auto loaded = Marray_alloc(LoadedSource)(&cache->processed_binary_files, a);
    loaded->sourcepath = sourcepath;
    loaded->sourcetext = base64ed;
    return (Errorable(LongString)){.result=base64ed};
    }

static inline
Nullable(StringView*)
find_link_target(Nonnull(DndcContext*)ctx, StringView kebabed){
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
        int c = StringView_cmp(&data[mid].key, &kebabed);
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
add_link_from_sv(Nonnull(DndcContext*)ctx, StringView str, bool check_valid){
    Errorable(void) result = {};
    const char* equals = memchr(str.text, '=', str.length);
    if(!equals){
        // TODO: print error from this node
        ctx->error.message = LS("no '=' in a link node");
        Raise(PARSE_ERROR);
        }
    MStringBuilder sb = {.allocator=ctx->allocator};
    msb_write_kebab(&sb, str.text, equals - str.text);
    if(!sb.cursor){
        // TODO: print error from this node
        ctx->error.message = LS("key is empty");
        Raise(PARSE_ERROR);
        }
    auto key = LS_to_SV(msb_detach(&sb));
    StringView value = stripped_view(equals + 1, (str.text+str.length)-(equals+1));
    if(!value.length){
        // TODO: print error from this node
        ctx->error.message = LS("link target is empty");
        Raise(PARSE_ERROR);
        }
    if(check_valid and value.text[0] == '#'){
        StringView target = {.text = value.text+1, .length = value.length-1};
        if(!target.length){
            // TODO: print error from this node
            ctx->error.message = LS("link target is empty after the '#'");
            Raise(PARSE_ERROR);
            }
        // TODO: keep a binary tree or something?
        for(size_t i = 0; i < ctx->links.count; i++){
            if(SV_equals(ctx->links.data[i].value, value))
                goto foundit;
            }
        // TODO: print error from this node
        ctx->error.message = LS("Anchor does not correspond to any link");
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
add_link_from_header(Nonnull(DndcContext*)ctx, StringView str){
    MStringBuilder sb = {.allocator=ctx->allocator};
    msb_write_char(&sb, '#');
    msb_write_kebab(&sb, str.text, str.length);
    if(sb.cursor==1){
        msb_destroy(&sb);
        return;
        }
    LongString anchor = msb_detach(&sb);
    StringView kebabed = {.text = anchor.text+1, .length=anchor.length-1};
    auto li = Marray_alloc(LinkItem)(&ctx->links, ctx->allocator);
    li->key = kebabed;
    li->value = LS_to_SV(anchor);
    return;
    }

static inline
force_inline
NodeHandle
alloc_handle(Nonnull(DndcContext*)ctx){
    size_t index = Marray_alloc_index(Node)(&ctx->nodes, ctx->allocator);
    ctx->nodes.data[index] = (Node){};
    // debug to help find nodes without parents
    ctx->nodes.data[index].parent = INVALID_NODE_HANDLE;
    return (NodeHandle){.index=index};
    }

static inline
Nonnull(Node*)
force_inline
get_node(Nonnull(DndcContext*)ctx, NodeHandle handle){
    assert(handle.index < ctx->nodes.count);
    auto result = &ctx->nodes.data[handle.index];
    return result;
    }

// for debugging
extern
Nonnull(Node*)
get_node_e(Nonnull(DndcContext*)ctx, NodeHandle handle){
    return get_node(ctx, handle);
    }

static inline
void
force_inline
append_child(Nonnull(DndcContext*)ctx, NodeHandle parent_handle, NodeHandle child_handle){
    auto parent = get_node(ctx, parent_handle);
    auto child = get_node(ctx, child_handle);
    child->parent = parent_handle;
    Marray_push(NodeHandle)(&parent->children, ctx->allocator, child_handle);
    }

static Errorable_f(void) check_node_depth(Nonnull(DndcContext*)ctx, NodeHandle handle, int depth);

static
Errorable_f(void)
check_depth(Nonnull(DndcContext*)ctx){
    return check_node_depth(ctx, ctx->root_handle, 0);
    }

static
Errorable_f(void)
check_node_depth(Nonnull(DndcContext*)ctx, NodeHandle handle, int depth){
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

static void gather_anchor(Nonnull(DndcContext*)ctx, NodeHandle handle);
static
void
gather_anchors(Nonnull(DndcContext*)ctx){
    auto root = ctx->root_handle;
    return gather_anchor(ctx, root);
    }

static inline
force_inline
void
gather_anchor_children(Nonnull(DndcContext*)ctx, Nonnull(Node*)node){
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        gather_anchor(ctx, children[i]);
        }
    }

static
void
gather_anchor(Nonnull(DndcContext*)ctx, NodeHandle handle){
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
        case NODE_CONTAINER:{
            auto id = node_get_id(node);
            if(id){
                add_link_from_header(ctx, *id);
                }
            }
            // fall-through
        case NODE_DATA: // this is a little sketchy
        case NODE_ROOT:
        case NODE_IMPORT:
        case NODE_LIST_ITEM:
        case NODE_KEYVALUEPAIR:{
            gather_anchor_children(ctx, node);
            }break;
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

static
inline
void
convert_node_to_container_containing_clone_of_former_self(Nonnull(DndcContext*)ctx, NodeHandle handle){
    auto new_handle = alloc_handle(ctx);
    auto new_node = get_node(ctx, new_handle);
    auto old_node = get_node(ctx, handle);
    assert(!old_node->children.count);
    memcpy(new_node, old_node, sizeof(*new_node));
    new_node->parent = handle;
    Marray_push(NodeHandle)(&old_node->children, ctx->allocator, new_handle);
    old_node->header = SV("");
    old_node->type = NODE_CONTAINER;
    old_node->attributes.count = 0;
    }

static inline
void
ctx_add_builtins(Nonnull(DndcContext*)ctx){
    ctx_store_builtin_file(ctx, LS("builtins/coords.js"), LS(
    // TODO: more maintainable way of doing this
        "document.addEventListener('DOMContentLoaded', function(){\n"
        "    const svgs = document.getElementsByTagName('svg');\n"
        "    for(let i = 0; i < svgs.length; i++){\n"
        "        const svg = svgs[i];\n"
        "        const first_text = svg.getElementsByTagName('text')[0];\n"
        "        const text_height = first_text.getBBox().height || 0;\n"
        "        const parent = svg.parentElement;\n"
        "        const p = document.createElement('p');\n"
        "        p.style = 'text-align:center';\n"
        "        parent.insertBefore(p, p.nextSibling);\n"
        "        svg.addEventListener('mousemove', function(e){\n"
        "            const x_scale = svg.width.baseVal.value / svg.viewBox.baseVal.width;\n"
        "            const y_scale = svg.height.baseVal.value / svg.viewBox.baseVal.height; \n"
        "            const rect = e.currentTarget.getBoundingClientRect();\n"
        "            const true_x = ((e.clientX - rect.x)/ x_scale) | 0;\n"
        "            const true_y = (((e.clientY - rect.y)/ y_scale) + text_height/2) | 0;\n"
        "            p.innerHTML = 'coord(' + true_x + ',' + true_y+ ')';\n"
        "        })\n"
        "    }\n"
        "})\n"
        ));
    }
#endif
