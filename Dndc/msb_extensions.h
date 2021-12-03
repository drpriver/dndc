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
    msb_ensure_additional(msb, length+2);
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
            case 'A' ... 'Z':
                c |= 0x20; // tolower
                // fall-through
            case 'a' ... 'z':
            case '0' ... '9':
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
    _check_msb_remaining_size(msb, len);
    bool wants_cap = true;
    for(size_t i = 0; i < len; i++){
        char c = str[i];
        switch(c){
            case 'a' ... 'z':
                if(wants_cap){
                    c &= ~0x20; // toupper
                    wants_cap = false;
                }
                break;
            case 'A' ... 'Z':
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
// by json (kind of, I don't think I handle unicode properly).
// Does not include the containing quotation marks. Write those yourself if you
// need them.
static inline
void
msb_write_json_escaped_str(MStringBuilder* restrict sb, const char* restrict str, size_t length){
    _check_msb_remaining_size(sb, length*2);
    char* data = sb->data;
    size_t cursor = sb->cursor;
    for(size_t i = 0; i < length; i++){
        switch(str[i]){
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
    _check_msb_remaining_size(sb, length);
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

//
// Writes the string into the buffer, but strips trailing and leading
// whitespace from each line in the input. A newline is still written for each
// line and one will be added to the end if the string is missing a trailing
// newline.
static inline
void
msb_write_stripped_lines(MStringBuilder* sb, const char* restrict str, size_t length){
    _check_msb_remaining_size(sb, length);
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
    _check_msb_remaining_size(sb, length+1);
    if(sb->cursor)
        sb->data[sb->cursor++] = '/';
    memcpy(sb->data + sb->cursor, path, length);
    sb->cursor += length;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
