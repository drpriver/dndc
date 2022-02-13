#ifndef MSB_URL_HELPERS_H
#define MSB_URL_HELPERS_H

#ifdef _WIN32
typedef long long ssize_t
#else
#include <sys/types.h> // ssize_t
#endif

#include <string.h>

#include "parse_numbers.h"
#include "MStringBuilder.h"

// TODO: speed up with SIMD
static inline
void
msb_url_percent_encode(MStringBuilder* sb, const char* text, size_t length){
    static const char* hex = "0123456789ABCDEF";
    for(size_t i = 0; i < length; i++){
        unsigned c = (unsigned char)text[i];
        if(c <= 32 || c >= 127){
            msb_write_char(sb, '%');
            msb_write_char(sb, hex[(c & 0xf0)>>4]);
            msb_write_char(sb, hex[c & 0xf]);
            continue;
        }
        switch(c){
            case ':': case '/': case '?': case '#': case '[':
            case ']': case '@': case '!': case '$': case '&':
            case '\'': case '(': case ')': case '*': case '+':
            case ',': case ';': case '=': case '%': case '"':
                msb_write_char(sb, '%');
                msb_write_char(sb, hex[(c & 0xf0)>>4]);
                msb_write_char(sb, hex[c & 0xf]);
                continue;
            default:
                msb_write_char(sb, c);
                continue;
        }
    }
}

// Like the above, but slashes are allowed.
static inline
void
msb_url_percent_encode_filepath(MStringBuilder* sb, const char* text, size_t length){
    static const char* hex = "0123456789ABCDEF";
    for(size_t i = 0; i < length; i++){
        unsigned c = (unsigned char)text[i];
        if(c <= 32 || c >= 127){
            msb_write_char(sb, '%');
            msb_write_char(sb, hex[(c & 0xf0)>>4]);
            msb_write_char(sb, hex[c & 0xf]);
            continue;
        }
        switch(c){
            case ':':           case '?': case '#': case '[':
            case ']': case '@': case '!': case '$': case '&':
            case '\'': case '(': case ')': case '*': case '+':
            case ',': case ';': case '=': case '%': case '"':
                msb_write_char(sb, '%');
                msb_write_char(sb, hex[(c & 0xf0)>>4]);
                msb_write_char(sb, hex[c & 0xf]);
                continue;
            default:
                msb_write_char(sb, c);
        }
    }
}

//
// Returns 0 on success, 1 on error (invalid % escape)
// TODO: speed up with SIMD
static inline
int
msb_url_percent_decode(MStringBuilder* sb, const char* text, size_t length){
    msb_ensure_additional(sb, length);
    for(size_t i = 0; i < length; i++){
        char c = text[i];
        if(c == '%'){
            ssize_t remainder = (ssize_t)length - (ssize_t)i - 1;
            if(remainder < 2){
                return 1;
            }
            const char* dig = text+i+1;
            Uint64Result h = parse_hex_inner(dig, 2);
            if(h.errored) return 1;
            msb_write_char(sb, (char)(unsigned char)h.result);
            i += 2;
        }
        else {
            msb_write_char(sb, c);
        }
    }
    return 0;
}

#endif
