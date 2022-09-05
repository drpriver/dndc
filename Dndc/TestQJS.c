//
// Copyright © 2022, David Priver
//

// Run's qjs's tests to make sure my changes to quickjs haven't broken it.
// #define HEAVY_RECORDING
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#include "Allocators/testing_allocator.h"
#include "Allocators/arena_allocator.h"
#include "Utils/testing.h"
#include "Utils/MStringBuilder.h"
#include "Utils/file_util.h"
#include "Vendored/quickjs/quickjs.h"

static TestFunc TestLanguage;
static TestFunc TestBuiltin;
static TestFunc TestLoop;

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTest(TestLanguage);
    RegisterTest(TestBuiltin);
    RegisterTest(TestLoop);
    int ret = test_main(argc, argv, NULL);
    testing_assert_all_freed();
    return ret;
}

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

typedef union SizeHeader SizeHeader;
union SizeHeader {
    size_t size;
    double _f;
    void* _p;
};

static
void*_Nullable
js_arena_malloc(QJSMallocState*s, size_t size){
    // This is stupid that we have to store the size in
    // the pointer, but how else are we going to
    // support realloc.
    size_t true_size = size + sizeof(SizeHeader);
    SizeHeader* p = ArenaAllocator_alloc(s->opaque, true_size);
    if(unlikely(!p))
        return NULL;
    p->size = true_size;
    return p+1;
}

static
void
js_arena_free(QJSMallocState*s, void* ptr){
    (void)ptr, (void)s;
}

// This is so dumb. They know what size of allocation
// they have, why don't they tell us?
static
void*_Nullable
js_arena_realloc(QJSMallocState*s, void* pointer, size_t size){
    if(!size && !pointer)
        return NULL;
    void* result;
    if(pointer){
        SizeHeader* true_pointer = ((SizeHeader*)pointer)-1;
        size_t true_size = size + sizeof(SizeHeader);
        SizeHeader* new_pointer = ArenaAllocator_realloc(s->opaque, true_pointer, true_pointer->size, true_size);
        if(!new_pointer) return NULL;
        new_pointer->size = true_size;
        result = new_pointer+1;
    }
    else
        result = js_arena_malloc(s, size);
    return result;
}

static
QJSRuntime*_Nullable
new_qjs_rt(ArenaAllocator* aa){
    static const QJSMallocFunctions mf = {
        .js_malloc = js_arena_malloc,
        .js_free = js_arena_free,
        .js_realloc = js_arena_realloc,
    };
    QJSRuntime* rt = NULL;
    rt = QJS_NewRuntime2(&mf, aa);
    if(unlikely(!rt)) return NULL;
    return rt;
}

static
void
free_qjs_rt(QJSRuntime* rt, ArenaAllocator* arena){
    (void)rt;
    ArenaAllocator_free_all(arena);
}

static
QJSContext*_Nullable
new_qjs_ctx(QJSRuntime* rt){
    QJSContext* jsctx = NULL;
    jsctx = QJS_NewContext(rt);
    if(!jsctx) goto fail;
    return jsctx;

    fail:
    if(jsctx)
        QJS_FreeContext(jsctx);
    return NULL;
}

// copy-pasted from dndc_qjs
static
void
log_js_traceback(QJSContext* jsctx){
    MStringBuilder msb = {.allocator = MALLOCATOR};
    QJSValue exception_val = QJS_GetException(jsctx);
    int is_error = QJS_IsError(jsctx, exception_val);
    {
        const char *str = QJS_ToCString(jsctx, exception_val);
        if(str){
            msb_write_str(&msb, str, strlen(str));
            msb_write_char(&msb, '\n');
            QJS_FreeCString(jsctx, str);
        }
        else {
            msb_write_literal(&msb, "[exception]\n");
        }
    }
    if(is_error){
        QJSValue val = QJS_GetPropertyStr(jsctx, exception_val, "stack");
        if(!QJS_IsUndefined(val)){
            const char *str = QJS_ToCString(jsctx, val);
            if(str){
                msb_write_str(&msb, str, strlen(str));
                msb_write_char(&msb, '\n');
                QJS_FreeCString(jsctx, str);
            }
            else {
                msb_write_literal(&msb, "[exception]\n");
            }
        }
        QJS_FreeValue(jsctx, val);
    }
    QJS_FreeValue(jsctx, exception_val);
    msb_erase(&msb, 2); // XXX: I get the one, but why two?
    msb_nul_terminate(&msb);
    if(unlikely(msb.errored)) return;
    LongString msg = msb_borrow_ls(&msb);
    fprintf(stderr, "%s\n", msg.text);
    msb_destroy(&msb);
}

//
// The main execution function.
// str must be nul-terminated (underlying library). I should
// probably type it as a LongString
//
static
warn_unused
int
execute_qjs_string(QJSContext* jsctx, const char* filename, const char* str, size_t length){
    int result = 0;

    {
        QJSValue err = QJS_Eval(jsctx, str, length, filename, 0);
        if(QJS_IsException(err)){
            log_js_traceback(jsctx);
            result = 1;
        }
        QJS_FreeValue(jsctx, err);
    }

    return result;
}
static
struct TestStats
run_test(const char* filename){
    TESTBEGIN();
    ArenaAllocator aa = {0};
    QJSRuntime* rt = new_qjs_rt(&aa);
    TestAssert(rt);
    TextFileResult fr = read_file(filename, MALLOCATOR);
    TestAssertSuccess(fr);
    LongString text = fr.result;
    QJSContext* ctx = new_qjs_ctx(rt);
    TestAssert(ctx);

    int err = execute_qjs_string(ctx, filename, text.text, text.length);
    TestExpectFalse(err);

    Allocator_free(MALLOCATOR, text.text, text.length+1);
    QJS_FreeContext(ctx);
    free_qjs_rt(rt, &aa);
    ArenaAllocator_free_all(&aa);
    TESTEND();
}

#ifdef __clang__
#pragma clang assume_nonnull end
#else
#define _Nullable
#endif


TestFunction(TestBuiltin){
    const char* filename = "Vendored/quickjs/tests/test_builtin.js";
    return run_test(filename);
}

TestFunction(TestLanguage){
    const char* filename = "Vendored/quickjs/tests/test_language.js";
    return run_test(filename);
}

TestFunction(TestLoop){
    const char* filename = "Vendored/quickjs/tests/test_loop.js";
    return run_test(filename);
}

#include "Allocators/allocator.c"
#define TESTING_QJS
#include "Vendored/libquickjs.c"
