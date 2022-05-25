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
static void write_md_string(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb);
static warn_unused int write_md_bullets(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int depth);
static warn_unused int write_md_list(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int depth);
static warn_unused int write_md_keyvalue(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb);
static warn_unused int write_md_table(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth);
static void write_md_raw(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb);

static
warn_unused
int
render_md(DndcContext* ctx, MStringBuilder* sb){
    NodeHandle root = ctx->root_handle;
    if(NodeHandle_eq(root, INVALID_NODE_HANDLE)) {
        LOG_ERROR(ctx, ctx->filename, -1, -1, "Request to render tree to markdown without a root node");
        return DNDC_ERROR_INVALID_TREE;
    }
    msb_write_literal(sb, "<!-- This md file was generated from a dnd file. -->\n");
    return render_node_as_md(ctx, root, sb, 2);
}

// returns how much the header depth has increased.
static
int
write_md_header(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth){
    Node* node = get_node(ctx, handle);
    StringView header = node->header;
    if(!header.length) return 0;
    MSB_FORMAT(sb, "<h", header_depth, ">");
    write_md_string(ctx, handle, sb);
    // msb_write_str(sb, header.text, header.length);
    MSB_FORMAT(sb, "</h", header_depth, ">");
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
        case NODE_DEFLIST:{
            header_depth += write_md_header(ctx, handle, sb, header_depth);
            msb_write_literal(sb, "<dl>\n");
            NODE_CHILDREN_FOR_EACH(ch, node){
                int err = render_node_as_md(ctx, *ch, sb, header_depth);
                if(err) return err;
                msb_write_char(sb, '\n');
            }
            msb_write_literal(sb, "</dl>\n");
        }return 0;
        case NODE_DEF:{
            msb_write_literal(sb, "<dt>");
            write_md_string(ctx, handle, sb);
            msb_write_literal(sb, "</dt>\n");
            msb_write_literal(sb, "<dd>\n");
            NODE_CHILDREN_FOR_EACH(ch, node){
                int err = render_node_as_md(ctx, *ch, sb, header_depth);
                if(err) return err;
                msb_write_char(sb, '\n');
            }
            msb_write_literal(sb, "</dd>\n");
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
            write_md_string(ctx, handle, sb);
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
            // some markdowns allow inline style tags.
            msb_write_literal(sb, "<style>\n");
            write_md_raw(ctx, handle, sb);
            msb_write_literal(sb, "</style>\n");
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
            int err = write_md_bullets(ctx, handle, sb, 0);
            if(err) return err;
        }return 0;
        case NODE_RAW:{
            write_md_raw(ctx, handle, sb);
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
            int err = write_md_list(ctx, handle, sb, 0);
            if(err) return err;
        }return 0;
        case NODE_LIST_ITEM:
            NODE_LOG_ERROR(ctx, node, "Unexpected list item");
            return DNDC_ERROR_INVALID_TREE;
        case NODE_KEYVALUE:{
            header_depth += write_md_header(ctx, handle, sb, header_depth);
            int err = write_md_keyvalue(ctx, handle, sb);
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
            msb_write_literal(sb, "<details><summary>");
            write_md_string(ctx, handle, sb);
            // header_depth += write_md_header(ctx, handle, sb, header_depth);
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
void
write_md_string(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb){
    // This is a slow character by character implementation, but
    // this is a rarely used piece. We can SIMD it later.
    //
    // We need to scan for links and convert them to markdown links.
    Node* node = get_node(ctx, handle);
    StringView header = node->header;
    if(!header.length) return;
    const char* text = header.text;
    size_t length = header.length;
    for(size_t i = 0; i < length; i++){
        char c = text[i];
        switch(c){
            case '[':{
                const char* closing_brace = memchr(text+i, ']', length-i);
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
}
static
warn_unused
int
write_md_bullets(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int depth){
    int result;
    Node* node = get_node(ctx, handle);
    NODE_CHILDREN_FOR_EACH(it, node){
        Node* li = get_node(ctx, *it);
        if(li->type != NODE_LIST_ITEM){
            NODE_LOG_ERROR(ctx, li, "Non list-item child of bullets: ", quoted(LS_to_SV(NODENAMES[li->type])));
            return DNDC_ERROR_INVALID_TREE;
        }
        msb_write_nchar(sb, ' ', 4*depth);
        switch(depth){
            case 0:  msb_write_literal(sb, "* "); break;
            case 1:  msb_write_literal(sb, "+ "); break;
            default: msb_write_literal(sb, "- "); break;
        }
        size_t count = node_children_count(li);
        if(!count){
            NODE_LOG_ERROR(ctx, li, "List item must have at least one child.");
            return DNDC_ERROR_INVALID_TREE;
        }
        size_t i = 0;
        NODE_CHILDREN_FOR_EACH(subitem, li){
            Node* sub = get_node(ctx, *subitem);
            if(i == 0){
                if(sub->type != NODE_STRING){
                    NODE_LOG_ERROR(ctx, sub, "First list item must be a string, got ", quoted(LS_to_SV(NODENAMES[sub->type])));
                    return DNDC_ERROR_INVALID_TREE;
                }
                write_md_string(ctx, *subitem, sb);
                msb_write_char(sb, '\n');
                i++;
                continue;
            }
            if(sub->type == NODE_STRING){
                msb_write_nchar(sb, ' ', (depth+1)*4);
                write_md_string(ctx, *subitem, sb);
                msb_write_char(sb, '\n');
            }
            else if(sub->type == NODE_BULLETS){
                result = write_md_bullets(ctx, *subitem, sb, depth+1);
                if(result) return result;
            }
            else if(sub->type == NODE_LIST){
                result = write_md_list(ctx, *subitem, sb, depth+1);
                if(result) return result;
            }
            else {
                NODE_LOG_ERROR(ctx, sub, "List items must contain strings, bullets or lists, got: ", quoted(LS_to_SV(NODENAMES[sub->type])));
                return DNDC_ERROR_INVALID_TREE;
            }
            i++;
        }
    }
    return 0;
}
static
warn_unused
int
write_md_list(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int depth){
    int result;
    Node* node = get_node(ctx, handle);
    NODE_CHILDREN_FOR_EACH(it, node){
        Node* li = get_node(ctx, *it);
        if(li->type != NODE_LIST_ITEM){
            NODE_LOG_ERROR(ctx, li, "Non list-item child of bullets: ", quoted(LS_to_SV(NODENAMES[li->type])));
            return DNDC_ERROR_INVALID_TREE;
        }
        msb_write_nchar(sb, ' ', 4*depth);
        msb_write_literal(sb, "1. ");
        size_t count = node_children_count(li);
        if(!count){
            NODE_LOG_ERROR(ctx, li, "List item must have at least one child.");
            return DNDC_ERROR_INVALID_TREE;
        }
        size_t i = 0;
        NODE_CHILDREN_FOR_EACH(subitem, li){
            Node* sub = get_node(ctx, *subitem);
            if(i == 0){
                if(sub->type != NODE_STRING){
                    NODE_LOG_ERROR(ctx, sub, "First list item must be a string, got ", quoted(LS_to_SV(NODENAMES[sub->type])));
                    return DNDC_ERROR_INVALID_TREE;
                }
                write_md_string(ctx, *subitem, sb);
                msb_write_char(sb, '\n');
                i++;
                continue;
            }
            if(sub->type == NODE_STRING){
                msb_write_nchar(sb, ' ', (depth+1)*4);
                write_md_string(ctx, *subitem, sb);
                msb_write_char(sb, '\n');
            }
            else if(sub->type == NODE_BULLETS){
                result = write_md_bullets(ctx, *subitem, sb, depth+1);
                if(result) return result;
            }
            else if(sub->type == NODE_LIST){
                result = write_md_list(ctx, *subitem, sb, depth+1);
                if(result) return result;
            }
            else {
                NODE_LOG_ERROR(ctx, sub, "List items must contain strings, bullets or lists, got: ", quoted(LS_to_SV(NODENAMES[sub->type])));
                return DNDC_ERROR_INVALID_TREE;
            }
            i++;
        }
    }
    return 0;
}
static
warn_unused
int
write_md_keyvalue(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb){
    // Most markdowns don't support tables without headers, so just use
    // an html table.
    msb_write_literal(sb, "<table>\n<tbody>\n");
    Node* node = get_node(ctx, handle);
    NODE_CHILDREN_FOR_EACH(ch, node){
        Node* child = get_node(ctx, *ch);
        if(child->type != NODE_KEYVALUEPAIR){
            NODE_LOG_ERROR(ctx, child, "Expected keyvaluepair child of keyvalue node when rendering md");
            return DNDC_ERROR_INVALID_TREE;
        }
        if(node_children_count(child) != 2){
            NODE_LOG_ERROR(ctx, child, "Expected two string children of keyvaluepair node when rendering md");
            return DNDC_ERROR_INVALID_TREE;
        }
        NodeHandle kh = node_children(child)[0];
        NodeHandle vh = node_children(child)[1];
        Node* key = get_node(ctx, kh);
        Node* value = get_node(ctx, vh);
        if(key->type != NODE_STRING){
            NODE_LOG_ERROR(ctx, key, "Expected two string children of keyvaluepair node when rendering md");
            return DNDC_ERROR_INVALID_TREE;
        }
        if(value->type != NODE_STRING && value->type != NODE_CONTAINER){
            NODE_LOG_ERROR(ctx, value, "Expected two string children of keyvaluepair node when rendering md");
            return DNDC_ERROR_INVALID_TREE;
        }
        msb_write_literal(sb, "<tr>\n<td>");
        write_md_string(ctx, kh, sb);
        msb_write_literal(sb, "</td>\n<td>");
        if(value->type == NODE_CONTAINER){
            NODE_CHILDREN_FOR_EACH(c, value){
                Node* chi = get_node(ctx, *c);
                if(chi->type != NODE_STRING) continue;
                write_md_string(ctx, *c, sb);
            }
        }
        else {
            write_md_string(ctx, vh, sb);
        }
        msb_write_literal(sb, "</td>\n</tr>\n");
    }
    msb_write_literal(sb, "</tbody>\n</table>\n");
    return 0;
}
static
warn_unused
int
write_md_table(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int header_depth){
    // Original markdown doesn't have table support, so just use an html
    // table anyway.
    msb_write_literal(sb, "<table>\n<thead>\n");
    Node* node = get_node(ctx, handle);
    size_t count = node_children_count(node);
    NodeHandle* children = node_children(node);
    if(count){
        Node* child = get_node(ctx, children[0]);
        if(unlikely(child->type != NODE_TABLE_ROW)){
            NODE_LOG_ERROR(ctx, child, LS("children of a table ought to be table rows..."));
            return DNDC_ERROR_INVALID_TREE;
        }
        // inline rendering table row here so we can do heads
        msb_write_literal(sb, "<tr>\n");
        NODE_CHILDREN_FOR_EACH(it, child){
            msb_write_literal(sb, "<th>");
            int e = render_node_as_md(ctx, *it, sb, header_depth+1);
            if(e) return e;
            msb_write_literal(sb, "</th>\n");
        }
        msb_write_literal(sb, "</tr>\n");
    }
    msb_write_literal(sb, "</thead>\n<tbody>\n");
    for(size_t i = 1; i < count; i++){
        NodeHandle ch = children[i];
        Node* child = get_node(ctx, ch);
        if(unlikely(child->type != NODE_TABLE_ROW)){
            NODE_LOG_ERROR(ctx, child, LS("children of a table ought to be table rows..."));
            return DNDC_ERROR_INVALID_TREE;
        }
        msb_write_literal(sb, "<tr>\n");
        NODE_CHILDREN_FOR_EACH(it, child){
            msb_write_literal(sb, "<td>");
            int e = render_node_as_md(ctx, *it, sb, header_depth+1);
            if(e) return e;
            msb_write_literal(sb, "</td>\n");
        }
        msb_write_literal(sb, "</tr>\n");
    }
    msb_write_literal(sb, "</tbody>\n</table>\n");
    return 0;
}
static
void
write_md_raw(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb){
    // trust the user
    Node* node = get_node(ctx, handle);
    NODE_CHILDREN_FOR_EACH(ch, node){
        Node* child = get_node(ctx, *ch);
        if(child->type != NODE_STRING){
            continue;
        }
        if(child->header.length)
            msb_write_str(sb, child->header.text, child->header.length);
        msb_write_char(sb, '\n');
    }
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
