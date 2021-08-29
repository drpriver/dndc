#include "dndc.h"
#include "dndc.c"
#include "msb_format.h"
#include "jsinter.h"

static
void dndc_error_func(void* error_user_data, int type, const char* filename, int filename_len, int line, int col, const char* message, int message_len){
    if(message_len+filename_len < 4096-64){
        MStringBuilder msb = {};
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
    LongString base = LS("");
    LongString output;
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_NO_CLEANUP
        | DNDC_NO_PYTHON
        | DNDC_NO_THREADS
        | DNDC_DONT_INLINE_IMAGES
        | DNDC_INPUT_IS_UNTRUSTED
        ;
    auto e = run_the_dndc(
            flags, base, text, LS("demo.html"), &output,
            NULL, NULL,            // caches
            dndc_error_func, NULL, // error func
            NULL, NULL,            // dependency funcs
            NULL, NULL             // ast funcs
            );
    if(e.errored)
        return NULL;
    PString* result = LongString_to_new_PString(output);
    return result;
}
extern
PString*
format_dnd(PString* source){
    LongString text = PString_to_LongString(source); 
    LongString base = LS("");
    LongString output;
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_NO_CLEANUP
        | DNDC_NO_PYTHON
        | DNDC_NO_THREADS
        | DNDC_DONT_INLINE_IMAGES
        | DNDC_INPUT_IS_UNTRUSTED
        | DNDC_REFORMAT_ONLY
        ;
    auto e = run_the_dndc(
            flags, base, text, LS(""), &output,
            NULL, NULL,            // caches
            dndc_error_func, NULL, // error func
            NULL, NULL,            // dependency funcs
            NULL, NULL             // ast funcs
            );
    if(e.errored)
        return NULL;
    PString* result = LongString_to_new_PString(output);
    return result;
    }
printf_func(5, 6)
static
void logfunc(int log_level, const char*_Nonnull file, const char*_Nonnull func, int line, const char*_Nonnull fmt, ...){
    MStringBuilder msb = {};
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
