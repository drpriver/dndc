#ifndef MSB_EXTENSIONS_H
#define MSB_EXTENSIONS_H
#include "MStringBuilder.h"

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
int
msb_write_kebab(Nonnull(MStringBuilder*)msb, Nonnull(const char*)text, size_t length){
    msb_reserve(msb, length);
    auto data = msb->data;
    auto cursor = msb->cursor;
    int n_written = 0;
    bool want_write_hyphen = false;
    for(size_t i = 0; i < length; i++){
        char c = text[i];
        switch(c){
            case 'A' ... 'Z':
                c |= 0x20; // tolower
                // fall-through
            case 'a' ... 'z':
            case '0' ... '9':
                if(want_write_hyphen){
                    // msb_write_char(msb, a, '-');
                    data[cursor++] = '-';
                    want_write_hyphen = false;
                    }
                // msb_write_char(msb, a, c);
                data[cursor++] = c;
                n_written += 1;
                continue;
            case ' ': case '\t': case '-':
                if(n_written)
                    want_write_hyphen = true;
                continue;
            default:
                continue;
            }
        }
    msb->cursor = cursor;
    return n_written;
    }

//
// Writes the string into the builder, but title-cases the string.
// Capitalizes the first letter of each word (even articles like "an" unfortunately).
// For example:
//  "this is some text." -> "This Is Some Text."
static inline
void
msb_write_title(Nonnull(MStringBuilder*) restrict msb, Nonnull(const char*) restrict str, size_t len){
    if(not len)
        return;
    _check_msb_size(msb, len);
    bool wants_cap = true;
    for(size_t i = 0; i < len; i++){
        char c = str[i];
        switch(c){
            case 'a' ... 'z':
                if(wants_cap){
                    c &= ~0x20;
                    wants_cap = false;
                    }
                break;
            case 'A' ... 'Z':
                wants_cap = false;
                break;
            default:
                c = ' ';
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
msb_write_json_escaped_str(Nonnull(MStringBuilder*)restrict sb, Nonnull(const char*)restrict str, size_t length){
    _check_msb_size(sb, length*2);
    auto data = sb->data;
    auto cursor = sb->cursor;
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
msb_write_str_with_backslashes_as_forward_slashes(Nonnull(MStringBuilder*)sb, Nonnull(const char*)restrict str, size_t length){
    _check_msb_size(sb, length);
    auto data = sb->data;
    auto cursor = sb->cursor;
    for(size_t i = 0; i < length; i++){
        char c = str[i];
        if(c == '\\'){
            c = '/';
            }
        data[cursor++] = c;
        }
    sb->cursor = cursor;
    }
#endif
