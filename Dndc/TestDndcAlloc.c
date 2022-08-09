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
static TestFunc TestExamplesDeep;

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTestFlags(TestExamples,     TEST_CASE_FLAGS_NONE);
    RegisterTestFlags(TestExamplesDeep, TEST_CASE_FLAGS_SKIP_UNLESS_NAMED);
    int ret = test_main(argc, argv, NULL);
    return ret;
}
void
null_log(void*_Nullable unused, int type, const char* filename, int filename_len, int line, int col, const char*_Nonnull message, int message_len){
    (void)unused, (void)type, (void)filename, (void)filename_len, (void)line, (void)col, (void)message, (void)message_len;
}

TestFunction(TestExamplesDeep){
    TESTBEGIN();
    uint64_t flags = DNDC_FLAGS_NONE | DNDC_PRINT_STATS;
#include "TESTEXAMPLES.h"
    DndcWorkerThread* worker = dndc_worker_thread_create();
    for(int j = 0; j < 800; j++){
        THE_TestingAllocator.fail_at = -j;
        for(size_t i = 0; i < arrlen(examples); i++){
            testing_assert_all_freed();
            TestExpectTrue(1);
            testing_reset();
            LongString output = {0};
            Allocator allocator = MALLOCATOR;
            TextFileResult data = read_file(examples[i].text, allocator);
            if(data.errored)
                continue;
            {
                int e = run_the_dndc(OUTPUT_HTML, flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, null_log, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
                if(!e) dndc_free_string(output);
            }
            {
                int e = run_the_dndc(OUTPUT_MD, flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, null_log, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
                if(!e) dndc_free_string(output);
            }
            {
                int e = run_the_dndc(OUTPUT_REFORMAT, flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, null_log, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
                if(!e) dndc_free_string(output);
            }
            {
                int e = run_the_dndc(OUTPUT_EXPAND, flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, null_log, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
                if(!e) dndc_free_string(output);
            }
            Allocator_free(allocator, data.result.text, data.result.length+1);
        }
    }
    dndc_worker_thread_destroy(worker);
    testing_assert_all_freed();
    TESTEND();
}

TestFunction(TestExamples){
    TESTBEGIN();
    uint64_t flags = DNDC_FLAGS_NONE | DNDC_PRINT_STATS;
#include "TESTEXAMPLES.h"
    DndcWorkerThread* worker = dndc_worker_thread_create();
    // accelerating step
    for(int j = 0, step=1; j < 800; j+=step, step+=7){
        THE_TestingAllocator.fail_at = -j;
        for(size_t i = 0; i < arrlen(examples); i++){
            testing_assert_all_freed();
            TestExpectTrue(1);
            testing_reset();
            LongString output = {0};
            Allocator allocator = MALLOCATOR;
            TextFileResult data = read_file(examples[i].text, allocator);
            if(data.errored)
                continue;
            {
                int e = run_the_dndc(OUTPUT_HTML, flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, null_log, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
                if(!e) dndc_free_string(output);
            }
            {
                int e = run_the_dndc(OUTPUT_MD, flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, null_log, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
                if(!e) dndc_free_string(output);
            }
            {
                int e = run_the_dndc(OUTPUT_REFORMAT, flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, null_log, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
                if(!e) dndc_free_string(output);
            }
            {
                int e = run_the_dndc(OUTPUT_EXPAND, flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &output, NULL, NULL, null_log, NULL, NULL, NULL, NULL, NULL, NULL, LS(""));
                if(!e) dndc_free_string(output);
            }
            Allocator_free(allocator, data.result.text, data.result.length+1);
        }
    }
    dndc_worker_thread_destroy(worker);
    testing_assert_all_freed();
    TESTEND();
}
