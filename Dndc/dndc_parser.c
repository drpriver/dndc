#ifndef DNDC_PARSER_C
#define DNDC_PARSER_C
#include "dndc_funcs.h"
#include "dndc_types.h"
#include "str_util.h"

#ifdef __x86_64__
#include <immintrin.h>
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef enum ParsedNodeFlags {
    PARSEDNODE_NONE = 0,
    PARSEDNODE_IS_IMPORT = 0x1,
    PARSEDNODE_IS_COMMENT = 0x2,
} ParsedNodeFlags;

struct ErrorableParsedNodeFlags{
    ParsedNodeFlags result;
    int errored;
};

static
struct ErrorableParsedNodeFlags
parse_post_colon(DndcContext* ctx, StringView postcolon, NodeHandle node_handle);

static
void
analyze_line(DndcContext*);
static
void
advance_row(DndcContext*);

#define PARSEFUNC(name) static Errorable_f(void) name(DndcContext* ctx, NodeHandle parent_handle, int indentation)
static
Errorable_f(void)
parse_node(DndcContext* ctx, NodeHandle parent_handle, NodeType parent_type, int indentation, ParsedNodeFlags flags);
PARSEFUNC(parse_text_node);
PARSEFUNC(parse_table_node);
PARSEFUNC(parse_keyvalue_node);
PARSEFUNC(parse_bullets_node);
PARSEFUNC(parse_bullet_node);
PARSEFUNC(parse_raw_node);
PARSEFUNC(parse_list_node);
PARSEFUNC(parse_list_item);
PARSEFUNC(parse_md_node);

#if defined(__ARM_NEON)
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
#if 1 && defined(__x86_64__)
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
        int n = __builtin_ctz(~mask);
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
#if 1 && defined(__ARM_NEON)
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
        int n = __builtin_ctz(~mask);
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
#if 1 && defined(__x86_64__)
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
            unsigned endoff = __builtin_ctz(end);
            unsigned colonoff = -1;
            if(colon0){
                unsigned off = __builtin_ctz(colon0);
                if(off < endoff && off < colonoff)
                    colonoff = off;
            }
            if(colon1){
                unsigned off = __builtin_ctz(colon1)+1;
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
                unsigned off = __builtin_ctz(colon0);
                colonoff = off;
            }
            if(colon1){
                unsigned off = __builtin_ctz(colon1)+1;
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
#if 1 && defined(__ARM_NEON)
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
            unsigned endoff = __builtin_ctz(end);
            unsigned colonoff = -1;
            if(colon0){
                unsigned off = __builtin_ctz(colon0);
                if(off < endoff && off < colonoff)
                    colonoff = off;
            }
            if(colon1){
                unsigned off = __builtin_ctz(colon1)+1;
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
                unsigned off = __builtin_ctz(colon0);
                colonoff = off;
            }
            if(colon1){
                unsigned off = __builtin_ctz(colon1)+1;
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
#if 1 && defined(__x86_64__)
    while(length >= 16){
        __m128i data    = _mm_loadu_si128((const __m128i*)(cursor));
        __m128i testnl  = _mm_cmpeq_epi8(data, newline);
        __m128i testzed = _mm_cmpeq_epi8(data, zed);
        __m128i testend = _mm_or_si128(testnl, testzed);
        unsigned end = _mm_movemask_epi8(testend);
        if(end){
            unsigned endoff = __builtin_ctz(end);
            endline = cursor + endoff;
            goto Lfinish;
        }
        cursor += 16;
        length -= 16;
    }
#endif
#if 1 && defined(__ARM_NEON)
    while(length >= 16){
        uint8x16_t data    = vld1q_u8((const unsigned char*)cursor);
        uint8x16_t testnl  = vceqq_u8(data, newline);
        uint8x16_t testzed = vceqq_u8(data, zed);
        uint8x16_t testend = vorrq_u8(testnl, testzed);
        unsigned end       = _mm_movemask_aarch64(testend);
        if(end){
            unsigned endoff = __builtin_ctz(end);
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
    auto node = get_node(ctx, handle);
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
    auto node = get_node(ctx, handle);
    int col = (int)(sv.text - ctx->linestart);
    node->col = col;
    node->filename_idx = ctx->filenames.count-1;
    node->row = ctx->lineno;
    node->type = NODE_STRING;
    node->header = sv;
}

static
Errorable_f(void)
dndc_parse(DndcContext* ctx, NodeHandle root_handle, StringView filename, const char* text, size_t length){
    Errorable(void) result = {};
    ctx->cursor = text;
    ctx->end = text + length;
    ctx->linestart = NULL;
    ctx->doublecolon = NULL;
    ctx->line_end = NULL;
    ctx->nspaces = 0;
    ctx->lineno = 0;
    ctx->filename = filename;
    Marray_push(StringView)(&ctx->filenames, ctx->allocator, filename);
    NodeType type = get_node(ctx, root_handle)->type;
    auto e = parse_node(ctx, root_handle, type, -1, 0);
    if(e.errored) return e;
    return result;
}

static
Errorable_f(void)
parse_double_colon(DndcContext* ctx, NodeHandle parent_handle){
    Errorable(void) result = {};
    // parse the node header
    const char* starttext = ctx->doublecolon + 2;
    size_t length = ctx->line_end - starttext;
    StringView postcolon = stripped_view(starttext, length);
    auto new_node_handle = alloc_handle(ctx);
    init_node(ctx, new_node_handle, ctx->linestart+ctx->nspaces, NODE_INVALID);
    ParsedNodeFlags flags;
    {
        auto e = parse_post_colon(ctx, postcolon, new_node_handle);
        if(e.errored){
            result.errored = e.errored;
            return result;
        }
        flags = e.result;
    }
    append_child(ctx, parent_handle, new_node_handle);
    NodeType type;
    {
        auto node = get_node(ctx, new_node_handle);
        const char* header = ctx->linestart + ctx->nspaces;
        node->header = stripped_view(header, ctx->doublecolon - header);
        // if(flags & PARSEDNODE_IS_COMMENT){
            // This is wrong... uggh.
            // node->type = NODE_COMMENT;
        // }
        type = node->type;
    }
    auto new_indent = ctx->nspaces;
    advance_row(ctx);
    auto e = parse_node(ctx, new_node_handle, type, new_indent, flags);
    if(e.errored) return e;
    return result;
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
struct ErrorableParsedNodeFlags
parse_post_colon(DndcContext* ctx, StringView postcolon, NodeHandle node_handle){
    struct ErrorableParsedNodeFlags result = {0};
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
        parse_set_err(ctx, postcolon.text, LS("no node type found after '::'"));
        Raise(PARSE_ERROR);
    }
    for(size_t i = 0; i < arrlen(NODEALIASES); i++){
        if(NODEALIASES[i].name.length == boundary){
            if(memcmp(NODEALIASES[i].name.text, postcolon.text, boundary)==0){
                auto type = NODEALIASES[i].type;
                switch(type){
                    case NODE_COMMENT:
                        result.result |= PARSEDNODE_IS_COMMENT;
                        break;
                    case NODE_JS:
                        Marray_push(NodeHandle)(&ctx->user_script_nodes, ctx->allocator, node_handle);
                        break;
                    case NODE_IMPORT:
                        // This is pushed later.
                        // Marray_push(NodeHandle)(&ctx->imports, ctx->allocator, node_handle);
                        result.result |= PARSEDNODE_IS_IMPORT;
                        break;
                    case NODE_STYLESHEETS:
                        Marray_push(NodeHandle)(&ctx->stylesheets_nodes, ctx->allocator, node_handle);
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
    parse_set_err_q(ctx, postcolon.text, SV("Unrecognized node name: "), (StringView){.text=postcolon.text,.length=boundary});
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
                    parse_set_err(ctx, aftertype.text, LS("Empty class name after a '.'"));
                    Raise(PARSE_ERROR);
                }
                StringView class_ = {.length = class_length, .text = class_start};
                node->classes = Rarray_push(StringView)(node->classes, ctx->allocator, class_);
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
                    parse_set_err(ctx, aftertype.text, LS("Empty attribute name after a '@'"));
                    Raise(PARSE_ERROR);
                }
                auto attr = Rarray_alloc(Attribute)(&node->attributes, ctx->allocator);
                attr->key.length = attribute_length;
                attr->key.text = attribute_start;
                if(SV_equals(attr->key, SV("import")))
                    result.result |= PARSEDNODE_IS_IMPORT;
                if(SV_equals(attr->key, SV("comment")))
                    result.result |= PARSEDNODE_IS_COMMENT;
                attr->value = SV("");
                if(aftertype.length){
                    eat_leading_tabspaces(&aftertype);
                    if(aftertype.length and aftertype.text[0] == '('){
                        size_t n_parens = 1;
                        advance_sv(&aftertype);
                        const char* valstart = aftertype.text;
                        for(;;){
                            if(!aftertype.length){
                                parse_set_err(ctx, aftertype.text, LS("End of line when expecting a closing ')'"));
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
                parse_set_err_q(ctx, aftertype.text, SV("illegal character when parsing type, classes and attributes: "), (StringView){.text=aftertype.text, .length=1});
                Raise(PARSE_ERROR);
        }
    }
    if(result.result & PARSEDNODE_IS_IMPORT){
        Marray_push(NodeHandle)(&ctx->imports, ctx->allocator, node_handle);
    }
    return result;
}

// generic parsing function
static
Errorable_f(void)
parse_node(DndcContext* ctx, NodeHandle parent_handle, NodeType parent_type, int indentation, ParsedNodeFlags flags){
    if(unlikely(indentation > 64)){
        node_set_err(ctx, get_node(ctx, parent_handle), LS("Too deep! Indentation greater than 64 is unsupported."));
        return (Errorable(void)){.errored=PARSE_ERROR};
    }
    if(flags & PARSEDNODE_IS_COMMENT)
        return parse_raw_node(ctx, parent_handle, indentation);
    if(flags & PARSEDNODE_IS_IMPORT)
        goto regular_string_parsing;
    switch(parent_type){
        case NODE_PRE:
        case NODE_RAW:
        case NODE_JS:
        case NODE_COMMENT:
            return parse_raw_node(ctx, parent_handle, indentation);
        case NODE_TABLE:
            return parse_table_node(ctx, parent_handle, indentation);
        case NODE_KEYVALUE:
            return parse_keyvalue_node(ctx, parent_handle, indentation);
        case NODE_DETAILS:
        case NODE_MD:
            return parse_md_node(ctx, parent_handle, indentation);
        case NODE_SCRIPTS:
            return parse_raw_node(ctx, parent_handle, indentation);
        case NODE_IMGLINKS:
        case NODE_DATA:
        case NODE_NAV:
        case NODE_LINKS:
        case NODE_IMPORT:
        case NODE_IMAGE:
        case NODE_DIV:
        case NODE_HEADING:
        case NODE_TITLE:
        case NODE_CONTAINER:
        case NODE_QUOTE:
        case NODE_HR:
            break; // do regular string parsing
        case NODE_STYLESHEETS:
            return parse_raw_node(ctx, parent_handle, indentation);
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
    regular_string_parsing:;
    Errorable(void) result = {};
    for(;ctx->cursor != ctx->end;){
        analyze_line(ctx);
        if(ctx->linestart+ctx->nspaces == ctx->line_end){
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
            (ctx->line_end - ctx->linestart)-ctx->nspaces);
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
                default:
                    parse_set_err_q(ctx, firstchar, SV("Non numeric found when parsing list: "), (StringView){.text=firstchar, .length=1});
                    Raise(PARSE_ERROR);
            }
        }
        after:;
        auto li_handle = alloc_handle(ctx);
        init_node(ctx, li_handle, ctx->linestart+ctx->nspaces, NODE_LIST_ITEM);
        append_child(ctx, parent_handle, li_handle);
        StringView text = stripped_view(firstchar, ctx->line_end - firstchar);
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
            parse_set_err(ctx, ctx->doublecolon, LS("This node type cannot contain subnodes, only strings"));
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
        StringView content = stripped_view(ctx->linestart + ctx->nspaces, (ctx->line_end - ctx->linestart)-ctx->nspaces);
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
    // off in the output. This is really unusual though.
    bool have_leading_indent = false;
    int leading_indent = 0;
    for(;ctx->cursor != ctx->end;){
        analyze_line(ctx);
        if(!have_leading_indent and ctx->linestart+ctx->nspaces != ctx->line_end){
            leading_indent = ctx->nspaces;
            have_leading_indent = true;
        }
        size_t length;
        const char* text;
        if(ctx->linestart + ctx->nspaces != ctx->line_end){
            if(ctx->nspaces <= indentation)
                break;
            length = ctx->line_end - ctx->linestart;
            auto effective_indent = leading_indent < ctx->nspaces?leading_indent: ctx->nspaces;
            length -= effective_indent;
            text = ctx->linestart + effective_indent;
        }
        else {
            int diff = ctx->nspaces - leading_indent;
            length = diff < 0 ? 0 : diff;
            text = ctx->linestart + ctx->nspaces - length;
        }
        // default: string node
        auto content = rstripped_view(text, length);
        if(ctx->flags & DNDC_STRIP_WHITESPACE)
            content = lstripped_view(content.text, content.length);
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
    int previous_row_indentation = indentation;
    Errorable(void) result = {};
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
            auto e = parse_double_colon(ctx, parent_handle);
            if(e.errored) return e;
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
                        auto str_handle = alloc_handle(ctx);
                        init_string_node(ctx, str_handle, content);
                        append_child(ctx, last_cell_handle, str_handle);
                    }
                    advance_row(ctx);
                    continue;
                }
            }
        }
        auto new_node_handle = alloc_handle(ctx);
        init_node(ctx, new_node_handle, ctx->linestart+ctx->nspaces, NODE_TABLE_ROW);
        append_child(ctx, parent_handle, new_node_handle);
        previous_row_indentation = ctx->nspaces;
        // last_cell_handle = INVALID_NODE_HANDLE;
        converted = false;
        while(pipe){
            auto cell_index = alloc_handle(ctx);
            size_t length = pipe - cursor;
            StringView content = stripped_view(cursor,length);
            init_string_node(ctx, cell_index, content);
            append_child(ctx, new_node_handle, cell_index);
            cursor = pipe+1;
            pipe = memchr(cursor, '|', ctx->line_end - cursor);
        }
        auto cell_index = alloc_handle(ctx);
        last_cell_handle = cell_index;
        StringView content = stripped_view(cursor, ctx->line_end-cursor);
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
            auto e = parse_double_colon(ctx, parent_handle);
            if(e.errored) return e;
            continue;
        }
        if(not NodeHandle_eq(previous_value, INVALID_NODE_HANDLE)){
            if(ctx->nspaces > previous_kv_indentation){
                if(!previous_value_was_converted){
                    convert_node_to_container_containing_clone_of_former_self(ctx, previous_value);
                    previous_value_was_converted = true;
                }
                StringView content = stripped_view(ctx->linestart+ctx->nspaces, ctx->line_end-(ctx->linestart+ctx->nspaces));
                auto str_handle = alloc_handle(ctx);
                init_string_node(ctx, str_handle, content);
                append_child(ctx, previous_value, str_handle);
                advance_row(ctx);
                continue;
            }
        }
        auto new_node_handle = alloc_handle(ctx);
        init_node(ctx, new_node_handle, ctx->linestart + ctx->nspaces, NODE_KEYVALUEPAIR);
        append_child(ctx, parent_handle, new_node_handle);
        const char* cursor = ctx->linestart+ctx->nspaces;
        const char* colon = memchr(cursor, ':', ctx->line_end - cursor);
        if(!colon){
            parse_set_err(ctx, cursor, LS("Expected a colon for key value pairs"));
            Raise(PARSE_ERROR);
        }
        const char* pre_text = ctx->linestart+ctx->nspaces;

        StringView pre = stripped_view(pre_text,colon - pre_text);
        StringView post = stripped_view(colon+1, (ctx->line_end-colon)-1);
        auto key_idx = alloc_handle(ctx);
        init_string_node(ctx, key_idx, pre);
        auto val_idx = alloc_handle(ctx);
        init_string_node(ctx, val_idx, post);
        append_child(ctx, new_node_handle, key_idx);
        append_child(ctx, new_node_handle, val_idx);
        advance_row(ctx);
        previous_value = val_idx;
        previous_kv_indentation = ctx->nspaces;
        previous_value_was_converted = false;
    }
    return result;
}

PARSEFUNC(parse_bullets_node){
    Errorable(void) result = {};
    {
        auto parent = get_node(ctx, parent_handle);
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
            auto e = parse_double_colon(ctx, parent_handle);
            if(e.errored) return e;
            continue;
        }
        const char* firstchar = ctx->linestart+ctx->nspaces;
        char first = *firstchar;
        if(first != '*' and first != '+' and first != '-'){
            parse_set_err_q(ctx, firstchar, SV("Bullets must begin with one of *-+, got "), (StringView){.text=firstchar, .length=1});
            Raise(PARSE_ERROR);
        }
        firstchar++;
        StringView bullet_text = stripped_view(firstchar, ctx->line_end - firstchar);
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
    for(;ctx->cursor != ctx->end;){
        analyze_line(ctx);
        if(ctx->linestart+ctx->nspaces == ctx->line_end){
            advance_row(ctx);
            continue;
        }
        if(ctx->nspaces <= indentation)
            break;
        if(ctx->doublecolon){
            parse_set_err(ctx, ctx->doublecolon,LS("This node type cannot contain subnodes, only strings"));
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
        StringView content = stripped_view(ctx->linestart + ctx->nspaces, (ctx->line_end - ctx->linestart)-ctx->nspaces);
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
    for(;ctx->cursor != ctx->end;){
        analyze_line(ctx);
        // blank line
        if(ctx->linestart+ctx->nspaces == ctx->line_end){
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
        StringView content = stripped_view(ctx->linestart+ctx->nspaces, (ctx->line_end - ctx->linestart)-ctx->nspaces);
        auto new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
        append_child(ctx, para_handle, new_node_handle);
        advance_row(ctx);
    }
    return result;
}
PARSEFUNC(parse_md_node){
    // This was originally for debugging, but `dndc_parse` will parse set the
    // parse mode to NODE_MD, which means we get to this assertion, which is no
    // longer useful (will go off if parent is a DIV or whatever.  Now, it is
    // possible that I want a whitelist of what paret nodes are allowed, but
    // idk if that is worth limiting the power of scripts and we can just
    // accept sloppy trees when we output anyway. I think we properly error
    // instead of asserting in htmlgen.
    if(0){
        auto parent = get_node(ctx, parent_handle);
        assert(parent->type == NODE_MD || parent->type == NODE_DETAILS);
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
    int si = -1; // stack index
    NodeHandle para_handle = INVALID_NODE_HANDLE;
    int normal_indent = -1;
    Errorable(void) result = {};
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
                if(firstchar+1 != ctx->end && firstchar[1] == ' '){
                    prefix_length = 1;
                    newstate = BULLET;
                }
                else
                    newstate = PARA;
                goto after;
            case '0' ... '9':{
                prefix_length = 1;
                newstate = PARA;
                for(const char* c = firstchar+1;c != ctx->end;c++){
                    switch(*c){
                        case '0' ... '9':
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
                        parse_set_err(ctx, ctx->linestart+ctx->nspaces, LS("Only up to 8 levels of nested lists are supported."));
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
                        if(si < 0){
                            parse_set_err(ctx, ctx->linestart+ctx->nspaces, LS("Dedent does not match initial indent"));
                            Raise(PARSE_ERROR);
                        }
                        assert(si >= 0);
                        auto indent = stack[si].indentation;
                        if(indent > ctx->nspaces)
                            continue;
                        if(indent == ctx->nspaces)
                            break;
                        if(indent < ctx->nspaces){
                            parse_set_err(ctx, ctx->linestart+ctx->nspaces, LS("Ambiguous dedent inside a list"));
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
            StringView content = stripped_view(ctx->linestart + ctx->nspaces+prefix_length, (ctx->line_end - ctx->linestart)-ctx->nspaces-prefix_length);
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
            StringView content = stripped_view( ctx->linestart + ctx->nspaces, (ctx->line_end - ctx->linestart)-ctx->nspaces);
            auto new_node_handle = alloc_handle(ctx);
            init_string_node(ctx, new_node_handle, content);
            append_child(ctx, para_handle, new_node_handle);
            advance_row(ctx);
            si = -1;
            state = newstate;
            continue;
        }
        if(ctx->nspaces <= stack[si].indentation){
            parse_set_err(ctx, ctx->linestart+ctx->nspaces, LS("Ambiguous dedent inside a list"));
            Raise(PARSE_ERROR);
        }
        // don't change state for these
        StringView content = stripped_view(ctx->linestart + ctx->nspaces, (ctx->line_end - ctx->linestart)-ctx->nspaces);
        auto new_node_handle = alloc_handle(ctx);
        init_string_node(ctx, new_node_handle, content);
        append_child(ctx, stack[si].item, new_node_handle);
        advance_row(ctx);
        continue;
    }
    return result;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
