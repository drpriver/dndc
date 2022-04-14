#include "dndc_api_def.h"
#include "dndc.h"
#include "dndc_long_string.h"
#include "dndc_ast.h"
#include "testing.h"

static TestFunc TestDndcAst;

int main(int argc, char** argv){
    RegisterTest(TestDndcAst);
    int ret = test_main(argc, argv);
    return ret;
}


TestFunction(TestDndcAst){
    TESTBEGIN();
    unsigned long long flags = DNDC_ALLOW_BAD_LINKS;
    DndcContext* ctx = dndc_create_ctx(flags, dndc_stderr_error_func, NULL, NULL, NULL, SV(""), SV(""), 0);
    int e = dndc_ctx_store_builtin_file(ctx, SV("hello"), SV("hello world"));
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

#include "dndc.c"
