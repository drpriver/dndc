#include "dndc.c"

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
    uint64_t flags = PARSE_FLAGS_NONE
        | PARSE_SUPPRESS_WARNINGS
        | PARSE_DONT_WRITE
        | PARSE_SOURCE_PATH_IS_DATA_NOT_PATH
        | PARSE_DONT_PRINT_ERRORS
        | PARSE_NO_PYTHON
        ;
    LongString source = {.text=str, .length=size};
    auto e = run_the_parser(flags, source, LS(""), LS(""));
    (void)e;
    free(str);
    return 0;
    }
