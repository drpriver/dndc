//
// Copyright © 2021-2022, David Priver
//
#ifndef DNDC_CONTEXT_C
#define DNDC_CONTEXT_C
// This file is kind of a grab bag of random functionality
// that directly interacts with nodes or with the context.
#include "dndc_api_def.h"
#include "dndc_types.h"
#include "dndc_funcs.h"
#include "dndc_logging.h"
#include "Utils/MStringBuilder.h"
#include "Utils/msb_extensions.h"
#include "Utils/msb_format.h"
#include "Utils/long_string.h"
#include "Utils/measure_time.h"
#include "Utils/file_util.h"
#include "Utils/str_util.h"
#include "Utils/path_util.h"

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
    RARRAY_FOR_EACH(Attribute, a, node->attributes){
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
    RARRAY_FOR_EACH(StringView, cls, node->classes){
        if(SV_equals(*cls, c))
            return true;
    }
    return false;
}
static inline
StringView*_Nullable
node_get_attribute(const Node* node, StringView attr){
    // TODO: maybe use a dict? Idk how many attributes we actually use.
    // Maybe if count is greater than some N we sort and do a binary search?
    // In using this program, I don't think I've ever exceeded 2 attributes.
    if(!node->attributes)
        return NULL;
    RARRAY_FOR_EACH(Attribute, a, node->attributes){
        if(SV_equals(a->key, attr))
            return &a->value;
    }
    return NULL;
}

static inline
warn_unused
int
node_set_attribute(Node* node, Allocator allocator, StringView attr, StringView value){
    RARRAY_FOR_EACH(Attribute, a, node->attributes){
        if(SV_equals(a->key, attr)){
            a->value = value;
            return 0;
        }
    }
    Attribute* a;
    int err = Rarray_alloc(Attribute)(&node->attributes, allocator, &a);
    if(unlikely(err)) return DNDC_ERROR_OOM;
    a->key = attr;
    a->value = value;
    return 0;
}

// Don't call this function unless you really need what was explicitly set
// by a #id() directive.
static inline
bool
node_get_explicit_id(DndcContext* ctx, NodeHandle handle, StringView* out){
    MARRAY_FOR_EACH(IdItem, item, ctx->explicit_node_ids){
        if(NodeHandle_eq(item->node, handle)){
            *out = item->text;
            return true;
        }
    }
    return false;
}

// Returns a zero-length string if noid
static inline
StringView
node_get_id(DndcContext* ctx, NodeHandle handle){
    Node* node = get_node(ctx, handle);
    if(node->flags & NODEFLAG_NOID)
        return SV("");
    if(node->type == NODE_STRING)
        return SV("");
    StringView sv = node->header;
    if(node->flags & NODEFLAG_ID)
        node_get_explicit_id(ctx, handle, &sv); // Ok to ignore return value.
    return sv;
}

static inline
void
node_set_id(DndcContext* ctx, NodeHandle handle, StringView sv){
    Node* node = get_node(ctx, handle);
    if(node->flags & NODEFLAG_ID){
        MARRAY_FOR_EACH(IdItem, item, ctx->explicit_node_ids){
            if(NodeHandle_eq(item->node, handle)){
                item->text = sv;
                return;
            }
        }
    }
    IdItem* item = Marray_alloc(IdItem)(&ctx->explicit_node_ids, main_allocator(ctx));
    item->node = handle;
    item->text = sv;
    node->flags |= NODEFLAG_ID;
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
        int err = Marray_extend(NodeHandle)(&dstnode->children, main_allocator(ctx), node_children(srcnode), node_children_count(srcnode));
        if(unlikely(err))
            return INVALID_NODE_HANDLE; // this is weird
    }
    RARRAY_FOR_EACH(Attribute, at, srcnode->attributes){
        int err = Rarray_push(Attribute)(&dstnode->attributes, main_allocator(ctx), *at);
        if(unlikely(err)) return INVALID_NODE_HANDLE; // this is weird
    }
    RARRAY_FOR_EACH(StringView, cls, srcnode->classes){
        int err = Rarray_push(StringView)(&dstnode->classes, main_allocator(ctx), *cls);
        if(unlikely(err)) return INVALID_NODE_HANDLE; // this is weird
    }
    dstnode->filename_idx = srcnode->filename_idx;
    dstnode->row = srcnode->row;
    dstnode->col = srcnode->col;
    dstnode->flags = srcnode->flags;
    if(srcnode->flags & NODEFLAG_ID){
        StringView sv;
        if(node_get_explicit_id(ctx, handle, &sv))
            node_set_id(ctx, result, sv);
        else {
            // Weird... This shouldn't happen really.
            // I guess just clear the id bit for destination.
            dstnode->flags &= ~NODEFLAG_ID;
        }
    }
    return result;
}

static inline
warn_unused
int
ctx_note_dependency(DndcContext* ctx, StringView path){
    // FIXME: O(n^2) deduplication
    MARRAY_FOR_EACH(StringView, dep, ctx->dependencies){
        if(SV_equals(*dep, path))
            return 0;
    }
    // This is weird that I am doing strndup, but then storing in a stringview
    StringView pathcpy = {.text = Allocator_strndup(string_allocator(ctx), path.text, path.length), .length=path.length};
    if(unlikely(!pathcpy.text))
        return DNDC_ERROR_OOM;
    int err = Marray_push(StringView)(&ctx->dependencies, main_allocator(ctx), pathcpy);
    if(err){
        Allocator_free(string_allocator(ctx), pathcpy.text, pathcpy.length+1);
        return DNDC_ERROR_OOM;
    }
    return 0;
}

static
StringViewResult
ctx_load_source_file(DndcContext* ctx, StringView sourcepath){
    MStringBuilder temp_builder = {.allocator=temp_allocator(ctx)};
    if(!sourcepath.length){
        return (StringViewResult){.errored=DNDC_ERROR_FILE_READ};
    }
    assert(sourcepath.length);

    if(! path_is_abspath(sourcepath) && ctx->base_directory.length){
        msb_write_str(&temp_builder, ctx->base_directory.text, ctx->base_directory.length);
        msb_append_path(&temp_builder, sourcepath.text, sourcepath.length);
        if(unlikely(temp_builder.errored)){
            msb_destroy(&temp_builder);
            return (StringViewResult){.errored=DNDC_ERROR_OOM};
        }
        sourcepath = msb_borrow_sv(&temp_builder);
    }
    int err = ctx_note_dependency(ctx, sourcepath);
    if(err){
        msb_destroy(&temp_builder);
        return (StringViewResult){.errored=err};
    }
    uint64_t before = get_t();
    StringResult cache_result = FileCache_read_file(ctx->textcache, sourcepath, !!(ctx->flags & DNDC_DONT_READ));
    msb_destroy(&temp_builder);
    if(cache_result.errored){
        return (StringViewResult){.errored = cache_result.errored};
    }
    uint64_t after = get_t();
    report_time(ctx, SV("Loading a file took "), after-before);
    return (StringViewResult){.result =LS_to_SV(cache_result.result)};
}

static
StringViewResult
ctx_load_processed_binary_file(DndcContext* ctx, StringView binarypath){
    MStringBuilder path_builder = {.allocator=temp_allocator(ctx)};
    if(! path_is_abspath(binarypath) && ctx->base_directory.length){
        msb_write_str(&path_builder, ctx->base_directory.text, ctx->base_directory.length);
        msb_append_path(&path_builder, binarypath.text, binarypath.length);
        if(unlikely(path_builder.errored)){
            msb_destroy(&path_builder);
            return (StringViewResult){.errored=DNDC_ERROR_OOM};
        }
        binarypath = msb_borrow_sv(&path_builder);
    }
    int err = ctx_note_dependency(ctx, binarypath);
    if(unlikely(err)){
        msb_destroy(&path_builder);
        return (StringViewResult){.errored=err};
    }
    StringResult cache_result = FileCache_read_and_b64_file(ctx->b64cache, binarypath, !!(ctx->flags & DNDC_DONT_READ));
    msb_destroy(&path_builder);
    if(cache_result.errored) return (StringViewResult){.errored=cache_result.errored};
    return (StringViewResult){.result=LS_to_SV(cache_result.result)};
}

static inline
const StringView*_Nullable
find_link_target(DndcContext* ctx, StringView kebabed){
    return string_table_get(&ctx->links, kebabed);
}

static inline
int
add_link_from_sv(DndcContext* ctx, Node* node){
    StringView str = node->header;
    const char* equals = memchr(str.text, '=', str.length);
    if(!equals){
        NODE_LOG_ERROR(ctx, node, LS("no '=' in a link node"));
        return DNDC_ERROR_PARSE;
    }
    MStringBuilder sb = {.allocator=string_allocator(ctx)};
    msb_write_kebab(&sb, str.text, equals - str.text);
    if(!sb.cursor){
        NODE_LOG_ERROR(ctx, node, LS("key is empty."));
        return DNDC_ERROR_PARSE;
    }
    StringView key = msb_detach_sv(&sb);
    StringView value = stripped_view(equals + 1, (str.text+str.length)-(equals+1));
    if(!value.length){
        NODE_LOG_ERROR(ctx, node, LS("link target is empty."));
        return DNDC_ERROR_PARSE;
    }
    if(value.text[0] == '#'){
        StringView target = {.text = value.text+1, .length = value.length-1};
        if(!target.length){
            NODE_LOG_ERROR(ctx, node, LS("link target is empty after the '#'"));
            return DNDC_ERROR_PARSE;
        }
        const StringView* values = ctx->links.keys + ctx->links.capacity_;
        for(size_t i = 0; i < ctx->links.capacity_; i++){
            if(SV_equals(values[i], value))
                goto foundit;
        }
        NODE_LOG_ERROR(ctx, node, LS("Anchor does not correspond to any link"));
        return DNDC_ERROR_LINK;
        foundit:;
    }
    int err = string_table_set(&ctx->links, key, value);
    if(unlikely(err)) return DNDC_ERROR_OOM;
    return 0;
}

static inline
warn_unused
int
add_link_from_header(DndcContext* ctx, StringView str){
    MStringBuilder sb = {.allocator=string_allocator(ctx)};
    msb_write_char(&sb, '#');
    msb_write_kebab(&sb, str.text, str.length);
    if(sb.cursor==1){
        msb_destroy(&sb);
        return 0;
    }
    msb_nul_terminate(&sb);
    if(unlikely(sb.errored))
        return 1;
    LongString anchor = msb_detach_ls(&sb);
    StringView kebabed = {.text = anchor.text+1, .length=anchor.length-1};
    int err = string_table_set(&ctx->links, kebabed, LS_to_SV(anchor));
    if(unlikely(err)) return DNDC_ERROR_OOM;
    return 0;
}

static inline
warn_unused
int
add_link_from_pair(DndcContext* ctx, StringView kebabed, StringView value){
    int err = string_table_set(&ctx->links, kebabed, value);
    if(err) return DNDC_ERROR_OOM;
    return 0;
}

static inline
force_inline
NodeHandle
alloc_handle(DndcContext* ctx){
    size_t index = Marray_alloc_index(Node)(&ctx->nodes, main_allocator(ctx));
    if(unlikely(index == (size_t)-1))
        return INVALID_NODE_HANDLE;
    ctx->nodes.data[index] = (Node){0};
    // debug to help find nodes without parents
    ctx->nodes.data[index].parent = INVALID_NODE_HANDLE;
    return (NodeHandle){.index=index};
}

static inline
force_inline
Node*
get_node(DndcContext* ctx, NodeHandle handle){
    assert(handle.index < ctx->nodes.count);
    Node* result = &ctx->nodes.data[handle.index];
    return result;
}

// for debugging
DNDC_API
Node*
get_node_e(DndcContext* ctx, NodeHandle handle){
    return get_node(ctx, handle);
}

static inline
force_inline
warn_unused
int
append_child(DndcContext* ctx, NodeHandle parent_handle, NodeHandle child_handle){
    Node* parent = get_node(ctx, parent_handle);
    Node* child = get_node(ctx, child_handle);
    child->parent = parent_handle;
    if(parent->children.count < 4){
        parent->inline_children[parent->children.count++] = child_handle;
        return 0;
    }
    if(parent->children.count == 4){
        Marray(NodeHandle) children = {0};
        int err = Marray_ensure_total(NodeHandle)(&children, main_allocator(ctx), 4);
        // unhandled_error_condition(err);
        if(unlikely(err))
            return DNDC_ERROR_OOM;
        memcpy(children.data, parent->inline_children, sizeof(parent->inline_children));
        children.count = 4;
        parent->children = children;
    }
    int err = Marray_push(NodeHandle)(&parent->children, main_allocator(ctx), child_handle);
    // unhandled_error_condition(err);
    if(unlikely(err))
        return DNDC_ERROR_OOM;
    return 0;
}

static inline
warn_unused
int
node_insert_child(DndcContext* ctx, NodeHandle parent, size_t i, NodeHandle child){
    Node* node = get_node(ctx, parent);
    if(i >= node_children_count(node)){
        int e = append_child(ctx, parent, child);
        return e;
    }
    // This is a sloppy way of doing things, but appending
    // a child means we already have the right amount of
    // space.
    // It's sloppy as we could end up memmoving twice in a row.
    int e = append_child(ctx, parent, child);
    if(unlikely(e)) return e;
    NodeHandle* data = node_children(node);
    size_t count = node_children_count(node);
    size_t nmove = count - i - 1;
    if(nmove)
        memmove(data+i+1, data+i, nmove*sizeof(*data));
    data[i] = child;
    return 0;
}

static warn_unused int gather_anchor(DndcContext* ctx, NodeHandle handle, int node_depth);

static
warn_unused
int
gather_anchors(DndcContext* ctx){
    NodeHandle root = ctx->root_handle;
    return gather_anchor(ctx, root, 0);
}

static
warn_unused
int
gather_anchor(DndcContext* ctx, NodeHandle handle, int node_depth){
    if(node_depth > DNDC_MAX_NODE_DEPTH) return 0;
    Node* node = get_node(ctx, handle);
    switch(node->type){
        case NODE_BULLETS:
        case NODE_TABLE:
        case NODE_HEADING:
        case NODE_TITLE:
        case NODE_PARA:
        case NODE_DIV:
        case NODE_IMAGE:
        case NODE_LIST:
        case NODE_KEYVALUE:
        case NODE_IMGLINKS:
        case NODE_DEFLIST:
        case NODE_DEF:
        case NODE_DETAILS:
        case NODE_MD:
        case NODE_QUOTE:
        case NODE_CONTAINER:{
            StringView id = node_get_id(ctx, handle);
            if(id.length){
                int err = add_link_from_header(ctx, id);
                if(unlikely(err))
                    return DNDC_ERROR_OOM;
            }
        }
        // fall-through
        case NODE_IMPORT:
        case NODE_LIST_ITEM:
        case NODE_KEYVALUEPAIR:{
            NODE_CHILDREN_FOR_EACH(it, node){
                int err = gather_anchor(ctx, *it, node_depth+1);
                if(unlikely(err))
                    return DNDC_ERROR_OOM;
            }
        }break;
        case NODE_META:
        case NODE_TABLE_ROW:
        case NODE_STYLESHEETS:
        case NODE_LINKS:
        case NODE_SCRIPTS:
        case NODE_JS:
        case NODE_STRING:
        case NODE_TOC:
        case NODE_COMMENT:
        case NODE_INVALID:
            break;
        case NODE_PRE:
        case NODE_RAW:{
            StringView id = node_get_id(ctx, handle);
            if(id.length){
                int err = add_link_from_header(ctx, id);
                if(unlikely(err))
                    return DNDC_ERROR_OOM;
            }
        }break;
    }
    return 0;
}

static
inline
void
convert_node_to_container_containing_clone_of_former_self(DndcContext* ctx, NodeHandle handle){
    NodeHandle new_handle = alloc_handle(ctx);
    Node* new_node = get_node(ctx, new_handle);
    Node* old_node = get_node(ctx, handle);
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

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
