//
// Copyright © 2021-2022, David Priver
//
#include "dndc.c"

#if 1
// Libfuzz calls this function to do its fuzzing.
// Generally ascii-only is more interesting.
// You can turn that off if you want.
int
LLVMFuzzerTestOneInput(const uint8_t*data, size_t size){
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_NO_THREADS
        // | DNDC_INPUT_IS_UNTRUSTED
#ifdef FUZZ_FORMAT
        | DNDC_REFORMAT_ONLY
#endif
#ifdef FUZZ_MD
        | DNDC_OUTPUT_MD
#endif
        ;
    StringView source = {.text=(const char*)data, .length=size};
    LongString out;
    int e = run_the_dndc(flags,
            SV(""), // base directory
            source, // source text
            SV("lmao.html"), // source path
            &out, // out
            NULL, NULL, // caches
            NULL, NULL, // errors
            NULL, NULL, // dependency
            NULL, NULL, // astfunc
            NULL, // worker
            LS("") // js args
            );
    if(!e) dndc_free_string(out);
    return 0;
}
#else
void
dndc_syntax_func(void* _Nullable data, int type, int line, int col, Nonnull(const char*)begin, size_t length){
    long long val = 0;
    for(size_t i = 0; i < length; i++){
        val += begin[i];
    }
    long long* d = data;
    *d = val;
}
int
LLVMFuzzerTestOneInput(const uint8_t*data, size_t size){
    long long d = 0;
    StringView source = {.text = (const char*)data, .length=size};
    dndc_analyze_syntax(source, dndc_syntax_func, &d);
    return 0;
}
#endif
