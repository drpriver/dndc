#ifndef DNDC_QJS_H
#define DNDC_QJS_H
#include "dndc_types.h"
#include "error_handling.h"
#include "arena_allocator.h"
typedef struct QJSRuntime QJSRuntime;
typedef struct QJSContext QJSContext;
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// Bitflags for controlling the qjs behavior
typedef enum DndcJsFlags {
    DNDC_JS_FLAGS_NONE = 0x0,
    // Enable the (custom) filesystem api for js
    DNDC_JS_ENABLE_FILESYSTEM = 0x1,
} DndcJsFlags;

static
Errorable_f(void)
execute_qjs_string(QJSContext*jsctx, DndcContext* ctx, const char* str, size_t length, NodeHandle handle, NodeHandle firstline);

static
QJSRuntime*
new_qjs_rt(ArenaAllocator*);

static
QJSContext*_Nullable
new_qjs_ctx(QJSRuntime*, DndcContext*, DndcJsFlags);

static
void
free_qjs_rt(QJSRuntime*, ArenaAllocator*);


#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
