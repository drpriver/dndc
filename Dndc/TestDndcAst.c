//
// Copyright © 2021-2022, David Priver
//
#include <stdio.h>
#define DNDC_API static inline
#include "dndc.h"
#include "dndc_long_string.h"
#define DNDC_AST_EXAMPLE
#include "dndc_ast.h"
#include "Utils/testing.h"
#include "Utils/file_util.h"
#include "Allocators/mallocator.h"

static TestFunc TestDndcAst;
static TestFunc TestAstExample;

int main(int argc, char** argv){
    RegisterTest(TestDndcAst);
    RegisterTest(TestAstExample);
    int ret = test_main(argc, argv);
    return ret;
}


TestFunction(TestDndcAst){
    TESTBEGIN();
    unsigned long long flags = DNDC_ALLOW_BAD_LINKS | DNDC_DONT_READ;
    DndcFileCache* textcache = dndc_create_filecache();
    DndcContext* ctx = dndc_create_ctx(flags, NULL, textcache);
    dndc_ctx_set_logger(ctx, dndc_stderr_log_func, NULL);
    int e = dndc_filecache_store_text(textcache, SV("hello"), SV("hello world"), 0);
    TestExpectFalse(e);
    e = dndc_ctx_parse_string(ctx, DNDC_NODE_HANDLE_INVALID, SV("yolo"), SV("::import\n  hello\n"));
    TestExpectTrue(e);
    DndcNodeHandle root = dndc_ctx_make_root(ctx, SV("yolo"));
    TestExpectNotEquals(root, DNDC_NODE_HANDLE_INVALID);
    e = dndc_ctx_parse_string(ctx, root, SV("yolo"), SV("::import\n  hello\n"));
    TestExpectFalse(e);
    dndc_ctx_destroy(ctx);
    TESTEND();
}

TestFunction(TestAstExample){
    TESTBEGIN();
    // Verify that the output of doing the ast funcs manually matches the output of
    // the integrated function.

    // Copy paste from TestDndc.TestExamplesWork
    // Possibly should put in common header.
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
        ;
#include "TESTEXAMPLES.h"
    _Static_assert(arrlen(base_dirs) == arrlen(examples), "");
    for(size_t i = 0; i < arrlen(examples); i++){
        LongString ast_output = {0};
        LongString dnd_output = {0};
        Allocator allocator = get_mallocator();
        TextFileResult data = read_file(examples[i].text, allocator);
        if(data.errored){
            TestPrintValue("Unable to open: examples[i]", examples[i]);
        }
        TestAssertSuccess(data);
        {
            int e;
            e = dndc_compile_dnd_file(flags, base_dirs[i], LS_to_SV(data.result), LS_to_SV(examples[i]), &dnd_output, NULL, NULL, dndc_stderr_log_func, NULL, NULL, LS(""));
            if(e){
                TestPrintValue("dndc_compile_dnd failed, example:", examples[i]);
                TestPrintValue("Base dir:", base_dirs[i]);
            }
            TestAssertFalse(e);
            e = compile_dnd_to_html(base_dirs[i], LS_to_SV(examples[i]), LS_to_SV(data.result), &ast_output);
            if(e){
                TestPrintValue("compile_dnd_to_html failed, example:", examples[i]);
                TestPrintValue("Base dir:", base_dirs[i]);
            }
            TestAssertFalse(e);
            TestExpectEquals2(LS_equals, ast_output, dnd_output);
            if(!LS_equals(ast_output, dnd_output)){
                TestPrintValue("Example failed:", examples[i]);
                TestPrintValue("Base dir:", base_dirs[i]);
                TEST_stats.assert_failures++;
                return TEST_stats;
            }
            dndc_free_string(ast_output);
            dndc_free_string(dnd_output);
        }
        Allocator_free(allocator, data.result.text, data.result.length+1);
    }
    TESTEND();
}
#include "dndc_node_types.h"
PushDiagnostic();
SuppressEnumCompare();
#define X(a, b) _Static_assert(NODE_##a == DNDC_NODE_TYPE_##a, #a " doesn't match in public and private header!");
NODETYPES(X)
#undef X
PopDiagnostic();



// include at end so we don't accidentally use private API
#include "dndc.c"
