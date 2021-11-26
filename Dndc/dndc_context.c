#ifndef DNDC_CONTEXT_C
#define DNDC_CONTEXT_C
// This file is kind of a grab bag of random functionality
// that directly interacts with nodes or with the context.
#include "dndc_api_def.h"
#include "MStringBuilder.h"
#include "dndc_types.h"
#include "dndc_funcs.h"
#include "long_string.h"
#include "measure_time.h"
#include "file_util.h"
#include "ByteBuilder.h"
#include "bb_extensions.h"
#include "msb_extensions.h"
#include "msb_format.h"
#include "str_util.h"
#include "path_util.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static inline
bool
node_has_attribute(const Node* node, StringView attr){
    // TODO: maybe use a dict? Idk how many attributes we actually use.
    // Maybe if count is greater than some N we sort and do a binary search?
    // In using this program, I don't think I've ever exceeded 2 attributes.
    if(!node->attributes)
        return false;
    RARRAY_FOR_EACH(a, node->attributes){
        if(SV_equals(a->key, attr))
            return true;
    }
    return false;
}

static inline
bool
node_has_class(const Node* node, StringView c){
    if(!node->classes)
        return false;
    RARRAY_FOR_EACH(cls, node->classes){
        if(SV_equals(*cls, c))
            return true;
    }
    return false;
}
static inline
Nullable(StringView*)
node_get_attribute(const Node* node, StringView attr){
    // TODO: maybe use a dict? Idk how many attributes we actually use.
    // Maybe if count is greater than some N we sort and do a binary search?
    // In using this program, I don't think I've ever exceeded 2 attributes.
    if(!node->attributes)
        return NULL;
    RARRAY_FOR_EACH(a, node->attributes){
        if(SV_equals(a->key, attr))
            return &a->value;
    }
    return NULL;
}

static inline
void
node_set_attribute(Node* node, Allocator allocator, StringView attr, StringView value){
    RARRAY_FOR_EACH(a, node->attributes){
        if(SV_equals(a->key, attr)){
            a->value = value;
            return;
        }
    }
    auto a = Rarray_alloc(Attribute)(&node->attributes, allocator);
    a->key = attr;
    a->value = value;
    return;
}

static inline
Nullable(const StringView*)
node_get_id(const Node* node){
    if(node_has_attribute(node, SV("noid")))
        return NULL;
    const StringView* id = node_get_attribute(node, SV("id"));
    if(likely(!id)){
        if(node->header.length)
            id = &node->header;
    }
    return id;
}

static inline
NodeHandle
node_clone(DndcContext* ctx, NodeHandle handle){
    NodeHandle result = alloc_handle(ctx);
    Node* dstnode = get_node(ctx, result);
    Node* srcnode = get_node(ctx, handle);
    dstnode->type = srcnode->type;
    dstnode->parent = INVALID_NODE_HANDLE;
    dstnode->header = srcnode->header;
    if(node_children_count(srcnode) <= 4){
        dstnode->children = srcnode->children;
    }
    else {
        Marray_extend(NodeHandle)(&dstnode->children, ctx->allocator, node_children(srcnode), node_children_count(srcnode));
    }
    RARRAY_FOR_EACH(at, srcnode->attributes){
        dstnode->attributes = Rarray_push(Attribute)(dstnode->attributes, ctx->allocator, *at);
    }
    RARRAY_FOR_EACH(cls, srcnode->classes){
        dstnode->classes = Rarray_push(StringView)(dstnode->classes, ctx->allocator, *cls);
    }
    dstnode->filename_idx = srcnode->filename_idx;
    dstnode->row = srcnode->row;
    dstnode->col = srcnode->col;
    return result;
}

static
void
parse_set_err(DndcContext* ctx, NullUnspec(const char*) errchar, LongString msg){
    int col = (int)(errchar - ctx->linestart);
    ctx->error.filename = ctx->filename;
    ctx->error.line = ctx->lineno;
    ctx->error.col = col;
    ctx->error.message = msg;
}

static
void
parse_set_err_q(DndcContext* ctx, NullUnspec(const char*) errchar, StringView msg, StringView quoted){
    int col = (int)(errchar - ctx->linestart);
    ctx->error.filename = ctx->filename;
    ctx->error.line = ctx->lineno;
    ctx->error.col = col;
    MStringBuilder msb = {.allocator = ctx->string_allocator};
    msb_write_str(&msb, msg.text, msg.length);
    msb_write_char(&msb, '\'');
    msb_write_str(&msb, quoted.text, quoted.length);
    msb_write_char(&msb, '\'');
    ctx->error.message = msb_detach(&msb);
}

static
void
node_set_err_q(DndcContext* ctx, const Node* node, StringView msg, StringView quoted){
    MStringBuilder msb = {.allocator=ctx->string_allocator};
    ctx->error.filename = ctx->filenames.data[node->filename_idx];
    ctx->error.line = node->row;
    ctx->error.col = node->col;
    msb_write_str(&msb, msg.text, msg.length);
    msb_write_char(&msb, '\'');
    msb_write_str(&msb, quoted.text, quoted.length);
    msb_write_char(&msb, '\'');
    ctx->error.message = msb_detach(&msb);
}

static
void
node_set_err(DndcContext* ctx, const Node* node, LongString ls){
    ctx->error.filename = ctx->filenames.data[node->filename_idx];
    ctx->error.line = node->row;
    ctx->error.col = node->col;
    ctx->error.message = ls;
}

static
void
node_set_err_offset(DndcContext* ctx, const Node* node, int offset, LongString message){
    ctx->error.filename = ctx->filenames.data[node->filename_idx];
    ctx->error.line = node->row;
    ctx->error.col = node->col+offset;
    ctx->error.message = message;
}

static
void
node_print_err(DndcContext* ctx, const Node* node, StringView msg){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(not ctx->error_func)
        return;
    auto filename = ctx->filenames.data[node->filename_idx];
    auto lineno = node->row;
    int col = node->col;
    ctx->error_func(ctx->error_user_data, DNDC_ERROR_MESSAGE, filename.text, filename.length, lineno, col, msg.text, msg.length);
}

static
void
node_print_warning(DndcContext* ctx, const Node* node, StringView msg){
    if(ctx->flags & DNDC_SUPPRESS_WARNINGS)
        return;
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(not ctx->error_func)
        return;
    auto filename = ctx->filenames.data[node->filename_idx];
    auto lineno = node->row;
    int col = node->col;
    ctx->error_func(ctx->error_user_data, DNDC_WARNING_MESSAGE, filename.text, filename.length, lineno, col, msg.text, msg.length);
}

static
void
node_print_warning2(DndcContext* ctx, const Node* node, StringView a, StringView b){
    if(ctx->flags & DNDC_SUPPRESS_WARNINGS)
        return;
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(not ctx->error_func)
        return;
    auto filename = ctx->filenames.data[node->filename_idx];
    auto lineno = node->row;
    int col = node->col;
    MStringBuilder msb = {.allocator = ctx->temp_allocator};
    msb_write_str(&msb, a.text, a.length);
    msb_write_str(&msb, b.text, b.length);
    auto msg = msb_borrow(&msb);
    ctx->error_func(ctx->error_user_data, DNDC_WARNING_MESSAGE, filename.text, filename.length, lineno, col, msg.text, msg.length);
    msb_destroy(&msb);
}

static
void
report_time(DndcContext* ctx, StringView msg, uint64_t microseconds){
    if(not (ctx->flags & DNDC_PRINT_STATS))
        return;
    if(not ctx->error_func)
        return;
    MStringBuilder temp = {.allocator=ctx->temp_allocator};
    msb_write_str(&temp, msg.text, msg.length);
    msb_write_us_as_ms(&temp, microseconds);
    auto str = msb_borrow(&temp);
    ctx->error_func(ctx->error_user_data, DNDC_STATISTIC_MESSAGE, "", 0, 0, 0, str.text, str.length);
    msb_destroy(&temp);
}

static
void
report_info(DndcContext* ctx, StringView msg){
    if(not (ctx->flags & DNDC_PRINT_STATS))
        return;
    if(not ctx->error_func)
        return;
    ctx->error_func(ctx->error_user_data, DNDC_STATISTIC_MESSAGE, "", 0, 0, 0, msg.text, msg.length);
}
static
void
report_size(DndcContext* ctx, StringView msg, uint64_t size){
    if(not (ctx->flags & DNDC_PRINT_STATS))
        return;
    if(not ctx->error_func)
        return;
    MStringBuilder temp = {.allocator=ctx->temp_allocator};
    msb_write_str(&temp, msg.text, msg.length);
    msb_write_uint64(&temp, size);
    auto str = msb_borrow(&temp);
    ctx->error_func(ctx->error_user_data, DNDC_STATISTIC_MESSAGE, "", 0, 0, 0, str.text, str.length);
    msb_destroy(&temp);
}

static
void
report_set_error(DndcContext* ctx){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(not ctx->error_func)
        return;
    ctx->error_func(ctx->error_user_data, DNDC_ERROR_MESSAGE, ctx->error.filename.text, ctx->error.filename.length, ctx->error.line, ctx->error.col, ctx->error.message.text, ctx->error.message.length);
}

static
void
report_system_error(DndcContext* ctx, StringView msg){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(not ctx->error_func)
        return;
    ctx->error_func(ctx->error_user_data, DNDC_NODELESS_MESSAGE, "", 0, 0, 0, msg.text, msg.length);
}

static inline
void
ctx_note_dependency(DndcContext* ctx, StringView path){
    // FIXME: O(n^2) deduplication
    MARRAY_FOR_EACH(dep, ctx->dependencies){
        if(SV_equals(*dep, path))
            return;
    }
    StringView pathcpy = {.text = Allocator_strndup(ctx->string_allocator, path.text, path.length), .length=path.length};
    Marray_push(StringView)(&ctx->dependencies, ctx->allocator, pathcpy);
}

static inline
void
ctx_store_builtin_file(DndcContext* ctx, LongString sourcepath, LongString text){
    auto loaded = Marray_alloc(LoadedSource)(&ctx->builtin_files, ctx->allocator);
    loaded->sourcepath = sourcepath;
    loaded->sourcetext = text;
}

static
Errorable_f(LongString)
ctx_load_source_file(DndcContext* ctx, StringView sourcepath){
    // check if we already have it as a builtin
    MARRAY_FOR_EACH(builtin, ctx->builtin_files){
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
    ctx_note_dependency(ctx, sourcepath);
    // check if we already have it.
    MARRAY_FOR_EACH(loaded, ctx->textcache.files){
        if(LS_SV_equals(loaded->sourcepath, sourcepath)){
            msb_destroy(&temp_builder);
            return (Errorable(LongString)){.result=loaded->sourcetext};
        }
    }
    if(unlikely(ctx->flags & DNDC_DONT_READ))
        return (Errorable(LongString)){.errored=PARSE_ERROR};
    char* path = Allocator_strndup(ctx->textcache.allocator, sourcepath.text, sourcepath.length);
    msb_destroy(&temp_builder);

    auto before = get_t();
    auto load_err = read_file(path, ctx->textcache.allocator);
    auto after = get_t();
    if(!load_err.errored){
        report_time(ctx, SV("Loading a file took "), after-before);
        auto loaded = Marray_alloc(LoadedSource)(&ctx->textcache.files, ctx->textcache.allocator);
        loaded->sourcepath.text = path;
        loaded->sourcepath.length = sourcepath.length;
        loaded->sourcetext = load_err.result;
    }
    else {
        Allocator_free(ctx->textcache.allocator, path, sourcepath.length+1);
    }
    return (Errorable(LongString)){.errored=load_err.errored, .result=load_err.result};
}

static
Errorable_f(LongString)
ctx_load_processed_binary_file(DndcContext* ctx, StringView binarypath){
    if(unlikely(ctx->flags & DNDC_DONT_READ))
        return (Errorable(LongString)){.errored=PARSE_ERROR};
    MStringBuilder path_builder = {.allocator=ctx->temp_allocator};
    if(not path_is_abspath(binarypath) && ctx->base_directory.length){
        msb_write_str(&path_builder, ctx->base_directory.text, ctx->base_directory.length);
        msb_append_path(&path_builder, binarypath.text, binarypath.length);
        binarypath = msb_borrow(&path_builder);
    }
    ctx_note_dependency(ctx, binarypath);
    ByteBuilder bb = {.allocator = ctx->allocator};
    auto result = load_processed_binary_file(&ctx->b64cache, binarypath, &bb);
    bb_destroy(&bb);
    msb_destroy(&path_builder);
    return result;
}

static
Errorable_f(LongString)
load_processed_binary_file(FileCache* cache, StringView binarypath, ByteBuilder* bb){
    // check if we already have it.
    MARRAY_FOR_EACH(loaded, cache->files){
        if(LS_SV_equals(loaded->sourcepath, binarypath)){
            // DBG("Returning cached b64: '%.*s'", (int)binarypath.length, binarypath.text);
            return (Errorable(LongString)){.result=loaded->sourcetext};
        }
    }
    // DBG("Not cached, processing b64: '%.*s'", (int)binarypath.length, binarypath.text);

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
    auto loaded = Marray_alloc(LoadedSource)(&cache->files, a);
    *loaded = (LoadedSource){
        .sourcepath = sourcepath,
        .sourcetext = base64ed,
    };
    return (Errorable(LongString)){.result=base64ed};
}

static inline
Nullable(StringView*)
find_link_target(DndcContext* ctx, StringView kebabed){
    if(!ctx->links.count)
        return NULL;
#if 1
    auto data = ctx->links.data;
    size_t low = 0, high = ctx->links.count-1;
    size_t mid;
    if(SV_equals(data[low].key, kebabed))
        return &data[low].value;
    if(SV_equals(data[high].key, kebabed))
        return &data[high].value;
    while(low < high){
        // This can't realistically overflow.
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
add_link_from_sv(DndcContext* ctx, Node* node){
    auto str = node->header;
    Errorable(void) result = {};
    const char* equals = memchr(str.text, '=', str.length);
    if(!equals){
        node_print_err(ctx, node, SV("no '=' in a link node"));
        Raise(PARSE_ERROR);
    }
    MStringBuilder sb = {.allocator=ctx->string_allocator};
    msb_write_kebab(&sb, str.text, equals - str.text);
    if(!sb.cursor){
        node_print_err(ctx, node, SV("key is empty."));
        Raise(PARSE_ERROR);
    }
    auto key = LS_to_SV(msb_detach(&sb));
    StringView value = stripped_view(equals + 1, (str.text+str.length)-(equals+1));
    if(!value.length){
        node_print_err(ctx, node, SV("link target is empty."));
        Raise(PARSE_ERROR);
    }
    if(value.text[0] == '#'){
        StringView target = {.text = value.text+1, .length = value.length-1};
        if(!target.length){
            node_print_err(ctx, node, SV("link target is empty after the '#'"));
            Raise(PARSE_ERROR);
        }
        // TODO: keep a binary tree or something?
        MARRAY_FOR_EACH(li, ctx->links){
            if(SV_equals(li->value, value))
                goto foundit;
        }
        node_print_err(ctx, node, SV("Anchor does not correspond to any link"));
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
add_link_from_header(DndcContext* ctx, StringView str){
    MStringBuilder sb = {.allocator=ctx->string_allocator};
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
void
add_link_from_pair(DndcContext* ctx, StringView kebabed, StringView value){
    auto li = Marray_alloc(LinkItem)(&ctx->links, ctx->allocator);
    li->key = kebabed;
    li->value = value;
}

static inline
force_inline
NodeHandle
alloc_handle(DndcContext* ctx){
    size_t index = Marray_alloc_index(Node)(&ctx->nodes, ctx->allocator);
    ctx->nodes.data[index] = (Node){};
    // debug to help find nodes without parents
    ctx->nodes.data[index].parent = INVALID_NODE_HANDLE;
    return (NodeHandle){.index=index};
}

static inline
Node*
force_inline
get_node(DndcContext* ctx, NodeHandle handle){
    assert(handle.index < ctx->nodes.count);
    auto result = &ctx->nodes.data[handle.index];
    return result;
}

// for debugging
DNDC_API
Node*
get_node_e(DndcContext* ctx, NodeHandle handle){
    return get_node(ctx, handle);
}

static inline
void
force_inline
append_child(DndcContext* ctx, NodeHandle parent_handle, NodeHandle child_handle){
    auto parent = get_node(ctx, parent_handle);
    auto child = get_node(ctx, child_handle);
    child->parent = parent_handle;
    if(parent->children.count < 4){
        parent->inline_children[parent->children.count++] = child_handle;
        return;
    }
    if(parent->children.count == 4){
        Marray(NodeHandle) children = {};
        Marray_ensure_total(NodeHandle)(&children, ctx->allocator, 4);
        memcpy(children.data, parent->inline_children, sizeof(parent->inline_children));
        children.count = 4;
        parent->children = children;
    }
    Marray_push(NodeHandle)(&parent->children, ctx->allocator, child_handle);
}

static inline
void
node_insert_child(DndcContext* ctx, NodeHandle parent, size_t i, NodeHandle child){
    Node* node = get_node(ctx, parent);
    if(i >= node_children_count(node)){
        append_child(ctx, parent, child);
        return;
    }
    // This is a sloppy way of doing things, but appending
    // a child means we already have the right amount of
    // space.
    // It's sloppy as we could end up memmoving twice in a row.
    append_child(ctx, parent, child);
    NodeHandle* data = node_children(node);
    size_t count = node_children_count(node);
    size_t nmove = count - i - 1;
    if(nmove)
        memmove(data+i+1, data+i, nmove*sizeof(*data));
    data[i] = child;
}

static Errorable_f(void) check_node_depth(DndcContext* ctx, NodeHandle handle, int depth);

static
Errorable_f(void)
check_depth(DndcContext* ctx){
    return check_node_depth(ctx, ctx->root_handle, 0);
}

static
Errorable_f(void)
check_node_depth(DndcContext* ctx, NodeHandle handle, int depth){
    auto node = get_node(ctx, handle);
    enum {MAX_DEPTH=64};
    if(unlikely(depth > MAX_DEPTH)){
        node_set_err(ctx, node, LS("Tree depth exceeded: greater than 64"));
        return (Errorable(void)){.errored=PARSE_ERROR};
    }
    NODE_CHILDREN_FOR_EACH(it, node){
        auto e = check_node_depth(ctx, *it, depth+1);
        if(e.errored) return e;
    }
    return (Errorable(void)){.errored=NO_ERROR};
}

static void gather_anchor(DndcContext* ctx, NodeHandle handle);

static
void
gather_anchors(DndcContext* ctx){
    auto root = ctx->root_handle;
    return gather_anchor(ctx, root);
}

static
void
gather_anchor(DndcContext* ctx, NodeHandle handle){
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
        case NODE_DETAILS:
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
        case NODE_IMPORT:
        case NODE_LIST_ITEM:
        case NODE_KEYVALUEPAIR:{
            NODE_CHILDREN_FOR_EACH(it, node){
                gather_anchor(ctx, *it);
            }
        }break;
        case NODE_TABLE_ROW:
        case NODE_STYLESHEETS:
        case NODE_LINKS:
        case NODE_SCRIPTS:
        case NODE_JS:
        case NODE_STRING:
        case NODE_NAV:
        case NODE_COMMENT:
        case NODE_INVALID:
        case NODE_HR:
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
convert_node_to_container_containing_clone_of_former_self(DndcContext* ctx, NodeHandle handle){
    auto new_handle = alloc_handle(ctx);
    auto new_node = get_node(ctx, new_handle);
    auto old_node = get_node(ctx, handle);
    assert(!node_children_count(old_node));
    memcpy(new_node, old_node, sizeof(*new_node));
    new_node->parent = handle;
    old_node->children.count = 1;
    old_node->inline_children[0] = new_handle;
    old_node->header = SV("");
    old_node->type = NODE_CONTAINER;
    if(old_node->attributes)
        old_node->attributes->count = 0;
}

static inline
void
ctx_add_builtins(DndcContext* ctx){
#define JSRAW(...) #__VA_ARGS__
    ctx_store_builtin_file(ctx, LS("builtins/coords.js"), LS(JSRAW(
        document.addEventListener("DOMContentLoaded", function(){
            const svgs = document.getElementsByTagName("svg");
            for(let i = 0; i < svgs.length; i++){
                const svg = svgs[i];
                const first_text = svg.getElementsByTagName("text")[0];
                const text_height = first_text.getBBox().height || 0;
                const parent = svg.parentElement;
                const p = document.createElement('p');
                p.style = "text-align:center";
                parent.insertBefore(p, p.nextSibling);
                svg.addEventListener("mousemove", function(e){
                    const x_scale = svg.width.baseVal.value / svg.viewBox.baseVal.width;
                    const y_scale = svg.height.baseVal.value / svg.viewBox.baseVal.height;
                    const rect = e.currentTarget.getBoundingClientRect();
                    const true_x = ((e.clientX - rect.x)/ x_scale) | 0;
                    const true_y = (((e.clientY - rect.y)/ y_scale) + text_height/2) | 0;
                    p.innerHTML = "coord(" + true_x + ',' + true_y+ ')';
                });
            }
        });
    )));
#undef JSRAW
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
