//
// Copyright © 2021-2022, David Priver
//
#define HEAVY_RECORDING
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#include "Allocators/testing_allocator.h"
#define DNDC_API static inline
#include "dndc.h"
#include "dndc_long_string.h"
#include "dndc.c"
#include "Utils/testing.h"
#include "Utils/argument_parsing.h"

static TestFunc TestExamples;
enum RunWhich {
    RunWhich_NONE     = 0,
    RunWhich_HTML     = 0x1,
    RunWhich_MD       = 0x2,
    RunWhich_REFORMAT = 0x4,
    RunWhich_EXPAND   = 0x8,
    RunWhich_ALL = RunWhich_HTML | RunWhich_MD | RunWhich_REFORMAT | RunWhich_EXPAND,
};
uint64_t RUNWHICH = 0;

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTestFlags(TestExamples,     TEST_CASE_FLAGS_NONE);
    ArgToParse kw_args[] = {
        {
            .name = SV("--html"),
            .dest = ArgBitFlagDest(&RUNWHICH, RunWhich_HTML),
        },
        {
            .name = SV("--md"),
            .dest = ArgBitFlagDest(&RUNWHICH, RunWhich_MD),
        },
        {
            .name = SV("--reformat"),
            .dest = ArgBitFlagDest(&RUNWHICH, RunWhich_REFORMAT),
        },
        {
            .name = SV("--expand"),
            .dest = ArgBitFlagDest(&RUNWHICH, RunWhich_EXPAND),
        },
        {
            .name = SV("--output-all"),
            .dest = ArgBitFlagDest(&RUNWHICH, RunWhich_ALL),
        },
    };
    ArgParseKwParams extra_kwargs = {
        .args = kw_args,
        .count = arrlen(kw_args),
    };
    int ret = test_main(argc, argv, &extra_kwargs);
    return ret;
}
void
null_log(void*_Nullable unused, int type, const char* filename, int filename_len, int line, int col, const char*_Nonnull message, int message_len){
    (void)unused, (void)type, (void)filename, (void)filename_len, (void)line, (void)col, (void)message, (void)message_len;
}

TestFunction(TestExamples){
    TESTBEGIN();
    if(!RUNWHICH) RUNWHICH = RunWhich_ALL;
    uint64_t flags = DNDC_FLAGS_NONE | DNDC_PRINT_STATS;
#include "TESTEXAMPLES.h"
    DndcWorkerThread* worker = dndc_worker_thread_create();
    for(int j = 0; j < 25; j++){
        THE_TestingAllocator.fail_at = -j;
        for(size_t i = 0; i < arrlen(examples); i++){
            testing_assert_all_freed();
            testing_reset();
            LongString output = {0};
            Allocator allocator = MALLOCATOR;
            TextFileResult data = read_file(examples[i].text, allocator);
            if(data.errored)
                continue;
            if(RUNWHICH & RunWhich_HTML){
                int e = run_the_dndc(OUTPUT_HTML, flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, null_log, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
                if(!e) dndc_free_string(output);
                TestExpectTrue(1);
            }
            if(RUNWHICH & RunWhich_MD){
                int e = run_the_dndc(OUTPUT_MD, flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, null_log, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
                if(!e) dndc_free_string(output);
                TestExpectTrue(1);
            }
            if(RUNWHICH & RunWhich_REFORMAT){
                int e = run_the_dndc(OUTPUT_REFORMAT, flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, null_log, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
                if(!e) dndc_free_string(output);
                TestExpectTrue(1);
            }
            if(RUNWHICH & RunWhich_EXPAND){
                int e = run_the_dndc(OUTPUT_EXPAND, flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, null_log, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
                if(!e) dndc_free_string(output);
                TestExpectTrue(1);
            }
            Allocator_free(allocator, data.result.text, data.result.length+1);
        }
    }
    dndc_worker_thread_destroy(worker);
    testing_assert_all_freed();
    TESTEND();
}
