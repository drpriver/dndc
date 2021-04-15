#ifndef DNDC_HTMLGEN_C
#define DNDC_HTMLGEN_C
#include "dndc_types.h"
#include "dndc_funcs.h"
#include "msb_extensions.h"
#include "path_util.h"
#include "str_util.h"
#include "parse_numbers.h"
#include "measure_time.h"
//
// For some reason I refer to generating html as rendering.
//

/* Rendering */


#define RENDERFUNCNAME(nt) render_##nt
#define RENDERFUNC(nt) static Errorable_f(void) RENDERFUNCNAME(nt)(Nonnull(DndcContext*)ctx, Nonnull(MStringBuilder*)sb, Nonnull(const Node*) node, int header_depth)

#define X(a, b) RENDERFUNC(a);
NODETYPES(X)
#undef X

typedef Errorable_f(void)(*_Nonnull renderfunc)(Nonnull(DndcContext*), Nonnull(MStringBuilder*), Nonnull(const Node*), int);

static
const
renderfunc renderfuncs[] = {
    #define X(a,b) [NODE_##a] = &RENDERFUNCNAME(a),
    NODETYPES(X)
    #undef X
};

static inline
force_inline
Errorable_f(void)
render_node(Nonnull(DndcContext*)ctx, Nonnull(MStringBuilder*) restrict sb, Nonnull(const Node*)node, int header_depth){
    bool hide = node_has_attribute(node, SV("hide"));
    if(hide) return (Errorable(void)){};
    return renderfuncs[node->type](ctx, sb, node, header_depth);
    }
static
Errorable_f(void)
render_tree(Nonnull(DndcContext*)ctx, Nonnull(MStringBuilder*)msb){
    Errorable(void) result = {};
    auto imgcount = ctx->img_nodes.count + ctx->imglinks_nodes.count;
    // estimate memory usage as 120 characters per node and 200 kb images.
    auto reserve_amount = ctx->nodes.count*120 + imgcount*200*1024;
    msb_reserve(msb, reserve_amount);
    msb_write_literal(msb,
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">\n"
        );
    if(!ctx->rendered_data.count){
        msb_write_literal(msb, "<script>\nconst data_blob = {};\n</script>\n");
        }
    else{
        msb_write_literal(msb, "<script>\nconst data_blob = {");
        for(size_t i = 0; i < ctx->rendered_data.count; i++){
            auto data = &ctx->rendered_data.data[i];
            msb_write_char(msb, '"');
            msb_write_str(msb, data->key.text, data->key.length);
            msb_write_literal(msb, "\": \"");
            msb_write_json_escaped_str(msb, data->value.text, data->value.length);
            msb_write_literal(msb, "\",\n");
            }
        msb_write_literal(msb, "};\n</script>\n");
        }
    if(!NodeHandle_eq(ctx->titlenode, INVALID_NODE_HANDLE)){
        auto n = get_node(ctx, ctx->titlenode);
        msb_sprintf(msb, "<title>%.*s</title>\n", (int)n->header.length, n->header.text);
        }
    else {
        auto filename = path_basename(path_strip_extension(LS_to_SV(ctx->outputfile)));
        msb_write_literal(msb, "<title>");
        msb_write_title(msb, filename.text, filename.length);
        msb_write_literal(msb, "</title>\n");
        }
    if(ctx->stylesheets_nodes.count){
        msb_write_literal(msb, "<style>\n");
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
                    msb_write_str(msb, child->header.text, child->header.length);
                    msb_write_char(msb, '\n');
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
                    auto style_e = ctx_load_source_file(ctx, child->header);
                    if(style_e.errored){
                        node_set_err(ctx, child, "Unable to load %.*s\n", (int)child->header.length, child->header.text);
                        Raise(style_e.errored);
                        }
                    LongString style = style_e.result;
                    msb_write_str(msb, style.text, style.length);
                    }
                }
            }
        msb_write_literal(msb, "</style>\n");
        }
    if(ctx->script_nodes.count){
        for(size_t i = 0; i < ctx->script_nodes.count; i++){
            msb_write_literal(msb, "<script>\n");
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
                        msb_write_str(msb, header.text, header.length);
                    msb_write_char(msb, '\n');
                    }
                msb_write_literal(msb, "</script>\n");
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
                auto script_e = ctx_load_source_file(ctx, child->header);
                if(script_e.errored){
                    node_set_err(ctx, child, "Unable to load %.*s\n", (int)child->header.length, child->header.text);
                    Raise(script_e.errored);
                    }
                LongString script = script_e.result;
                msb_write_str(msb, script.text, script.length);
                }
            msb_write_literal(msb, "</script>\n");
            }
        }
    msb_write_literal(msb, "</head>\n");
    msb_write_literal(msb, "<body>\n");
    auto root_node = get_node(ctx, ctx->root_handle);
    auto e = render_node(ctx, msb, root_node, 1);
    if(e.errored) return e;
    msb_write_literal(msb,
        "</body>\n"
        "</html>\n"
        );
    return result;
    }

static void build_nav_block_node(Nonnull(DndcContext*), NodeHandle, Nonnull(MStringBuilder*), int);
static void build_nav_block_children(Nonnull(DndcContext*), NodeHandle, Nonnull(MStringBuilder*), int);

static
void
build_nav_block(Nonnull(DndcContext*)ctx){
    MStringBuilder sb = {.allocator=ctx->allocator};
    msb_write_literal(&sb, "<nav>\n<ul>\n");
    build_nav_block_node(ctx, ctx->root_handle, &sb, 1);
    msb_write_literal(&sb, "</ul>\n</nav>");
    ctx->renderednav = msb_detach(&sb);
    }

static
void
build_nav_block_node(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(MStringBuilder*)sb, int depth){
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
        case NODE_CONTAINER:{
            auto id = node_get_id(node);
            if(id){
                msb_write_literal(sb, "<li><a href=\"#");
                msb_write_kebab(sb, id->text, id->length);
                msb_sprintf(sb, "\">%.*s</a>\n<ul>\n", (int)node->header.length, node->header.text);
                // kind of a hack
                auto cursor = sb->cursor;
                build_nav_block_children(ctx, handle, sb, depth+1);
                if(cursor != sb->cursor){
                    msb_write_literal(sb, "</ul>\n");
                    }
                else{
                    msb_erase(sb, sizeof("\n<ul>\n")-1);
                    }
                msb_write_literal(sb,"</li>\n");
                break;
                }
            // fall-through
            }
        case NODE_DATA: // this is a little sketchy
        case NODE_ROOT:
        case NODE_IMPORT:
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
        case NODE_HR:
            break;
        case NODE_PRE:
        case NODE_RAW:{
            auto id = node_get_id(node);
            if(id){
                msb_write_literal(sb, "<li><a href=\"#");
                msb_write_kebab(sb, id->text, id->length);
                msb_sprintf(sb, "\">%.*s</a>", (int)node->header.length, node->header.text);
                msb_write_literal(sb, "</li>\n");
                }
            }break;
        }
    }

static
void
build_nav_block_children(Nonnull(DndcContext*)ctx, NodeHandle handle, Nonnull(MStringBuilder*)sb, int depth){
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
write_tag_escaped_str(Nonnull(MStringBuilder*)sb, NullUnspec(const char*)text, size_t length){
    for(size_t i = 0; i < length; i++){
        char c = text[i];
        switch(c){
            case '&':
                msb_write_literal(sb, "&amp;");
                break;
            case '<':
                msb_write_literal(sb, "&lt;");
                break;
            case '>':
                msb_write_literal(sb, "&gt;");
                break;
            case '\r':
            case '\f':
                msb_write_char(sb, ' ');
                break;
            // Don't print control characters.
            case  0 ... 8:
            case 10 ... 11:
            case 14 ... 31:
                break;
            default:
                msb_write_char(sb, c);
                break;
            }
        }
    }


static inline
Errorable_f(void)
write_link_escaped_str(Nonnull(DndcContext*) ctx, Nonnull(MStringBuilder*)sb, Nonnull(const char*)text, size_t length, Nonnull(const Node*)node){
    Errorable(void) result = {};
    for(size_t i = 0; i < length; i++){
        char c = text[i];
        switch(c){
            case '[':{
                msb_write_literal(sb, "<a href=\"");
                const char* closing_brace = memchr(text+i, ']', length-i);
                if(!closing_brace){
                    node_set_err_offset(ctx, node, i, "Unterminated '[']");
                    Raise(PARSE_ERROR);
                    }
                size_t link_length = closing_brace - (text+i);
                {
                    MStringBuilder temp = {.allocator=ctx->temp_allocator};
                    msb_write_kebab(&temp, text+i+1, link_length-1);
                    auto temp_str = msb_borrow(&temp);
                    auto value = find_link_target(ctx, temp_str);
                    if(!value){
                        if(ctx->flags & DNDC_ALLOW_BAD_LINKS){
                            node_print_warning(ctx, node, "Unable to resolve link '%.*s'", (int)temp_str.length, temp_str.text);
                            msb_write_str(sb, temp_str.text, temp_str.length);
                            }
                        else {
                            node_set_err(ctx, node, "Unable to resolve link '%.*s'", (int)temp_str.length, temp_str.text);
                            msb_destroy(&temp);
                            Raise(PARSE_ERROR);
                            }
                        }
                    else {
                        StringView* val = value;
                        msb_write_str(sb, val->text, val->length);
                        }
                    msb_destroy(&temp);
                }
                msb_write_literal(sb, "\">");
                msb_write_str(sb, text+i+1, link_length-1);
                msb_write_literal(sb, "</a>");
                i += link_length;
                continue;
                }break;
            case '-':{
                if(i < length - 1){
                    char peek1 = text[i+1];
                    if(peek1 == '-'){
                        if(i < length - 2){
                            char peek2 = text[i+2];
                            if(peek2 == '-'){
                                msb_write_literal(sb, "&mdash;");
                                i += 2;
                                continue;
                                }
                            }
                            msb_write_literal(sb, "&ndash;");
                            i += 1;
                            continue;
                        }
                    }
                msb_write_char(sb, c);
                }break;
            case '&':{
                msb_write_literal(sb, "&amp;");
                }break;
            case '<':{
                // we allow inline <b>, <s>, <i>, </b>, </s>, </i>
                if(i < length - 1){
                    char peek1 = text[i+1];
                    switch(peek1){
                        case 'b':
                        case 's':
                        case 'i':
                        case '/':
                            break;
                        default:
                            msb_write_literal(sb, "&lt;");
                            continue;
                        }
                    if(i < length - 2){
                        char peek2 = text[i+2];
                        if(peek1 != '/'){
                            if(peek2 == '>'){
                                msb_write_char(sb, c);
                                msb_write_char(sb, peek1);
                                msb_write_char(sb, peek2);
                                i += 2;
                                continue;
                                }
                            msb_write_literal(sb, "&lt;");
                            continue;
                            }
                        switch(peek2){
                            case 'b':
                            case 's':
                            case 'i':
                                break;
                            default:
                                msb_write_literal(sb, "&lt;");
                                continue;
                            }
                        if(i < length - 3){
                            char peek3 = text[i+3];
                            if(peek3 == '>'){
                                msb_write_char(sb, c);
                                msb_write_char(sb, peek1);
                                msb_write_char(sb, peek2);
                                msb_write_char(sb, peek3);
                                i += 3;
                                continue;
                                }
                            }
                        }
                    }
                msb_write_literal(sb, "&lt;");
                }break;
            case '>':{
                msb_write_literal(sb, "&gt;");
                }break;
            case '\r':
            case '\f':{
                msb_write_char(sb, ' ');
                }break;
            // Don't print control characters.
            case  0 ... 8:
            case 10 ... 11:
            case 14 ... 31:{
                }break;
            default:{
                msb_write_char(sb, c);
                }break;
            }
        }
    return result;
    }

static inline
Errorable_f(void)
write_header(Nonnull(DndcContext*)ctx, Nonnull(MStringBuilder*)sb, Nonnull(const Node*)node, int header_level){
    auto id = node_get_id(node);
    if(!id){
        msb_sprintf(sb, "<h%d>", header_level);
        }
    else{
        msb_sprintf(sb, "<h%d id=\"", header_level);
        msb_write_kebab(sb, id->text, id->length);
        msb_write_literal(sb, "\">");
        }
    auto e = write_link_escaped_str(ctx, sb, node->header.text, node->header.length, node);
    if(e.errored) return e;
    msb_sprintf(sb, "</h%d>", header_level);
    return (Errorable(void)){};
    }

static inline
void
write_classes(Nonnull(MStringBuilder*)sb, Nonnull(const Node*)node){
    auto count = node->classes.count;
    if(!count) return;
    auto classes = node->classes.data;
    msb_write_literal(sb, " class=\"");
    for(size_t i = 0; i < count; i++){
        if(i != 0){
            msb_write_char(sb, ' ');
            }
        auto c = &classes[i];
        msb_write_str(sb, c->text, c->length);
        }
    msb_write_char(sb, '"');
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
    (void)header_depth;
    if(unlikely(node->classes.count))
        node_print_warning(ctx, node, "Ignoring classes on string node");
    if(unlikely(node->children.count))
        node_print_warning(ctx, node, "Ignoring children of string node");
    auto e = write_link_escaped_str(ctx, sb, node->header.text, node->header.length, node);
    if(e.errored) return e;
    msb_write_char(sb, '\n');
    return (Errorable(void)){};
    }

RENDERFUNC(TEXT){
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node, header_depth);
        if(e.errored) return e;
        msb_write_char(sb, '\n');
        }
    auto children = &node->children;
    auto count = children->count;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children->data[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(DIV){
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node, header_depth);
        if(e.errored) return e;
        msb_write_char(sb, '\n');
        }
    auto children = &node->children;
    auto count = children->count;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children->data[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "</div>\n");
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
    msb_write_str(sb, ctx->renderednav.text, ctx->renderednav.length);
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
    msb_write_literal(sb, "<p>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child_handle = children[i];
        auto child = get_node(ctx, child_handle);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "</p>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(TITLE){
    auto e = write_header(ctx, sb, node, header_depth);
    if(e.errored) return e;
    msb_write_char(sb, '\n');
    if(node->children.count){
        node_print_warning(ctx, node, "Ignoring children of title");
        }
    if(node->classes.count){
        node_print_warning(ctx, node, "UNIMPLEMENTED: classes on the title");
        }
    return (Errorable(void)){};
    }
RENDERFUNC(HEADING){
    auto e = write_header(ctx, sb, node, header_depth+1);
    if(e.errored) return e;
    msb_write_char(sb, '\n');
    if(node->children.count){
        node_print_warning(ctx, node, "Ignoring children of heading");
        }
    if(node->classes.count){
        node_print_warning(ctx, node, "UNIMPLEMENTED: classes on the heading");
        }
    return (Errorable(void)){};
    }
RENDERFUNC(HR){
    (void)header_depth;
    if(node->header.length){
        node_print_warning(ctx, node, "Ignoring header of hr");
        }
    if(node->children.count){
        node_print_warning(ctx, node, "Ignoring children of hr");
        }
    msb_write_char(sb, '\n');
    msb_write_literal(sb, "<hr>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(TABLE){
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "<table>\n<thead>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    if(count){
        auto child = get_node(ctx, children[0]);
        if(child->type != NODE_TABLE_ROW){
            node_set_err(ctx, child, "children of a table ought to be table rows...");
            return (Errorable(void)){.errored=GENERIC_ERROR};
            }
        // inline rendering table row here so we can do heads
        msb_write_literal(sb, "<tr>\n");
        auto child_count = child->children.count;
        auto child_children = child->children.data;
        for(size_t i = 0; i < child_count; i++){
            auto child_child = get_node(ctx, child_children[i]);
            msb_write_literal(sb, "<th>");
            auto e = render_node(ctx, sb, child_child, header_depth);
            if(e.errored) return e;
            msb_write_literal(sb, "</th>\n");
            }
        msb_write_literal(sb, "</tr>\n");
        }
    msb_write_literal(sb, "</thead>\n<tbody>\n");
    for(size_t i = 1; i < count; i++){
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "</tbody></table>\n</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(TABLE_ROW){
    // TODO: odd even class?
    msb_write_literal(sb, "<tr>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        msb_write_literal(sb, "<td>");
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        msb_write_literal(sb, "</td>\n");
        }
    msb_write_literal(sb, "</tr>\n");
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
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node, header_depth);
        if(e.errored) return e;
        msb_write_char(sb, '\n');
        }
    if(!node->children.count){
        node_set_err(ctx, node, "Image node missing any children (first should be a string that is path to the image");
        Raise(PARSE_ERROR);
        }
    auto children = &node->children;
    // CLEANUP: lots of copy and paste here.
    if(ctx->flags & DNDC_USE_DND_URL_SCHEME){
        auto first_child = get_node(ctx, children->data[0]);
        if(first_child->type != NODE_STRING){
            node_set_err(ctx, first_child, "First child of an image node should be a string that is path to the image.");
            Raise(PARSE_ERROR);
            }
        auto imgpath_node = get_node(ctx, node->children.data[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, "First should be a string and be the path to the image.");
            Raise(PARSE_ERROR);
            }
        auto header = imgpath_node->header;
        msb_write_literal(sb, "<img src=\"dnd:");
        if((not path_is_abspath(header)) and ctx->base_directory.length){
            msb_write_str_with_backslashes_as_forward_slashes(sb, ctx->base_directory.text, ctx->base_directory.length);
            msb_write_char(sb, '/');
            msb_write_str_with_backslashes_as_forward_slashes(sb, header.text, header.length);
            }
        else {
            msb_write_str_with_backslashes_as_forward_slashes(sb, header.text, header.length);
            }
        msb_write_literal(sb, "\">");
        }
    else if(ctx->flags & DNDC_DONT_INLINE_IMAGES){
        auto first_child = get_node(ctx, children->data[0]);
        if(first_child->type != NODE_STRING){
            node_set_err(ctx, first_child, "First child of an image node should be a string that is path to the image.");
            Raise(PARSE_ERROR);
            }
        auto imgpath_node = get_node(ctx, node->children.data[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, "First should be a string and be the path to the image.");
            Raise(PARSE_ERROR);
            }
        auto header = imgpath_node->header;
        msb_write_literal(sb, "<img src=\"");
        msb_write_str_with_backslashes_as_forward_slashes(sb, header.text, header.length);
        msb_write_literal(sb, "\">");
        }
    else{
        auto first_child = get_node(ctx, children->data[0]);
        if(first_child->type != NODE_STRING){
            node_set_err(ctx, first_child, "First child of an image node should be a string that is path to the image.");
            Raise(PARSE_ERROR);
            }
        auto imgpath_node = get_node(ctx, node->children.data[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, "First should be a string and be the path to the image.");
            Raise(PARSE_ERROR);
            }
        auto header = imgpath_node->header;
        auto processed_e = ctx_load_processed_binary_file(ctx, header);
        if(processed_e.errored){
            node_set_err(ctx, imgpath_node, "Unable to read '%.*s'", (int)header.length, header.text);
            Raise(processed_e.errored);
            }
        else {
            msb_write_literal(sb, "<img src=\"data:image/png;base64,");
            auto b64 = processed_e.result;
            msb_write_str(sb, b64.text, b64.length);
            }
        msb_write_literal(sb, "\">");
    }
    auto count = children->count;
    for(size_t i = 1; i < count; i++){
        auto child = get_node(ctx, children->data[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "</div>\n");
    return result;
    }
RENDERFUNC(QUOTE){
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb,  node, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "<blockquote>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "</blockquote>\n</div>\n");
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
        msb_write_str(sb, child->header.text, child->header.length);
        msb_write_char(sb, '\n');
        }
    (void)header_depth;
    return (Errorable(void)){};
    }
RENDERFUNC(PRE){
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "<pre>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        if(unlikely(child->type != NODE_STRING))
            node_print_warning(ctx, child, "pre node with a non-string child");
        write_tag_escaped_str(sb, child->header.text, child->header.length);
        msb_write_char(sb, '\n');
        }
    msb_write_literal(sb, "</pre>\n</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(BULLETS){
    if(unlikely(node->header.length))
        node_print_warning(ctx, node, "ignoring header on bullet list");
    if(unlikely(node->classes.count))
        node_print_warning(ctx, node, "Ignoring classes on bullet list");
    msb_write_literal(sb, "<ul>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "</ul>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(LIST){
    if(unlikely(node->header.length))
        node_print_warning(ctx, node, "ignoring header on list");
    if(unlikely(node->classes.count))
        node_print_warning(ctx, node, "Ignoring classes on list");
    msb_write_literal(sb, "<ol>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "</ol>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(LIST_ITEM){
    msb_write_literal(sb, "<li>\n");
    if(unlikely(node->header.length))
        node_print_warning(ctx, node, "ignoring header on list item");
    if(unlikely(node->classes.count))
        node_print_warning(ctx, node, "Ignoring classes on list item");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        if(i != 0)
            msb_write_char(sb, ' ');
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "</li>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(KEYVALUE){
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "<table><tbody>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "</tbody></table>\n</div>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(KEYVALUEPAIR){
    // TODO: maybe this should be lowered into a table row node?
    // TODO: odd even class?
    msb_write_literal(sb, "<tr>\n");
    auto count = node->children.count;
    auto children = node->children.data;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children[i]);
        msb_write_literal(sb, "<td>");
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        msb_write_literal(sb, "</td>\n");
        }
    msb_write_literal(sb, "</tr>\n");
    return (Errorable(void)){};
    }
RENDERFUNC(IMGLINKS){
    Errorable(void) result = {};
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node, header_depth);
        if(e.errored) return e;
        }
    if(node->children.count < 4){
        node_set_err(ctx, node, "Too few children of an imglinks node (expected path to the image, width, height, viewBox in that order)");
        Raise(PARSE_ERROR);
        }

    LongString imgdatab64 = {};
    if(not (ctx->flags & (DNDC_DONT_INLINE_IMAGES | DNDC_USE_DND_URL_SCHEME))){
        auto imgpath_node = get_node(ctx, node->children.data[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, "First should be a string and be the path to the image");
            Raise(PARSE_ERROR);
            }
        auto header = imgpath_node->header;
        auto processed_e = ctx_load_processed_binary_file(ctx, header);
        if(processed_e.errored){
            node_set_err(ctx, imgpath_node, "Unable to read '%.*s'", (int)header.length, header.text);
            Raise(processed_e.errored);
            }
        imgdatab64 = processed_e.result;
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
        width = e.result;
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
        height = e.result;
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
            viewbox[which++] = e.result;
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
    msb_sprintf(sb, "<svg width=\"%d\" height=\"%d\" viewbox=\"%d %d %d %d\" style=\"background-size: 100%% 100%%; ", width, height, viewbox[0], viewbox[1], viewbox[2], viewbox[3]);
    if(ctx->flags & DNDC_USE_DND_URL_SCHEME){
        msb_write_literal(sb, "background-image: url('dnd:");
        auto imgpath_node = get_node(ctx, node->children.data[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, "First should be a string and be the path to the image");
            Raise(PARSE_ERROR);
            }
        auto header = imgpath_node->header;
        if((not path_is_abspath(header)) and ctx->base_directory.length){
            msb_write_str_with_backslashes_as_forward_slashes(sb, ctx->base_directory.text, ctx->base_directory.length);
            msb_write_char(sb, '/');
            msb_write_str_with_backslashes_as_forward_slashes(sb, header.text, header.length);
            }
        else {
            msb_write_str_with_backslashes_as_forward_slashes(sb, header.text, header.length);
            }
        msb_write_literal(sb, "');\">\n");
        }
    else if(ctx->flags & DNDC_DONT_INLINE_IMAGES){
        msb_write_literal(sb, "background-image: url('");
        auto imgpath_node = get_node(ctx, node->children.data[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, "First should be a string and be the path to the image");
            Raise(PARSE_ERROR);
            }
        auto header = imgpath_node->header;
        msb_write_str_with_backslashes_as_forward_slashes(sb, header.text, header.length);
        msb_write_literal(sb, "');\">\n");
        }
    else {
        msb_write_literal(sb, "background-image: url('data:image/png;base64,");
        auto before = get_t();
        assert(imgdatab64.length);
        msb_write_str(sb, imgdatab64.text, imgdatab64.length);
        auto after = get_t();
        report_stat(ctx, "Base64ing an imglinks took %.3fms", (after-before)/1000.);
        msb_write_literal(sb, "');\">\n");
        }
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
        auto x = x_err.result;
        auto y_err = parse_int(fourth.text, fourth.length);
        if(y_err.errored){
            node_set_err(ctx, child, "Unable to parse an int from '%.*s'", (int)third.length, third.text);
            Raise(y_err.errored);
            }
        auto y = y_err.result;
        msb_sprintf(sb,
                "<a href=\"%.*s\"><text style=\"text-anchor:middle;\" transform=\"translate(%d,%d)\">\n"
                "%.*s\n"
                "</text></a>\n", (int)second.length, second.text, x, y, (int)first.length, first.text);
        }
    msb_write_literal(sb, "</svg>\n");
    msb_write_literal(sb, "</div>\n");
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
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        auto e = write_header(ctx, sb, node, header_depth);
        if(e.errored) return e;
        msb_write_char(sb, '\n');
        }
    auto children = &node->children;
    auto count = children->count;
    for(size_t i = 0; i < count; i++){
        auto child = get_node(ctx, children->data[i]);
        auto e = render_node(ctx, sb, child, header_depth);
        if(e.errored) return e;
        }
    msb_write_literal(sb, "</div>\n");
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
#undef RENDERFUNC
#undef RENDERFUNCNAME

#endif
