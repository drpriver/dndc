#ifndef DNDC_HTMLGEN_C
#define DNDC_HTMLGEN_C
#include "dndc_types.h"
#include "dndc_funcs.h"
#include "msb_extensions.h"
#include "path_util.h"
#include "str_util.h"
#include "parse_numbers.h"
#include "measure_time.h"
#include "msb_format.h"
#include "error_handling.h"

#ifndef NO_SIMD
#ifdef __x86_64__
#include <immintrin.h>
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#endif



#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

//
// For some reason I refer to generating html as rendering.
//

/* Rendering */

#define RENDERFUNCNAME(nt) render_##nt
#define RENDERFUNC(nt) static warn_unused int RENDERFUNCNAME(nt)(DndcContext* ctx, MStringBuilder* sb, NodeHandle handle, int header_depth)

#define X(a, b) RENDERFUNC(a);
NODETYPES(X)
#undef X

typedef int(renderfunc)(DndcContext*, MStringBuilder*, NodeHandle, int);

static
renderfunc*_Nonnull const RENDERFUNCS[] = {
    #define X(a,b) [NODE_##a] = &RENDERFUNCNAME(a),
    NODETYPES(X)
    #undef X
};

static inline
force_inline
warn_unused
int
render_node(DndcContext* ctx, MStringBuilder* restrict sb, NodeHandle handle, int header_depth){
    Node* node = get_node(ctx, handle);
    bool hide = !!node->flags & NODEFLAG_HIDE;
    if(hide) return 0;
#if 0
    switch(node->type){
#define X(a, b) case NODE_##a: return RENDERFUNCNAME(a)(ctx, sb, node, header_depth);
        NODETYPES(X)
#undef X
        }
#else
    return RENDERFUNCS[node->type](ctx, sb, handle, header_depth);
#endif
    }

static
warn_unused
int
render_tree(DndcContext* ctx, MStringBuilder* msb){
    size_t imgcount = ctx->img_nodes.count + ctx->imglinks_nodes.count;
    // estimate memory usage as 120 characters per node and 200 kb images.
    size_t reserve_amount = ctx->nodes.count*120 + imgcount*200*1024;
    msb_ensure_additional(msb, reserve_amount);
    bool complete_document = !(ctx->flags & DNDC_FRAGMENT_ONLY);
    if(complete_document){
        msb_write_literal(msb,
            "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            // We only support UTF-8
            "<meta charset=\"UTF-8\">\n"
            // This is just forced on everyone, idk if that is
            // desirable. It is certainly convenient this way.
            // Could add a flag to suppress this, but that is
            // kind of expensive for just a flag.
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=yes\">\n"
        );
        if(!(ctx->flags & DNDC_INPUT_IS_UNTRUSTED)){
            MARRAY_FOR_EACH(NodeHandle, m, ctx->meta_nodes){
                Node* mn = get_node(ctx, *m);
                if(mn->type != NODE_META) continue;
                NODE_CHILDREN_FOR_EACH(s, mn){
                    Node* ch = get_node(ctx, *s);
                    if(ch->type != NODE_STRING)
                        continue;
                    StringView content = stripped_view(ch->header.text, ch->header.length);
                    if(!content.length) continue;
                    MSB_FORMAT(msb, SV("<meta "), ch->header, SV(">\n"));
                }
            }
        }
        else {
            if(ctx->meta_nodes.count){
                node_print_warning(ctx, get_node(ctx, ctx->meta_nodes.data[0]), SV("Meta nodes are not allowed for untrusted input. Ignoring meta nodes."));
            }
        }
    }
    if(ctx->rendered_data.count){
        msb_write_literal(msb, "<script>\nconst data_blob = {");
        MARRAY_FOR_EACH(DataItem, data, ctx->rendered_data){
            msb_write_char(msb, '"');
            msb_write_str(msb, data->key.text, data->key.length);
            msb_write_literal(msb, "\": \"");
            msb_write_json_escaped_str(msb, data->value.text, data->value.length);
            msb_write_literal(msb, "\",\n");
        }
        msb_write_literal(msb, "};\n</script>\n");
    }
    if(complete_document){
        if(!NodeHandle_eq(ctx->titlenode, INVALID_NODE_HANDLE)){
            Node* n = get_node(ctx, ctx->titlenode);
            MSB_FORMAT(msb, "<title>", n->header, "</title>\n");
        }
        else {
            StringView filename = path_basename(path_strip_extension(ctx->outputfile));
            msb_write_literal(msb, "<title>");
            msb_write_title(msb, filename.text, filename.length);
            msb_write_literal(msb, "</title>\n");
        }
    }
    if(ctx->stylesheets_nodes.count){
        msb_write_literal(msb, "<style>\n");
        bool written = false;
        MARRAY_FOR_EACH(NodeHandle, ss, ctx->stylesheets_nodes){
            Node* node = get_node(ctx, *ss);
            // css nodes can change node types after they are registered
            if(unlikely(node->type != NODE_STYLESHEETS))
                continue;
            if(node->flags & NODEFLAG_NOINLINE){
                if(not written){
                    msb_erase(msb, sizeof("<style>\n")-1);
                }
                else {
                    msb_write_literal(msb, "</style>\n");
                }
                written = false;
                NODE_CHILDREN_FOR_EACH(it, node){
                    Node* child = get_node(ctx, *it);
                    if(unlikely(child->type != NODE_STRING)){
                        node_print_warning(ctx, child, SV("Non-string child of a style sheet is being ignored."));
                        continue;
                    }
                    MSB_FORMAT(msb, "<link rel=\"stylesheet\" href=\"", child->header, "\">\n");
                }
                continue;
            }
            written = true;
            NODE_CHILDREN_FOR_EACH(it, node){
                Node* child = get_node(ctx, *it);
                if(unlikely(child->type != NODE_STRING)){
                    node_print_warning(ctx, child, SV("Non-string child of a style sheet is being ignored."));
                    continue;
                }
                msb_write_str(msb, child->header.text, child->header.length);
                msb_write_char(msb, '\n');
            }
        }
        if(written)
            msb_write_literal(msb, "</style>\n");
    }
    if(ctx->script_nodes.count){
        MARRAY_FOR_EACH(NodeHandle, s, ctx->script_nodes){
            Node* node = get_node(ctx, *s);
            // script nodes can change node types after they are registered
            if(unlikely(node->type != NODE_SCRIPTS))
                continue;
            msb_write_literal(msb, "<script>\n");
            if(node->flags & NODEFLAG_NOINLINE){
                msb_erase(msb, sizeof("<script>\n")-1);
                if(node_children_count(node) != 1){
                    if(node_children_count(node))
                        node_print_warning(ctx, node, SV("Lines afer the first of a noninline js block are ignored"));
                    else {
                        node_print_warning(ctx, node, SV("Empty noinline js block"));
                        continue;
                    }
                }
                Node* child  = get_node(ctx, node_children(node)[0]);
                MSB_FORMAT(msb, "<script src=\"", child->header, "\"></script>\n");
                continue;
            }
            NODE_CHILDREN_FOR_EACH(it, node){
                Node* child = get_node(ctx, *it);
                if(unlikely(child->type != NODE_STRING)){
                    node_print_warning(ctx, child, SV("script with a non-string child is being ignored"));
                    continue;
                }
                StringView header = child->header;
                if(header.length)
                    msb_write_str(msb, header.text, header.length);
                msb_write_char(msb, '\n');
            }
            msb_write_literal(msb, "</script>\n");
            continue;
        }
    }
    if(complete_document){
        msb_write_literal(msb, "</head>\n");
        msb_write_literal(msb, "<body>\n");
    }
    Node* root_node = get_node(ctx, ctx->root_handle);
    // elide useless wrapper div.
    if(root_node->type == NODE_MD && node_children_count(root_node) == 1){
        if(!root_node->attributes && !root_node->classes){
            Node* child = get_node(ctx, node_children(root_node)[0]);
            if(child->type == NODE_DIV || child->type == NODE_MD)
                root_node = child;
        }
    }
    int e = render_node(ctx, msb, ctx->root_handle, 1);
    if(e) return e;
    if(complete_document){
        msb_write_literal(msb,
            "</body>\n"
            "</html>\n"
        );
    }
    return 0;
}

static void build_nav_block_node(DndcContext* , NodeHandle, MStringBuilder*, int);
static void build_nav_block_children(DndcContext* , NodeHandle, MStringBuilder*, int);

static
void
build_nav_block(DndcContext* ctx){
    MStringBuilder sb = {.allocator=ctx->string_allocator};
    build_nav_block_node(ctx, ctx->root_handle, &sb, 1);
    if(sb.cursor)
        ctx->renderednav = msb_detach_ls(&sb);
}

static
void
build_nav_block_node(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int depth){
    Node* node = get_node(ctx, handle);
    switch(node->type){
        case NODE_BULLETS:
        case NODE_TABLE:
        case NODE_HEADING:
        case NODE_PARA:
        case NODE_DIV:
        case NODE_IMAGE:
        case NODE_LIST:
        case NODE_KEYVALUE:
        case NODE_IMGLINKS:
        case NODE_DETAILS:
        case NODE_MD:
        case NODE_QUOTE:
        case NODE_CONTAINER:{
            if(node->header.length){
                StringView id = node_get_id(ctx, handle);
                if(id.length){
                    msb_write_literal(sb, "<li><a href=\"#");
                    msb_write_kebab(sb, id.text, id.length);
                    MSB_FORMAT(sb, "\">", node->header, "</a>\n<ul>\n");
                    // kind of a hack
                    size_t cursor = sb->cursor;
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
            }
        }
        // fall-through
        case NODE_DATA: // this is a little sketchy
        case NODE_IMPORT:
        case NODE_LIST_ITEM:
        case NODE_KEYVALUEPAIR:{
            build_nav_block_children(ctx, handle, sb, depth);
        }break;
        case NODE_META:
        case NODE_TITLE: // skip title as everything would be a child of it
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
        case NODE_RAW:{
            if(node->header.length){
                StringView id = node_get_id(ctx, handle);
                if(id.length){
                    msb_write_literal(sb, "<li><a href=\"#");
                    msb_write_kebab(sb, id.text, id.length);
                    MSB_FORMAT(sb, "\">", node->header, "</a>");
                    msb_write_literal(sb, "</li>\n");
                }
            }break;
        }break;
    }
}

static
void
build_nav_block_children(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb, int depth){
    if(depth > 2)
        return;
    Node* node = get_node(ctx, handle);
    NODE_CHILDREN_FOR_EACH(it, node){
        build_nav_block_node(ctx, *it, sb, depth);
    }
}

static inline
void
write_tag_escaped_str(MStringBuilder* sb, NullUnspec(const char*)text, size_t length){
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
            case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
            case 10: case 11:
            // This would've been so much nicer!
            // case 14 ... 31:
            case 14: case 15: case 16: case 17: case 18: case 19: case 20: 
            case 21: case 22: case 23: case 24: case 25: case 26: case 27: 
            case 28: case 29: case 30: case 31:

                break;
            default:
                msb_write_char(sb, c);
                break;
        }
    }
}

static inline
int
write_link_escaped_str_slow(DndcContext* ctx, MStringBuilder* sb, const char* text, size_t length, const Node* node){
    for(size_t i = 0; i < length; i++){
        char c = text[i];
        switch(c){
            case '[':{
                msb_write_literal(sb, "<a href=\"");
                const char* closing_brace = memchr(text+i, ']', length-i);
                if(unlikely(!closing_brace)){
                    node_set_err_offset(ctx, node, i, LS("Unterminated '['"));
                    return PARSE_ERROR;
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

                {
                    MStringBuilder temp = {.allocator=ctx->temp_allocator};
                    msb_write_kebab(&temp, alias, alias_length);
                    StringView temp_str = msb_borrow_sv(&temp);
                    StringView* value = find_link_target(ctx, temp_str);
                    if(unlikely(!value)){
                        if(ctx->flags & DNDC_ALLOW_BAD_LINKS){
                            node_print_warning2(ctx, node, SV("Unable to resolve link: "), temp_str);
                            msb_write_str(sb, temp_str.text, temp_str.length);
                        }
                        else {
                            node_set_err_q(ctx, node, SV("Unable to resolve link: "), temp_str);
                            msb_destroy(&temp);
                            return PARSE_ERROR;
                        }
                    }
                    else {
                        StringView* val = value;
                        msb_write_str(sb, val->text, val->length);
                    }
                    msb_destroy(&temp);
                }
                msb_write_literal(sb, "\">");
                StringView sv = stripped_view(text+i+1, text_length);
                msb_write_str(sb, sv.text, sv.length);
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
            case '&':{ // allow &lt;, &gt;
                if(length - i >= 4){
                    if(memcmp(text+i, "&lt;", 4) == 0){
                        msb_write_literal(sb, "&lt;");
                        i += 3;
                        continue;
                    }
                    if(memcmp(text+i, "&gt;", 4) == 0){
                        msb_write_literal(sb, "&gt;");
                        i += 3;
                        continue;
                    }
                }
                msb_write_literal(sb, "&amp;");
            }break;
            case '<':{
                // we allow inline <b>, <s>, <i>, </b>, </s>, </i>, <br>, <code>, </code>
                // This is a big mess and should be done in an easier to do way.
                if(length - i >= 2){
                    char peek1 = text[i+1];
                    if(peek1 == 'c'){ // could be <code> tag
                        if(length - i >= sizeof("<code>")-2){
                            if(memcmp(text+i, "<code>", sizeof("<code>")-1) == 0){
                                msb_write_literal(sb, "<code>");
                                i += 5;
                                continue;
                            }
                        }
                    }
                    if(peek1 == '/'){
                        if(length - i >= sizeof("</code>")-2){
                            if(memcmp(text+i, "</code>", sizeof("</code>")-1) == 0){
                                msb_write_literal(sb, "</code>");
                                i += sizeof("</code>")-2;
                                continue;
                            }
                        }
                    }
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
                    if(length - i >= 3){
                        char peek2 = text[i+2];
                        if(peek1 == 'b' && peek2 == 'r'){
                            if(length - i >= 4 && text[i+3] == '>'){
                                msb_write_literal(sb, "<br>");
                                i += 3;
                                continue;
                            }
                        }
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
                        if(length -i >= 4){
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
            case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
            case 10: case 11:
            // This would've been so much nicer!
            // case 14 ... 31:
            case 14: case 15: case 16: case 17: case 18: case 19: case 20: 
            case 21: case 22: case 23: case 24: case 25: case 26: case 27: 
            case 28: case 29: case 30: case 31:
                break;
            default:
                msb_write_char(sb, c);
                break;
        }
    }
    return 0;
}

#if 0 && !defined(NO_SIMD) && defined(__ARM_NEON)
// For printf debugging
static inline
void
print_u8x16(const char* prefix, uint8x16_t v){
    unsigned char buff[16];
    memcpy(buff, &v, 16);
    fprintf(stdout, "%s) ", prefix);
    for(int i = 0; i < 16; i++){
        fprintf(stdout, "[%d: %3u] ", i, buff[i]);
    }
    fprintf(stdout, "\n");
 }
#endif

static inline
int
write_link_escaped_str(DndcContext* ctx, MStringBuilder* sb, const char* text, size_t length, const Node* node){
    msb_ensure_additional(sb, length);
#if 1 && !defined(NO_SIMD) && defined(__x86_64__)
    size_t cursor = sb->cursor;
    char* sbdata = sb->data + cursor;
    __m128i lsquare = _mm_set1_epi8('[');
    __m128i hyphen  = _mm_set1_epi8('-');
    __m128i langle  = _mm_set1_epi8('<');
    __m128i rangle  = _mm_set1_epi8('>');
    __m128i amp     = _mm_set1_epi8('&');
    __m128i control = _mm_set1_epi8(32);
    while(length >= 16){
        // This code is straightforward. Check each 16byte chunk for the
        // presence of one of the special characters.  Also check for control
        // characters (ascii < 32), as those are not valid in html, with the
        // exception of newline.
        //
        // For the common case of no special character this is much faster
        // than the byte at a time processing we'd otherwise have to do.
        __m128i data         = _mm_loadu_si128((const __m128i*)text);
        __m128i test_lsquare = _mm_cmpeq_epi8(data, lsquare);
        __m128i test_hyphen  = _mm_cmpeq_epi8(data, hyphen);
        __m128i test_langle  = _mm_cmpeq_epi8(data, langle);
        __m128i test_rangle  = _mm_cmpeq_epi8(data, rangle);
        __m128i test_amp     = _mm_cmpeq_epi8(data, amp);
        __m128i test_control = _mm_cmplt_epi8(data, control);
        // Combine the results together so we can do a single check
        __m128i Ored  = _mm_or_si128(test_lsquare, test_hyphen);
        __m128i Ored2 = _mm_or_si128(test_langle, test_rangle);
        __m128i Ored3 = _mm_or_si128(test_amp, test_control);
        __m128i Ored4 = _mm_or_si128(Ored, Ored2);
        __m128i Ored5 = _mm_or_si128(Ored3, Ored4);
        int had_it = _mm_movemask_epi8(Ored5);
        if(had_it)
            break;
        // Safe to store as we did the ensure additional above and we only
        // write 1 byte of output per byte of input in this loop.
        _mm_storeu_si128((__m128i_u*)sbdata, data);
        cursor += 16;
        sbdata += 16;
        length -= 16;
        text += 16;
    }
    sb->cursor = cursor;
#endif
#if 1 && !defined(NO_SIMD) && defined(__ARM_NEON)
    // NOTE: this code is untested on actual arm chip.
    // It compiles and the logic is the same, but I didn't have
    // access to this arch when I wrote it.
    size_t cursor = sb->cursor;
    unsigned char* sbdata = (unsigned char*)sb->data + cursor;
    uint8x16_t lsquare = vdupq_n_u8('[');
    uint8x16_t hyphen = vdupq_n_u8('-');
    uint8x16_t langle  = vdupq_n_u8('<');
    uint8x16_t rangle  = vdupq_n_u8('>');
    uint8x16_t amp     = vdupq_n_u8('&');
    uint8x16_t control = vdupq_n_u8(32);
    while(length >= 16){
        uint8x16_t data         = vld1q_u8((const unsigned char*)text);
        uint8x16_t test_lsquare = vceqq_u8(data, lsquare);
        uint8x16_t test_hyphen  = vceqq_u8(data, hyphen);
        uint8x16_t test_langle  = vceqq_u8(data, langle);
        uint8x16_t test_rangle  = vceqq_u8(data, rangle);
        uint8x16_t test_amp     = vceqq_u8(data, amp);
        uint8x16_t test_control = vcltq_u8(data, control);
        // Combine the results together so we can do a single
        // check
        uint8x16_t Ored  = vorrq_u8(test_lsquare, test_hyphen);
        uint8x16_t Ored2 = vorrq_u8(test_langle, test_rangle);
        uint8x16_t Ored3 = vorrq_u8(test_amp, test_control);
        uint8x16_t Ored4 = vorrq_u8(Ored, Ored2);
        uint8x16_t Ored5 = vorrq_u8(Ored3, Ored4);
        uint64x2_t had_it = vreinterpretq_u64_u8(Ored5);

        if(vgetq_lane_u64(had_it, 0) | vgetq_lane_u64(had_it, 1)){
            #if 0
            fprintf(stdout, "'%.*s'\n", 16, text);
            print_u8x16("data", data);
            print_u8x16("   [", test_lsquare);
            print_u8x16("   -", test_hyphen);
            print_u8x16("   <", test_langle);
            print_u8x16("   >", test_rangle);
            print_u8x16("   &", test_amp);
            print_u8x16("ctrl", test_control);
            print_u8x16("comb", Ored5);
            #endif
            break;
        }
        // Safe to store as we did the ensure additional above and
        // we only write 1 byte of output per byte of input in
        // this loop.
        vst1q_u8(sbdata, data);
        cursor += 16;
        sbdata += 16;
        length -= 16;
        text += 16;
    }
    sb->cursor = cursor;
#endif
    return write_link_escaped_str_slow(ctx, sb, text, length, node);
}

static inline
int
write_header(DndcContext* ctx, MStringBuilder* sb, NodeHandle handle, int header_level){
    StringView id = node_get_id(ctx, handle);
    Node* node = get_node(ctx, handle);
    if(!id.length){
        MSB_FORMAT(sb, "<h", header_level, ">");
    }
    else{
        MSB_FORMAT(sb, "<h", header_level, " id=\"");
        msb_write_kebab(sb, id.text, id.length);
        msb_write_literal(sb, "\">");
    }
    int e = write_link_escaped_str(ctx, sb, node->header.text, node->header.length, node);
    if(e) return e;
    MSB_FORMAT(sb, "</h", header_level, ">");
    return 0;
}

static inline
void
write_classes(MStringBuilder* sb, const Node* node){
    size_t count = node->classes?node->classes->count:0;
    if(!count) return;
    StringView* classes = node->classes->data;
    msb_write_literal(sb, " class=\"");
    for(size_t i = 0; i < count; i++){
        if(i != 0){
            msb_write_char(sb, ' ');
        }
        StringView* c = &classes[i];
        msb_write_str(sb, c->text, c->length);
    }
    msb_write_char(sb, '"');
    return;
}

RENDERFUNC(STRING){
    (void)header_depth;
    Node* node = get_node(ctx, handle);
    if(unlikely(node->classes))
        node_print_warning(ctx, node, SV("Ignoring classes on string node"));
    if(unlikely(node_children_count(node)))
        node_print_warning(ctx, node, SV("Ignoring children of string node"));
    int e = write_link_escaped_str(ctx, sb, node->header.text, node->header.length, node);
    if(e) return e;
    msb_write_char(sb, '\n');
    return 0;
}

RENDERFUNC(DIV){
    msb_write_literal(sb, "<div");
    Node* node = get_node(ctx, handle);
    write_classes(sb, node);
    if(!node->header.length){
        StringView id = node_get_id(ctx, handle);
        if(id.length){
            MSB_FORMAT(sb, " id=\"", id, "\"");
        }
    }
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        int e = write_header(ctx, sb, handle, header_depth);
        if(e) return e;
        msb_write_char(sb, '\n');
    }
    NODE_CHILDREN_FOR_EACH(it, node){
        int e = render_node(ctx, sb, *it, header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "</div>\n");
    return 0;
}
RENDERFUNC(NAV){
    (void)header_depth;
    Node* node = get_node(ctx, handle);
    if(node->header.length){
        node_print_warning(ctx, node, SV("Headers on navs unsupported"));
    }
    if(node_children_count(node)){
        node_print_warning(ctx, node, SV("Children on navs unsupported"));
    }
    StringView id = node_get_id(ctx, handle);
    msb_write_literal(sb, "<nav");
    if(id.length){
        MSB_FORMAT(sb, " id=\"", id, "\"");
    }
    write_classes(sb, node);
    msb_write_literal(sb, ">\n<ul>\n");
    msb_write_str(sb, ctx->renderednav.text, ctx->renderednav.length);
    msb_write_literal(sb, "</ul>\n</nav>");
    return 0;
}
RENDERFUNC(PARA){
    Node* node = get_node(ctx, handle);
    if(node->classes){
        // maybe we should allow classes though?
        node_print_warning(ctx, node, SV("Ignoring classes on paragraph node"));
    }
    if(node->header.length){
        node_print_warning(ctx, node, SV("Ignoring header on paragraph node"));
    }
    msb_write_literal(sb, "<p>\n");
    NODE_CHILDREN_FOR_EACH(it, node){
        int e = render_node(ctx, sb, *it, header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "</p>\n");
    return 0;
}
RENDERFUNC(TITLE){
    Node* node = get_node(ctx, handle);
    int e = write_header(ctx, sb, handle, header_depth);
    if(e) return e;
    msb_write_char(sb, '\n');
    if(node_children_count(node)){
        node_print_warning(ctx, node, SV("Ignoring children of title"));
    }
    if(node->classes){
        node_print_warning(ctx, node, SV("UNIMPLEMENTED: classes on the title"));
    }
    return 0;
}
RENDERFUNC(HEADING){
    Node* node = get_node(ctx, handle);
    int e = write_header(ctx, sb, handle, header_depth+1);
    if(e) return e;
    msb_write_char(sb, '\n');
    if(node_children_count(node)){
        node_print_warning(ctx, node, SV("Ignoring children of heading"));
    }
    if(node->classes){
        node_print_warning(ctx, node, SV("UNIMPLEMENTED: classes on the heading"));
    }
    return 0;
}
RENDERFUNC(HR){
    (void)header_depth;
    Node* node = get_node(ctx, handle);
    if(node->header.length){
        node_print_warning(ctx, node, SV("Ignoring header of hr"));
    }
    if(node_children_count(node)){
        node_print_warning(ctx, node, SV("Ignoring children of hr"));
    }
    msb_write_char(sb, '\n');
    msb_write_literal(sb, "<hr>\n");
    return 0;
}
RENDERFUNC(TABLE){
    Node* node = get_node(ctx, handle);
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        int e = write_header(ctx, sb, handle, header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "<table>\n<thead>\n");
    size_t count = node_children_count(node);
    NodeHandle* children = node_children(node);
    if(count){
        Node* child = get_node(ctx, children[0]);
        if(child->type != NODE_TABLE_ROW){
            node_set_err(ctx, child, LS("children of a table ought to be table rows..."));
            return GENERIC_ERROR;
        }
        // inline rendering table row here so we can do heads
        msb_write_literal(sb, "<tr>\n");
        NODE_CHILDREN_FOR_EACH(it, child){
            msb_write_literal(sb, "<th>");
            int e = render_node(ctx, sb, *it, header_depth);
            if(e) return e;
            msb_write_literal(sb, "</th>\n");
        }
        msb_write_literal(sb, "</tr>\n");
    }
    msb_write_literal(sb, "</thead>\n<tbody>\n");
    for(size_t i = 1; i < count; i++){
        int e = render_node(ctx, sb, children[i], header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "</tbody></table>\n</div>\n");
    return 0;
}
RENDERFUNC(TABLE_ROW){
    Node* node = get_node(ctx, handle);
    msb_write_literal(sb, "<tr>\n");
    NODE_CHILDREN_FOR_EACH(it, node){
        msb_write_literal(sb, "<td>");
        int e = render_node(ctx, sb, *it, header_depth);
        if(e) return e;
        msb_write_literal(sb, "</td>\n");
    }
    msb_write_literal(sb, "</tr>\n");
    return 0;
}
RENDERFUNC(STYLESHEETS){
    // intentionally do not render stylesheets
    (void)ctx;
    (void)sb;
    (void)handle;
    (void)header_depth;
    return 0;
}
RENDERFUNC(LINKS){
    // intentionally do not render links
    (void)ctx;
    (void)sb;
    (void)handle;
    (void)header_depth;
    return 0;
}
RENDERFUNC(SCRIPTS){
    // intentionally do not render scripts
    (void)ctx;
    (void)sb;
    (void)handle;
    (void)header_depth;
    return 0;
}
RENDERFUNC(IMPORT){
    Node* node = get_node(ctx, handle);
    // An imports members are replaced with containers that were the things
    // they imported.
    // Don't render the import itself though.
    if(node->header.length){
        node_print_warning(ctx, node, SV("Ignoring import header"));
    }
    NODE_CHILDREN_FOR_EACH(it, node){
        int e = render_node(ctx, sb, *it, header_depth);
        if(e) return e;
    }
    return 0;
}
RENDERFUNC(IMAGE){
    Node* node = get_node(ctx, handle);
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    StringView id = node_get_id(ctx, handle);
    if(id.length){
        MSB_FORMAT(sb, SV(" id=\""), id, SV("\" "));
    }
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        int e = write_header(ctx, sb, handle, header_depth);
        if(e) return e;
        msb_write_char(sb, '\n');
    }
    size_t count = node_children_count(node);
    if(!count){
        node_set_err(ctx, node, LS("Image node missing any children (first should be a string that is path to the image"));
        return PARSE_ERROR;
    }
    NodeHandle* children = node_children(node);
    // CLEANUP: lots of copy and paste here.
    if(ctx->flags & DNDC_USE_DND_URL_SCHEME){
        Node* imgpath_node = get_node(ctx, children[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, LS("First should be a string and be the path to the image."));
            return PARSE_ERROR;
        }
        StringView header = imgpath_node->header;
        msb_write_literal(sb, "<img src=\"dnd://");
        if((not path_is_abspath(header)) and ctx->base_directory.length){
            msb_write_str_with_backslashes_as_forward_slashes(sb, ctx->base_directory.text, ctx->base_directory.length);
            msb_write_char(sb, '/');
            msb_write_str_with_backslashes_as_forward_slashes(sb, header.text, header.length);
        }
        else {
            msb_write_str_with_backslashes_as_forward_slashes(sb, header.text, header.length);
        }
        msb_write_char(sb, '"');
    }
    else if((ctx->flags & DNDC_DONT_INLINE_IMAGES) || (node->flags & NODEFLAG_NOINLINE)){
        Node* imgpath_node = get_node(ctx, children[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, LS("First should be a string and be the path to the image."));
            return PARSE_ERROR;
        }
        StringView header = imgpath_node->header;
        msb_write_literal(sb, "<img src=\"");
        msb_write_str_with_backslashes_as_forward_slashes(sb, header.text, header.length);
        msb_write_char(sb, '"');
    }
    else{
        Node* imgpath_node = get_node(ctx, children[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, LS("First should be a string and be the path to the image."));
            return PARSE_ERROR;
        }
        StringView header = imgpath_node->header;
        StringViewResult processed_e = ctx_load_processed_binary_file(ctx, header);
        if(processed_e.errored){
            node_set_err_q(ctx, imgpath_node, SV("Unable to read "), header);
            return processed_e.errored;
        }
        else {
            msb_write_literal(sb, "<img src=\"data:image/png;base64,");
            uint64_t before = get_t();
            StringView b64 = processed_e.result;
            msb_write_str(sb, b64.text, b64.length);
            uint64_t after = get_t();
            report_time(ctx, SV("Copying the base64 data of an img took "), after-before);
        }
        msb_write_char(sb, '"');
    }
    for(size_t i = 1; i < count; i++){
        Node* node = get_node(ctx, children[i]);
        if(node->type == NODE_STRING){
            SplitPair stripped = stripped_split(node->header.text, node->header.length, '=');
            if(stripped.tail.length){
                if(SV_equals(stripped.head, SV("width"))){
                    MSB_FORMAT(sb, SV(" "), stripped.head, SV("=\""));
                    msb_write_html_quote_escaped_string(sb, stripped.tail.text, stripped.tail.length);
                    msb_write_char(sb, '"');
                    continue;
                }
                if(SV_equals(stripped.head, SV("height"))){
                    MSB_FORMAT(sb, SV(" "), stripped.head, SV("=\""));
                    msb_write_html_quote_escaped_string(sb, stripped.tail.text, stripped.tail.length);
                    msb_write_char(sb, '"');
                    continue;
                }
                if(SV_equals(stripped.head, SV("alt"))){
                    MSB_FORMAT(sb, SV(" "), stripped.head, SV("=\""));
                    msb_write_html_quote_escaped_string(sb, stripped.tail.text, stripped.tail.length);
                    msb_write_char(sb, '"');
                    continue;
                }
            }
        }
        int e = render_node(ctx, sb, children[i], header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, ">\n");
    msb_write_literal(sb, "</div>\n");
    return 0;
}
RENDERFUNC(QUOTE){
    Node* node = get_node(ctx, handle);
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        int e = write_header(ctx, sb,  handle, header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "<blockquote>\n");
    NODE_CHILDREN_FOR_EACH(it, node){
        int e = render_node(ctx, sb, *it, header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "</blockquote>\n</div>\n");
    return 0;
}
RENDERFUNC(JS){
    // intentionally not outputting this
    (void)ctx;
    (void)sb;
    (void)handle;
    (void)header_depth;
    return 0;
}
RENDERFUNC(RAW){
    Node* node = get_node(ctx, handle);
    // Don't let people smuggle <script> tags in!
    // Changes the semantics a bit, but oh well.
    if(unlikely(ctx->flags & DNDC_INPUT_IS_UNTRUSTED)){
        NODE_CHILDREN_FOR_EACH(it, node){
            Node* child = get_node(ctx, *it);
            if(unlikely(child->type != NODE_STRING))
                node_print_warning(ctx, child, SV("Raw node with a non-string child"));
            write_tag_escaped_str(sb, child->header.text, child->header.length);
            msb_write_char(sb, '\n');
        }
        return 0;
    }
    // ignoring the header for now. Idk what the semantics are supposed to be.
    NODE_CHILDREN_FOR_EACH(it, node){
        Node* child = get_node(ctx, *it);
        if(unlikely(child->type != NODE_STRING))
            node_print_warning(ctx, child, SV("Raw node with a non-string child"));
        msb_write_str(sb, child->header.text, child->header.length);
        msb_write_char(sb, '\n');
    }
    (void)header_depth;
    return 0;
}
RENDERFUNC(PRE){
    Node* node = get_node(ctx, handle);
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        int e = write_header(ctx, sb, handle, header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "<pre>\n");
    NODE_CHILDREN_FOR_EACH(it, node){
        Node* child = get_node(ctx, *it);
        if(unlikely(child->type != NODE_STRING))
            node_print_warning(ctx, child, SV("pre node with a non-string child"));
        write_tag_escaped_str(sb, child->header.text, child->header.length);
        msb_write_char(sb, '\n');
    }
    msb_write_literal(sb, "</pre>\n</div>\n");
    return 0;
}
RENDERFUNC(BULLETS){
    Node* node = get_node(ctx, handle);
    if(unlikely(node->header.length))
        node_print_warning(ctx, node, SV("ignoring header on bullet list"));
    if(unlikely(node->classes))
        node_print_warning(ctx, node, SV("Ignoring classes on bullet list"));
    msb_write_literal(sb, "<ul>\n");
    NODE_CHILDREN_FOR_EACH(it, node){
        int e = render_node(ctx, sb, *it, header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "</ul>\n");
    return 0;
}
RENDERFUNC(LIST){
    Node* node = get_node(ctx, handle);
    if(unlikely(node->header.length))
        node_print_warning(ctx, node, SV("ignoring header on list"));
    if(unlikely(node->classes))
        node_print_warning(ctx, node, SV("Ignoring classes on list"));
    msb_write_literal(sb, "<ol>\n");
    NODE_CHILDREN_FOR_EACH(it, node){
        int e = render_node(ctx, sb, *it, header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "</ol>\n");
    return 0;
}
RENDERFUNC(LIST_ITEM){
    Node* node = get_node(ctx, handle);
    msb_write_literal(sb, "<li>\n");
    if(unlikely(node->header.length))
        node_print_warning(ctx, node, SV("ignoring header on list item"));
    if(unlikely(node->classes))
        node_print_warning(ctx, node, SV("Ignoring classes on list item"));
    size_t count = node_children_count(node);
    NodeHandle* children = node_children(node);
    for(size_t i = 0; i < count; i++){
        if(i != 0)
            msb_write_char(sb, ' ');
        int e = render_node(ctx, sb, children[i], header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "</li>\n");
    return 0;
}
RENDERFUNC(KEYVALUE){
    Node* node = get_node(ctx, handle);
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        int e = write_header(ctx, sb, handle, header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "<table><tbody>\n");
    NODE_CHILDREN_FOR_EACH(it, node){
        int e = render_node(ctx, sb, *it, header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "</tbody></table>\n</div>\n");
    return 0;
}
RENDERFUNC(KEYVALUEPAIR){
    Node* node = get_node(ctx, handle);
    msb_write_literal(sb, "<tr>\n");
    NODE_CHILDREN_FOR_EACH(it, node){
        msb_write_literal(sb, "<td>");
        int e = render_node(ctx, sb, *it, header_depth);
        if(e) return e;
        // This is sort of hacky, but we need the td to not
        // have a trailing newline so that css content stuff
        // works right.
        while(msb_peek(sb) == '\n')
            msb_erase(sb, 1);
        msb_write_literal(sb, "</td>\n");
    }
    msb_write_literal(sb, "</tr>\n");
    return 0;
}
RENDERFUNC(IMGLINKS){
    Node* node = get_node(ctx, handle);
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        int e = write_header(ctx, sb, handle, header_depth);
        if(e) return e;
    }
    if(node_children_count(node) < 4){
        node_set_err(ctx, node, LS("Too few children of an imglinks node (expected path to the image, width, height, viewBox in that order)"));
        return PARSE_ERROR;
    }

    // FIXME: It's kind of janky that I parse at htmlgen time.
    StringView imgdatab64 = {0};
    NodeHandle* children = node_children(node);
    if(not (ctx->flags & (DNDC_DONT_INLINE_IMAGES | DNDC_USE_DND_URL_SCHEME))){
        Node* imgpath_node = get_node(ctx, children[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, LS("First should be a string and be the path to the image"));
            return PARSE_ERROR;
        }
        StringView header = imgpath_node->header;
        StringViewResult processed_e = ctx_load_processed_binary_file(ctx, header);
        if(processed_e.errored){
            node_set_err_q(ctx, imgpath_node, SV("Unable to read "), header);
            return processed_e.errored;
        }
        imgdatab64 = processed_e.result;
    }
    int width;
    {
        Node* width_node = get_node(ctx, children[1]);
        if(width_node->type != NODE_STRING){
            node_set_err(ctx, width_node, LS("Second should be a string and be 'width = WIDTH'"));
            return PARSE_ERROR;
        }
        StringView header = width_node->header;
        SplitPair pair = stripped_split(header.text, header.length, '=');
        if(pair.head.length == header.length){
            node_set_err(ctx, width_node, LS("Missing a '='"));
            return PARSE_ERROR;
        }
        if(!SV_equals(pair.head, SV("width"))){
            node_set_err_q(ctx, width_node, SV("Expected 'width', got "), pair.head);
            return PARSE_ERROR;
        }
        IntResult e = parse_int(pair.tail.text, pair.tail.length);
        if(e.errored){
            node_set_err_q(ctx, width_node, SV("Unable to parse an int from "), pair.tail);
            return PARSE_ERROR;
        }
        width = e.result;
    }
    int height;
    {
        Node* height_node  = get_node(ctx, children[2]);
        if(height_node->type != NODE_STRING){
            node_set_err(ctx, height_node, LS("Third should be a string and be 'height = HEIGHT'"));
            return PARSE_ERROR;
        }
        StringView header = height_node->header;
        SplitPair pair = stripped_split(header.text, header.length, '=');
        if(pair.head.length == header.length){
            node_set_err(ctx, height_node, LS("Missing a '='"));
            return PARSE_ERROR;
        }
        if(!SV_equals(pair.head, SV("height"))){
            node_set_err_q(ctx, height_node, SV("Expected 'height', got "), pair.head);
            return PARSE_ERROR;
        }
        IntResult e = parse_int(pair.tail.text, pair.tail.length);
        if(e.errored){
            node_set_err_q(ctx, height_node, SV("Unable to parse an int from "), pair.tail);
            return PARSE_ERROR;
        }
        height = e.result;
    }
    int viewbox[4];
    {
        Node* viewBox_node = get_node(ctx, children[3]);
        if(viewBox_node->type != NODE_STRING){
            node_set_err(ctx, viewBox_node, LS("Fourth should be a string and be 'viewBox = x0 y0 x1 y1'"));
            return PARSE_ERROR;
        }
        StringView header = viewBox_node->header;
        const char* equals = memchr(header.text, '=', header.length);
        if(!equals){
            node_set_err(ctx, viewBox_node, LS("Missing a '='"));
            return PARSE_ERROR;
        }
        StringView lead = stripped_view(header.text, equals - header.text);
        if(!SV_equals(lead, SV("viewBox"))){
            node_set_err_q(ctx, viewBox_node, SV("Expected 'viewBox', got "), lead);
            return PARSE_ERROR;
        }
        const char* cursor = equals+1;
        int which = 0;
        const char* end = header.text + header.length;
        for(;;){
            if(cursor == end){
                node_set_err(ctx, viewBox_node, LS("Unexpected end of line before we finished parsing the ints"));
                return PARSE_ERROR;
            }
            switch(*cursor){
                case ' ': case '\t': case '\r': case '\n':
                    cursor++;
                    continue;
                case CASE_0_9:
                    break;
                default:
                    node_set_err(ctx, viewBox_node, LS("Found non-numeric when trying to parse the viewBox"));
                    return PARSE_ERROR;
            }
            const char* after_number = cursor+1;
            for(;;){
                if(after_number == end)
                    break;
                switch(*after_number){
                    case CASE_0_9:
                        after_number++;
                        continue;
                    default:
                        break;
                }
                break;
            }
            ptrdiff_t num_length = after_number - cursor;
            IntResult e = parse_int(cursor, num_length);
            if(e.errored){
                node_set_err_q(ctx, viewBox_node, SV("Failed to parse an int from "), (StringView){.length=num_length, .text=cursor});
                return PARSE_ERROR;
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
                    node_set_err_q(ctx, viewBox_node, SV("Found trailing text after successfully parsing 4 ints: "), (StringView){.text=cursor, .length = end-cursor});
                    return PARSE_ERROR;
            }
        }
    }
    MSB_FORMAT(sb, "<svg width=\"", width, "\" height=\"", height, "\" viewBox=\"", viewbox[0], " ", viewbox[1], " ", viewbox[2], " ", viewbox[3], "\" style=\"background-size: 100% 100%; ");

    if(ctx->flags & DNDC_USE_DND_URL_SCHEME){
        msb_write_literal(sb, "background-image: url('dnd://");
        Node* imgpath_node = get_node(ctx, children[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, LS("First should be a string and be the path to the image"));
            return PARSE_ERROR;
        }
        StringView header = imgpath_node->header;
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
    else if((ctx->flags & DNDC_DONT_INLINE_IMAGES) || (node->flags & NODEFLAG_NOINLINE)){
        msb_write_literal(sb, "background-image: url('");
        Node* imgpath_node = get_node(ctx, children[0]);
        if(imgpath_node->type != NODE_STRING){
            node_set_err(ctx, imgpath_node, LS("First should be a string and be the path to the image"));
            return PARSE_ERROR;
        }
        StringView header = imgpath_node->header;
        msb_write_str_with_backslashes_as_forward_slashes(sb, header.text, header.length);
        msb_write_literal(sb, "');\">\n");
    }
    else {
        msb_write_literal(sb, "background-image: url('data:image/png;base64,");
        uint64_t before = get_t();
        assert(imgdatab64.length);
        msb_write_str(sb, imgdatab64.text, imgdatab64.length);
        uint64_t after = get_t();
        report_time(ctx, SV("Copying the base64 data of an imglinks took "), after-before);
        msb_write_literal(sb, "');\">\n");
    }
    for(size_t i = 4; i < node_children_count(node); i++){
        Node* child = get_node(ctx, children[i]);
        if(child->type != NODE_STRING){
            if(child->type == NODE_JS || child->type == NODE_COMMENT)
                continue;
            node_print_warning2(ctx, child, SV("Non-string node child of imglinks node: "), LS_to_SV(NODENAMES[child->type]));
            continue;
        }
        StringView header = child->header;
        const char* end = header.text + header.length;
        const char* equals = memchr(header.text, '=', header.length);
        if(!equals){
            node_set_err(ctx, child, LS("No '=' found in an imglinks line"));
            return PARSE_ERROR;
        }
        const char* at = memchr(equals, '@', end - equals);
        if(!at){
            node_set_err(ctx, child, LS("No '@' found in an imglinks line"));
            return PARSE_ERROR;
        }
        const char* comma = memchr(at, ',', end - at);
        if(!comma){
            node_set_err(ctx, child, LS("No ',' found in an imglinks line separating the coordinates"));
            return PARSE_ERROR;
        }
        StringView first = stripped_view(header.text, equals - header.text);
        StringView second = stripped_view(equals+1, at - (equals + 1));
        StringView third = stripped_view(at+1, comma - (at+1));
        StringView fourth = stripped_view(comma+1, end - (comma+1));
        IntResult x_err = parse_int(third.text, third.length);
        if(x_err.errored){
            node_set_err_q(ctx, child, SV("Unable to parse an int from "), third);
            return x_err.errored;
        }
        int x = x_err.result;
        IntResult y_err = parse_int(fourth.text, fourth.length);
        if(y_err.errored){
            node_set_err_q(ctx, child, SV("Unable to parse an int from "), fourth);
            return y_err.errored;
        }
        int y = y_err.result;
        MSB_FORMAT(sb, "<a href=\"", second, "\"><text style=\"text-anchor:middle;\" transform=\"translate(",x,",",y,")\">\n",
                first, "\n</text></a>\n");
    }
    msb_write_literal(sb, "</svg>\n");
    msb_write_literal(sb, "</div>\n");
    return 0;
}
RENDERFUNC(DATA){
    // intentionally not rendering this
    (void)ctx;
    (void)sb;
    (void)handle;
    (void)header_depth;
    return 0;
}
RENDERFUNC(COMMENT){
    // intentionally not rendering a comment
    (void)ctx;
    (void)sb;
    (void)handle;
    (void)header_depth;
    return 0;
}
RENDERFUNC(MD){
    Node* node = get_node(ctx, handle);
    msb_write_literal(sb, "<div");
    write_classes(sb, node);
    msb_write_literal(sb, ">\n");
    if(node->header.length){
        header_depth++;
        int e = write_header(ctx, sb, handle, header_depth);
        if(e) return e;
        msb_write_char(sb, '\n');
    }
    NODE_CHILDREN_FOR_EACH(it, node){
        int e = render_node(ctx, sb, *it, header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "</div>\n");
    return 0;
}
RENDERFUNC(CONTAINER){
    Node* node = get_node(ctx, handle);
    if(node->header.length){
        node_print_warning(ctx, node, SV("Ignoring container header."));
    }
    NODE_CHILDREN_FOR_EACH(it, node){
        int e = render_node(ctx, sb, *it, header_depth);
        if(e) return e;
    }
    return 0;
}
RENDERFUNC(INVALID){
    Node* node = get_node(ctx, handle);
    node_set_err(ctx, node, LS("Invalid node when rendering."));
    (void)ctx;
    (void)sb;
    (void)handle;
    (void)header_depth;
    return GENERIC_ERROR;
}
RENDERFUNC(DETAILS){
    Node* node = get_node(ctx, handle);
    msb_write_literal(sb, "<details");
    write_classes(sb, node);
    StringView id = node_get_id(ctx, handle);
    if(id.length){
        MSB_FORMAT(sb, SV(" id=\""), id, SV("\""));
    }
    msb_write_literal(sb, ">\n");
    msb_write_literal(sb, "<summary style=\"cursor:pointer\">\n");
    if(node->header.length){
        msb_write_str(sb, node->header.text, node->header.length);
    }
    msb_write_literal(sb, "</summary>\n");
    msb_write_literal(sb, "<div>\n");
    NODE_CHILDREN_FOR_EACH(it, node){
        int e = render_node(ctx, sb, *it, header_depth);
        if(e) return e;
    }
    msb_write_literal(sb, "</div>\n</details>\n");
    return 0;
}
RENDERFUNC(META){
    (void)ctx;
    (void)sb;
    (void)handle;
    (void)header_depth;
    return 0;
}
#undef RENDERFUNC
#undef RENDERFUNCNAME

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
