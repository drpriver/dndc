#ifndef DNDC_QJS_H
#define DNDC_QJS_H
#include "dndc_long_string.h"
#include "dndc_types.h"
#include "Allocators/arena_allocator.h"
typedef struct QJSRuntime QJSRuntime;
typedef struct QJSContext QJSContext;
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
warn_unused
int
execute_qjs_string(QJSContext*jsctx, DndcContext* ctx, const char* str, size_t length, NodeHandle handle, NodeHandle firstline);

static
QJSRuntime*_Nullable
new_qjs_rt(ArenaAllocator*);

static
QJSContext*_Nullable
new_qjs_ctx(QJSRuntime*, DndcContext*, LongString);

static
void
free_qjs_rt(QJSRuntime*, ArenaAllocator*);


#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
