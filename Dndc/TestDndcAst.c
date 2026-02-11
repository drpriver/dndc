//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//

#include "compiler_warnings.h"
// #define HEAVY_RECORDING
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#include "common_macros.h"
#include "Allocators/testing_allocator.h"
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
static TestFunc TestParseClasses;
static TestFunc TestParseAttributes;

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTest(TestDndcAst);
    RegisterTest(TestAstExample);
    RegisterTest(TestParseClasses);
    RegisterTest(TestParseAttributes);
    ArgToParse kw_args[] = {
        {
            .name = SV("-F"),
            .altname1 = SV("--fail-at"),
            .help = "Fail after this many allocations",
            .dest = ARGDEST(&THE_TestingAllocator.fail_at),
        }
    };
    ArgParseKwParams extra_kwargs = {
        .args = kw_args,
        .count = arrlen(kw_args),
    };
    int ret = test_main(argc, argv, &extra_kwargs);
    testing_assert_all_freed();
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
    dndc_filecache_destroy(textcache);
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
        Allocator allocator = MALLOCATOR;
        LongString data;
        FileError ferr = read_file(examples[i].text, allocator, &data);
        if(ferr.errored){
            TestPrintValue("Unable to open: examples[i]", examples[i]);
        }
        TestAssertSuccess(ferr);
        {
            int e;
            e = dndc_compile_dnd_file(flags, base_dirs[i], LS_to_SV(data), LS_to_SV(examples[i]), &dnd_output, NULL, NULL, dndc_stderr_log_func, NULL, NULL, LS(""));
            if(e){
                TestPrintValue("dndc_compile_dnd failed, example:", examples[i]);
                TestPrintValue("Base dir:", base_dirs[i]);
            }
            TestAssertFalse(e);
            e = compile_dnd_to_html(base_dirs[i], LS_to_SV(examples[i]), LS_to_SV(data), &ast_output);
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
        Allocator_free(allocator, data.text, data.length+1);
    }
    TESTEND();
}

TestFunction(TestParseClasses){
    TESTBEGIN();
    const struct {StringView input; StringView cls;} testclasses[] = {
#define X(cls_) {.input=SV("::md #id(test) ." cls_), .cls=SV(cls_)}
        X("foo"),
        X("foo-bar"),
        X("foo_bar"),
        X("_bar"),
        X("-bar"),
        X("AYY"),
#undef X
    };
    for(size_t i = 0; i < arrlen(testclasses); i++){
        StringView input = testclasses[i].input;
        StringView cls = testclasses[i].cls;
        DndcContext* ctx = dndc_create_ctx(0, NULL, NULL);
        dndc_ctx_set_logger(ctx, dndc_stderr_log_func, NULL);
        TestAssert(ctx);
        DndcNodeHandle root = dndc_ctx_make_root(ctx, SV("foo"));
        TestAssertNotEqual(root, DNDC_NODE_HANDLE_INVALID);
        int err = dndc_ctx_parse_string(ctx, root, SV("foo"), input);
        TestAssertEquals(err, 0);
        DndcNodeHandle handle = dndc_ctx_node_by_id(ctx, SV("test"));
        TestAssertNotEqual(handle, DNDC_NODE_HANDLE_INVALID);
        int has_it = dndc_node_has_class(ctx, handle, cls);
        TestExpectTrue(has_it);
        dndc_ctx_destroy(ctx);
    }
    TESTEND();
}
TestFunction(TestParseAttributes){
    TESTBEGIN();
    PushDiagnostic();
    SuppressStringPlusInt();
    const struct {StringView input; StringView attr; StringView arg;} testattrs[] = {
#define X(attr_, arg_) {.input=SV("::md #id(test) @" attr_ arg_), .attr=SV(attr_), .arg=sizeof(arg_)>1?(StringView){sizeof(arg_)-3, arg_+1}:SV(arg_)}
        X("foo", ""),
        X("foo", "(1)"),
        X("foo", "(ayy lmao)"),
        X("foo", "(1, 2, 3, 4)"),
        X("foo", "(1, 2, 3, 4)"),
        X("foo-bar", ""),
        X("foo_bar", ""),
        X("_bar", ""),
        X("-bar", ""),
        X("AYY", ""),
#undef X
    };
    PopDiagnostic();
    for(size_t i = 0; i < arrlen(testattrs); i++){
        StringView input = testattrs[i].input;
        StringView attr = testattrs[i].attr;
        StringView arg = testattrs[i].arg;
        DndcContext* ctx = dndc_create_ctx(0, NULL, NULL);
        dndc_ctx_set_logger(ctx, dndc_stderr_log_func, NULL);
        TestAssert(ctx);
        DndcNodeHandle root = dndc_ctx_make_root(ctx, SV("foo"));
        TestAssertNotEqual(root, DNDC_NODE_HANDLE_INVALID);
        int err = dndc_ctx_parse_string(ctx, root, SV("foo"), input);
        TestAssertEquals(err, 0);
        DndcNodeHandle handle = dndc_ctx_node_by_id(ctx, SV("test"));
        TestAssertNotEqual(handle, DNDC_NODE_HANDLE_INVALID);
        int has_it = dndc_node_has_attribute(ctx, handle, attr);
        TestExpectTrue(has_it);
        if(arg.length){
            StringView value;
            int e = dndc_node_get_attribute(ctx, handle, attr, &value);
            TestExpectFalse(e);
            if(e){
                TestPrintValue("attr", attr);
                TestPrintValue("arg", arg);
                TestPrintValue("input", input);
            }
            else {
                TestExpectEquals2(SV_equals, arg, value);
            }
        }
        dndc_ctx_destroy(ctx);
    }
    TESTEND();
}
#include "dndc_node_types.h"
PushDiagnostic();
SuppressEnumCompare();
#define X(a, b) _Static_assert((int)NODE_##a == (int)DNDC_NODE_TYPE_##a, #a " doesn't match in public and private header!");
NODETYPES(X)
#undef X
PopDiagnostic();



// include at end so we don't accidentally use private API
#include "dndc.c"
