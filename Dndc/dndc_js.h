//
// Copyright © 2022-2024, David Priver <david@davidpriver.com>
//
#ifndef DNDC_JS_H
#define DNDC_JS_H
#include "dndc_long_string.h"
#include "dndc_types.h"
#include "Allocators/arena_allocator.h"

typedef struct DndcJsRuntime DndcJsRuntime;
typedef struct DndcJsContext DndcJsContext;

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
warn_unused
int
dndc_execute_js_string(DndcJsContext*jsctx, DndcContext* ctx, const char* str, size_t length, NodeHandle handle, NodeHandle firstline);

static
DndcJsRuntime*_Nullable
dndc_new_js_rt(ArenaAllocator*);

static
DndcJsContext*_Nullable
dndc_new_js_ctx(DndcJsRuntime*, DndcContext*, LongString);

static
void
dndc_free_js_rt(DndcJsRuntime*, ArenaAllocator*);


#ifdef __clang__
#pragma clang assume_nonnull end
#endif


#endif
