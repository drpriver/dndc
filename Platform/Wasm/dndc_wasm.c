//
// Copyright © 2021-2022, David Priver
//
#include "Dndc/dndc.h"
#include "Dndc/dndc.c"
#include "Utils/msb_format.h"
#include "jsinter.h"

struct {
    size_t length;
    char data[sizeof(DNDC_VERSION)];
} VERSION = {
    .length = sizeof(DNDC_VERSION)-1,
    .data = DNDC_VERSION,
};

static
void dndc_log_func(void* log_user_data, int type, const char* filename, int filename_len, int line, int col, const char* message, int message_len){
    if(message_len+filename_len < 4096-64){
        MStringBuilder msb = {0};
        char buff[4096];
        msb.data = buff;
        msb.capacity = sizeof(buff);
        msb_write_str(&msb, filename, filename_len);
        msb_write_char(&msb, ':');
        msb_write_int32(&msb, line);
        msb_write_char(&msb, ':');
        msb_write_int32(&msb, col);
        msb_write_char(&msb, ':');
        msb_write_str(&msb, message, message_len);
        msb_nul_terminate(&msb);
        log_string(buff, msb.cursor);
    }
    else{
        log_string(message, message_len);
    }
}

extern
PString*
make_html(PString* source){
    LongString text = PString_to_LongString(source);
    StringView base = SV("");
    LongString output;
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_NO_CLEANUP
        | DNDC_NO_THREADS
        | DNDC_DONT_INLINE_IMAGES
        | DNDC_INPUT_IS_UNTRUSTED
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    int e = run_the_dndc(
            OUTPUT_HTML,
            flags,
            base,                // base_directory
            LS_to_SV(text),      // source
            SV("(string input"), // source_path
            &output,             // outstring
            NULL, NULL,          // caches
            dndc_log_func, NULL, // log func
            NULL, NULL,          // dependency funcs
            NULL, NULL,          // ast funcs
            NULL,                // worker
            LS("")               // args
            );
    if(e) return NULL;
    PString* result = LongString_to_new_PString(output);
    return result;
}

#if 0
extern
PString*
make_fragment(PString* source){
    LongString text = PString_to_LongString(source);
    StringView base = SV("");
    LongString output;
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_NO_CLEANUP
        | DNDC_NO_THREADS
        | DNDC_DONT_INLINE_IMAGES
        | DNDC_INPUT_IS_UNTRUSTED
        | DNDC_FRAGMENT_ONLY
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    int e = run_the_dndc(
            flags,
            base,
            LS_to_SV(text),
            SV("(string input"),
            &output,               // outstring
            NULL, NULL,            // caches
            dndc_log_func, NULL,   // log func
            NULL, NULL,            // dependency funcs
            NULL, NULL,            // ast funcs
            NULL                   // worker
            );
    if(e) return NULL;
    PString* result = LongString_to_new_PString(output);
    return result;
}
#endif

extern
PString*
format_dnd(PString* source){
    LongString text = PString_to_LongString(source);
    StringView base = SV("");
    LongString output;
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_NO_CLEANUP
        | DNDC_NO_THREADS
        | DNDC_DONT_INLINE_IMAGES
        | DNDC_INPUT_IS_UNTRUSTED
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
    int e = run_the_dndc(
            OUTPUT_REFORMAT,
            flags, base, LS_to_SV(text), SV(""),
            &output,
            NULL, NULL,          // caches
            dndc_log_func, NULL, // log func
            NULL, NULL,          // dependency funcs
            NULL, NULL,          // ast funcs
            NULL,                // worker
            LS("")               // jsargs
            );
    if(e) return NULL;
    PString* result = LongString_to_new_PString(output);
    return result;
}

#if 0
printf_func(5, 6)
static
void logfunc(int log_level, const char*_Nonnull file, const char*_Nonnull func, int line, const char*_Nonnull fmt, ...){
    MStringBuilder msb = {0};
    char buff[4096];
    msb.data = buff;
    msb.capacity = sizeof(buff);
    msb_write_str(&msb, file, strlen(file));
    msb_write_str(&msb, func, strlen(func));
    msb_write_int32(&msb, line);
    msb_write_str(&msb, fmt, strlen(fmt));
    msb_nul_terminate(&msb);
    log_string(buff, msb.cursor);
}
#endif
