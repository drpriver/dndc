#ifndef DNDC_FORMAT_C
#define DNDC_FORMAT_C
#include "dndc_node_types.h"
#include "dndc_types.h"
#include "dndc_funcs.h"
#include "str_util.h"
#include "msb_format.h"

// NOTE: Formatting can only be applied to freshly parsed syntax
//       trees. The code makes assertions about what kinds of nodes
//       can be children of other nodes, which won't necessarily hold
//       true if python blocks start inserting nodes willy-nilly.
//
//       It would be nice to be able to apply this to post-import and
//       post-python blocks to see what something expands to.

#ifdef _WIN32
// Move to header?
typedef long long ssize_t;
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum {FORMAT_WIDTH=80};
enum {FORMAT_INDENT=2};
typedef struct FormatState {
    // signed so it can go negative (simplifies things)
    // Check for <= 0, not == 0
    int lead;
    int col;
} FormatState;

typedef struct FormatTokenized {
    StringView token;
    StringView rest;
} FormatTokenized;

// Splits a string into (token, rest). Splits on ascii whitespace, treating all
// other characters as opaque (which means it actually works with non-ascii
// text).
static
FormatTokenized
format_next_token(StringView sv){
    sv = lstripped_view(sv.text, sv.length);
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
                return (FormatTokenized){
                    .token.text = sv.text,
                    .token.length = i,
                    .rest.text = sv.text+i,
                    .rest.length = sv.length - i,
                    };
            default:
                continue;
            }
        }
    return (FormatTokenized){
        .token = sv,
        .rest = {},
        };
    }

static inline
void
format_write_wrapped_string(MStringBuilder* sb, FormatState* state, StringView sv){
    FormatTokenized tokenized= {.rest=sv};
    if(state->col < state->lead){
        msb_write_nchar(sb, ' ', state->lead);
        state->col = state->lead;
        }
    while(tokenized.rest.length){
        tokenized = format_next_token(tokenized.rest);
        if(!tokenized.token.length)
            break;
        if(state->col+tokenized.token.length > FORMAT_WIDTH){
            msb_write_char(sb, '\n');
            msb_write_nchar(sb, ' ', state->lead);
            state->col = state->lead;
            }
        else if(state->col != state->lead){
            msb_write_char(sb, ' ');
            state->col++;
            }
        msb_write_str(sb, tokenized.token.text, tokenized.token.length);
        state->col += tokenized.token.length;
        }
    }

#define FORMATFUNCNAME(nt) format_##nt
#define FORMATFUNC(nt) static Errorable_f(void) FORMATFUNCNAME(nt)(DndcContext* ctx, MStringBuilder* sb, Node* node, int indent)

FORMATFUNC(regular_node);
FORMATFUNC(md_node);
FORMATFUNC(table_node);
FORMATFUNC(text_node);
FORMATFUNC(kv_node);
FORMATFUNC(raw_node);
FORMATFUNC(md_list);
FORMATFUNC(para_node);

static Errorable_f(void) format_md_bullets(DndcContext* ctx, MStringBuilder* sb, Node* node, int indent, int bullet_depth);

static inline
Errorable_f(void)
format_node(DndcContext* ctx, MStringBuilder* sb, Node* node, int indent){
    switch(node->type){
        case NODE_DIV:
        case NODE_TITLE:
        case NODE_HEADING:
        case NODE_NAV:
        case NODE_DATA:
        case NODE_QUOTE:
        case NODE_HR:
            return format_regular_node(ctx, sb, node, indent);
        case NODE_TEXT:
            return format_text_node(ctx, sb, node, indent);
        case NODE_KEYVALUE:
            return format_kv_node(ctx, sb, node, indent);
        case NODE_MD:
            return format_md_node(ctx, sb, node, indent);
        case NODE_RAW:
        case NODE_PRE:
        case NODE_PYTHON:
        case NODE_JS:
        case NODE_DEPENDENCIES:
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
            // Error cond
            unreachable();
        }
    unreachable();
    }


static
Errorable_f(void)
format_tree(DndcContext* ctx, MStringBuilder* sb){
    auto root = get_node(ctx, ctx->root_handle);
    Errorable(void) result = {};
    NODE_CHILDREN_FOR_EACH(it, root){
        auto child = get_node(ctx, *it);
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
        if(result.errored)
            return result;
        }
    if(sb->cursor && sb->data[sb->cursor-1] != '\n')
        msb_write_char(sb, '\n');
    return result;
    }
static inline
void
format_header(MStringBuilder* sb, Node* node, int indent){
    msb_write_nchar(sb, ' ', indent);
    if(node->header.length){
        msb_write_str(sb, node->header.text,node->header.length);
        }
    msb_write_char(sb, ':');
    msb_write_char(sb, ':');
    auto alias = &NODETYPE_TO_NODE_ALIASES[node->type];
    // we fucked up if this is 0
    assert(alias->length);
    msb_write_str(sb, alias->text, alias->length);
    RARRAY_FOR_EACH(cls, node->classes){
        msb_write_literal(sb, " .");
        msb_write_str(sb, cls->text, cls->length);
        }
    RARRAY_FOR_EACH(attr, node->attributes){
        msb_write_literal(sb, " @");
        msb_write_str(sb, attr->key.text, attr->key.length);
        if(attr->value.length){
            msb_write_char(sb, '(');
            msb_write_str(sb, attr->value.text, attr->value.length);
            msb_write_char(sb, ')');
            }
        }
    msb_write_char(sb, '\n');
    }


FORMATFUNC(regular_node){
    format_header(sb, node, indent);
    indent += FORMAT_INDENT;
    Errorable(void) result = {};
    FormatState state = {.lead = indent};
    NODE_CHILDREN_FOR_EACH(it, node){
        auto child = get_node(ctx, *it);
        if(child->type == NODE_STRING){
            format_write_wrapped_string(sb, &state, child->header);
            }
        else {
            if(state.col)
                msb_write_char(sb, '\n');
            state.col = 0;
            result = format_node(ctx, sb, child, indent);
            if(result.errored)
                return result;
            }
        }
    // unsure about this.
    if(state.col)
        msb_write_char(sb, '\n');
    return result;
    }
FORMATFUNC(para_node){
    Errorable(void) result = {};
    FormatState state = {.lead = indent};
    NODE_CHILDREN_FOR_EACH(it, node){
        auto child = get_node(ctx, *it);
        assert(child->type == NODE_STRING);
        format_write_wrapped_string(sb, &state, child->header);
        }
    if(state.col)
        msb_write_char(sb, '\n');
    msb_write_char(sb, '\n');
    return result;
    }
static
Errorable_f(void)
format_md_bullets(DndcContext* ctx, MStringBuilder* sb, Node* node, int indent, int bullet_depth){
    Errorable(void) result = {};
    NODE_CHILDREN_FOR_EACH(it, node){
        auto child = get_node(ctx, *it);
        assert(child->type == NODE_LIST_ITEM);
        msb_write_nchar(sb, ' ', indent);
        switch(bullet_depth){
            case 0:
                msb_write_literal(sb, "* ");
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
            auto subchild = get_node(ctx, *subit);
            if(subchild->type == NODE_STRING){
                format_write_wrapped_string(sb, &state, subchild->header);
                }
            else if(subchild->type == NODE_BULLETS){
                if(state.col != state.lead)
                    msb_write_char(sb, '\n');
                result = format_md_bullets(ctx, sb, subchild, indent+2, bullet_depth+1);
                if(result.errored)
                    return result;
                // FIXME: this is sketch - we shouldn't have strings after a nested list.
                state = (FormatState){.lead=indent+2, .col=indent+2};
                }
            else {
                if(state.col != state.lead)
                    msb_write_char(sb, '\n');
                result = format_node(ctx, sb, subchild, indent+2);
                if(result.errored) return result;
                // FIXME: this is sketch - we shouldn't have strings after a nested list.
                state = (FormatState){.lead=indent+2, .col=indent+2};
                }
            }
        if(state.col != state.lead)
            msb_write_char(sb, '\n');
        }
    if(!bullet_depth)
        msb_write_char(sb, '\n');
    return result;
    }
FORMATFUNC(md_list){
    Errorable(void) result = {};
    auto count = node->children.count;
    auto numwidth = 1;
    numwidth += count > 9;
    numwidth += count > 99;
    numwidth += count > 999;
    numwidth += count > 9999;
    for(size_t i = 0; i < node->children.count; i++){
        auto child = get_node(ctx, node_children(node)[i]);
        assert(child->type == NODE_LIST_ITEM);
        msb_write_nchar(sb, ' ', indent);
        MSB_FORMAT(sb, int_fmt(i+1), ". ");
        FormatState state = {.lead = indent+numwidth+2, .col=indent+numwidth+2};
        for(size_t j = 0; j < child->children.count; j++){
            auto subchild = get_node(ctx, node_children(child)[j]);
            if(subchild->type == NODE_STRING){
                format_write_wrapped_string(sb, &state, subchild->header);
                }
            else {
                if(state.col != state.lead)
                    msb_write_char(sb, '\n');
                result = format_node(ctx, sb, subchild, indent+numwidth+2);
                if(result.errored) return result;
                // FIXME: this is sketch - we shouldn't have strings after a nested list.
                state = (FormatState){.lead=indent+numwidth+2, .col=indent+numwidth+2};
                }
            }
        if(state.col != state.lead)
            msb_write_char(sb, '\n');
        }
    return result;
    }
FORMATFUNC(md_node){
    Errorable(void) result = {};
    format_header(sb, node, indent);
    indent += FORMAT_INDENT;
    NODE_CHILDREN_FOR_EACH(it, node){
        auto child = get_node(ctx, *it);
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
        if(result.errored) return result;
        }
    // unsure about this.
    // msb_write_char(sb, '\n');
    return result;
    }
FORMATFUNC(text_node){
    Errorable(void) result = {};
    format_header(sb, node, indent);
    indent += FORMAT_INDENT;
    NODE_CHILDREN_FOR_EACH(it, node){
        auto child = get_node(ctx, *it);
        switch(child->type){
            case NODE_PARA:
                result = format_para_node(ctx, sb, child, indent);
                break;
            default:
                result = format_node(ctx, sb, child, indent);
                break;
            }
        if(result.errored) return result;
        }
    // unsure about this.
    msb_write_char(sb, '\n');
    return result;
    }
static inline
size_t
write_str_or_container(DndcContext* ctx, MStringBuilder* sb, Node* node){
    if(node->type == NODE_STRING){
        msb_write_str(sb, node->header.text, node->header.length);
        return node->header.length;
        }
    size_t writ = 0;
    for(size_t i = 0; i < node->children.count; i++){
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
    Errorable(void) result = {};
    format_header(sb, node, indent);
    indent += FORMAT_INDENT;
    ssize_t n_cells = 0;
    ssize_t widths[100] = {};
    // pre-pass to figure out widths
    NODE_CHILDREN_FOR_EACH(row_iter, node){
        Node* row = get_node(ctx, *row_iter);
        if(row->type != NODE_TABLE_ROW)
            continue;
        if(unlikely(row->children.count > arrlen(widths))){
            node_print_err(ctx, row, SV("Row of a table has more than 100 entries. This is not handled!"));
            Raise(FORMAT_ERROR);
            }
        if(row->children.count > n_cells)
            n_cells = row->children.count;
        for(size_t j = 0; j < row->children.count; j++){
            Node* cell = get_node(ctx, node_children(row)[j]);
            if(cell->type == NODE_STRING){
                if(cell->header.length > widths[j])
                    widths[j] = cell->header.length;
                }
            else {
                assert(cell->type == NODE_CONTAINER);
                size_t this_width = 0;
                NODE_CHILDREN_FOR_EACH(str_iter, cell){
                    Node* str = get_node(ctx, *str_iter);
                    assert(str->type == NODE_STRING);
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
                if(result.errored)
                    return result;
                continue;
                }
            for(size_t j = 0; j < row->children.count; j++){
                if(j != 0){
                    msb_write_literal(sb, " | ");
                    }
                else
                    msb_write_nchar(sb, ' ', indent);
                Node* cell = get_node(ctx, node_children(row)[j]);
                size_t writ = write_str_or_container(ctx, sb, cell);
                if(j != row->children.count-1 && widths[j] > writ){
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
                if(result.errored)
                    return result;
                continue;
                }
            for(ssize_t j = 0; j < row->children.count - 1; j++){
                if(j != 0){
                    msb_write_literal(sb, " | ");
                    }
                else
                    msb_write_nchar(sb, ' ', indent);
                Node* cell = get_node(ctx, node_children(row)[j]);
                size_t writ = write_str_or_container(ctx, sb, cell);
                if(j != row->children.count-1 && widths[j] > writ){
                    msb_write_nchar(sb, ' ', widths[j] - writ);
                    }
                }
            if(row->children.count > 1)
                msb_write_literal(sb, " | ");
            Node* last_cell = get_node(ctx, node_children(row)[row->children.count-1]);
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
    return result;
    }
FORMATFUNC(kv_node){
    Errorable(void) result = {};
    format_header(sb, node, indent);
    indent += FORMAT_INDENT;
    size_t key_width = 0;
    NODE_CHILDREN_FOR_EACH(it, node){
        auto child = get_node(ctx, *it);
        if(child->type != NODE_KEYVALUEPAIR)
            continue;
        auto key = get_node(ctx, node_children(child)[0])->header;
        if(key.length > key_width)
            key_width = key.length;
        }
    key_width += 2; // for the ": "
    NODE_CHILDREN_FOR_EACH(it, node){
        auto child = get_node(ctx, *it);
        if(child->type != NODE_KEYVALUEPAIR){
            result = format_node(ctx, sb, child, indent);
            if(result.errored) return result;
            continue;
            }
        msb_write_nchar(sb, ' ', indent);
        assert(child->children.count == 2);
        auto key = get_node(ctx, node_children(child)[0])->header;
        auto value_node = get_node(ctx, node_children(child)[1]);
        msb_write_str(sb, key.text, key.length);
        msb_write_literal(sb, ": ");
        auto padding = key_width - key.length - 2;
        msb_write_nchar(sb, ' ', padding);
        FormatState state = {.lead=key_width+indent, .col=key_width+indent};
        if(value_node->type == NODE_STRING){
            auto value = value_node->header;
            format_write_wrapped_string(sb, &state, value);
            }
        else {
            assert(value_node->type == NODE_CONTAINER);
            NODE_CHILDREN_FOR_EACH(c, value_node){
                auto n = get_node(ctx, *c);
                assert(n->type == NODE_STRING);
                format_write_wrapped_string(sb, &state, n->header);
                }
            }
        msb_write_char(sb, '\n');
        }
    // msb_write_char(sb, '\n');
    return result;
    }
FORMATFUNC(raw_node){
    Errorable(void) result = {};
    format_header(sb, node, indent);
    indent += FORMAT_INDENT;
    auto nspace = indent < 80? indent: 80;
    NODE_CHILDREN_FOR_EACH(it, node){
        auto child = get_node(ctx, *it);
        if(child->type != NODE_STRING){
            result = format_node(ctx, sb, child, indent);
            if(result.errored) return result;
            continue;
            }
        assert(child->type == NODE_STRING);
        if(child->header.length){
            msb_write_nchar(sb, ' ', nspace);
            msb_write_str(sb, child->header.text, child->header.length);
            }
        msb_write_char(sb, '\n');
        }
    // msb_write_char(sb, '\n');
    return result;
    }

#undef FORMATFUNC
#undef FORMATFUNCNAME

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
