#ifndef DNDC_FORMAT_C
#define DNDC_FORMAT_C
#include "dndc_types.h"
#include "dndc_funcs.h"
#include "str_util.h"

enum {FORMAT_WIDTH=80};
enum {FORMAT_INDENT=2};
const char* _Nonnull const EIGHTYSPACES = "                                                                                ";
typedef struct FormatState {
    // signed so it can go negative (simplifies things)
    // Check for <= 0, not == 0
    int lead;
    int col;
} FormatState;

#define FORMATFUNCNAME(nt) format_##nt
#define FORMATFUNC(nt) static void FORMATFUNCNAME(nt)(Nonnull(DndcContext*)ctx, Nonnull(MStringBuilder*)sb, Nonnull(Node*)node, int indent)

FORMATFUNC(regular_node);
FORMATFUNC(md_node);
FORMATFUNC(table_node);
FORMATFUNC(text_node);
FORMATFUNC(kv_node);
FORMATFUNC(raw_node);
FORMATFUNC(md_bullets);
FORMATFUNC(md_list);
FORMATFUNC(para_node);

static inline
void
format_node(Nonnull(DndcContext*)ctx, Nonnull(MStringBuilder*)sb, Nonnull(Node*)node, int indent){
    switch(node->type){
        case NODE_DIV:
        case NODE_TITLE:
        case NODE_HEADING:
        case NODE_NAV:
        case NODE_DATA:
        case NODE_QUOTE:
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
        case NODE_ROOT:
            unreachable();
            // return format_raw_node(ctx, sb, node, indent);
        case NODE_CONTAINER:
        case NODE_BULLET:
        case NODE_TABLE_ROW:
        case NODE_LIST:
        case NODE_BULLETS:
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
void
format_tree(Nonnull(DndcContext*)ctx, Nonnull(MStringBuilder*)sb){
    auto root = get_node(ctx, ctx->root_handle);
    for(size_t i = 0; i < root->children.count; i++){
        auto child = get_node(ctx, root->children.data[i]);
        // this is copy-paste from md_node, as the root is an
        // implicit, header-less md node.
        switch(child->type){
            case NODE_PARA:
                format_para_node(ctx, sb, child, 0);
                break;
            case NODE_BULLETS:
                format_md_bullets(ctx, sb, child, 0);
                break;
            case NODE_LIST:
                format_md_list(ctx, sb, child, 0);
                break;
            default:
                format_node(ctx, sb, child, 0);
                break;
            }
        }
    if(sb->cursor && sb->data[sb->cursor-1] != '\n')
        msb_write_char(sb, ctx->allocator, '\n');
    }
static inline
void
format_header(Nonnull(DndcContext*)ctx, Nonnull(MStringBuilder*)sb, Nonnull(Node*)node, int indent){
    msb_reserve(sb, ctx->allocator, indent);
    for(int i = 0; i < indent; i++){
        sb->data[sb->cursor++] = ' ';
        }
    if(node->header.length){
        msb_write_str(sb, ctx->allocator, node->header.text,node->header.length);
        }
    msb_write_char(sb, ctx->allocator, ':');
    msb_write_char(sb, ctx->allocator, ':');
    auto alias = &nodetype_to_node_aliases[node->type];
    // we fucked up if this is 0
    assert(alias->length);
    msb_write_str(sb, ctx->allocator, alias->text, alias->length);
    for(size_t i = 0; i < node->classes.count; i++){
        msb_write_literal(sb, ctx->allocator, " .");
        auto cls = &node->classes.data[i];
        msb_write_str(sb, ctx->allocator, cls->text, cls->length);
        }
    for(size_t i = 0; i < node->attributes.count; i++){
        msb_write_literal(sb, ctx->allocator, " @");
        auto attr = &node->attributes.data[i];
        msb_write_str(sb, ctx->allocator, attr->key.text, attr->key.length);
        if(attr->value.length){
            msb_write_char(sb, ctx->allocator, '(');
            msb_write_str(sb, ctx->allocator, attr->value.text, attr->value.length);
            msb_write_char(sb, ctx->allocator, ')');
            }
        }
    msb_write_char(sb, ctx->allocator, '\n');
    }

typedef struct FormatTokenized {
    StringView token;
    StringView rest;
} FormatTokenized;

FormatTokenized format_next_token(StringView sv){
    sv = lstripped_view(sv.text, sv.length);
    size_t i = 0;
    if(sv.length && sv.text[0] == '['){
        for(; i < sv.length; i++){
            switch(sv.text[i]){
                case ']':
                    goto endloop;
                    // return (FormatTokenized){
                        // .token.text = sv.text,
                        // .token.length = i+1,
                        // .rest.text = sv.text+i+1,
                        // .rest.length = sv.length - i-1,
                        // };
                default:
                    continue;
                }
            }
        endloop:;
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
format_write_wrapped_string(Nonnull(DndcContext*)ctx, Nonnull(MStringBuilder*)sb, Nonnull(FormatState*)state, StringView sv){
    FormatTokenized tokenized= {.rest=sv};
    if(state->col < state->lead){
        msb_write_str(sb, ctx->allocator, EIGHTYSPACES, state->lead);
        state->col = state->lead;
        }
    while(tokenized.rest.length){
        tokenized = format_next_token(tokenized.rest);
        if(!tokenized.token.length)
            break;
        if(state->col+tokenized.token.length > FORMAT_WIDTH){
            msb_write_char(sb, ctx->allocator, '\n');
            msb_write_str(sb, ctx->allocator, EIGHTYSPACES, state->lead);
            state->col = state->lead;
            }
        else if(state->col != state->lead){
            msb_write_char(sb, ctx->allocator, ' ');
            state->col++;
            }
        msb_write_str(sb, ctx->allocator, tokenized.token.text, tokenized.token.length);
        state->col += tokenized.token.length;
        }
    }

FORMATFUNC(regular_node){
    format_header(ctx, sb, node, indent);
    indent += FORMAT_INDENT;
    FormatState state = {.lead = indent};
    for(size_t i = 0; i < node->children.count; i++){
        auto child = get_node(ctx, node->children.data[i]);
        if(child->type == NODE_STRING){
            format_write_wrapped_string(ctx, sb, &state, child->header);
            }
        else {
            state.col = 0;
            format_node(ctx, sb, child, indent);
            }
        }
    // unsure about this.
    if(state.col)
        msb_write_char(sb, ctx->allocator, '\n');
    }
FORMATFUNC(para_node){
    FormatState state = {.lead = indent};
    for(size_t i = 0; i < node->children.count; i++){
        auto child = get_node(ctx, node->children.data[i]);
        assert(child->type == NODE_STRING);
        format_write_wrapped_string(ctx, sb, &state, child->header);
        }
    if(state.col)
        msb_write_char(sb, ctx->allocator, '\n');
    msb_write_char(sb, ctx->allocator, '\n');
    }
FORMATFUNC(md_bullets){
    for(size_t i = 0; i < node->children.count; i++){
        auto child = get_node(ctx, node->children.data[i]);
        assert(child->type == NODE_BULLET);
        msb_write_str(sb, ctx->allocator, EIGHTYSPACES, indent);
        msb_write_literal(sb, ctx->allocator, "* ");
        FormatState state = {.lead = indent+2, .col=indent+2};
        for(size_t j = 0; j < child->children.count; j++){
            auto subchild = get_node(ctx, child->children.data[j]);
            assert(subchild->type == NODE_STRING);
            format_write_wrapped_string(ctx, sb, &state, subchild->header);
            }
        msb_write_char(sb, ctx->allocator, '\n');
        }
    }
FORMATFUNC(md_list){
    auto count = node->children.count;
    auto numwidth = 1;
    numwidth += count > 9;
    numwidth += count > 99;
    numwidth += count > 999;
    numwidth += count > 9999;
    for(size_t i = 0; i < node->children.count; i++){
        auto child = get_node(ctx, node->children.data[i]);
        assert(child->type == NODE_LIST_ITEM);
        msb_write_str(sb, ctx->allocator, EIGHTYSPACES, indent);
        msb_sprintf(sb, ctx->allocator, "%*d. ", numwidth, (int)i+1);
        FormatState state = {.lead = indent+numwidth+2, .col=indent+numwidth+2};
        for(size_t j = 0; j < child->children.count; j++){
            auto subchild = get_node(ctx, child->children.data[j]);
            assert(subchild->type == NODE_STRING);
            format_write_wrapped_string(ctx, sb, &state, subchild->header);
            }
        msb_write_char(sb, ctx->allocator, '\n');
        }
    }
FORMATFUNC(md_node){
    format_header(ctx, sb, node, indent);
    indent += FORMAT_INDENT;
    for(size_t i = 0; i < node->children.count; i++){
        auto child = get_node(ctx, node->children.data[i]);
        switch(child->type){
            case NODE_PARA:
                format_para_node(ctx, sb, child, indent);
                break;
            case NODE_BULLETS:
                format_md_bullets(ctx, sb, child, indent);
                break;
            case NODE_LIST:
                format_md_list(ctx, sb, child, indent);
                break;
            default:
                format_node(ctx, sb, child, indent);
                break;
            }
        }
    // unsure about this.
    // msb_write_char(sb, ctx->allocator, '\n');
    }
FORMATFUNC(text_node){
    format_header(ctx, sb, node, indent);
    indent += FORMAT_INDENT;
    for(size_t i = 0; i < node->children.count; i++){
        auto child = get_node(ctx, node->children.data[i]);
        switch(child->type){
            case NODE_PARA:
                format_para_node(ctx, sb, child, indent);
                break;
            default:
                format_node(ctx, sb, child, indent);
                break;
            }
        }
    // unsure about this.
    msb_write_char(sb, ctx->allocator, '\n');
    }
FORMATFUNC(table_node){
    format_header(ctx, sb, node, indent);
    indent += FORMAT_INDENT;
    size_t n_cells = 0;
    size_t widths[100] = {};
    // pre-pass to figure out widths
    for(size_t i = 0; i < node->children.count; i++){
        auto row = get_node(ctx, node->children.data[i]);
        if(row->type != NODE_TABLE_ROW)
            continue;
        unhandled_error_condition(row->children.count > arrlen(widths));
        if(row->children.count > n_cells)
            n_cells = row->children.count;
        for(size_t j = 0; j < row->children.count; j++){
            auto cell = get_node(ctx, row->children.data[j]);
            assert(cell->type == NODE_STRING);
            if(cell->header.length > widths[j])
                widths[j] = cell->header.length;
            }
        }
    for(size_t i = 0; i < node->children.count; i++){
        auto row = get_node(ctx, node->children.data[i]);
        if(row->type != NODE_TABLE_ROW){
            format_node(ctx, sb, row, indent);
            continue;
            }
        // FIXME: remove these ifs, cleaner way to do this
        for(size_t j = 0; j < row->children.count; j++){
            if(j != 0){
                msb_write_literal(sb, ctx->allocator, " | ");
                }
            else
                msb_write_str(sb, ctx->allocator, EIGHTYSPACES, indent);
            auto cell = get_node(ctx, row->children.data[j]);
            if(j != row->children.count-1){
                msb_sprintf(sb, ctx->allocator, "%-*.*s", (int)widths[j], (int)cell->header.length, cell->header.text);
                }
            else
                msb_write_str(sb, ctx->allocator, cell->header.text, cell->header.length);
            }
        msb_write_char(sb, ctx->allocator, '\n');
        }
    }
FORMATFUNC(kv_node){
    format_header(ctx, sb, node, indent);
    indent += FORMAT_INDENT;
    for(size_t i = 0; i < node->children.count; i++){
        auto child = get_node(ctx, node->children.data[i]);
        if(child->type != NODE_KEYVALUEPAIR){
            format_node(ctx, sb, child, indent);
            continue;
            }
        msb_write_str(sb, ctx->allocator, EIGHTYSPACES, indent);
        assert(child->children.count == 2);
        auto key   = get_node(ctx, child->children.data[0])->header;
        auto value = get_node(ctx, child->children.data[1])->header;
        msb_write_str(sb, ctx->allocator, key.text, key.length);
        msb_write_literal(sb, ctx->allocator, ": ");
        msb_write_str(sb, ctx->allocator, value.text, value.length);
        msb_write_char(sb, ctx->allocator, '\n');
        }
    // msb_write_char(sb, ctx->allocator, '\n');
    }
FORMATFUNC(raw_node){
    format_header(ctx, sb, node, indent);
    indent += FORMAT_INDENT;
    auto nspace = Min(indent, 80);
    for(size_t i = 0; i < node->children.count; i++){
        auto child = get_node(ctx, node->children.data[i]);
        if(child->type != NODE_STRING){
            format_node(ctx, sb, child, indent);
            continue;
            }
        assert(child->type == NODE_STRING);
        if(child->header.length){
            msb_write_str(sb, ctx->allocator, EIGHTYSPACES, nspace);
            msb_write_str(sb, ctx->allocator, child->header.text, child->header.length);
            }
        msb_write_char(sb, ctx->allocator, '\n');
        }
    // msb_write_char(sb, ctx->allocator, '\n');
    }

#undef FORMATFUNC
#undef FORMATFUNCNAME
#endif
