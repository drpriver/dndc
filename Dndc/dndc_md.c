#ifndef DNDC_MD_C
#define DNDC_MD_C
#include "dndc_long_string.h"
#include "dndc_types.h"
#include "dndc_node_types.h"
#include "dndc_funcs.h"
#include "dndc_logging.h"
#include "Utils/MStringBuilder.h"
#include "Utils/msb_format.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
warn_unused
int
render_node_as_md(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth);
static warn_unused int write_md_string(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb);
static warn_unused int write_md_bullets(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth);
static warn_unused int write_md_list(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth);
static warn_unused int write_md_keyvalue(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth);
static warn_unused int write_md_table(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth);
static warn_unused int write_md_raw(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb);

static
warn_unused
int
render_md(DndcContext* ctx, MStringBuilder* sb){
    NodeHandle root = ctx->root_handle;
    if(NodeHandle_eq(root, INVALID_NODE_HANDLE)) {
        LOG_ERROR(ctx, ctx->filename, -1, -1, "Request to render tree to markdown without a root node");
        return DNDC_ERROR_INVALID_TREE;
    }
    return render_node_as_md(ctx, root, sb, 2);
}

// returns how much the header depth has increased.
static 
int
write_md_header(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth){
    Node* node = get_node(ctx, handle);
    StringView header = node->header;
    if(!header.length) return 0;
    msb_write_nchar(sb, '#', header_depth);
    int err = write_md_string(ctx, handle, sb);
    (void)err; // can't fail.
    // msb_write_str(sb, header.text, header.length);
    msb_write_char(sb, '\n');
    return 1;
}


static
warn_unused
int
render_node_as_md(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth){
    Node* node = get_node(ctx, handle);
    switch(node->type){
        case NODE_MD:{
            header_depth += write_md_header(ctx, handle, sb, header_depth);
            NODE_CHILDREN_FOR_EACH(ch, node){
                int err = render_node_as_md(ctx, *ch, sb, header_depth);
                if(err) return err;
                msb_write_char(sb, '\n');
            }
        }return 0;
        case NODE_DIV:{
            header_depth += write_md_header(ctx, handle, sb, header_depth);
            msb_write_literal(sb, "<div>\n");
            NODE_CHILDREN_FOR_EACH(ch, node){
                int err = render_node_as_md(ctx, *ch, sb, header_depth);
                if(err) return err;
                msb_write_char(sb, '\n');
            }
            msb_write_literal(sb, "</div>\n");
        }return 0;
        case NODE_STRING:{
            int err = write_md_string(ctx, handle, sb);
            if(err) return err;
            return 0;
        }
        case NODE_PARA:{
            msb_write_char(sb, '\n');
            NODE_CHILDREN_FOR_EACH(ch, node){
                int err = render_node_as_md(ctx, *ch, sb, header_depth);
                if(err) return err;
                msb_write_char(sb, '\n');
            }
        }return 0;
        case NODE_TITLE:
            write_md_header(ctx, handle, sb, 1);
            return 0;
        case NODE_HEADING:
            write_md_header(ctx, handle, sb, header_depth);
            return 0;
        case NODE_TABLE:{
            header_depth += write_md_header(ctx, handle, sb, header_depth);
            int err = write_md_table(ctx, handle, sb, header_depth);
            if(err) return err;
        }return 0;
        case NODE_TABLE_ROW:
            NODE_LOG_ERROR(ctx, node, "Unexpected table row");
            return DNDC_ERROR_INVALID_TREE;
        case NODE_STYLESHEETS:
            return 0; // ignore
        case NODE_LINKS:
            return 0; // ignore
        case NODE_SCRIPTS:
            return 0; // ignore
        case NODE_IMPORT:
            return 0; // ignore
        case NODE_IMAGE:
            return 0; // punt
        case NODE_BULLETS:{
            header_depth += write_md_header(ctx, handle, sb, header_depth);
            int err = write_md_bullets(ctx, handle, sb, header_depth);
            if(err) return err;
        }return 0;
        case NODE_RAW:{
            int err = write_md_raw(ctx, handle, sb);
            if(err) return err;
        }return 0;
        case NODE_PRE:{
            msb_write_literal(sb, "<pre>\n");
            NODE_CHILDREN_FOR_EACH(ch, node){
                int err = render_node_as_md(ctx, *ch, sb, header_depth);
                if(err) return err;
                msb_write_char(sb, '\n');
            }
            msb_write_literal(sb, "</pre>\n");
        }return 0;
        case NODE_LIST:{
            header_depth += write_md_header(ctx, handle, sb, header_depth);
            int err = write_md_list(ctx, handle, sb, header_depth);
            if(err) return err;
        }return 0;
        case NODE_LIST_ITEM:
            NODE_LOG_ERROR(ctx, node, "Unexpected list item");
            return DNDC_ERROR_INVALID_TREE;
        case NODE_KEYVALUE:{
            header_depth += write_md_header(ctx, handle, sb, header_depth);
            int err = write_md_keyvalue(ctx, handle, sb, header_depth);
            if(err) return err;
        }return 0;
        case NODE_KEYVALUEPAIR:
            NODE_LOG_ERROR(ctx, node, "Unexpected kv pair");
            return DNDC_ERROR_INVALID_TREE;
        case NODE_IMGLINKS:
            return 0; // punt
        case NODE_TOC:
            return 0;
        case NODE_COMMENT:
            return 0;
        case NODE_CONTAINER:
            NODE_CHILDREN_FOR_EACH(ch, node){
                int err = render_node_as_md(ctx, *ch, sb, header_depth);
                if(err) return err;
                msb_write_char(sb, '\n');
            }
            return 0;
        case NODE_QUOTE:{
            msb_write_literal(sb, "<blockquote>\n");
            NODE_CHILDREN_FOR_EACH(ch, node){
                int err = render_node_as_md(ctx, *ch, sb, header_depth);
                if(err) return err;
                msb_write_char(sb, '\n');
            }
            msb_write_literal(sb, "</blockquote>\n");
        }return 0;
        case NODE_JS:
            return 0;
        case NODE_DETAILS:
            msb_write_literal(sb, "<details><summary>\n");
            header_depth += write_md_header(ctx, handle, sb, header_depth);
            msb_write_literal(sb, "</summary>\n");
            NODE_CHILDREN_FOR_EACH(ch, node){
                int err = render_node_as_md(ctx, *ch, sb, header_depth);
                if(err) return err;
                msb_write_char(sb, '\n');
            }
            msb_write_literal(sb, "</details>\n");
            return 0;
        case NODE_META:
            return 0;
        case NODE_INVALID:
            break;
    }
    return DNDC_ERROR_INVALID_TREE;
}

static 
warn_unused 
int 
write_md_string(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb){
    // This is a slow character by character implementation, but 
    // this is a rarely used piece. We can SIMD it later.
    //
    // We need to scan for links and convert them to markdown links.
    Node* node = get_node(ctx, handle);
    StringView header = node->header;
    if(!header.length) return 0;
    const char* text = header.text;
    size_t length = header.length;
    for(size_t i = 0; i < length; i++){
        char c = text[i];
        switch(c){
            case '[':{
                const char* closing_brace = memchr(text+i, ']', length-1);
                if(unlikely(!closing_brace)){
                    msb_write_char(sb, c);
                    break;
                }
                const size_t link_length = closing_brace - (text+i);
                size_t text_length = link_length-1;
                // Check for '|'-delimited alias.
                const char* alias = memchr(text+i+1, '|', link_length);
                if(likely(!alias)){
                    alias = text+i+1;
                }
                else {
                    text_length = alias - (text+i+1);
                    alias += 1;
                }
                size_t alias_length = closing_brace - alias;
                MStringBuilder temp = {.allocator=temp_allocator(ctx)};
                msb_write_kebab(&temp, alias, alias_length);
                if(!temp.cursor){
                    msb_write_literal(sb, "[ ]");
                }
                else {
                    StringView temp_str = msb_borrow_sv(&temp);
                    if(SV_equals(temp_str, SV("x"))){
                        msb_write_literal(sb, "[x]");
                    }
                    else {
                        msb_write_literal(sb, "[");
                        StringView sv = stripped_view(text+i+1, text_length);
                        msb_write_str(sb, sv.text, sv.length);
                        msb_write_literal(sb, "](");
                        const StringView* value = find_link_target(ctx, temp_str);
                        if(unlikely(!value)){
                            msb_write_str(sb, temp_str.text, temp_str.length);
                        }
                        else {
                            const StringView* val = value;
                            msb_write_str(sb, val->text, val->length);
                        }
                        msb_write_literal(sb, ")");
                    }
                }
                i += link_length;
            }break;
            // replace <code> and </code> with `
            case '<':{
                if(length - i >= sizeof("<code>")-1){
                    if(memcmp(text+i, "<code>", sizeof("<code>")-1) == 0){
                        msb_write_char(sb, '`');
                        i += sizeof("<code>")-1-1;
                        break;
                    }
                }
                if(length - i >= sizeof("</code>")-1){
                    if(memcmp(text+i, "</code>", sizeof("</code>")-1) == 0){
                        msb_write_char(sb, '`');
                        i += sizeof("</code>")-1-1;
                        break;
                    }
                }
            }
            // fall-through
            default:
                msb_write_char(sb, c);
        }
    }
    return 0;
}
static 
warn_unused 
int 
write_md_bullets(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth){
    (void)ctx, (void)handle, (void)sb, (void)header_depth;
    return DNDC_ERROR_FILE_READ;
}
static 
warn_unused 
int 
write_md_list(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth){
    (void)ctx, (void)handle, (void)sb, (void)header_depth;
    return DNDC_ERROR_FILE_READ;
}
static 
warn_unused 
int 
write_md_keyvalue(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth){
    (void)ctx, (void)handle, (void)sb, (void)header_depth;
    return DNDC_ERROR_FILE_READ;
}
static 
warn_unused 
int 
write_md_table(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth){
    (void)ctx, (void)handle, (void)sb, (void)header_depth;
    return DNDC_ERROR_FILE_READ;
}
static 
warn_unused 
int 
write_md_raw(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb){
    (void)ctx, (void)handle, (void)sb;
    return DNDC_ERROR_FILE_READ;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
