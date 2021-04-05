#include "dndc.c"

#if 0
// Libfuzz calls this function to do its fuzzing.
// Generally ascii-only is more interesting.
// You can turn that off if you want.
int LLVMFuzzerTestOneInput(const uint8_t*data, size_t size){
    // We only accept null-terminated and I can't find any indication that that
    // we can get fuzzer to give us nul-terminated, so do it ourselves I guess.
    size_t msize = size+1;
    char* str = malloc(msize);
    assert(str);
    memcpy(str, data, size);
    str[size] = '\0';
    // It'd be nice to fuzz Python blocks as well, but I don't want
    // the fuzzer to accidentally do an os.system of anything.
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DONT_WRITE
        | DNDC_SOURCE_PATH_IS_DATA_NOT_PATH
        | DNDC_DONT_PRINT_ERRORS
        | DNDC_NO_PYTHON
        | DNDC_NO_THREADS
#ifdef FUZZ_FORMAT
        | DNDC_REFORMAT_ONLY
#endif
        ;
    LongString source = {.text=str, .length=size};
    auto e = run_the_dndc(flags, SV(""), source, NULL, LS(""), NULL);
    (void)e;
    free(str);
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
int LLVMFuzzerTestOneInput(const uint8_t*data, size_t size){
    // We only accept null-terminated and I can't find any indication that that
    // we can get fuzzer to give us nul-terminated, so do it ourselves I guess.
    long long d = 0;
    StringView source = {.text = (const char*)data, .length=size};
    dndc_analyze_syntax(source, dndc_syntax_func, &d);
    return 0;
    }
#endif
