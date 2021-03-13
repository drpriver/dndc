#ifndef DNDC_PARSER_C
#define DNDC_PARSER_C
#include "dndc_funcs.h"
#include "dndc_types.h"
#include "str_util.h"
static
Errorable_f(void)
parse_post_colon(Nonnull(DndcContext*)ctx, StringView postcolon, NodeHandle node_handle);
static
void
analyze_line(Nonnull(DndcContext*));
static
void
advance_row(Nonnull(DndcContext*));

#define PARSEFUNC(name) static Errorable_f(void) name(Nonnull(DndcContext*)ctx, NodeHandle parent_handle, int indentation)
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

static inline
void
analyze_line(Nonnull(DndcContext*)ctx){
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
                if(!(ctx->flags & DNDC_SUPPRESS_WARNINGS))
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
advance_row(Nonnull(DndcContext*)ctx){
    if(!unlikely(ctx->lineend[0]))
        ctx->cursor = ctx->lineend;
    else
        ctx->cursor = ctx->lineend+1;
    ctx->lineno++;
    }


static inline
void
force_inline
init_node(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(const char*) src_char, NodeType type){
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
init_string_node(Nonnull(DndcContext*)ctx, NodeHandle handle, StringView sv){
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
dndc_parse(Nonnull(DndcContext*)ctx, NodeHandle root_handle, StringView filename, Nonnull(const char*)text){
    Errorable(void) result = {};
    ctx->cursor = text;
    ctx->linestart = NULL;
    ctx->doublecolon = NULL;
    ctx->lineend = NULL;
    ctx->nspaces = 0;
    ctx->lineno = 0;
    ctx->filename = filename;
    auto e = parse_node(ctx, root_handle, -1);
    if(e.errored) return e;
    return result;
    }

static
Errorable_f(void)
parse_double_colon(Nonnull(DndcContext*)ctx, NodeHandle parent_handle){
    Errorable(void) result = {};
    // parse the node header
    const char* starttext = ctx->doublecolon + 2;
    size_t length = ctx->lineend - starttext;
    StringView postcolon = stripped_view(starttext, length);
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
        node->header = stripped_view(header, ctx->doublecolon - header);
        if(node_has_attribute(node, SV("comment"))){
            node->type = NODE_COMMENT;
            }
    }
    auto new_indent = ctx->nspaces;
    advance_row(ctx);
    auto e = parse_node(ctx, new_node_handle, new_indent);
    if(e.errored) return e;
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
parse_post_colon(Nonnull(DndcContext*)ctx, StringView postcolon, NodeHandle node_handle){
    Errorable(void) result = {};
    auto node = get_node(ctx, node_handle);
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
        parse_set_err(ctx, postcolon.text, "no node type found after '::'");
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
    parse_set_err(ctx, postcolon.text, "Unrecognized node name: '%.*s'", (int)boundary, postcolon.text);
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
                    parse_set_err(ctx, aftertype.text, "Empty class name after a '.'");
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
                    parse_set_err(ctx, aftertype.text, "Empty attribute name after a '@'");
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
                                parse_set_err(ctx, aftertype.text, "End of line when expecting a closing ')'");
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
                parse_set_err(ctx, aftertype.text, "illegal character when parsing type, classes and attributes: '%c'", aftertype.text[0]);
                Raise(PARSE_ERROR);
            }
        }
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
        case NODE_COMMENT:
            return parse_raw_node(ctx, parent_handle, indentation);
        case NODE_TABLE:
            return parse_table_node(ctx, parent_handle, indentation);
        case NODE_KEYVALUE:
            return parse_keyvalue_node(ctx, parent_handle, indentation);
        case NODE_MD:
            return parse_md_node(ctx, parent_handle, indentation);
        case NODE_IMGLINKS:
        case NODE_DATA:
        case NODE_NAV:
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
            break; // do regular string parsing
        case NODE_STYLESHEETS:
            // kind of gross
            if(node_has_attribute(parent, SV("inline"))){
                return parse_raw_node(ctx, parent_handle, indentation);
                }
            break; // do regular string parsing
        case NODE_TEXT:
            return parse_text_node(ctx, parent_handle, indentation);
        case NODE_LIST_ITEM:
        case NODE_TABLE_ROW:
        case NODE_PARA:
        case NODE_STRING:
        case NODE_KEYVALUEPAIR:
        case NODE_INVALID:
        case NODE_BULLETS:
        case NODE_LIST:
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
        StringView content = stripped_view(ctx->linestart + ctx->nspaces,
            (ctx->lineend - ctx->linestart)-ctx->nspaces);
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
                    parse_set_err(ctx, firstchar, "Non numeric found when parsing list: '%c'", *firstchar);
                    Raise(PARSE_ERROR);
                }
            }
        after:;
        auto li_handle = alloc_handle(ctx);
        init_node(ctx, li_handle, ctx->linestart+ctx->nspaces, NODE_LIST_ITEM);
        append_child(ctx, parent_handle, li_handle);
        StringView text = stripped_view(firstchar, ctx->lineend - firstchar);
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
            parse_set_err(ctx, ctx->doublecolon, "This node type cannot contain subnodes, only strings");
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
        StringView content = stripped_view(ctx->linestart + ctx->nspaces, (ctx->lineend - ctx->linestart)-ctx->nspaces);
        auto new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
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
        auto content = rstripped_view(text, length);
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
    NodeHandle last_cell_handle = INVALID_NODE_HANDLE;
    bool converted = false;
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
        const char* cursor = ctx->linestart+ctx->nspaces;
        const char* pipe = memchr(cursor, '|', ctx->lineend - cursor);
        if(!pipe){
            if(!NodeHandle_eq(last_cell_handle, INVALID_NODE_HANDLE)){
                StringView content = stripped_view(cursor, ctx->lineend-cursor);
                if(content.length){
                    if(!converted){
                        convert_node_to_container_containing_clone_of_former_self(ctx, last_cell_handle);
                        converted = true;
                        }
                    auto str_handle = alloc_handle(ctx);
                    init_string_node(ctx, str_handle, content);
                    append_child(ctx, last_cell_handle, str_handle);
                    }
                }
            advance_row(ctx);
            continue;
            }
        auto new_node_handle = alloc_handle(ctx);
        init_node(ctx, new_node_handle, ctx->linestart+ctx->nspaces, NODE_TABLE_ROW);
        append_child(ctx, parent_handle, new_node_handle);
        last_cell_handle = INVALID_NODE_HANDLE;
        converted = false;
        while(pipe){
            auto cell_index = alloc_handle(ctx);
            size_t length = pipe - cursor;
            StringView content = stripped_view(cursor,length);
            init_string_node(ctx, cell_index, content);
            append_child(ctx, new_node_handle, cell_index);
            cursor = pipe+1;
            pipe = memchr(cursor, '|', ctx->lineend - cursor);
            }
        auto cell_index = alloc_handle(ctx);
        last_cell_handle = cell_index;
        StringView content = stripped_view(cursor, ctx->lineend-cursor);
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
            parse_set_err(ctx, cursor, "Expected a colon for key value pairs");
            Raise(PARSE_ERROR);
            }
        const char* pre_text = ctx->linestart+ctx->nspaces;

        StringView pre = stripped_view(pre_text,colon - pre_text);
        StringView post = stripped_view(colon+1, (ctx->lineend-colon)-1);
        auto key_idx = alloc_handle(ctx);
        init_string_node(ctx, key_idx, pre);
        auto val_idx = alloc_handle(ctx);
        init_string_node(ctx, val_idx, post);
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
            parse_set_err(ctx, firstchar, "Bullets must begin with one of *-+, got '%c'", first);
            Raise(PARSE_ERROR);
            }
        firstchar++;
        StringView bullet_text = stripped_view(firstchar, ctx->lineend - firstchar);
        auto bullet_node_handle = alloc_handle(ctx);
        init_node(ctx, bullet_node_handle, ctx->linestart+ctx->nspaces, NODE_LIST_ITEM);
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
        assert(parent->type == NODE_LIST_ITEM);
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
            parse_set_err(ctx, ctx->doublecolon,"This node type cannot contain subnodes, only strings");
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
        StringView content = stripped_view(ctx->linestart + ctx->nspaces, (ctx->lineend - ctx->linestart)-ctx->nspaces);
        auto new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
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
    NodeHandle para_handle = INVALID_NODE_HANDLE;
    Errorable(void) result = {};
    for(;ctx->cursor[0];){
        analyze_line(ctx);
        // blank line
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
        StringView content = stripped_view(ctx->linestart+ctx->nspaces, (ctx->lineend - ctx->linestart)-ctx->nspaces);
        auto new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
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
    struct {
        NodeHandle list;
        NodeHandle item;
        int indentation;
        enum MDSTATE state;
        } stack[8];
    int si = -1;
    NodeHandle para_handle = INVALID_NODE_HANDLE;
    int normal_indent = -1;
    Errorable(void) result = {};
    for(;ctx->cursor[0];){
        analyze_line(ctx);
        // skip_blanks
        if(ctx->linestart+ctx->nspaces == ctx->lineend){
            state = NONE;
            advance_row(ctx);
            continue;
            }
        if(normal_indent < 0){
            normal_indent = ctx->nspaces;
            }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            state = NONE;
            auto e = parse_double_colon(ctx, parent_handle);
            if(e.errored) return e;
            continue;
            }
        enum MDSTATE newstate = NONE;
        const char* firstchar = ctx->linestart + ctx->nspaces;
        int prefix_length = 0;
        switch(*firstchar){
            case '+':
            case '-':
            case '*':
                if(firstchar[1] == ' '){
                    prefix_length = 1;
                    newstate = BULLET;
                    }
                else
                    newstate = PARA;
                goto after;
            case '0' ... '9':{
                prefix_length = 1;
                for(const char* c = firstchar+1;;c++){
                    switch(*c){
                        case '0' ... '9':
                            prefix_length++;
                            continue;
                        case '.':
                            prefix_length++;
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
        if(newstate == BULLET or newstate == LIST){
            if(si == -1){
                si = 0;
                auto s = &stack[si];
                s->list = alloc_handle(ctx);
                s->item = INVALID_NODE_HANDLE;
                s->indentation = ctx->nspaces;
                s->state = newstate;
                init_node(ctx, s->list, ctx->linestart+ctx->nspaces, newstate==BULLET?NODE_BULLETS:NODE_LIST);
                append_child(ctx, parent_handle, s->list);
                }
            else {
                // new level of list
                if(ctx->nspaces > stack[si].indentation){
                    si++;
                    if(si == arrlen(stack)){
                        parse_set_err(ctx, ctx->linestart+ctx->nspaces, "Only up to 8 levels of nested lists are supported.");
                        Raise(PARSE_ERROR);
                        }
                    auto s = &stack[si];
                    s->list = alloc_handle(ctx);
                    s->item = INVALID_NODE_HANDLE;
                    s->indentation = ctx->nspaces;
                    s->state = newstate;
                    init_node(ctx, s->list, ctx->linestart+ctx->nspaces, newstate==BULLET?NODE_BULLETS:NODE_LIST);
                    assert(si > 0);
                    append_child(ctx, stack[si-1].item, s->list);
                    }
                // neighbors
                else if(ctx->nspaces == stack[si].indentation){
                    auto s = &stack[si];
                    if(s->state != newstate){
                        // neighbor of different type
                        NodeHandle prev = si>0? stack[si-1].item : parent_handle;
                        s->list = alloc_handle(ctx);
                        s->item = INVALID_NODE_HANDLE;
                        s->indentation = ctx->nspaces;
                        s->state = newstate;
                        init_node(ctx, s->list, ctx->linestart+ctx->nspaces, newstate==BULLET?NODE_BULLETS:NODE_LIST);
                        append_child(ctx, prev, s->list);
                        }
                    else {
                        // Neighbor of same type, do nothing
                        }
                    }
                // go back up
                else {
                    for(;;){
                        si--;
                        assert(si >= 0);
                        auto indent = stack[si].indentation;
                        if(indent > ctx->nspaces)
                            continue;
                        if(indent == ctx->nspaces)
                            break;
                        if(indent < ctx->nspaces){
                            parse_set_err(ctx, ctx->linestart+ctx->nspaces, "Ambiguous dedent inside a list");
                            Raise(PARSE_ERROR);
                            }
                        }
                    auto s = &stack[si];
                    if(s->state != newstate){
                        s->list = alloc_handle(ctx);
                        s->item = INVALID_NODE_HANDLE;
                        s->indentation = ctx->nspaces;
                        s->state = newstate;
                        init_node(ctx, s->list, ctx->linestart+ctx->nspaces, newstate==BULLET?NODE_BULLETS:NODE_LIST);
                        if(si)
                            append_child(ctx, stack[si-1].item, s->list);
                        else
                            append_child(ctx, parent_handle, s->list);
                        }
                    }
                }
            auto s = &stack[si];
            s->item = alloc_handle(ctx);
            init_node(ctx, s->item, ctx->linestart+ctx->nspaces, NODE_LIST_ITEM);
            append_child(ctx, s->list, s->item);
            StringView content = stripped_view(ctx->linestart + ctx->nspaces+prefix_length, (ctx->lineend - ctx->linestart)-ctx->nspaces-prefix_length);
            NodeHandle new_node_handle = alloc_handle(ctx);
            init_string_node(ctx, new_node_handle, content);
            append_child(ctx, s->item, new_node_handle);
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
            StringView content = stripped_view( ctx->linestart + ctx->nspaces, (ctx->lineend - ctx->linestart)-ctx->nspaces);
            auto new_node_handle = alloc_handle(ctx);
            init_string_node(ctx, new_node_handle, content);
            append_child(ctx, para_handle, new_node_handle);
            advance_row(ctx);
            state = newstate;
            continue;
            }
        if(ctx->nspaces <= stack[si].indentation){
            parse_set_err(ctx, ctx->linestart+ctx->nspaces, "Ambiguous dedent inside a list");
            Raise(PARSE_ERROR);
            }
        // don't change state for these
        StringView content = stripped_view(ctx->linestart + ctx->nspaces, (ctx->lineend - ctx->linestart)-ctx->nspaces);
        auto new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
        append_child(ctx, stack[si].item, new_node_handle);
        advance_row(ctx);
        continue;
        }
    return result;
    }
#endif
