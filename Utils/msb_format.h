#ifndef MSB_FORMAT_H
#define MSB_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include "MStringBuilder.h"
#include "long_string.h"
#include "msb_extensions.h" // FIXME: dependency on msb_write_json_escaped_str

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline __attribute__((always_inline))
#else
#define force_inline
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum FormatType {
    FORMATTYPE_STRING = 0,
    FORMATTYPE_INT32 = 1,
    FORMATTYPE_UINT32 = 2,
    FORMATTYPE_INT_PADDED = 3,
    FORMATTYPE_INT64 = 4,
    FORMATTYPE_UINT64 = 5,
    FORMATTYPE_QUOTED_STRING = 6,
};

typedef struct FormatArg FormatArg;
struct FormatArg {
    enum FormatType type;
    union {
        int32_t int32_value;
        uint32_t uint32_value;
        int64_t int64_value;
        uint64_t uint64_value;
        StringView string_value;
        struct {
            int value;
            int padding;
        } padded_int;
    };
};

static inline
force_inline
FormatArg
int_fmt(int value){
    _Static_assert(sizeof(value) == sizeof(int32_t), "");
    return (FormatArg){.type = FORMATTYPE_INT32, .int32_value=value};
}
static inline
force_inline
FormatArg
padded_int_fmt(int value, int padding){
    return (FormatArg){
        .type = FORMATTYPE_INT_PADDED,
        .padded_int.value=value,
        .padded_int.padding=padding
    };
}

static inline
force_inline
FormatArg
fmt_fmt(FormatArg value){
    return value;
}

static inline
force_inline
FormatArg
uint_fmt(uint32_t value){
    _Static_assert(sizeof(value) == sizeof(unsigned), "");
    return (FormatArg){.type = FORMATTYPE_UINT32, .uint32_value=value};
}

static inline
force_inline
FormatArg
ulong_fmt(unsigned long value){
    if(sizeof(value) == sizeof(uint64_t)){
        return (FormatArg){.type = FORMATTYPE_UINT64, .uint64_value=value};
    }
    else {
        return (FormatArg){.type = FORMATTYPE_UINT32, .uint32_value=value};
    }
}

static inline
force_inline
FormatArg
long_fmt(long value){
    if(sizeof(value) == sizeof(int64_t)){
        return (FormatArg){.type = FORMATTYPE_INT64, .int64_value=value};
    }
    else {
        return (FormatArg){.type = FORMATTYPE_INT32, .int32_value=value};
    }
}

static inline
force_inline
FormatArg
longlong_fmt(long long value){
    _Static_assert(sizeof(value) == sizeof(int64_t), "");
    return (FormatArg){.type = FORMATTYPE_INT64, .int64_value=value};
}
static inline
force_inline
FormatArg
ulonglong_fmt(unsigned long long value){
    _Static_assert(sizeof(value) == sizeof(uint64_t), "");
    return (FormatArg){.type = FORMATTYPE_UINT64, .uint64_value=value};
}

static inline
force_inline
FormatArg
sv_fmt(StringView value){
    return (FormatArg){.type=FORMATTYPE_STRING, .string_value=value};
}
static inline
force_inline
FormatArg
str_fmt(const char* value){
    return (FormatArg){
        .type=FORMATTYPE_STRING,
        .string_value.text=value,
        .string_value.length=strlen(value),
    };
}
static inline
force_inline
FormatArg
ls_fmt(LongString value){
    return sv_fmt(LS_to_SV(value));
}


// The first 100 characters of 00 - 99.
// Assumes a little endian cpu. (0x3733 translates to the string '37').
// 0x30 is '0'.
static const uint16_t ZERO_TO_NINETY_NINE[] = {
    0x3030, 0x3130, 0x3230, 0x3330, 0x3430, 0x3530, 0x3630, 0x3730, 0x3830, 0x3930,
    0x3031, 0x3131, 0x3231, 0x3331, 0x3431, 0x3531, 0x3631, 0x3731, 0x3831, 0x3931,
    0x3032, 0x3132, 0x3232, 0x3332, 0x3432, 0x3532, 0x3632, 0x3732, 0x3832, 0x3932,
    0x3033, 0x3133, 0x3233, 0x3333, 0x3433, 0x3533, 0x3633, 0x3733, 0x3833, 0x3933,
    0x3034, 0x3134, 0x3234, 0x3334, 0x3434, 0x3534, 0x3634, 0x3734, 0x3834, 0x3934,
    0x3035, 0x3135, 0x3235, 0x3335, 0x3435, 0x3535, 0x3635, 0x3735, 0x3835, 0x3935,
    0x3036, 0x3136, 0x3236, 0x3336, 0x3436, 0x3536, 0x3636, 0x3736, 0x3836, 0x3936,
    0x3037, 0x3137, 0x3237, 0x3337, 0x3437, 0x3537, 0x3637, 0x3737, 0x3837, 0x3937,
    0x3038, 0x3138, 0x3238, 0x3338, 0x3438, 0x3538, 0x3638, 0x3738, 0x3838, 0x3938,
    0x3039, 0x3139, 0x3239, 0x3339, 0x3439, 0x3539, 0x3639, 0x3739, 0x3839, 0x3939,};
_Static_assert(sizeof(ZERO_TO_NINETY_NINE)==200, "");

//
// buff: A pointer to a buffer that is at least 10 bytes long.
// value: the value to be turned into a string.
//
// Returns: A pointer into the buffer that is the first character of the string.
// Note that this is not necessarily the first character of the buffer.
// You can get the length of the written string via pointer arithmetic:
//
// Example:
//   buff[10]; // note that a length of 10 leaves no room for a nul.
//   char* p = uint32_to_str_buffer(buff, some_unsigned_integer);
//   ptrdiff_t length = (buff+10) - p; // always buff+10, even if buff is longer.
//
static inline
char*
uint32_to_str_buffer(char*  buff, uint32_t value){
    // UINT32_MAX: 4294967295 (10 characters)
    char *p = buff+10; // 1 past the end
    // Loop over the traditional naive way, but write two characters at a time.
    // Write back to front, in a single pass.
    while(value >= 100){
        uint32_t old = value;
        p -= 2;
        // any compiler worth its salt should optimize this to a mul + shift
        value /= 100;
        uint32_t last_two_digits = old - 100*value; // Will always be in range of [00, 99]
        memcpy(p, &ZERO_TO_NINETY_NINE[last_two_digits], sizeof(uint16_t));
    }
    p -= 2;
    // Value is < 100 at this point.
    memcpy(p, &ZERO_TO_NINETY_NINE[value], sizeof(uint16_t));
    // If value < 10, then we ended up writing an extra leading 0.
    // So, add one if less than 10.
    // Also note, that in the exact case that value == 0, we write 00
    // and then return a pointer to the second 0.
    return p+(value < 10);
}

// Buff must be at least 11 chars long.
static inline
void
uint32_to_ascii(char* buff, ptrdiff_t bufsize, uint32_t value){
    char tmp[10];
    char* begin = uint32_to_str_buffer(tmp, value);
    ptrdiff_t length = (tmp+10) - begin;
    if(bufsize < length)
        length = bufsize;
    memcpy(buff, begin, length);
    if(bufsize > length+1)
        buff[length] = 0;
    else
        buff[bufsize-1] = 0;
}

// Just do the same for u64
// There might be a faster way to do this? It kind of depends on your prior for
// what the ranges of values are.
//
// The math is:
//   ptrdiff_t length = (buff+20) - p for this, as UINT64_MAX is 20 characters.

//
// buff: A pointer to a buffer that is at least 20 bytes long.
// value: the value to be turned into a string.
//
// Returns: A pointer into the buffer that is the first character of the string.
// Note that this is not necessarily the first character of the buffer.
// You can get the length of the written string via pointer arithmetic:
//
static inline
char*
uint64_to_str_buffer(char*  buff, uint64_t value){
    // UINT64_MAX: 18446744073709551615 (20 characters)
    char *p = buff+20; // 1 past the end
    // Loop over the traditional naive way, but write two characters at a time.
    // Write back to front, in a single pass.
    while(value >= 100){
        uint64_t old = value;
        p -= 2;
        value /= 100;
        uint64_t last_two_digits = old - 100*value; // Will always be in range of [00, 99]
        memcpy(p, &ZERO_TO_NINETY_NINE[last_two_digits], sizeof(uint16_t));
    }
    p -= 2;
    // Value is < 100 at this point.
    memcpy(p, &ZERO_TO_NINETY_NINE[value], sizeof(uint16_t));
    // If value < 10, then we ended up writing an extra leading 0.
    // So, add one if less than 10.
    // Also note, that in the exact case that value == 0, we write 00
    // and then return pointer to the second 0.
    return p+(value < 10);
}


// buff must be at least 21 long
static inline
void
uint64_to_ascii(char* buff, ptrdiff_t bufsize, uint64_t value){
    if(!bufsize) return;
    char tmp[20];
    char* begin = uint64_to_str_buffer(tmp, value);
    ptrdiff_t length = (tmp+20)-begin;
    if(bufsize < length)
        length = bufsize;
    memcpy(buff, begin, length);
    if(bufsize > length+1)
        buff[length] = 0;
    else
        buff[bufsize-1] = 0;
}

// Does NOT include prefix.
// always pads to 4 characters
// buff should be 4 characters long
static inline
void
uint16_to_hex(char* buff, uint16_t value){
    const char* hexstring = "0123456789aabcdef";
    uint64_t v = value;
    for(int i = 3; i >= 0; i--){
        *buff++ = hexstring[(v >> i*4) &0xf];
    }
}
// Does NOT include prefix.
// always pads to 8 characters
// buff should be 8 characters long
static inline
void
uint32_to_hex(char* buff, uint32_t value){
    const char* hexstring = "0123456789aabcdef";
    uint64_t v = value;
    for(int i = 7; i >= 0; i--){
        *buff++ = hexstring[(v >> i*4) &0xf];
    }
}

static inline
void
msb_write_int32(MStringBuilder* sb, int32_t value){
    if(value == INT32_MIN){
        msb_write_literal(sb, "-2147483648");
        return;
    }
    char buff[10];
    if(value < 0){
        msb_write_char(sb, '-');
        value = -value;
    }
    char* p = uint32_to_str_buffer(buff, value);
    ptrdiff_t size = (buff+10) - p;
    _check_msb_remaining_size(sb, size);
    memcpy(sb->data+sb->cursor, p, size);
    sb->cursor += size;
}

static inline
void
msb_write_int64(MStringBuilder* sb, int64_t value){
    if(value == INT64_MIN){
        msb_write_literal(sb, "-9223372036854775808");
        return;
    }
    if(value < 0){
        msb_write_char(sb, '-');
        value = -value;
    }
    char buff[20];
    char* p = uint64_to_str_buffer(buff, value);
    ptrdiff_t size = (buff+20) - p;
    _check_msb_remaining_size(sb, size);
    memcpy(sb->data+sb->cursor, p, size);
    sb->cursor += size;
}

static inline
void
msb_write_int_space_padded(MStringBuilder* sb, int32_t value, int width){
    assert(width >= 0);
    char buff[10];
    const char* p;
    size_t size;
    bool is_negative = false;
    if(value == INT32_MIN){
        p = "-2147483648";
        size = sizeof("-2147483648")-1;
    }
    else {
        if(value < 0){
            value = -value;
            is_negative = true;
        }
        p = uint32_to_str_buffer(buff, value);
        size = (buff+10) - p;
    }
    size_t needed_size = size + is_negative;
    size_t cursor = sb->cursor;
    char* data;
    if(needed_size >= (unsigned)width){
        _check_msb_remaining_size(sb, needed_size);
        data = sb->data;
    }
    else {
        _check_msb_remaining_size(sb, width);
        data = sb->data;
        intptr_t pad = width - needed_size;
        memset(data+cursor, ' ', pad);
        cursor += pad;
    }
    if(is_negative)
        data[cursor++] = '-';
    memcpy(data+cursor, p, size);
    cursor += size;
    sb->cursor = cursor;
}

static inline
void
msb_write_uint32(MStringBuilder* sb, uint32_t value){
    char buff[10];
    char* p = uint32_to_str_buffer(buff, value);
    ptrdiff_t size = (buff+10) - p;
    _check_msb_remaining_size(sb, size);
    memcpy(sb->data+sb->cursor, p, size);
    sb->cursor += size;
}

static inline
void
msb_write_uint64(MStringBuilder* sb, uint64_t value){
    char buff[20];
    char* p = uint64_to_str_buffer(buff, value);
    ptrdiff_t size = (buff+20) - p;
    _check_msb_remaining_size(sb, size);
    memcpy(sb->data+sb->cursor, p, size);
    sb->cursor += size;
}

static inline
force_inline
void
msb_apply_format(MStringBuilder* sb, FormatArg arg){
    switch(arg.type){
        case FORMATTYPE_STRING:
            msb_write_str(sb, arg.string_value.text, arg.string_value.length);
            break;
        case FORMATTYPE_INT32:
            msb_write_int32(sb, arg.int32_value);
            break;
        case FORMATTYPE_UINT32:
            msb_write_uint32(sb, arg.uint32_value);
            break;
        case FORMATTYPE_INT_PADDED:
            msb_write_int_space_padded(sb, arg.padded_int.value, arg.padded_int.padding);
            break;
        case FORMATTYPE_INT64:
            msb_write_int64(sb, arg.int64_value);
            break;
        case FORMATTYPE_UINT64:
            msb_write_uint64(sb, arg.uint64_value);
            break;
        case FORMATTYPE_QUOTED_STRING:
            msb_write_char(sb, '"');
            // FIXME: this file accidentally depends on this functionality.
            msb_write_json_escaped_str(sb, arg.string_value.text, arg.string_value.length);
            msb_write_char(sb, '"');
            break;
    }
}

static inline
void
msb_format(MStringBuilder* sb, size_t n_items, const FormatArg* args){
    for(size_t i = 0; i < n_items; i++){
        msb_apply_format(sb, args[i]);
    }
}

#define SV_FMT(lit) sv_fmt(SV(lit))

#define QUOTED(x) (FormatArg){.type=FORMATTYPE_QUOTED_STRING, .string_value={.text="" x, .length=sizeof(x)}}
static inline
FormatArg
quoted(StringView sv){
    return (FormatArg){.type=FORMATTYPE_QUOTED_STRING, .string_value=sv};
}

static inline
FormatArg
quotedls(LongString ls){
    return (FormatArg){.type=FORMATTYPE_QUOTED_STRING, .string_value=LS_to_SV(ls)};
}

#define FMT(x) _Generic(x, \
        FormatArg: fmt_fmt,\
        unsigned: uint_fmt,\
        int: int_fmt,\
        unsigned long: ulong_fmt,\
        unsigned long long: ulonglong_fmt,\
        long: long_fmt,\
        long long: longlong_fmt,\
        char*: str_fmt,\
        const char*: str_fmt,\
        StringView: sv_fmt,\
        LongString: ls_fmt)(x)

#define MSB_FORMAT_(sb, ...) do{ \
    FormatArg _format_args[] = {__VA_ARGS__}; \
    msb_format(sb, arrlen(_format_args), _format_args); \
}while(0)

#define SELECT_MSB_FORMAT_IMPL(n) MSB_FORMAT_IMPL##n
#define SELECT_MSB_FORMAT(n) SELECT_MSB_FORMAT_IMPL(n)
// These use the variant-style formatter, but the FMT is generic and knows the
// type, so we could dispatch to the specific format funcs instead.
//
// But whatever, these are mainly for convenience. If you care about
// performance, you need to reserve and then directly write to the buffer.
#define MSB_FORMAT_IMPL1(sb, a) do{ \
    msb_apply_format(sb, FMT(a));\
}while(0)
#define MSB_FORMAT_IMPL2(sb, a, b) do{\
    msb_apply_format(sb, FMT(a));\
    msb_apply_format(sb, FMT(b));\
}while(0)
#define MSB_FORMAT_IMPL3(sb, a, b, c) do {\
    msb_apply_format(sb, FMT(a));\
    msb_apply_format(sb, FMT(b));\
    msb_apply_format(sb, FMT(c));\
}while(0)
#define MSB_FORMAT_IMPL4(sb, a, b, c, d) do {\
    msb_apply_format(sb, FMT(a));\
    msb_apply_format(sb, FMT(b));\
    msb_apply_format(sb, FMT(c));\
    msb_apply_format(sb, FMT(d));\
}while(0)
#define MSB_FORMAT_IMPL5(sb, a, b, c, d, e) do {\
    msb_apply_format(sb, FMT(a));\
    msb_apply_format(sb, FMT(b));\
    msb_apply_format(sb, FMT(c));\
    msb_apply_format(sb, FMT(d));\
    msb_apply_format(sb, FMT(e));\
}while(0)
#define MSB_FORMAT_IMPL6(sb, a, b, c, d, e, f) do {\
    msb_apply_format(sb, FMT(a));\
    msb_apply_format(sb, FMT(b));\
    msb_apply_format(sb, FMT(c));\
    msb_apply_format(sb, FMT(d));\
    msb_apply_format(sb, FMT(e));\
    msb_apply_format(sb, FMT(f));\
}while(0)
#define MSB_FORMAT_IMPL7(sb, a, b, c, d, e, f, g) do {\
    msb_apply_format(sb, FMT(a));\
    msb_apply_format(sb, FMT(b));\
    msb_apply_format(sb, FMT(c));\
    msb_apply_format(sb, FMT(d));\
    msb_apply_format(sb, FMT(e));\
    msb_apply_format(sb, FMT(f));\
    msb_apply_format(sb, FMT(g));\
}while(0)
#define MSB_FORMAT_IMPL8(sb, a, b, c, d, e, f, g, h) do {\
    msb_apply_format(sb, FMT(a));\
    msb_apply_format(sb, FMT(b));\
    msb_apply_format(sb, FMT(c));\
    msb_apply_format(sb, FMT(d));\
    msb_apply_format(sb, FMT(e));\
    msb_apply_format(sb, FMT(f));\
    msb_apply_format(sb, FMT(g));\
    msb_apply_format(sb, FMT(h));\
}while(0)
#define MSB_FORMAT_IMPL9(sb, a, b, c, d, e, f, g, h, i) do {\
    msb_apply_format(sb, FMT(a));\
    msb_apply_format(sb, FMT(b));\
    msb_apply_format(sb, FMT(c));\
    msb_apply_format(sb, FMT(d));\
    msb_apply_format(sb, FMT(e));\
    msb_apply_format(sb, FMT(f));\
    msb_apply_format(sb, FMT(g));\
    msb_apply_format(sb, FMT(h));\
    msb_apply_format(sb, FMT(i));\
}while(0)
#define MSB_FORMAT_IMPL10(sb, a, b, c, d, e, f, g, h, i, j) do {\
    msb_apply_format(sb, FMT(a));\
    msb_apply_format(sb, FMT(b));\
    msb_apply_format(sb, FMT(c));\
    msb_apply_format(sb, FMT(d));\
    msb_apply_format(sb, FMT(e));\
    msb_apply_format(sb, FMT(f));\
    msb_apply_format(sb, FMT(g));\
    msb_apply_format(sb, FMT(h));\
    msb_apply_format(sb, FMT(i));\
    msb_apply_format(sb, FMT(j));\
}while(0)
#define MSB_FORMAT_IMPL13(sb, a, b, c, d, e, f, g, h, i, j,k,l,m) do {\
    msb_apply_format(sb, FMT(a));\
    msb_apply_format(sb, FMT(b));\
    msb_apply_format(sb, FMT(c));\
    msb_apply_format(sb, FMT(d));\
    msb_apply_format(sb, FMT(e));\
    msb_apply_format(sb, FMT(f));\
    msb_apply_format(sb, FMT(g));\
    msb_apply_format(sb, FMT(h));\
    msb_apply_format(sb, FMT(i));\
    msb_apply_format(sb, FMT(j));\
    msb_apply_format(sb, FMT(k));\
    msb_apply_format(sb, FMT(l));\
    msb_apply_format(sb, FMT(m));\
}while(0)

#define GET_ARG_16(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,N,...) N
#define COUNT_MACRO_ARGS(...) GET_ARG_16( __VA_ARGS__, 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,I_CANNOT_SEE_ZERO_ARGS)

#define MSB_FORMAT(sb, ...) SELECT_MSB_FORMAT(COUNT_MACRO_ARGS(__VA_ARGS__))(sb, __VA_ARGS__)

static inline
void
msb_write_us_as_ms(MStringBuilder* sb, uint64_t microseconds){
    char buff[20];
    char* p = uint64_to_str_buffer(buff, microseconds);
    ptrdiff_t size = (buff+20) - p;
    if(size <= 3){
        msb_write_literal(sb, "0.");
        if(size < 3){
            msb_write_nchar(sb, '0', 3-size);
        }
        msb_write_str(sb, p, size);
    }
    else {
        msb_write_str(sb, p, size-3);
        msb_write_char(sb, '.');
        msb_write_str(sb, p+size-3, 3);
    }
    msb_write_literal(sb, "ms");
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
