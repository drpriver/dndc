#include "dndc.c"
#include "dndc_flags.h"

static
void dndc_error_func(void* error_user_data, int type, const char* filename, int filename_len, int line, int col, const char* message, int message_len){
    if(message_len+filename_len < 4096-64){
        MStringBuilder msb = {};
        char buff[4096];
        msb.data = buff;
        msb.capacity = sizeof(buff);
        msb_write_str(&msb, filename, filename_len);
        msb_write_char(&msb, ':');
        msb_write_int(&msb, line);
        msb_write_char(&msb, ':');
        msb_write_int(&msb, col);
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
    LongString text = {.text=(char*)source->text, .length=source->length-1};
    StringView base = SV("");
    LongString output;
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_OUTPUT_PATH_IS_OUT_PARAM
        | DNDC_SOURCE_PATH_IS_DATA_NOT_PATH
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_NO_CLEANUP
        | DNDC_NO_PYTHON
        | DNDC_NO_THREADS
        | DNDC_DONT_INLINE_IMAGES
        | DNDC_INPUT_IS_UNTRUSTED
        ;
    auto e = run_the_dndc(flags, base, text, &output, (DependsArg){}, NULL, NULL, dndc_error_func, NULL);
    if(e.errored)
        return NULL;
    // Wow, this is a lot of copies.
    PString* result = malloc(sizeof(*result) + output.length);
    result->length = output.length;
    memcpy(result->text, output.text, output.length);
    return result;
}
printf_func(5, 6)
extern
void logfunc(int log_level, const char*_Nonnull file, const char*_Nonnull func, int line, const char*_Nonnull fmt, ...){
    MStringBuilder msb = {};
    char buff[4096];
    msb.data = buff;
    msb.capacity = sizeof(buff);
    msb_write_str(&msb, file, strlen(afile));
    msb_write_str(&msb, func, strlen(func));
    msb_write_int(&msb, line);
    msb_write_str(&msb, fmt, strlen(fmt));
    msb_nul_terminate(&msb);
    log_string(buff, msb.cursor);
    }
