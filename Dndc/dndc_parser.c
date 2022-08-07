//
// Copyright © 2021-2022, David Priver
//
#ifndef DNDC_PARSER_C
#define DNDC_PARSER_C
#include "dndc_funcs.h"
#include "dndc_types.h"
#include "dndc_logging.h"
#include "Utils/bit_util.h"
#include "Utils/str_util.h"

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

typedef struct ErrorableNodeFlags{
    enum NodeFlags result;
    int errored;
} ErrorableNodeFlags;

static
ErrorableNodeFlags
parse_post_colon(DndcContext* ctx, StringView postcolon, NodeHandle node_handle);

static
void
analyze_line(DndcContext*);
static
void
advance_row(DndcContext*);

#define PARSEFUNC(name) static int name(DndcContext* ctx, NodeHandle parent_handle, int indentation)
static
int
parse_node(DndcContext* ctx, NodeHandle parent_handle, NodeType parent_type, int indentation, NodeFlags flags);
PARSEFUNC(parse_table_node);
PARSEFUNC(parse_keyvalue_node);
#if 0
PARSEFUNC(parse_bullets_node);
PARSEFUNC(parse_bullet_node);
PARSEFUNC(parse_list_node);
PARSEFUNC(parse_list_item);
#endif
PARSEFUNC(parse_raw_node);
PARSEFUNC(parse_md_node);

static inline
void
parse_log_err(DndcContext* ctx, const char*_Null_unspecified errchar, LongString mess){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(!ctx->log_func)
        return;
    int col = (int)(errchar - ctx->linestart);
    StringView filename = ctx->filename;
    int line = ctx->lineno;
    ctx->log_func(ctx->log_user_data, DNDC_ERROR_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
}

static inline
void
parse_log_err_q(DndcContext* ctx, const char*_Null_unspecified errchar, StringView msg, StringView q){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(!ctx->log_func)
        return;
    int col = (int)(errchar - ctx->linestart);
    StringView filename = ctx->filename;
    int line = ctx->lineno;
    MStringBuilder msb = {.allocator = temp_allocator(ctx)};
    MSB_FORMAT(&msb, msg, quoted(q));
    LongString mess = msb_borrow_ls(&msb);
    ctx->log_func(ctx->log_user_data, DNDC_ERROR_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&msb);
}

#if 1 &&!defined(NO_SIMD) && defined(__ARM_NEON)
// Copied from https://stackoverflow.com/a/68694558
// Is there a better way to do this?
// It's pretty simple.
// So annoying that there is no movemask for arm!
static inline
uint32_t
_mm_movemask_aarch64(uint8x16_t input){
    _Alignas(16) const uint8_t ucShift[] = {-7,-6,-5,-4,-3,-2,-1,0,-7,-6,-5,-4,-3,-2,-1,0};
    uint8x16_t vshift = vld1q_u8(ucShift);
    // Mask to only the msb of each lane.
    uint8x16_t vmask = vandq_u8(input, vdupq_n_u8(0x80));

    // Shift the mask into place.
    vmask = vshlq_u8(vmask, vshift);
    uint32_t out = vaddv_u8(vget_low_u8(vmask));
    // combine
    out += vaddv_u8(vget_high_u8(vmask)) << 8;

    return out;
}
#endif

static inline
void
analyze_line(DndcContext* ctx){
    if(ctx->cursor == ctx->linestart)
        return;
    const char* doublecolon = NULL;
    const char* endline = NULL;
    const char* cursor = ctx->cursor;
    int nspace = 0;
    size_t length = ctx->end - ctx->cursor;
#if 1 && !defined(NO_SIMD) && defined(__x86_64__)
    __m128i spaces  = _mm_set1_epi8(' ');
    __m128i cr      = _mm_set1_epi8('\r');
    __m128i tabs    = _mm_set1_epi8('\t');
    while(length >= 16){
        __m128i data         = _mm_loadu_si128((const __m128i*)cursor);
        __m128i test_space = _mm_cmpeq_epi8(data, spaces);
        __m128i test_cr    = _mm_cmpeq_epi8(data, cr);
        __m128i test_tabs  = _mm_cmpeq_epi8(data, tabs);
        __m128i spacecr    = _mm_or_si128(test_space, test_cr);
        __m128i whitespace = _mm_or_si128(spacecr, test_tabs);
        unsigned mask = _mm_movemask_epi8(whitespace);
        int n = ctz_32(~mask);
        nspace += n;
        if(n != 16){
            cursor += n;
            length -= n;
            goto Lafterwhitespace;
        }
        cursor += 16;
        length -= 16;
    }
#endif
#if 1 && !defined(NO_SIMD) && defined(__ARM_NEON)
    uint8x16_t spaces = vdupq_n_u8(' ');
    uint8x16_t cr     = vdupq_n_u8('\r');
    uint8x16_t tabs   = vdupq_n_u8('\t');
    while(length >= 16){
        uint8x16_t data       = vld1q_u8((const unsigned char*)cursor);
        uint8x16_t test_space = vceqq_u8(data, spaces);
        uint8x16_t test_cr    = vceqq_u8(data, cr);
        uint8x16_t test_tabs  = vceqq_u8(data, tabs);
        uint8x16_t spacecr    = vorrq_u8(test_space, test_cr);
        uint8x16_t whitespace = vorrq_u8(spacecr, test_tabs);
        // uint64x2_t had_it     = vreinterpretq_u64_u8(whitespace);
        unsigned mask = _mm_movemask_aarch64(whitespace);
        int n = ctz_32(~mask);
        nspace += n;
        if(n != 16){
            cursor += n;
            length -= n;
            goto Lafterwhitespace;
        }
        cursor += 16;
        length -= 16;
    }
#endif
    for(;length;length--,cursor++){
        char ch = *cursor;
        switch(ch){
            case ' ': case '\r': case '\t':
                nspace++;
                continue;
            default:
                goto Lafterwhitespace;
        }
    }
    Lafterwhitespace:;
    length = ctx->end - cursor;
#if 1 && !defined(NO_SIMD) && defined(__x86_64__)
    __m128i colons  = _mm_set1_epi8(':');
    __m128i newline = _mm_set1_epi8('\n');
    __m128i zed     = _mm_set1_epi8(0);
    while(length >= 17){
        __m128i data0      = _mm_loadu_si128((const __m128i*)(cursor+0));
        __m128i data1      = _mm_loadu_si128((const __m128i*)(cursor+1));
        __m128i testcolon0 = _mm_cmpeq_epi16(data0, colons);
        __m128i testcolon1 = _mm_cmpeq_epi16(data1, colons);
        __m128i testnl     = _mm_cmpeq_epi8(data0, newline);
        __m128i testzed    = _mm_cmpeq_epi8(data0, zed);
        __m128i testend    = _mm_or_si128(testnl, testzed);
        unsigned colon0    = _mm_movemask_epi8(testcolon0);
        unsigned colon1    = _mm_movemask_epi8(testcolon1);
        unsigned end       = _mm_movemask_epi8(testend);
        if(end){
            unsigned endoff = ctz_32(end);
            unsigned colonoff = -1;
            if(colon0){
                unsigned off = ctz_32(colon0);
                if(off < endoff && off < colonoff)
                    colonoff = off;
            }
            if(colon1){
                unsigned off = ctz_32(colon1)+1;
                if(off < endoff && off < colonoff)
                    colonoff = off;
            }
            if(colonoff != (unsigned)-1){
                doublecolon = cursor + colonoff;
            }
            endline = cursor + endoff;
            goto Lfinish;
        }
        if(colon0 || colon1){
            unsigned colonoff = -1;
            if(colon0){
                unsigned off = ctz_32(colon0);
                colonoff = off;
            }
            if(colon1){
                unsigned off = ctz_32(colon1)+1;
                if(off < colonoff)
                    colonoff = off;
            }
            if(colonoff != (unsigned)-1){
                doublecolon = cursor + colonoff;
                cursor += 16;
                length -= 16;
                goto Lendonly;
            }
        }
        cursor += 16;
        length -= 16;
    }
#endif
#if 1 && !defined(NO_SIMD) && defined(__ARM_NEON)
    uint8x16_t colons  = vdupq_n_u8(':');
    uint8x16_t newline = vdupq_n_u8('\n');
    uint8x16_t zed     = vdupq_n_u8(0);
    while(length >= 17){
        uint8x16_t data0 = vld1q_u8((const unsigned char*)cursor);
        uint8x16_t data1 = vld1q_u8((const unsigned char*)cursor+1);
        uint8x16_t testcolon0 = vreinterpretq_u8_u16(vceqq_u16(vreinterpretq_u16_u8(data0), vreinterpretq_u16_u8(colons)));
        uint8x16_t testcolon1 = vreinterpretq_u8_u16(vceqq_u16(vreinterpretq_u16_u8(data1), vreinterpretq_u16_u8(colons)));
        uint8x16_t testnl = vceqq_u8(data0, newline);
        uint8x16_t testzed = vceqq_u8(data0, zed);
        uint8x16_t testend = vorrq_u8(testnl, testzed);
        unsigned colon0 = _mm_movemask_aarch64(testcolon0);
        unsigned colon1 = _mm_movemask_aarch64(testcolon1);
        unsigned end    = _mm_movemask_aarch64(testend);
        if(end){
            unsigned endoff = ctz_32(end);
            unsigned colonoff = -1;
            if(colon0){
                unsigned off = ctz_32(colon0);
                if(off < endoff && off < colonoff)
                    colonoff = off;
            }
            if(colon1){
                unsigned off = ctz_32(colon1)+1;
                if(off < endoff && off < colonoff)
                    colonoff = off;
            }
            if(colonoff != (unsigned)-1){
                doublecolon = cursor + colonoff;
            }
            endline = cursor + endoff;
            goto Lfinish;
        }
        if(colon0 || colon1){
            unsigned colonoff = -1;
            if(colon0){
                unsigned off = ctz_32(colon0);
                colonoff = off;
            }
            if(colon1){
                unsigned off = ctz_32(colon1)+1;
                if(off < colonoff)
                    colonoff = off;
            }
            if(colonoff != (unsigned)-1){
                doublecolon = cursor + colonoff;
                cursor += 16;
                length -= 16;
                goto Lendonly;
            }
        }
        cursor += 16;
        length -= 16;
    }
#endif
    for(;length;length--,cursor++){
        switch(*cursor){
            case '\n': case '\0':
                endline = cursor;
                goto Lfinish;
            case ':':
                if(length > 1 && cursor[1] == ':'){
                    doublecolon = cursor;
                    goto Lendonly;
                }
                continue;
            default:
                continue;
        }
        break;
    }
    Lendonly:;
    length = ctx->end - cursor;
#if 1 && !defined(NO_SIMD) && defined(__x86_64__)
    while(length >= 16){
        __m128i data    = _mm_loadu_si128((const __m128i*)(cursor));
        __m128i testnl  = _mm_cmpeq_epi8(data, newline);
        __m128i testzed = _mm_cmpeq_epi8(data, zed);
        __m128i testend = _mm_or_si128(testnl, testzed);
        unsigned end = _mm_movemask_epi8(testend);
        if(end){
            unsigned endoff = ctz_32(end);
            endline = cursor + endoff;
            goto Lfinish;
        }
        cursor += 16;
        length -= 16;
    }
#endif
#if 1 && !defined(NO_SIMD) && defined(__ARM_NEON)
    while(length >= 16){
        uint8x16_t data    = vld1q_u8((const unsigned char*)cursor);
        uint8x16_t testnl  = vceqq_u8(data, newline);
        uint8x16_t testzed = vceqq_u8(data, zed);
        uint8x16_t testend = vorrq_u8(testnl, testzed);
        unsigned end       = _mm_movemask_aarch64(testend);
        if(end){
            unsigned endoff = ctz_32(end);
            endline = cursor + endoff;
            goto Lfinish;
        }
        cursor += 16;
        length -= 16;
    }
#endif
    for(;length;length--,cursor++){
        switch(*cursor){
            case '\n': case '\0':
                endline = cursor;
                goto Lfinish;
            default:
                continue;
        }
    }
    endline = cursor;

    Lfinish:;
    ctx->doublecolon = doublecolon;
    ctx->line_end = endline;
    ctx->linestart = ctx->cursor;
    ctx->nspaces = nspace;
}

static inline
void
force_inline
advance_row(DndcContext* ctx){
    if(unlikely(ctx->line_end == ctx->end))
        ctx->cursor = ctx->line_end;
    else
        ctx->cursor = ctx->line_end+1;
    ctx->lineno++;
}


static inline
void
force_inline
init_node(DndcContext* ctx, NodeHandle handle, const char* src_char, NodeType type){
    Node* node = get_node(ctx, handle);
    int col = (int)(src_char - ctx->linestart);
    node->col = col;
    assert(node->col >= 0);
    node->filename_idx = ctx->filenames.count-1;
    node->row = ctx->lineno;
    node->type = type;
}

static inline
void
force_inline
init_string_node(DndcContext* ctx, NodeHandle handle, StringView sv){
    Node* node = get_node(ctx, handle);
    int col = (int)(sv.text - ctx->linestart);
    node->col = col;
    node->filename_idx = ctx->filenames.count-1;
    node->row = ctx->lineno;
    node->type = NODE_STRING;
    node->header = sv;
}

static
int
dndc_parse(DndcContext* ctx, NodeHandle root_handle, StringView filename, const char* text, size_t length){
    ctx->cursor = text;
    ctx->end = text + length;
    ctx->linestart = NULL;
    ctx->doublecolon = NULL;
    ctx->line_end = NULL;
    ctx->nspaces = 0;
    ctx->lineno = 0;
    int dont_copy_it = 0;
    size_t fn_idx = ctx_add_filename(ctx, filename, dont_copy_it);
    ctx->filename = ctx->filenames.data[fn_idx];
    NodeType type = get_node(ctx, root_handle)->type;
    int e = parse_node(ctx, root_handle, type, -1, NODEFLAG_NONE);
    if(e) return e;
    return 0;
}

static
int
parse_double_colon(DndcContext* ctx, NodeHandle parent_handle){
    // parse the node header
    const char* starttext = ctx->doublecolon + 2;
    size_t length = ctx->line_end - starttext;
    StringView postcolon = stripped_view(starttext, length);
    NodeHandle new_node_handle = alloc_handle(ctx);
    init_node(ctx, new_node_handle, ctx->linestart+ctx->nspaces, NODE_INVALID);
    NodeFlags flags;
    {
        ErrorableNodeFlags e = parse_post_colon(ctx, postcolon, new_node_handle);
        if(e.errored){
            return e.errored;
        }
        flags = e.result;
    }
    append_child(ctx, parent_handle, new_node_handle);
    NodeType type;
    {
        Node* node = get_node(ctx, new_node_handle);
        const char* header = ctx->linestart + ctx->nspaces;
        node->header = stripped_view(header, ctx->doublecolon - header);
        // if(flags & PARSEDNODE_IS_COMMENT){
            // This is wrong... uggh.
            // node->type = NODE_COMMENT;
        // }
        type = node->type;
    }
    int new_indent = ctx->nspaces;
    advance_row(ctx);
    int e = parse_node(ctx, new_node_handle, type, new_indent, flags);
    if(e) return e;
    return 0;
}

static
void
eat_leading_tabspaces(StringView* sv){
    size_t length = sv->length;
    if(!length) return;
    const char* text = sv->text;
    while(length){
        char first = text[0];
        if(first != ' ' && first != '\t')
            break;
        length--;
        text++;
    }
    sv->text = text;
    sv->length = length;
}

static inline
void
advance_sv(StringView* sv){
    assert(sv->length);
    sv->text++;
    sv->length--;
}

static
ErrorableNodeFlags
parse_post_colon(DndcContext* ctx, StringView postcolon, NodeHandle node_handle){
    ErrorableNodeFlags result = {0};
    Node* node = get_node(ctx, node_handle);
    size_t boundary = postcolon.length;
    for(size_t i = 0; i < postcolon.length;i++){
        switch(postcolon.text[i]){
            case CASE_a_z:
                continue;
            default:
                boundary = i;
                break;
        }
        break;
    }
    if(!boundary){
        parse_log_err(ctx, postcolon.text, LS("no node type found after '::'"));
        return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
    }
    for(size_t i = 0; i < arrlen(NODEALIASES); i++){
        if(NODEALIASES[i].name.length == boundary){
            if(memcmp(NODEALIASES[i].name.text, postcolon.text, boundary)==0){
                NodeType type = NODEALIASES[i].type;
                int err = 0;
                switch(type){
                    // case NODE_COMMENT:
                        // result.result |= NODEFLAG_COMMENT;
                        // break;
                    case NODE_JS:
                        err = Marray_push(NodeHandle)(&ctx->user_script_nodes, main_allocator(ctx), node_handle);
                        break;
                    case NODE_IMPORT:
                        // This is pushed later.
                        // err = Marray_push(NodeHandle)(&ctx->imports, main_allocator(ctx), node_handle);
                        result.result |= NODEFLAG_IMPORT;
                        break;
                    case NODE_STYLESHEETS:
                        err = Marray_push(NodeHandle)(&ctx->stylesheets_nodes, main_allocator(ctx), node_handle);
                        break;
                    case NODE_LINKS:
                        err = Marray_push(NodeHandle)(&ctx->link_nodes, main_allocator(ctx), node_handle);
                        break;
                    case NODE_SCRIPTS:
                        err = Marray_push(NodeHandle)(&ctx->script_nodes, main_allocator(ctx), node_handle);
                        break;
                    case NODE_META:
                        err = Marray_push(NodeHandle)(&ctx->meta_nodes, main_allocator(ctx), node_handle);
                        break;
                    case NODE_IMAGE:
                        err = Marray_push(NodeHandle)(&ctx->img_nodes, main_allocator(ctx), node_handle);
                        break;
                    case NODE_IMGLINKS:
                        err = Marray_push(NodeHandle)(&ctx->imglinks_nodes, main_allocator(ctx), node_handle);
                        break;
                    case NODE_TITLE:
                        ctx->titlenode = node_handle;
                        break;
                    case NODE_TOC:
                        ctx->tocnode = node_handle;
                        break;
                    default: break;
                }
                if(unlikely(err))
                    return (ErrorableNodeFlags){.errored=DNDC_ERROR_OOM};
                node->type = type;
                goto foundit;
            }
        }
    }
    parse_log_err_q(ctx, postcolon.text, SV("Unrecognized node name: "), (StringView){.text=postcolon.text,.length=boundary});
    return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
    foundit:;
    StringView aftertype = {.text=postcolon.text + boundary, .length=postcolon.length-boundary};
    for(;;){
        eat_leading_tabspaces(&aftertype);
        if(!aftertype.length)
            break;
        switch(aftertype.text[0]){
            case '#':{
                advance_sv(&aftertype);
                eat_leading_tabspaces(&aftertype);
                const char* directive_start = aftertype.text;
                while(aftertype.length){
                    char first = aftertype.text[0];
                    if(first == ' ' || first == '\t' || first == '@' || first == '.' || first == '#' || first == '(')
                        break;
                    advance_sv(&aftertype);
                }
                size_t directive_length = aftertype.text - directive_start;
                StringView directive = {.length = directive_length, .text = directive_start};
                if(!directive_length){
                    parse_log_err(ctx, aftertype.text, LS("Empty directive name after a '#'"));
                    return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
                }
                if(aftertype.length && aftertype.text[0] == '('){
                    if(SV_equals(directive, SV("id"))){
                        advance_sv(&aftertype);
                        eat_leading_tabspaces(&aftertype);
                        const char* argstart = aftertype.text;
                        size_t n_parens = 1;
                        while(aftertype.length){
                            if(aftertype.text[0] == ')')
                                n_parens--;
                            if(aftertype.text[0] == '(')
                                n_parens++;
                            advance_sv(&aftertype);
                            if(!n_parens)
                                break;
                        }
                        if(n_parens){
                            parse_log_err(ctx, aftertype.text, LS("End of line when expecting a closing ')'"));
                            return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
                        }
                        StringView contents = stripped_view(argstart, aftertype.text-1-argstart);
                        if(!contents.length){
                            parse_log_err(ctx, aftertype.text, LS("#id needs non-empty argument."));
                            return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
                        }
                        node_set_id(ctx, node_handle, contents);
                        result.result |= NODEFLAG_ID;
                        continue;
                    }
                    else {
                        parse_log_err(ctx, aftertype.text, LS("only #id takes an argument"));
                        return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
                    }
                }
                switch(*directive_start){
                    case 'i':
                        if(SV_equals(directive, SV("import"))){
                            result.result |= NODEFLAG_IMPORT;
                            continue;
                        }
                        break;
                    case 'n':
                        if(SV_equals(directive, SV("noid"))){
                            result.result |= NODEFLAG_NOID;
                            continue;
                        }
                        if(SV_equals(directive, SV("noinline"))){
                            result.result |= NODEFLAG_NOINLINE;
                            continue;
                        }
                        break;
                    case 'h':
                        if(SV_equals(directive, SV("hide"))){
                            result.result |= NODEFLAG_HIDE;
                            continue;
                        }
                        break;
                }
                parse_log_err(ctx, aftertype.text, LS("Unrecognized directive"));
                return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
            }break;
            case '.':{
                // classes are written verbatim, so we need to only allow valid characters
                advance_sv(&aftertype);
                eat_leading_tabspaces(&aftertype);
                const char* class_start = aftertype.text;
                while(aftertype.length){
                    char first = aftertype.text[0];
                    switch(first){
                        case CASE_a_z:
                        case CASE_A_Z:
                        case CASE_0_9:
                        case '-':
                            advance_sv(&aftertype);
                            continue;
                        case ' ': case '\t': case '@': case '.': case '#':
                            goto Break;
                        default:
                            parse_log_err_q(ctx, aftertype.text, SV("Illegal character when parsing a class: "), aftertype);
                            return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
                    }
                }
                Break:;
                size_t class_length = aftertype.text - class_start;
                if(!class_length){
                    parse_log_err(ctx, aftertype.text, LS("Empty class name after a '.'"));
                    return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
                }
                StringView class_ = {.length = class_length, .text = class_start};
                int err = Rarray_push(StringView)(&node->classes, main_allocator(ctx), class_);
                if(unlikely(err))
                    return (ErrorableNodeFlags){.errored=DNDC_ERROR_OOM};
            }break;
            case '@':{
                advance_sv(&aftertype);
                eat_leading_tabspaces(&aftertype);
                const char* attribute_start = aftertype.text;
                while(aftertype.length){
                    char first = aftertype.text[0];
                    if(first == ' ' || first == '\t' || first == '@' || first == '.' || first == '(' || first == '#')
                        break;
                    advance_sv(&aftertype);
                }
                size_t attribute_length = aftertype.text - attribute_start;
                if(!attribute_length){
                    parse_log_err(ctx, aftertype.text, LS("Empty attribute name after a '@'"));
                    return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
                }
                StringView attr_name = {
                    .text = attribute_start,
                    .length = attribute_length,
                };
                if(unlikely(ctx->flags & DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP)){
                    switch(attr_name.text[0]){
                        case 'i':
                            if(SV_equals(attr_name, SV("import"))){
                                parse_log_err(ctx, aftertype.text, LS("#import is the name of a directive."));
                                return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
                            }
                            if(SV_equals(attr_name, SV("id"))){
                                parse_log_err(ctx, aftertype.text, LS("#id is the name of a directive."));
                                return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
                            }
                            break;
                        case 'n':
                            if(SV_equals(attr_name, SV("noid"))){
                                parse_log_err(ctx, aftertype.text, LS("#noid is the name of a directive."));
                                return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
                            }
                            if(SV_equals(attr_name, SV("noinline"))){
                                parse_log_err(ctx, aftertype.text, LS("#noinline is the name of a directive."));
                                return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
                            }
                            break;
                        case 'h':
                            if(SV_equals(attr_name, SV("hide"))){
                                parse_log_err(ctx, aftertype.text, LS("#hide is the name of a directive."));
                                return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
                            }
                            break;
                        }
                }
                Attribute* attr; int err = Rarray_alloc(Attribute)(&node->attributes, main_allocator(ctx), &attr);
                if(unlikely(err)) return (ErrorableNodeFlags){.errored=DNDC_ERROR_OOM};
                attr->key = attr_name;
                attr->value = SV("");
                if(aftertype.length){
                    eat_leading_tabspaces(&aftertype);
                    if(aftertype.length && aftertype.text[0] == '('){
                        size_t n_parens = 1;
                        advance_sv(&aftertype);
                        const char* valstart = aftertype.text;
                        for(;;){
                            if(!aftertype.length){
                                parse_log_err(ctx, aftertype.text, LS("End of line when expecting a closing ')'"));
                                return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
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
                parse_log_err_q(ctx, aftertype.text, SV("illegal character when parsing type, classes and attributes: "), (StringView){.text=aftertype.text, .length=1});
                return (ErrorableNodeFlags){.errored=DNDC_ERROR_PARSE};
        }
    }
    if(result.result & NODEFLAG_IMPORT){
        int err = Marray_push(NodeHandle)(&ctx->imports, main_allocator(ctx), node_handle);
        if(unlikely(err))
            return (ErrorableNodeFlags){.errored=DNDC_ERROR_OOM};
    }
    node->flags = result.result;
    return result;
}

// generic parsing function
static
int
parse_node(DndcContext* ctx, NodeHandle parent_handle, NodeType parent_type, int indentation, NodeFlags flags){
    if(unlikely(indentation > 64)){
        HANDLE_LOG_ERROR(ctx, parent_handle, "Too deep! Indentation greater than 64 is unsupported.");
        return DNDC_ERROR_PARSE;
    }
    if(flags & NODEFLAG_IMPORT)
        goto regular_string_parsing;
    switch(parent_type){
        case NODE_META:
        case NODE_PRE:
        case NODE_RAW:
        case NODE_JS:
        case NODE_COMMENT:
            return parse_raw_node(ctx, parent_handle, indentation);
        case NODE_TABLE:
            return parse_table_node(ctx, parent_handle, indentation);
        case NODE_KEYVALUE:
            return parse_keyvalue_node(ctx, parent_handle, indentation);
        case NODE_DEFLIST:
        case NODE_DEF:
        case NODE_DETAILS:
        case NODE_MD:
            return parse_md_node(ctx, parent_handle, indentation);
        case NODE_SCRIPTS:
            return parse_raw_node(ctx, parent_handle, indentation);
        case NODE_IMGLINKS:
        case NODE_TOC:
        case NODE_LINKS:
        case NODE_IMPORT:
        case NODE_IMAGE:
        case NODE_DIV:
        case NODE_HEADING:
        case NODE_TITLE:
        case NODE_CONTAINER:
        case NODE_QUOTE:
            break; // do regular string parsing
        case NODE_STYLESHEETS:
            return parse_raw_node(ctx, parent_handle, indentation);
            break; // do regular string parsing
        case NODE_LIST_ITEM:
        case NODE_TABLE_ROW:
        case NODE_PARA:
        case NODE_STRING:
        case NODE_KEYVALUEPAIR:
        case NODE_INVALID:
        case NODE_BULLETS:
        case NODE_LIST:
            parse_log_err(ctx, ctx->linestart, LS("Invalid node to parse from"));
            return DNDC_ERROR_PARSE;
    }
    regular_string_parsing:;
    for(;ctx->cursor != ctx->end;){
        analyze_line(ctx);
        if(ctx->linestart+ctx->nspaces == ctx->line_end){
            advance_row(ctx);
            continue;
        }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            int e = parse_double_colon(ctx, parent_handle);
            if(e) return e;
            continue;
        }
        // default: string node
        StringView content = stripped_view(ctx->linestart + ctx->nspaces,
            (ctx->line_end - ctx->linestart)-ctx->nspaces);
        NodeHandle new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
        append_child(ctx, parent_handle, new_node_handle);
        advance_row(ctx);
    }
    return 0;
}
#if 0
PARSEFUNC(parse_list_node){
    {
        Node* parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_LIST);
    }
    for(;ctx->cursor != ctx->end;){
        analyze_line(ctx);
        // skip blanks
        if(ctx->linestart+ctx->nspaces == ctx->line_end){
            advance_row(ctx);
            continue;
        }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            // This looks weird, but we allow double colon nodes in the table
            // so that things like ::links and ::js nodes work properly.
            // Those will be removed at render time, so it's not invalid to
            // parse them.
            int e = parse_double_colon(ctx, parent_handle);
            if(e) return e;
            continue;
        }
        const char* firstchar = ctx->linestart + ctx->nspaces;
        for(;;firstchar++){
            switch(*firstchar){
                case CASE_0_9:
                    continue;
                case '.':
                    firstchar++;
                    goto after;
                default:
                    parse_log_err_q(ctx, firstchar, SV("Non numeric found when parsing list: "), (StringView){.text=firstchar, .length=1});
                    return DNDC_ERROR_PARSE;
            }
        }
        after:;
        NodeHandle li_handle = alloc_handle(ctx);
        init_node(ctx, li_handle, ctx->linestart+ctx->nspaces, NODE_LIST_ITEM);
        append_child(ctx, parent_handle, li_handle);
        StringView text = stripped_view(firstchar, ctx->line_end - firstchar);
        NodeHandle first_child = alloc_handle(ctx);
        init_string_node(ctx, first_child, text);
        advance_row(ctx);
        int e = parse_list_item(ctx, li_handle, ctx->nspaces);
        if(e) return e;
    }
    return 0;
}

PARSEFUNC(parse_list_item){
    {
        Node* parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_LIST_ITEM);
    }
    for(;ctx->cursor != ctx->end;){
        top:;
        analyze_line(ctx);
        if(ctx->linestart+ctx->nspaces == ctx->line_end){
            advance_row(ctx);
            continue;
        }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            parse_log_err(ctx, ctx->doublecolon, LS("This node type cannot contain subnodes, only strings"));
            return DNDC_ERROR_PARSE;
        }
        const char* firstchar = ctx->linestart + ctx->nspaces;
        for(;;firstchar++){
            switch(*firstchar){
                case CASE_0_9:
                    continue;
                case '.':{
                    NodeHandle new_handle = alloc_handle(ctx);
                    init_node(ctx, new_handle, ctx->linestart + ctx->nspaces, NODE_LIST);
                    append_child(ctx, parent_handle, new_handle);
                    int e = parse_list_node(ctx, new_handle, indentation);
                    if(e) return e;
                    goto top;
                }break;
                default:
                    goto after;
            }
        }
        after:;
        // default: string node
        StringView content = stripped_view(ctx->linestart + ctx->nspaces, (ctx->line_end - ctx->linestart)-ctx->nspaces);
        NodeHandle new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
        append_child(ctx, parent_handle, new_node_handle);
        advance_row(ctx);
    }
    return 0;
}
#endif
PARSEFUNC(parse_raw_node){
    // In order to avoid needing to scan all of the lines in the text
    // to figure out what the minimum leading indent is, we use the indent
    // of the first non-blank line. However, this can be greater than the indentation
    // of subsequent lines (which are indented less than what would cause us to
    // break out of this node). So for those, we'll pretend like they are indented at
    // the same level as our leading indent, which means their indentation will be
    // off in the output. This is really unusual though.
    bool have_leading_indent = false;
    int leading_indent = 0;
    for(;ctx->cursor != ctx->end;){
        analyze_line(ctx);
        if(!have_leading_indent && ctx->linestart+ctx->nspaces != ctx->line_end){
            leading_indent = ctx->nspaces;
            have_leading_indent = true;
        }
        size_t length;
        const char* text;
        if(ctx->linestart + ctx->nspaces != ctx->line_end){
            if(ctx->nspaces <= indentation)
                break;
            length = ctx->line_end - ctx->linestart;
            int effective_indent = leading_indent < ctx->nspaces?leading_indent: ctx->nspaces;
            length -= effective_indent;
            text = ctx->linestart + effective_indent;
        }
        else {
            int diff = ctx->nspaces - leading_indent;
            length = diff < 0 ? 0 : diff;
            text = ctx->linestart + ctx->nspaces - length;
        }
        // default: string node
        StringView content = rstripped_view(text, length);
        if(ctx->flags & DNDC_STRIP_WHITESPACE)
            content = lstripped_view(content.text, content.length);
        NodeHandle new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
        append_child(ctx, parent_handle, new_node_handle);
        advance_row(ctx);
    }
    return 0;
}

PARSEFUNC(parse_table_node){
    {
        Node* parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_TABLE);
    }
    NodeHandle last_cell_handle = INVALID_NODE_HANDLE;
    bool converted = false;
    int previous_row_indentation = indentation;
    for(;ctx->cursor != ctx->end;){
        analyze_line(ctx);
        // skip blanks
        if(ctx->linestart+ctx->nspaces == ctx->line_end){
            advance_row(ctx);
            continue;
        }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            // This looks weird, but we allow double colon nodes in the table
            // so that things like ::links and ::js nodes work properly.
            // Those will be removed at render time, so it's not invalid to
            // parse them.
            int e = parse_double_colon(ctx, parent_handle);
            if(e) return e;
            continue;
        }
        const char* cursor = ctx->linestart+ctx->nspaces;
        const char* pipe = memchr(cursor, '|', ctx->line_end - cursor);
        if(!pipe){
            if(!NodeHandle_eq(last_cell_handle, INVALID_NODE_HANDLE)){
                if(ctx->nspaces > previous_row_indentation){
                    StringView content = stripped_view(cursor, ctx->line_end-cursor);
                    if(content.length){
                        if(!converted){
                            convert_node_to_container_containing_clone_of_former_self(ctx, last_cell_handle);
                            converted = true;
                        }
                        NodeHandle str_handle = alloc_handle(ctx);
                        init_string_node(ctx, str_handle, content);
                        append_child(ctx, last_cell_handle, str_handle);
                    }
                    advance_row(ctx);
                    continue;
                }
            }
        }
        NodeHandle new_node_handle = alloc_handle(ctx);
        init_node(ctx, new_node_handle, ctx->linestart+ctx->nspaces, NODE_TABLE_ROW);
        append_child(ctx, parent_handle, new_node_handle);
        previous_row_indentation = ctx->nspaces;
        // last_cell_handle = INVALID_NODE_HANDLE;
        converted = false;
        while(pipe){
            NodeHandle cell_index = alloc_handle(ctx);
            size_t length = pipe - cursor;
            StringView content = stripped_view(cursor,length);
            init_string_node(ctx, cell_index, content);
            append_child(ctx, new_node_handle, cell_index);
            cursor = pipe+1;
            pipe = memchr(cursor, '|', ctx->line_end - cursor);
        }
        NodeHandle cell_index = alloc_handle(ctx);
        last_cell_handle = cell_index;
        StringView content = stripped_view(cursor, ctx->line_end-cursor);
        init_string_node(ctx, cell_index, content);
        append_child(ctx, new_node_handle, cell_index);
        advance_row(ctx);
    }
    return 0;
}
PARSEFUNC(parse_keyvalue_node){
    {
        Node* parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_KEYVALUE);
    }
    NodeHandle previous_value = INVALID_NODE_HANDLE;
    int previous_kv_indentation = indentation;
    bool previous_value_was_converted = false;
    for(;ctx->cursor != ctx->end;){
        analyze_line(ctx);
        // skip blanks
        if(ctx->linestart+ctx->nspaces == ctx->line_end){
            advance_row(ctx);
            continue;
        }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            int e = parse_double_colon(ctx, parent_handle);
            if(e) return e;
            continue;
        }
        if(! NodeHandle_eq(previous_value, INVALID_NODE_HANDLE)){
            if(ctx->nspaces > previous_kv_indentation){
                if(!previous_value_was_converted){
                    convert_node_to_container_containing_clone_of_former_self(ctx, previous_value);
                    previous_value_was_converted = true;
                }
                StringView content = stripped_view(ctx->linestart+ctx->nspaces, ctx->line_end-(ctx->linestart+ctx->nspaces));
                NodeHandle str_handle = alloc_handle(ctx);
                init_string_node(ctx, str_handle, content);
                append_child(ctx, previous_value, str_handle);
                advance_row(ctx);
                continue;
            }
        }
        NodeHandle new_node_handle = alloc_handle(ctx);
        init_node(ctx, new_node_handle, ctx->linestart + ctx->nspaces, NODE_KEYVALUEPAIR);
        append_child(ctx, parent_handle, new_node_handle);
        const char* cursor = ctx->linestart+ctx->nspaces;
        const char* colon = memchr(cursor, ':', ctx->line_end - cursor);
        if(!colon){
            parse_log_err(ctx, cursor, LS("Expected a colon for key value pairs"));
            return DNDC_ERROR_PARSE;
        }
        const char* pre_text = ctx->linestart+ctx->nspaces;

        StringView pre = stripped_view(pre_text,colon - pre_text);
        StringView post = stripped_view(colon+1, (ctx->line_end-colon)-1);
        NodeHandle key_idx = alloc_handle(ctx);
        init_string_node(ctx, key_idx, pre);
        NodeHandle val_idx = alloc_handle(ctx);
        init_string_node(ctx, val_idx, post);
        append_child(ctx, new_node_handle, key_idx);
        append_child(ctx, new_node_handle, val_idx);
        advance_row(ctx);
        previous_value = val_idx;
        previous_kv_indentation = ctx->nspaces;
        previous_value_was_converted = false;
    }
    return 0;
}

#if 0
PARSEFUNC(parse_bullets_node){
    {
        Node* parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_BULLETS);
    }
    for(;ctx->cursor != ctx->end;){
        analyze_line(ctx);
        // skip blanks
        if(ctx->linestart+ctx->nspaces == ctx->line_end){
            advance_row(ctx);
            continue;
        }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            // same comment as the table parser. Makes ::links and such work
            int e = parse_double_colon(ctx, parent_handle);
            if(e) return e;
            continue;
        }
        const char* firstchar = ctx->linestart+ctx->nspaces;
        char first = *firstchar;
        if(first != '*' && first != '+' && first != '-'){
            parse_log_err_q(ctx, firstchar, SV("Bullets must begin with one of *-+, got "), (StringView){.text=firstchar, .length=1});
            return DNDC_ERROR_PARSE;
        }
        firstchar++;
        StringView bullet_text = stripped_view(firstchar, ctx->line_end - firstchar);
        NodeHandle bullet_node_handle = alloc_handle(ctx);
        init_node(ctx, bullet_node_handle, ctx->linestart+ctx->nspaces, NODE_LIST_ITEM);
        append_child(ctx, parent_handle, bullet_node_handle);
        NodeHandle first_child_index = alloc_handle(ctx);
        init_string_node(ctx, first_child_index, bullet_text);
        append_child(ctx, bullet_node_handle, first_child_index);
        advance_row(ctx);
        int e = parse_bullet_node(ctx, bullet_node_handle, ctx->nspaces);
        if(e) return e;
    }
    return 0;
}

PARSEFUNC(parse_bullet_node){
    {
        Node* parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_LIST_ITEM);
    }
    for(;ctx->cursor != ctx->end;){
        analyze_line(ctx);
        if(ctx->linestart+ctx->nspaces == ctx->line_end){
            advance_row(ctx);
            continue;
        }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            parse_log_err(ctx, ctx->doublecolon,LS("This node type cannot contain subnodes, only strings"));
            return DNDC_ERROR_PARSE;
        }
        const char* firstchar = ctx->linestart + ctx->nspaces;
        char first = *firstchar;
        if(first == '*' || first == '+' || first == '-'){
            NodeHandle new_index = alloc_handle(ctx);
            init_node(ctx, new_index, firstchar, NODE_BULLETS);
            append_child(ctx, parent_handle, new_index);
            int e = parse_bullets_node(ctx, new_index, indentation);
            if(e) return e;
            continue;
        }
        // default: string node
        StringView content = stripped_view(ctx->linestart + ctx->nspaces, (ctx->line_end - ctx->linestart)-ctx->nspaces);
        NodeHandle new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
        append_child(ctx, parent_handle, new_node_handle);
        advance_row(ctx);
    }
    return 0;
}
#endif

PARSEFUNC(parse_md_node){
    // This was originally for debugging, but `dndc_parse` will set the
    // parse mode to NODE_MD, which means we get to this assertion, which is no
    // longer useful (will go off if parent is a DIV or whatever.  Now, it is
    // possible that I want a whitelist of what parent nodes are allowed, but
    // idk if that is worth limiting the power of scripts and we can just
    // accept sloppy trees when we output anyway. I think we properly error
    // instead of asserting in htmlgen.
    if(0){
        Node* parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_MD || parent->type == NODE_DETAILS || parent->type == NODE_DEF || parent->type == NODE_DEFLIST);
    }
    enum MDSTATE {
        NONE = 0,
        PARA = 1,
        BULLET = 2,
        LIST = 3,
    };
    enum MDSTATE state = NONE;
    struct StackItem {
        NodeHandle list;
        NodeHandle item;
        int indentation;
        enum MDSTATE state;
    } stack[8];
    int si = -1; // stack index
    NodeHandle para_handle = INVALID_NODE_HANDLE;
    int normal_indent = -1;
    for(;ctx->cursor != ctx->end;){
        analyze_line(ctx);
        // skip_blanks
        if(ctx->linestart+ctx->nspaces == ctx->line_end){
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
            si = -1;
            int e = parse_double_colon(ctx, parent_handle);
            if(e) return e;
            continue;
        }
        enum MDSTATE newstate = NONE;
        const char* firstchar = ctx->linestart + ctx->nspaces;
        int prefix_length = 0;
        switch(*firstchar){
            case '+':
            case '-':
            case '*':
                if(firstchar+1 != ctx->end && firstchar[1] == ' '){
                    prefix_length = 1;
                    newstate = BULLET;
                }
                else
                    newstate = PARA;
                goto after;
            case CASE_0_9:{
                prefix_length = 1;
                newstate = PARA;
                for(const char* c = firstchar+1;c != ctx->end;c++){
                    switch(*c){
                        case CASE_0_9:
                            prefix_length++;
                            continue;
                        case '.':
                            prefix_length++;
                            newstate = LIST;
                            goto after;
                        default:
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
        if(newstate == BULLET || newstate == LIST){
            if(si == -1){
                si = 0;
                struct StackItem* s = &stack[si];
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
                        parse_log_err(ctx, ctx->linestart+ctx->nspaces, LS("Only up to 8 levels of nested lists are supported."));
                        return DNDC_ERROR_PARSE;
                    }
                    struct StackItem* s = &stack[si];
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
                    struct StackItem* s = &stack[si];
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
                        if(si < 0){
                            parse_log_err(ctx, ctx->linestart+ctx->nspaces, LS("Dedent does not match initial indent"));
                            return DNDC_ERROR_PARSE;
                        }
                        assert(si >= 0);
                        int indent = stack[si].indentation;
                        if(indent > ctx->nspaces)
                            continue;
                        if(indent == ctx->nspaces)
                            break;
                        if(indent < ctx->nspaces){
                            parse_log_err(ctx, ctx->linestart+ctx->nspaces, LS("Ambiguous dedent inside a list"));
                            return DNDC_ERROR_PARSE;
                        }
                    }
                    struct StackItem* s = &stack[si];
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
            struct StackItem* s = &stack[si];
            s->item = alloc_handle(ctx);
            init_node(ctx, s->item, ctx->linestart+ctx->nspaces, NODE_LIST_ITEM);
            append_child(ctx, s->list, s->item);
            StringView content = stripped_view(ctx->linestart + ctx->nspaces+prefix_length, (ctx->line_end - ctx->linestart)-ctx->nspaces-prefix_length);
            NodeHandle new_node_handle = alloc_handle(ctx);
            init_string_node(ctx, new_node_handle, content);
            append_child(ctx, s->item, new_node_handle);
            advance_row(ctx);
            state = newstate;
            continue;
        }
        assert(newstate == PARA);
        if(state == PARA || state == NONE || ctx->nspaces == normal_indent){
            if(state != PARA){
                para_handle = alloc_handle(ctx);
                init_node(ctx, para_handle, ctx->linestart+ctx->nspaces, NODE_PARA);
                append_child(ctx, parent_handle, para_handle);
            }
            StringView content = stripped_view( ctx->linestart + ctx->nspaces, (ctx->line_end - ctx->linestart)-ctx->nspaces);
            NodeHandle new_node_handle = alloc_handle(ctx);
            init_string_node(ctx, new_node_handle, content);
            append_child(ctx, para_handle, new_node_handle);
            advance_row(ctx);
            si = -1;
            state = newstate;
            continue;
        }
        if(ctx->nspaces <= stack[si].indentation){
            parse_log_err(ctx, ctx->linestart+ctx->nspaces, LS("Ambiguous dedent inside a list"));
            return DNDC_ERROR_PARSE;
        }
        // don't change state for these
        StringView content = stripped_view(ctx->linestart + ctx->nspaces, (ctx->line_end - ctx->linestart)-ctx->nspaces);
        NodeHandle new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
        append_child(ctx, stack[si].item, new_node_handle);
        advance_row(ctx);
        continue;
    }
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
