//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#include "compiler_warnings.h"
#define DNDC_API static inline
#include "Dndc/dndc.h"
#include "Dndc/dndc_funcs.h"
#include "Allocators/nullacator.h"
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
        char buff[4096];
        MStringBuilder msb = {
            .data = buff,
            .capacity = sizeof buff,
            .allocator = NULLACATOR,
        };
        StringView m = {message_len, message};
        StringView f = {filename_len, filename};
        MSB_FORMAT(&msb, f, ":", line, ":", col, ":", m);
        log_string(buff, msb.cursor);
    }
    else{
        log_string(message, message_len);
    }
}

extern
PString*
make_html(PString* source){
    StringView text = PString_to_sv(source);
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
            text,                // source
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
    StringView text = PString_to_sv(source);
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
            text,
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
    StringView text = PString_to_sv(source);
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
            flags, base, text, SV(""),
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

#include "Dndc/dndc.c"
