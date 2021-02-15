#ifndef JSON_UTIL_H
#define JSON_UTIL_H
#include "common_macros.h"
#include "MStringBuilder.h"

static inline
void
msb_write_json_escaped_str(Nonnull(MStringBuilder*)restrict sb, Nonnull(const Allocator*)a, Nonnull(const char*)restrict str, size_t length){
    _check_msb_size(sb, a, length*2);
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

#endif
