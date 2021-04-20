#include "dndc.c"
#include "dndc_flags.h"
#if 0
static
Errorable_f(void)
run_the_dndc(uint64_t flags, StringView base_directory, LongString source_path,
        Nullable(LongString*) output_path, DependsArg depends,
        Nullable(FileCache*)external_b64cache,
        Nullable(FileCache*)external_textcache,
        Nullable(ErrorFunc*)error_func, Nullable(void*)error_user_data);
#endif

void dndc_error_func(void* error_user_data, int type, const char* filename, int filename_len, int line, int col, const char* message, int message_len){
    log_string(message, message_len);
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
        | DNDC_NO_CLEANUP
        | DNDC_NO_PYTHON
        | DNDC_NO_THREADS
        | DNDC_DONT_INLINE_IMAGES
        ;
    auto e = run_the_dndc(flags, base, text, &output, (DependsArg){}, NULL, NULL, dndc_error_func, NULL);
    if(e.errored)
        return NULL;
    PString* result = malloc(sizeof(*result) + output.length);
    result->length = output.length;
    memcpy(result->text, output.text, output.length);
    return result;
}
printf_func(5, 6)
extern
void logfunc(int log_level, const char*_Nonnull file, const char*_Nonnull func, int line, const char*_Nonnull fmt, ...){
    logi32(log_level);
    log_string(file, strlen(file));
    log_string(func, strlen(func));
    logi32(line);
    log_string(fmt, strlen(fmt));
    }
