//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#ifndef DNDC_FORMAT_C
#define DNDC_FORMAT_C

#ifdef __linux__
#include <sys/types.h> // ssize_t
#endif
#include "dndc_node_types.h"
#include "dndc_types.h"
#include "dndc_funcs.h"
#include "dndc_logging.h"
#include "Utils/MStringBuilder.h"
#include "Utils/str_util.h"
#include "Utils/msb_format.h"

// NOTE: Formatting assumes a certain structure of the input tree
//       which may not necessarily hold if user scripts or the ast api
//       messes with the tree. It should always work on a freshly imported
//       tree though.

#ifdef _WIN32
// Move to header?
typedef long long ssize_t;
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum {FORMAT_WIDTH=79};
enum {FORMAT_INDENT=2};
typedef struct FormatState FormatState;
struct FormatState {
    // signed so it can go negative (simplifies things)
    // Check for <= 0, not == 0
    int lead;
    int col;
};

// Splits a string into (token, rest). Splits on ascii whitespace, treating all
// other characters as opaque (which means it actually works with non-ascii
// text).
//
// Rest is both an input parameter and an output parameter.
static
StringView
format_next_token(StringView* rest){
    StringView sv = lstripped_view(rest->text, rest->length);
    size_t i = 0;
    // Treat links as a single token.
    if(sv.length && sv.text[0] == '['){
        for(; i < sv.length; i++){
            switch(sv.text[i]){
                case ']':
                    goto breakloop;
                default:
                    continue;
            }
        }
        breakloop:;
    }
    for(; i < sv.length; i++){
        switch(sv.text[i]){
            case ' ':
            case '\t':
            case '\f':
            case '\v':
            case '\r':
                *rest = (StringView){
                    .text = sv.text+i,
                    .length = sv.length - i,
                };
                return (StringView){
                    .text = sv.text,
                    .length = i,
                };
            default:
                continue;
        }
    }
    *rest = (StringView){0};
    return sv;
}

// FIXME: If we have a really long token we break before inserting it even if that makes no sense.
static inline
void
format_write_wrapped_string(MStringBuilder* sb, FormatState* state, StringView sv){
    StringView rest = sv;
    if(state->col < state->lead){
        msb_write_nchar(sb, ' ', state->lead);
        state->col = state->lead;
    }
    while(rest.length){
        StringView token = format_next_token(&rest);
        if(!token.length)
            break;
        if(state->col+token.length > FORMAT_WIDTH){
            msb_write_char(sb, '\n');
            msb_write_nchar(sb, ' ', state->lead);
            state->col = state->lead;
        }
        else if(state->col != state->lead){
            msb_write_char(sb, ' ');
            state->col++;
        }
        msb_write_str(sb, token.text, token.length);
        state->col += token.length;
    }
}
static inline
_Bool
format_is_list_start(StringView sv){
    if(sv.length == 1){
        switch(sv.text[0]){
            case '-':
            case '+':
            case '*':
            case 'o':
                return 1;
            default:
                return 0;
        }
    }
    if(sv.length == 3 && SV_equals(sv, SV("•"))){
        return 1;
    }
    if(sv.length < 2) return 0;
    if(sv.text[sv.length-1] != '.') return 0;
    for(size_t i = 0; i < sv.length-1; i++){
        switch(sv.text[i]){
            case CASE_0_9:
                continue;
            default:
                return 0;
        }
    }
    return 1;
}

// MD nodes need their own implementation so they don't accidentally create
// lists. Other nodes don't need to worry about that.

static inline
void
format_md_write_wrapped_string(MStringBuilder* sb, FormatState* state, StringView sv){
    StringView rest = sv;
    if(state->col < state->lead){
        msb_write_nchar(sb, ' ', state->lead);
        state->col = state->lead;
    }
    while(rest.length){
        StringView token = format_next_token(&rest);
        if(!token.length)
            break;
        size_t len = token.length;
        if(rest.length){
            // This approach is kind of janky, but we tokenize as long as the
            // next token would be a list starter. Instead of pushing this into
            // an array, we just tokenize again.
            // This avoids dynamic allocation, but is almost surely slower.
            StringView rtmp = rest;
            while(rtmp.length){
                StringView next = format_next_token(&rtmp);
                if(format_is_list_start(next)){
                    len += next.length + 1;
                }
                else
                    break;
            }
        }
        if(state->col == state->lead){
        }
        else if(state->col+len > FORMAT_WIDTH){
            msb_write_char(sb, '\n');
            msb_write_nchar(sb, ' ', state->lead);
            state->col = state->lead;
        }
        else if(state->col != state->lead){
            msb_write_char(sb, ' ');
            state->col++;
        }
        msb_write_str(sb, token.text, token.length);
        state->col += token.length;
        len -= token.length;
        // Tokenize again to recover the rest of the tokens.
        while(len){
            // assert((ssize_t)len >= 0);
            // assert(rest.length);
            // assert(rest.text);
            StringView tok = format_next_token(&rest);
            // assert(format_is_list_start(tok));
            msb_write_char(sb, ' ');
            msb_write_str(sb, tok.text, tok.length);
            len -= 1 + tok.length;
        }
    }
}


static inline
void
remove_trailing_blank_lines(MStringBuilder* sb){
    while(sb->cursor >=2 && sb->data[sb->cursor-1] == '\n' && sb->data[sb->cursor-2] == '\n')
        msb_erase(sb, 1);
}

#define FORMATFUNCNAME(nt) format_##nt
#define FORMATFUNC(nt) static warn_unused int FORMATFUNCNAME(nt)(DndcContext* ctx, MStringBuilder* sb, Node* node, int indent)

FORMATFUNC(regular_node);
FORMATFUNC(md_node);
FORMATFUNC(table_node);
FORMATFUNC(kv_node);
FORMATFUNC(raw_node);
FORMATFUNC(md_list);
FORMATFUNC(para_node);

static warn_unused int format_md_bullets(DndcContext* ctx, MStringBuilder* sb, Node* node, int indent, int bullet_depth);

static inline
int
format_node(DndcContext* ctx, MStringBuilder* sb, Node* node, int indent){
    switch(node->type){
        case NODE_DIV:
        case NODE_TITLE:
        case NODE_HEADING:
        case NODE_TOC:
        case NODE_QUOTE:
            return format_regular_node(ctx, sb, node, indent);
        case NODE_KEYVALUE:
            return format_kv_node(ctx, sb, node, indent);
        case NODE_DEFLIST:
        case NODE_DEF:
        case NODE_DETAILS:
        case NODE_MD:
            return format_md_node(ctx, sb, node, indent);
        case NODE_META:
        case NODE_HEAD:
        case NODE_RAW:
        case NODE_PRE:
        case NODE_JS:
        case NODE_LINKS:
        case NODE_SCRIPTS:
        case NODE_STYLESHEETS:
        case NODE_IMPORT:
        case NODE_IMGLINKS:
        case NODE_IMAGE:
        case NODE_COMMENT:
            return format_raw_node(ctx, sb, node, indent);
        case NODE_TABLE:
            return format_table_node(ctx, sb, node, indent);
        case NODE_LIST:
            return format_md_list(ctx, sb, node, indent);
        case NODE_BULLETS:
            return format_md_bullets(ctx, sb, node, indent, 0);
        case NODE_CONTAINER:
        case NODE_TABLE_ROW:
        case NODE_PARA:
        case NODE_STRING:
        case NODE_LIST_ITEM:
        case NODE_KEYVALUEPAIR:
        case NODE_INVALID:
            NODE_LOG_ERROR(ctx, node, "Requested to format a node that can't be expressed as a top level dnd node: ", NODENAMES[node->type], ". It is possible that the parent node is not what you expect.");
            return DNDC_ERROR_INVALID_TREE;
    }
    unreachable();
}


static
int
format_tree(DndcContext* ctx, MStringBuilder* sb){
    Node* root = get_node(ctx, ctx->root_handle);
    if(root->type != NODE_MD && root->type != NODE_DIV && root->type != NODE_CONTAINER){
        NODE_LOG_ERROR(ctx, root, "Node is not of type MD, DIV or CONTAINER: ", NODENAMES[root->type]);
        return DNDC_ERROR_INVALID_TREE;
    }
    int result = 0;
    NODE_CHILDREN_FOR_EACH(it, root){
        Node* child = get_node(ctx, *it);
        // this is copy-paste from md_node, as the root is an
        // implicit, header-less md node.
        switch(child->type){
            case NODE_PARA:
                result = format_para_node(ctx, sb, child, 0);
                break;
            case NODE_BULLETS:
                result = format_md_bullets(ctx, sb, child, 0, 0);
                break;
            case NODE_LIST:
                result = format_md_list(ctx, sb, child, 0);
                break;
            default:
                result = format_node(ctx, sb, child, 0);
                break;
        }
        if(result)
            return result;
    }
    if(sb->cursor && sb->data[sb->cursor-1] != '\n')
        msb_write_char(sb, '\n');
    remove_trailing_blank_lines(sb);
    return 0;
}
static inline
void
format_header(DndcContext* ctx, MStringBuilder* sb, Node* node, int indent){
    msb_write_nchar(sb, ' ', indent);
    if(node->header.length)
        msb_write_str(sb, node->header.text,node->header.length);
    msb_write_char(sb, ':');
    msb_write_char(sb, ':');
    const StringView* alias = &NODETYPE_TO_NODE_ALIASES[node->type];
    // we fucked up if this is 0
    assert(alias->length);
    msb_write_str(sb, alias->text, alias->length);
    RARRAY_FOR_EACH(StringView, cls, node->classes){
        msb_write_literal(sb, " .");
        msb_write_str(sb, cls->text, cls->length);
    }
    if(node->attributes)
        for(size_t i = 0; i < node->attributes->count; i++){
            StringView2* attr = AttrTable_items(node->attributes)+i;
            if(!attr->key.length) continue;
            msb_write_literal(sb, " @");
            msb_write_str(sb, attr->key.text, attr->key.length);
            if(attr->value.length){
                msb_write_char(sb, '(');
                msb_write_str(sb, attr->value.text, attr->value.length);
                msb_write_char(sb, ')');
            }
        }
    NodeFlags flags = node->flags;
    if(node->type != NODE_IMPORT && (flags & NODEFLAG_IMPORT))
        msb_write_literal(sb, " #import");
    if(flags & NODEFLAG_ID){
        // #uggh
        // This is super gross, I hate this.
        // FIXME: use handles in the formatter instead of pointers.
        NodeHandle handle = {._value = (uint32_t)(node - ctx->nodes.data)};
        StringView idcontent = SV("");
        node_get_explicit_id(ctx, handle, &idcontent);
        MSB_FORMAT(sb, SV(" #id("), idcontent, SV(")"));
    }
    if(flags & NODEFLAG_NOID) msb_write_literal(sb, " #noid");
    if(flags & NODEFLAG_NOINLINE) msb_write_literal(sb, " #noinline");
    if(flags & NODEFLAG_HIDE) msb_write_literal(sb, " #hide");
    msb_write_char(sb, '\n');
}


FORMATFUNC(regular_node){
    format_header(ctx, sb, node, indent);
    indent += FORMAT_INDENT;
    int result = 0;
    FormatState state = {.lead = indent};
    NODE_CHILDREN_FOR_EACH(it, node){
        Node* child = get_node(ctx, *it);
        if(child->type == NODE_STRING){
            format_write_wrapped_string(sb, &state, child->header);
        }
        else {
            if(state.col)
                msb_write_char(sb, '\n');
            state.col = 0;
            result = format_node(ctx, sb, child, indent);
            if(result)
                return result;
        }
    }
    // unsure about this.
    if(state.col)
        msb_write_char(sb, '\n');
    return 0;
}

FORMATFUNC(para_node){
    FormatState state = {.lead = indent};
    NODE_CHILDREN_FOR_EACH(it, node){
        Node* child = get_node(ctx, *it);
        // assert(child->type == NODE_STRING);
        if(child->type != NODE_STRING){
            NODE_LOG_ERROR(ctx, node, "Children of paragraphs must be STRINGs");
            return DNDC_ERROR_INVALID_TREE;
        }
        format_md_write_wrapped_string(sb, &state, child->header);
    }
    if(state.col)
        msb_write_char(sb, '\n');
    msb_write_char(sb, '\n');
    return 0;
}

static
int
format_md_bullets(DndcContext* ctx, MStringBuilder* sb, Node* node, int indent, int bullet_depth){
    int result = 0;
    NODE_CHILDREN_FOR_EACH(it, node){
        Node* child = get_node(ctx, *it);
        // assert(child->type == NODE_LIST_ITEM);
        if(child->type != NODE_LIST_ITEM){
            NODE_LOG_ERROR(ctx, node, "Children of bullets must be LIST_ITEMs");
            return DNDC_ERROR_INVALID_TREE;
        }
        msb_write_nchar(sb, ' ', indent);
        switch(bullet_depth){
            case 0:
                msb_write_literal(sb, "• ");
                break;
            case 1:
                msb_write_literal(sb, "- ");
                break;
            default:
                msb_write_literal(sb, "+ ");
                break;
        }
        FormatState state = {.lead = indent+2, .col=indent+2};
        NODE_CHILDREN_FOR_EACH(subit, child){
            Node* subchild = get_node(ctx, *subit);
            if(subchild->type == NODE_STRING){
                format_md_write_wrapped_string(sb, &state, subchild->header);
            }
            else if(subchild->type == NODE_BULLETS){
                if(state.col != state.lead)
                    msb_write_char(sb, '\n');
                result = format_md_bullets(ctx, sb, subchild, indent+2, bullet_depth+1);
                if(result) return result;
                // FIXME: this is sketch - we shouldn't have strings after a nested list.
                state = (FormatState){.lead=indent+2, .col=indent+2};
            }
            else {
                if(state.col != state.lead)
                    msb_write_char(sb, '\n');
                result = format_node(ctx, sb, subchild, indent+2);
                if(result) return result;
                // FIXME: this is sketch - we shouldn't have strings after a nested list.
                state = (FormatState){.lead=indent+2, .col=indent+2};
            }
        }
        if(state.col != state.lead)
            msb_write_char(sb, '\n');
    }
    if(!bullet_depth)
        msb_write_char(sb, '\n');
    return 0;
}
FORMATFUNC(md_list){
    int result = 0;
    size_t count = node_children_count(node);
    int64_t numwidth = 1;
    numwidth += count > 9;
    numwidth += count > 99;
    numwidth += count > 999;
    numwidth += count > 9999;
    for(size_t i = 0; i < node_children_count(node); i++){
        Node* child = get_node(ctx, node_children(node)[i]);
        // assert(child->type == NODE_LIST_ITEM);
        if(child->type != NODE_LIST_ITEM){
            NODE_LOG_ERROR(ctx, node, "Children of lists must be LIST_ITEMs");
            return DNDC_ERROR_INVALID_TREE;
        }
        msb_write_nchar(sb, ' ', indent);
        MSB_FORMAT(sb, int_fmt(i+1), ". ");
        FormatState state = {.lead = indent+numwidth+2, .col=indent+numwidth+2};
        for(size_t j = 0; j < node_children_count(child); j++){
            Node* subchild = get_node(ctx, node_children(child)[j]);
            if(subchild->type == NODE_STRING){
                format_md_write_wrapped_string(sb, &state, subchild->header);
            }
            else {
                if(state.col != state.lead)
                    msb_write_char(sb, '\n');
                result = format_node(ctx, sb, subchild, indent+numwidth+2);
                if(result) return result;
                // FIXME: this is sketch - we shouldn't have strings after a nested list.
                state = (FormatState){.lead=indent+numwidth+2, .col=indent+numwidth+2};
            }
        }
        if(state.col != state.lead)
            msb_write_char(sb, '\n');
    }
    return 0;
}
FORMATFUNC(md_node){
    int result = 0;
    format_header(ctx, sb, node, indent);
    indent += FORMAT_INDENT;
    NODE_CHILDREN_FOR_EACH(it, node){
        Node* child = get_node(ctx, *it);
        switch(child->type){
            case NODE_PARA:
                result = format_para_node(ctx, sb, child, indent);
                break;
            case NODE_BULLETS:
                result = format_md_bullets(ctx, sb, child, indent, 0);
                break;
            case NODE_LIST:
                result = format_md_list(ctx, sb, child, indent);
                break;
            default:
                result = format_node(ctx, sb, child, indent);
                break;
        }
        if(result) return result;
    }
    // unsure about this.
    // msb_write_char(sb, '\n');
    return 0;
}

static inline
size_t
write_str_or_container(DndcContext* ctx, MStringBuilder* sb, Node* node){
    if(node->type == NODE_STRING){
        msb_write_str(sb, node->header.text, node->header.length);
        return node->header.length;
    }
    size_t writ = 0;
    for(size_t i = 0; i < node_children_count(node); i++){
        if(i != 0){
            msb_write_char(sb, ' ');
            writ++;
        }
        Node* str = get_node(ctx, node_children(node)[i]);
        msb_write_str(sb, str->header.text, str->header.length);
        writ += str->header.length;
    }
    return writ;
}

FORMATFUNC(table_node){
    int result = 0;
    format_header(ctx, sb, node, indent);
    indent += FORMAT_INDENT;
    ssize_t n_cells = 0;
    ssize_t widths[100] = {0};
    // pre-pass to figure out widths
    NODE_CHILDREN_FOR_EACH(row_iter, node){
        Node* row = get_node(ctx, *row_iter);
        if(row->type != NODE_TABLE_ROW)
            continue;
        if(unlikely(node_children_count(row) > arrlen(widths))){
            NODE_LOG_ERROR(ctx, row, SV("Row of a table has more than 100 entries. ("), node_children_count(row), SV("). This is not handled!"));
            return DNDC_ERROR_INVALID_TREE;
        }
        if(node_children_count(row) > (size_t)n_cells)
            n_cells = node_children_count(row);
        for(size_t j = 0; j < node_children_count(row); j++){
            Node* cell = get_node(ctx, node_children(row)[j]);
            if(cell->type == NODE_STRING){
                if((ssize_t)cell->header.length > widths[j])
                    widths[j] = cell->header.length;
            }
            else {
                assert(cell->type == NODE_CONTAINER);
                if(cell->type != NODE_CONTAINER){
                    NODE_LOG_ERROR(ctx, cell, "Expected a CONTAINER");
                    return DNDC_ERROR_INVALID_TREE;
                }
                size_t this_width = 0;
                NODE_CHILDREN_FOR_EACH(str_iter, cell){
                    Node* str = get_node(ctx, *str_iter);
                    // assert(str->type == NODE_STRING);
                    if(str->type != NODE_STRING){
                        NODE_LOG_ERROR(ctx, str, "Expected a STRING");
                        return DNDC_ERROR_INVALID_TREE;
                    }
                    this_width += 1 + str->header.length;
                }
                this_width -= 1;
                widths[j] = this_width;
            }
        }
    }
    // figure out if widths fit
    size_t total_except_last = indent;
    for(ssize_t i = 0; i < n_cells-1; i++){
        total_except_last += widths[i];
        total_except_last += sizeof(" | ")-1;
    }
    size_t total = total_except_last + (n_cells? widths[n_cells-1] : 0);
    size_t effective_space = FORMAT_WIDTH - indent;
    if(total < effective_space || total_except_last > effective_space){
        // do them all as a single line
        NODE_CHILDREN_FOR_EACH(row_iter, node){
            Node* row = get_node(ctx, *row_iter);
            if(row->type != NODE_TABLE_ROW){
                result = format_node(ctx, sb, row, indent);
                if(result) return result;
                continue;
            }
            for(size_t j = 0; j < node_children_count(row); j++){
                if(j != 0){
                    msb_write_literal(sb, " | ");
                }
                else
                    msb_write_nchar(sb, ' ', indent);
                Node* cell = get_node(ctx, node_children(row)[j]);
                size_t writ = write_str_or_container(ctx, sb, cell);
                if(j != node_children_count(row)-1 && widths[j] > (ssize_t)writ){
                    msb_write_nchar(sb, ' ', widths[j] - writ);
                }
            }
            msb_write_char(sb, '\n');
        }
    }
    else {
        // we need to wrap just the last line
        NODE_CHILDREN_FOR_EACH(row_iter, node){
            Node* row = get_node(ctx, *row_iter);
            if(row->type != NODE_TABLE_ROW){
                result = format_node(ctx, sb, row, indent);
                if(result) return result;
                continue;
            }
            for(ssize_t j = 0; j < (ssize_t)node_children_count(row) - 1; j++){
                if(j != 0){
                    msb_write_literal(sb, " | ");
                }
                else
                    msb_write_nchar(sb, ' ', indent);
                Node* cell = get_node(ctx, node_children(row)[j]);
                size_t writ = write_str_or_container(ctx, sb, cell);
                if(j != (ssize_t)node_children_count(row)-1 && widths[j] > (ssize_t)writ){
                    msb_write_nchar(sb, ' ', widths[j] - writ);
                }
            }
            if(node_children_count(row) > 1)
                msb_write_literal(sb, " | ");
            Node* last_cell = get_node(ctx, node_children(row)[node_children_count(row)-1]);
            FormatState state = {.lead = total_except_last, .col=total_except_last};
            if(last_cell->type == NODE_STRING){
                format_write_wrapped_string(sb, &state, last_cell->header);
            }
            else {
                NODE_CHILDREN_FOR_EACH(it, last_cell){
                    Node* str = get_node(ctx, *it);
                    format_write_wrapped_string(sb, &state, str->header);
                }
            }
            msb_write_char(sb, '\n');
        }
    }
    return 0;
}

FORMATFUNC(kv_node){
    int result = 0;
    remove_trailing_blank_lines(sb);
    format_header(ctx, sb, node, indent);
    indent += FORMAT_INDENT;
    size_t key_width = 0;
    NODE_CHILDREN_FOR_EACH(it, node){
        Node* child = get_node(ctx, *it);
        if(child->type != NODE_KEYVALUEPAIR)
            continue;
        StringView key = get_node(ctx, node_children(child)[0])->header;
        if(key.length > key_width)
            key_width = key.length;
    }
    key_width += 2; // for the ": "
    NODE_CHILDREN_FOR_EACH(it, node){
        Node* child = get_node(ctx, *it);
        if(child->type != NODE_KEYVALUEPAIR){
            result = format_node(ctx, sb, child, indent);
            if(result) return result;
            continue;
        }
        msb_write_nchar(sb, ' ', indent);
        // assert(node_children_count(child) == 2);
        if(node_children_count(child) != 2){
            NODE_LOG_ERROR(ctx, child, "Expected two children");
            return DNDC_ERROR_INVALID_TREE;
        }
        StringView key = get_node(ctx, node_children(child)[0])->header;
        Node* value_node = get_node(ctx, node_children(child)[1]);
        msb_write_str(sb, key.text, key.length);
        msb_write_literal(sb, ": ");
        int64_t padding = key_width - key.length - 2;
        msb_write_nchar(sb, ' ', padding);
        FormatState state = {.lead=key_width+indent, .col=key_width+indent};
        if(value_node->type == NODE_STRING){
            StringView value = value_node->header;
            format_write_wrapped_string(sb, &state, value);
        }
        else {
            // assert(value_node->type == NODE_CONTAINER);
            if(value_node->type != NODE_CONTAINER){
                NODE_LOG_ERROR(ctx, value_node, "Expected a CONTAINER");
                return DNDC_ERROR_INVALID_TREE;
            }
            NODE_CHILDREN_FOR_EACH(c, value_node){
                Node* n = get_node(ctx, *c);
                // assert(n->type == NODE_STRING);
                if(n->type != NODE_STRING){
                    NODE_LOG_ERROR(ctx, n, "Expected a STRING");
                    return DNDC_ERROR_INVALID_TREE;
                }
                format_write_wrapped_string(sb, &state, n->header);
            }
        }
        msb_write_char(sb, '\n');
    }
    // msb_write_char(sb, '\n');
    return 0;
}
FORMATFUNC(raw_node){
    int result = 0;
    format_header(ctx, sb, node, indent);
    indent += FORMAT_INDENT;
    size_t nspace = indent < 80? indent: 80;
    NODE_CHILDREN_FOR_EACH(it, node){
        Node* child = get_node(ctx, *it);
        if(child->type != NODE_STRING){
            result = format_node(ctx, sb, child, indent);
            if(result) return result;
            continue;
        }
        // assert(child->type == NODE_STRING);
        if(child->type != NODE_STRING){
            NODE_LOG_ERROR(ctx, child, "Expected A STRING");
            return DNDC_ERROR_INVALID_TREE;
        }
        if(child->header.length){
            msb_write_nchar(sb, ' ', nspace);
            msb_write_str(sb, child->header.text, child->header.length);
        }
        msb_write_char(sb, '\n');
    }
    // msb_write_char(sb, '\n');
    return 0;
}

#undef FORMATFUNC
#undef FORMATFUNCNAME

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
