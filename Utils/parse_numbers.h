#ifndef PARSE_NUMBERS_H
#define PARSE_NUMBERS_H
#include <stdint.h>
#include <limits.h>
#include "common_macros.h"
#include "error_handling.h"

static inline
Errorable_f(uint64_t)
power64(uint64_t base, uint64_t exp){
    Errorable(uint64_t) result = {};
    uint64_t res = 1;
    for(;;){
        if (exp & 1){
            if(__builtin_mul_overflow(res, base, &res))
                Raise(OVERFLOWED_VALUE);
            }
        exp /= 2;
        if(!exp)
            break;
        if(__builtin_mul_overflow(base, base, &base))
            Raise(OVERFLOWED_VALUE);
        }
    result.result = res;
    return result;
    }


static inline
Errorable_f(uint64_t)
parse_uint64(Nonnull(const char*) str, size_t length){
    Errorable(uint64_t) result = {};
    if (not length)
        Raise(UNEXPECTED_END);
    if (*str == '+'){
        str++;
        length--;
        }
    if(length > 20)
        Raise(OVERFLOWED_VALUE); // UINT64_MAX is 18,446,744,073,709,551,615 (20 characters)
    int bad = false;
    uint64_t value = 0;
    for(size_t i=0;i < length-1; i++){
        unsigned cval = str[i];
        cval -= '0';
        if(cval > 9u)
            bad = true;
        value *= 10;
        value += cval;
        }
    if(bad)
        Raise(INVALID_SYMBOL);
    // Handle the last char differently as it's the only
    // one that can overflow.
    {
        unsigned cval = str[length-1];
        cval -= '0';
        if(cval > 9u)
            Raise(INVALID_SYMBOL);
        if(__builtin_mul_overflow(value, 10, &value))
            Raise(OVERFLOWED_VALUE);
        if(__builtin_add_overflow(value, cval, &value))
            Raise(OVERFLOWED_VALUE);
    }
    result.result = value;
    return result;
    }

static inline
Errorable_f(int64_t)
parse_int64(Nonnull(const char*) str, size_t length){
    Errorable(int64_t) result = {};
    if(not length)
        Raise(UNEXPECTED_END);
    bool negative = (*str == '-');
    if(negative){
        str++;
        length--;
        }
    else if (*str == '+'){
        str++;
        length--;
        }
    if(length > 19) Raise(OVERFLOWED_VALUE); // INT64_MAX is 9223372036854775807 (19 characters)
    int bad = false;
    uint64_t value = 0;
    for(size_t i=0;i < length-1; i++){
        unsigned cval = str[i];
        cval -= '0';
        if(cval > 9u)
            bad = true;
        value *= 10;
        value += cval;
        }
    if(bad)
        Raise(INVALID_SYMBOL);
    // Handle the last char differently as it's the only
    // one that can overflow.
    {
        unsigned cval = str[length-1];
        cval -= '0';
        if(cval > 9u)
            Raise(INVALID_SYMBOL);
        if(__builtin_mul_overflow(value, 10, &value))
            Raise(OVERFLOWED_VALUE);
        if(__builtin_add_overflow(value, cval, &value))
            Raise(OVERFLOWED_VALUE);
    }
    if(negative){
        if(value > (uint64_t)INT64_MAX+1){
            Raise(OVERFLOWED_VALUE);
            }
        value *= -1;
        }
    else{
        if(value > (uint64_t)INT64_MAX)
            Raise(OVERFLOWED_VALUE);
        }
    result.result = value;
    return result;
    }

static inline
Errorable_f(int)
parse_int(Nonnull(const char*) str, size_t length){
    Errorable(int) result = {};
    auto val = attempt(parse_int64(str, length));
    if (val > INT_MAX or val < INT_MIN){
        Raise (OVERFLOWED_VALUE);
        }
    result.result = (int)val;
    return result;
    }

static inline
Errorable_f(uint64_t)
parse_hex_inner(Nonnull(const char*) str, size_t length){
    Errorable(uint64_t) result = {};
    if(length > sizeof(result.result)*2)
        Raise(OVERFLOWED_VALUE);
    uint64_t value = 0;
    for(size_t i = 0; i < length; i++){
        char c = str[i];
        uint64_t char_value;
        switch(c){
            case '0'...'9':
                char_value = c - '0';
                break;
            case 'a'...'f':
                char_value = c - 'a' + 10;
                break;
            case 'A'...'F':
                char_value = c - 'A' + 10;
                break;
            default:
                Raise(INVALID_SYMBOL);
            }
        value <<= 4;
        value |= char_value;
        }
    result.result = value;
    return result;
    }

static inline
Errorable_f(uint64_t)
parse_pound_hex(Nonnull(const char*) str, size_t length){
    Errorable(uint64_t) result = {};
    if(length < 2)
        Raise(UNEXPECTED_END);
    if(str[0] != '#')
        Raise(INVALID_SYMBOL);
    return parse_hex_inner(str+1, length-1);
    }

static inline
Errorable_f(uint64_t)
parse_hex(Nonnull(const char*) str, size_t length){
    Errorable(uint64_t) result = {};
    if(length<3)
        Raise(UNEXPECTED_END);
    if(str[0] != '0')
        Raise(INVALID_SYMBOL);
    if(str[1] != 'x' and str[1] != 'X')
        Raise(INVALID_SYMBOL);
    return parse_hex_inner(str+2, length-2);
    }

static inline Errorable_f(uint64_t) parse_binary_inner(Nonnull(const char*), size_t);

static inline
Errorable_f(uint64_t)
parse_binary(Nonnull(const char*) str, size_t length){
    Errorable(uint64_t) result = {};
    if(length<3)
        Raise(UNEXPECTED_END);
    if(str[0] != '0')
        Raise(INVALID_SYMBOL);
    if(str[1] != 'b' and str[1] != 'B')
        Raise(INVALID_SYMBOL);
    return parse_binary_inner(str+2, length-2);
    }

static inline
Errorable_f(uint64_t)
parse_binary_inner(Nonnull(const char*) str, size_t length){
    Errorable(uint64_t) result = {};
    unsigned long long mask = 1llu << 63;
    mask >>= (64 - length);
    // @speed
    // 2**4 is only 16, so we could definitely
    // read 4 bytes at a time and then do a fixup.
    // You'd have to see what code is generated though
    // (does the compiler turn it into a binary decision tree?)
    // 2**8 is only 256, that's probably not worth it.
    for(size_t i = 0; i < length; i++, mask>>=1){
        switch(str[i]){
            case '1':
                result.result |= mask;
                continue;
            case '0':
                continue;
            default:
                Raise(INVALID_SYMBOL);
            }
        }
    return result;
    }

// parses an unsigned integer, in whatever format is comfortable for a human
// accepts 0x hexes, 0b binary, plain integers, and also # hexes
static inline
Errorable_f(uint64_t)
parse_unsigned_human(Nonnull(const char*) str, size_t length){
    Errorable(uint64_t) result = {};
    if(not length)
        Raise(UNEXPECTED_END);
    if(str[0] == '#')
        return parse_pound_hex(str, length);
    if(str[0] == '0' and length > 1){
        if(str[1] == 'x' or str[1] == 'X')
            return parse_hex(str, length);
        if(str[1] == 'b' or str[1] == 'B')
            return parse_binary(str, length);
        }
    return parse_uint64(str, length);
    }

typedef struct Integer {
    bool is_signed;
    union {
        uint64_t value;
        int64_t svalue;
        };
    } Integer;

Errorable_declare(Integer);
static inline
Errorable_f(Integer)
parse_human_integer(Nonnull(const char*) str, size_t length){
    Errorable(Integer) result = {};
    if(not length)
        Raise(UNEXPECTED_END);
    if(str[0] == '-'){
        result.result.svalue = attempt(parse_int64(str, length));
        result.result.is_signed = true;
        return result;
        }
    if(str[0] == '#'){
        result.result.value = attempt(parse_pound_hex(str, length));
        return result;
        }
    if(str[0] == '0' and length > 1){
        if(str[1] == 'x' or str[1] == 'X'){
            result.result.value = attempt(parse_hex(str, length));
            return result;
            }
        if(str[1] == 'b' or str[1] == 'B'){
            result.result.value = attempt(parse_binary(str, length));
            return result;
            }
        }
    result.result.value = attempt(parse_uint64(str, length));
    return result;
    }

#endif
