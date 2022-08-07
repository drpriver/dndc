//
// Copyright © 2021-2022, David Priver
//
#ifndef MSB_EXTENSIONS_H
#define MSB_EXTENSIONS_H
#include "MStringBuilder.h"
#include "str_util.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

//
// These functions are not universal enough to go in the main MStringBuilder
// header, but require access to internal functions and state for efficiency
// purposes. Thus we prefix them with msb_, but define them here.
// One of the upsides of no method-call syntax is extension methods like this
// look identical to methods in the data type's definition.
// Whether that's worth not getting to do sb.write_str("foo", 3); is
// left as an exercise to the reader.
//

// It'd be nicer to use gnu-case ranges, but this is where we're at.
#ifndef CASE_0_9
#define CASE_0_9 '0': case '1': case '2': case '3': case '4': case '5': \
    case '6': case '7': case '8': case '9'
#endif

#ifndef CASE_a_z
#define CASE_a_z 'a': case 'b': case 'c': case 'd': case 'e': case 'f': \
    case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': \
    case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't': \
    case 'u': case 'v': case 'w': case 'x': case 'y': case 'z'
#endif

#ifndef CASE_A_Z
#define CASE_A_Z 'A': case 'B': case 'C': case 'D': case 'E': case 'F': \
    case 'G': case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': \
    case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': \
    case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z'
#endif


//
// Writes the string into the builder, but kebabs the string.
// "Kebabs" the string, turning into something usable as an html id.
// Preserves digits, lowercases alphabetical characters and turns
// gaps between alphanumeric into a single hyphen (thus the kebab).
//
// For example (quotes are just for clarity, they are not included):
//  "My wonderful cat, Lucy" -> "my-wonderful-cat-lucy"
//  "123, North Elm St." -> "123-north-elm-st"
//
static inline
void
msb_write_kebab(MStringBuilder* msb, const char* text, size_t length){
    // SPEED: Measurements show that this is ran on a lot of nodes as we need
    // to generate ids for every md node with a header for example.
    //
    // On the benchmark document, the following stats were collected for the
    // length of the strings to be kebabed:
    //
    //      records:  1557
    //      sum:     25874
    //      avg:        16.61
    //      std:         7.19
    //      med:        19
    //      max:        45
    //      min:         3
    //
    // Note that the median length is greater than 15. This means that if I can
    // figure out a simd version of this algorithm we can do it in 16 byte
    // chunks and just chew through that data.
    //
    // Currently we only lowercase letters, but I'm ok with turning a raw 17 into
    // an ascii '1'. Those are unprintable characters anyway.  Could also mask
    // out the unprintables in the simd version.

    int err = msb_ensure_additional(msb, length+2);
    if(unlikely(err)) return;
    char* data = msb->data;
    size_t cursor = msb->cursor;
    // A bit of explanation is in order.
    //
    // Essentially, when a dash is called for, we first check if we have
    // already written one into the buffer at the current location. If we have,
    // then we advance the cursor by one. If we have not, then we don't advance
    // the cursor.
    //
    // Now, that sounds weird, but notice that dashes only trail after
    // characters, they are never at the beginning of the string. So, we just
    // always write the character + a dash, but we don't advance the cursor for
    // that dash until we actually need one.
    //
    // We need this sentinel not for it's value, but rather so that we will
    // have a value past the dash that is not a dash, or at the beginning of
    // the string. Without writing it, we would read uninitialized memory.
#define BEGINSENTINEL '@'
#define ENDSENTINEL '$'
    data[cursor] = BEGINSENTINEL;
    for(size_t i = 0; i < length; i++){
        char c = text[i];
        switch(c){
            case CASE_A_Z:
                c |= 0x20; // tolower
                // fall-through
            case CASE_a_z:
            case CASE_0_9:
                data[cursor++] = c;
                data[cursor] = '-';
                data[cursor+1] = ENDSENTINEL;
                continue;
            case ' ': case '\t': case '-': case '/':{ // this slash is kind of sus
                int is_dash = data[cursor] == '-';
                cursor += is_dash;
                continue;
            }
            default:
                continue;
        }
    }
    cursor -= (data[cursor] == ENDSENTINEL);
#undef BEGINSENTINEL
#undef ENDSENTINEL
    msb->cursor = cursor;
    return;
}

//
// Writes the string into the builder, but title-cases the string.
// Capitalizes the first letter of each word (even articles like "an" unfortunately).
// For example:
//  "this is some text." -> "This Is Some Text."
static inline
void
msb_write_title(MStringBuilder* restrict msb, const char* restrict str, size_t len){
    if(!len)
        return;
    int err = _check_msb_remaining_size(msb, len);
    if(unlikely(err)) return;
    bool wants_cap = true;
    for(size_t i = 0; i < len; i++){
        char c = str[i];
        switch(c){
            case CASE_a_z:
                if(wants_cap){
                    c &= ~0x20; // toupper
                    wants_cap = false;
                }
                break;
            case CASE_A_Z:
                wants_cap = false;
                break;
            default:
                wants_cap = true;
                break;
        }
        msb->data[msb->cursor++] = c;
    }
}

//
// Writes the given string into the builder, escaping those characters required
// by json.
// Does not include the containing quotation marks. Write those yourself if you
// need them.
static inline
void
msb_write_json_escaped_str(MStringBuilder* restrict sb, const char* restrict str, size_t length){
    size_t datalength = length*2;
    int err = _check_msb_remaining_size(sb, datalength);
    if(unlikely(err)) return;
    char* data = sb->data;
    size_t cursor = sb->cursor;
    const char* const hex = "0123456789abcdef";
    for(size_t i = 0; i < length; i++){
        switch(str[i]){
            // 0x0 through 0x1f (0 through 31) have to all be
            // escaped with the 6 character sequence of \u00xx
            // Why on god's green earth did they force utf16 escapes?

            // gnu case ranges are nicer, but nonstandard
            // just spell them all out.
            case  0: case  1: case  2: case  3: case  4:
            // 8 is '\b', 9 is '\t'
            case  5: case  6: case  7:
            // 10 is '\n', 12 is '\f', 13 is '\r'
                     case 11:                   case 14:
            case 15: case 16: case 17: case 18: case 19:
            case 20: case 21: case 22: case 23: case 24:
            case 25: case 26: case 27: case 28: case 29:
            case 30: case 31:
                // These are rare, so only reserve more space when we actually hit them.
                datalength += 4;
                err = _check_msb_remaining_size(sb, datalength);
                if(unlikely(err)) return;
                // re-acquire the invalidated pointer.
                data = sb->data;
                data[cursor++] = '\\';
                data[cursor++] = 'u';
                data[cursor++] = '0';
                data[cursor++] = '0';
                data[cursor++] = hex[(str[i] & 0xf0)>>4];
                data[cursor++] = hex[(str[i] & 0xf)];
                break;
            case '"':
                data[cursor++] = '\\';
                data[cursor++] = '"';
                break;
            case '\\':
                data[cursor++] = '\\';
                data[cursor++] = '\\';
                break;
            case '\b':
                data[cursor++] = '\\';
                data[cursor++] = 'b';
                break;
            case '\f':
                data[cursor++] = '\\';
                data[cursor++] = 'f';
                break;
            case '\n':
                data[cursor++] = '\\';
                data[cursor++] = 'n';
                break;
            case '\r':
                data[cursor++] = '\\';
                data[cursor++] = 'r';
                break;
            case '\t':
                data[cursor++] = '\\';
                data[cursor++] = 't';
                break;
            // Other characters are allowed through as is
            default:
                data[cursor++] = str[i];
                break;
        }
    }
    sb->cursor = cursor;
}

static inline
void
msb_write_str_with_backslashes_as_forward_slashes(MStringBuilder* sb, const char* restrict str, size_t length){
    int err = _check_msb_remaining_size(sb, length);
    if(unlikely(err)) return;
    char* data = sb->data;
    size_t cursor = sb->cursor;
    for(size_t i = 0; i < length; i++){
        char c = str[i];
        if(c == '\\')
            c = '/';
        data[cursor++] = c;
    }
    sb->cursor = cursor;
}

static inline
void
msb_write_html_quote_escaped_string(MStringBuilder*sb, const char* restrict str, size_t length){
    int err = _check_msb_remaining_size(sb, length);
    if(unlikely(err)) return;
    for(;;){
        const char* quote = memchr(str, '"', length);
        if(!quote)
            break;
        msb_write_str(sb, str, quote-str);
        msb_write_literal(sb, "&quot;");
        length -= quote + 1 - str;
        str = quote+1;
    }
    msb_write_str(sb, str, length);
}

//
// Writes the string into the buffer, but strips trailing and leading
// whitespace from each line in the input. A newline is still written for each
// line and one will be added to the end if the string is missing a trailing
// newline.
static inline
void
msb_write_stripped_lines(MStringBuilder* sb, const char* restrict str, size_t length){
    int err = _check_msb_remaining_size(sb, length);
    if(unlikely(err)) return;
    char* data = sb->data;
    size_t cursor = sb->cursor;
    const char* remainder = str;
    const char* end = str + length;
    for(;remainder != end;){
        const char* endline = memchr(remainder, '\n', end - remainder);
        if(endline){
            StringView stripped = stripped_view(remainder, endline-remainder);
            if(stripped.length){
                memcpy(data+cursor, stripped.text, stripped.length);
                cursor += stripped.length;
            }
            remainder = endline + 1;
            data[cursor++] = '\n';
        }
        else {
            StringView stripped = stripped_view(remainder, end - remainder);
            if(stripped.length){
                memcpy(data+cursor, stripped.text, stripped.length);
                cursor += stripped.length;
            }
            data[cursor++] = '\n';
            break;
        }
    }
    sb->cursor = cursor;
}

//
// Appends a path separator to the builder and then writes the given string.
// If the builder is empty, a path separator is not appended. This prevents
// accidentally turning a relative path into the wrong absolute path.
//
static inline
void
msb_append_path(MStringBuilder* sb, const char* restrict path, size_t length){
    int err = _check_msb_remaining_size(sb, length+1);
    if(unlikely(err)) return;
    if(sb->cursor)
        sb->data[sb->cursor++] = '/';
    memcpy(sb->data + sb->cursor, path, length);
    sb->cursor += length;
}

//
// Escapes the special characters for html
// TODO: This could be sped up with SIMD.
static inline
void
msb_write_tag_escaped_str(MStringBuilder* sb, const char* text, size_t length){
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
void
msb_shell_quote_arg(MStringBuilder* sb, const char* text, size_t length){
    // STRATEGY: surround arg by single quotes.
    // Single quotes themselves need to be escaped, which can be achieved by string
    // concatenation
    msb_write_char(sb, '\'');
    for(size_t i = 0; i < length; i++){
        switch(text[i]){
            case '\'':
                msb_write_literal(sb, "'\"'\"'");
                continue;
            default:
                msb_write_char(sb, text[i]);
        }
    }
    msb_write_char(sb, '\'');
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
