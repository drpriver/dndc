#ifndef DNDC_QJS_H
#define DNDC_QJS_H
#include "dndc_types.h"
#include "error_handling.h"
#include "arena_allocator.h"
typedef struct QJSRuntime QJSRuntime;
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
Errorable_f(void)
execute_qjs_string(QJSRuntime*rt, DndcContext* ctx, const char* str, size_t length, NodeHandle handle);

static
QJSRuntime*
new_qjs_rt(ArenaAllocator*);

static
void 
free_qjs_rt(QJSRuntime*, ArenaAllocator*);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
