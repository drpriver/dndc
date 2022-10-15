//
// Copyright © 2021-2022, David Priver
//

// Unity build simplifies build system and also allows better
// control of visibility of symbols for library.
#define CONFIG_VERSION "2021-03-27"

#if defined(__clang__)
#ifdef _WIN32
#pragma clang diagnostic ignored "-Wmicrosoft-enum-forward-reference"
#endif
// These are basically harmless.
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wbad-function-cast"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
// These warrant investigation.
#pragma clang diagnostic ignored "-Wduplicate-enum"
#pragma clang diagnostic ignored "-Wconditional-uninitialized"
// Can be a real bug, but too annoying
#pragma clang diagnostic ignored "-Wsign-compare"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wbad-function-cast"
// What is up with these?
#pragma GCC diagnostic ignored "-Wcast-function-type"
// Can be a real bug, but too annoying
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wattributes"
#endif


#include "quickjs/cutils.c"

// definitions conflict with the ones in quickjs.
#define compute_stack_size re_compute_stack_size
#define is_digit re_is_digit
#include "quickjs/libregexp.c"
#undef is_digit
#undef compute_stack_size

#include "quickjs/libunicode.c"

#if !defined(QJS_API)
#if !defined(BUILDING_SHARED_OBJECT)

#if defined(_MSC_VER) && !defined(__clang__)
#define QJS_API extern
#else
#define QJS_API extern __attribute__((visibility("hidden")))
#endif

#elif defined(_WIN32)
#define QJS_API __declspec(dllexport)
#else
#define QJS_API extern __attribute__((visibility("default")))
#endif
#endif

#include "quickjs/quickjs.c"

//
// NOTE(dpriver): This was not originally in quickjs
// I added it so I could implement logging
//
QJS_API
int
QJS_get_caller_location(QJSContext* ctx, const char** filename, const char** funcname, int* line_num){
    QJSStackFrame* sf = ctx->rt->current_stack_frame;
    if(sf == NULL) return -1;
    sf = sf->prev_frame;
    if(sf == NULL) return -1;
    if(funcname) *funcname = get_func_name(ctx, sf->cur_func);
    QJSObject* p = QJS_VALUE_GET_OBJ(sf->cur_func);
    if(js_class_has_bytecode(p->class_id)){
        QJSFunctionBytecode* b = p->u.func.function_bytecode;
        if(b->has_debug) {
            if(line_num)
                *line_num = find_line_num(ctx, b, sf->cur_pc - b->byte_code_buf - 1);
            if(filename)
                *filename = QJS_AtomToCString(ctx, b->debug.filename);
        }
    }
    return 0;
}

//
// NOTE(dpriver): This not being exposed was super
// annoying.
//
QJS_API
QJSValue
QJS_ArrayPush(QJSContext *ctx, QJSValueConst this_val, int argc, QJSValueConst *argv){
    return js_array_push(ctx, this_val, argc, argv, 0);
}
