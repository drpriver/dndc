//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
#ifndef DNDC_LOGGING_C
#define DNDC_LOGGING_C
#include "dndc_logging.h"
#include "dndc_funcs.h"
#include "common_macros.h"
#include "Allocators/nullacator.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

PushDiagnostic()
SuppressUnusedFunction()
static
void
log_error(DndcContext* ctx, StringView filename, int line, int col, size_t nargs, FormatArg* args){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(!ctx->log_func)
        return;
    MStringBuilder sb = {.allocator=temp_allocator(ctx)};
    for(size_t i = 0; i < nargs; i++)
        msb_apply_format(&sb, args[i]);
    LongString mess = msb_borrow_ls(&sb);
    ctx->log_func(ctx->log_user_data, DNDC_ERROR_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&sb);
}

static
void
log_warning(DndcContext* ctx, StringView filename, int line, int col, size_t nargs, FormatArg* args){
    if(ctx->flags & DNDC_SUPPRESS_WARNINGS)
        return;
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(!ctx->log_func)
        return;
    MStringBuilder sb = {.allocator=temp_allocator(ctx)};
    for(size_t i = 0; i < nargs; i++)
        msb_apply_format(&sb, args[i]);
    msb_nul_terminate(&sb);
    if(unlikely(sb.errored)) return;
    LongString mess = msb_borrow_ls(&sb);
    ctx->log_func(ctx->log_user_data, DNDC_WARNING_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&sb);
}

static
void
log_info(DndcContext* ctx, StringView filename, int line, int col, size_t nargs, FormatArg* args){
    if(!(ctx->flags & DNDC_PRINT_STATS))
        return;
    if(!ctx->log_func)
        return;
    MStringBuilder sb = {.allocator=temp_allocator(ctx)};
    for(size_t i = 0; i < nargs; i++)
        msb_apply_format(&sb, args[i]);
    msb_nul_terminate(&sb);
    if(unlikely(sb.errored)) return;
    LongString mess = msb_borrow_ls(&sb);
    ctx->log_func(ctx->log_user_data, DNDC_STATISTIC_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&sb);
}

static
void
log_debug(DndcContext* ctx, StringView filename, int line, int col, size_t nargs, FormatArg* args){
    if(!ctx->log_func)
        return;
    MStringBuilder sb = {.allocator=temp_allocator(ctx)};
    for(size_t i = 0; i < nargs; i++)
        msb_apply_format(&sb, args[i]);
    msb_nul_terminate(&sb);
    if(unlikely(sb.errored)) return;
    LongString mess = msb_borrow_ls(&sb);
    ctx->log_func(ctx->log_user_data, DNDC_DEBUG_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&sb);
}

static
void
node_log_error(DndcContext* ctx, const Node* node, size_t nargs, FormatArg* args){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(!ctx->log_func)
        return;
    MStringBuilder sb = {.allocator=temp_allocator(ctx)};
    for(size_t i = 0; i < nargs; i++)
        msb_apply_format(&sb, args[i]);
    msb_nul_terminate(&sb);
    if(sb.errored) return;
    msb_nul_terminate(&sb);
    if(unlikely(sb.errored)) return;
    LongString mess = msb_borrow_ls(&sb);
    StringView filename = ctx->filenames.data[node->filename_idx];
    int line = node->row;
    int col = node->col;
    ctx->log_func(ctx->log_user_data, DNDC_ERROR_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&sb);
}

static
void
node_log_err_offset(DndcContext* ctx, const Node* node, int offset, LongString mess){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(!ctx->log_func)
        return;
    StringView filename = ctx->filenames.data[node->filename_idx];
    int line = node->row;
    int col = node->col + offset;
    ctx->log_func(ctx->log_user_data, DNDC_ERROR_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
}

static
void
handle_log_err_offset(DndcContext* ctx, NodeHandle handle, int offset, LongString mess){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(!ctx->log_func)
        return;
    Node* node = get_node(ctx, handle);
    StringView filename = ctx->filenames.data[node->filename_idx];
    int line = node->row;
    int col = node->col + offset;
    ctx->log_func(ctx->log_user_data, DNDC_ERROR_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
}

static
void
node_log_warning(DndcContext* ctx, const Node* node, size_t nargs, FormatArg* args){
    if(ctx->flags & DNDC_SUPPRESS_WARNINGS)
        return;
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(!ctx->log_func)
        return;
    MStringBuilder sb = {.allocator=temp_allocator(ctx)};
    for(size_t i = 0; i < nargs; i++)
        msb_apply_format(&sb, args[i]);
    msb_nul_terminate(&sb);
    if(unlikely(sb.errored)) return;
    LongString mess = msb_borrow_ls(&sb);
    StringView filename = ctx->filenames.data[node->filename_idx];
    int line = node->row;
    int col = node->col;
    ctx->log_func(ctx->log_user_data, DNDC_WARNING_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&sb);
}

static
void
node_log_info(DndcContext* ctx, const Node* node, size_t nargs, FormatArg* args){
    if(!(ctx->flags & DNDC_PRINT_STATS))
        return;
    if(!ctx->log_func)
        return;
    MStringBuilder sb = {.allocator=temp_allocator(ctx)};
    for(size_t i = 0; i < nargs; i++)
        msb_apply_format(&sb, args[i]);
    msb_nul_terminate(&sb);
    if(unlikely(sb.errored)) return;
    LongString mess = msb_borrow_ls(&sb);
    StringView filename = ctx->filenames.data[node->filename_idx];
    int line = node->row;
    int col = node->col;
    ctx->log_func(ctx->log_user_data, DNDC_STATISTIC_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&sb);
}

static
void
node_log_debug(DndcContext* ctx, const Node* node, size_t nargs, FormatArg* args){
    if(!ctx->log_func)
        return;
    MStringBuilder sb = {.allocator=temp_allocator(ctx)};
    for(size_t i = 0; i < nargs; i++)
        msb_apply_format(&sb, args[i]);
    msb_nul_terminate(&sb);
    if(unlikely(sb.errored)) return;
    LongString mess = msb_borrow_ls(&sb);
    StringView filename = ctx->filenames.data[node->filename_idx];
    int line = node->row;
    int col = node->col;
    ctx->log_func(ctx->log_user_data, DNDC_DEBUG_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&sb);
}

static
void
handle_log_error(DndcContext* ctx, NodeHandle handle, size_t nargs, FormatArg* args){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(!ctx->log_func)
        return;
    Node* node = get_node(ctx, handle);
    MStringBuilder sb = {.allocator=temp_allocator(ctx)};
    for(size_t i = 0; i < nargs; i++)
        msb_apply_format(&sb, args[i]);
    msb_nul_terminate(&sb);
    if(unlikely(sb.errored)) return;
    LongString mess = msb_borrow_ls(&sb);
    StringView filename = ctx->filenames.data[node->filename_idx];
    int line = node->row;
    int col = node->col;
    ctx->log_func(ctx->log_user_data, DNDC_ERROR_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&sb);
}

static
void
handle_log_warning(DndcContext* ctx, NodeHandle handle, size_t nargs, FormatArg* args){
    if(ctx->flags & DNDC_SUPPRESS_WARNINGS)
        return;
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(!ctx->log_func)
        return;
    Node* node = get_node(ctx, handle);
    MStringBuilder sb = {.allocator=temp_allocator(ctx)};
    for(size_t i = 0; i < nargs; i++)
        msb_apply_format(&sb, args[i]);
    msb_nul_terminate(&sb);
    if(unlikely(sb.errored)) return;
    LongString mess = msb_borrow_ls(&sb);
    StringView filename = ctx->filenames.data[node->filename_idx];
    int line = node->row;
    int col = node->col;
    ctx->log_func(ctx->log_user_data, DNDC_WARNING_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&sb);
}

static
void
handle_log_info(DndcContext* ctx, NodeHandle handle, size_t nargs, FormatArg* args){
    if(!(ctx->flags & DNDC_PRINT_STATS))
        return;
    if(!ctx->log_func)
        return;
    Node* node = get_node(ctx, handle);
    MStringBuilder sb = {.allocator=temp_allocator(ctx)};
    for(size_t i = 0; i < nargs; i++)
        msb_apply_format(&sb, args[i]);
    msb_nul_terminate(&sb);
    if(unlikely(sb.errored)) return;
    LongString mess = msb_borrow_ls(&sb);
    StringView filename = ctx->filenames.data[node->filename_idx];
    int line = node->row;
    int col = node->col;
    ctx->log_func(ctx->log_user_data, DNDC_STATISTIC_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&sb);
}

static
void
handle_log_debug(DndcContext* ctx, NodeHandle handle, size_t nargs, FormatArg* args){
    if(!ctx->log_func)
        return;
    Node* node = get_node(ctx, handle);
    MStringBuilder sb = {.allocator=temp_allocator(ctx)};
    for(size_t i = 0; i < nargs; i++)
        msb_apply_format(&sb, args[i]);
    msb_nul_terminate(&sb);
    if(unlikely(sb.errored)) return;
    LongString mess = msb_borrow_ls(&sb);
    StringView filename = ctx->filenames.data[node->filename_idx];
    int line = node->row;
    int col = node->col;
    ctx->log_func(ctx->log_user_data, DNDC_DEBUG_MESSAGE, filename.text, filename.length, line, col, mess.text, mess.length);
    msb_destroy(&sb);
}

static
void
report_time(DndcContext* ctx, StringView msg, uint64_t microseconds){
    // This is called after freeing the allocators, thus the stack allocation.
    if(! (ctx->flags & DNDC_PRINT_STATS))
        return;
    if(! ctx->log_func)
        return;
    char buff[512];
    MStringBuilder temp = {.allocator=NULLACATOR, .capacity=sizeof buff, .data=buff};
    msb_write_str(&temp, msg.text, msg.length);
    msb_write_us_as_ms(&temp, microseconds);
    msb_nul_terminate(&temp);
    if(unlikely(temp.errored))
        return;
    LongString str = msb_borrow_ls(&temp);
    ctx->log_func(ctx->log_user_data, DNDC_STATISTIC_MESSAGE, "", 0, 0, 0, str.text, str.length);
}

static
void
report_size(DndcContext* ctx, StringView msg, uint64_t size){
    if(! (ctx->flags & DNDC_PRINT_STATS))
        return;
    if(! ctx->log_func)
        return;
    MStringBuilder temp = {.allocator=temp_allocator(ctx)};
    msb_write_str(&temp, msg.text, msg.length);
    msb_write_uint64(&temp, size);
    msb_nul_terminate(&temp);
    if(unlikely(temp.errored)) return;
    LongString str = msb_borrow_ls(&temp);
    ctx->log_func(ctx->log_user_data, DNDC_STATISTIC_MESSAGE, "", 0, 0, 0, str.text, str.length);
    msb_destroy(&temp);
}

static
void
report_system_error(DndcContext* ctx, StringView msg){
    if(ctx->flags & DNDC_DONT_PRINT_ERRORS)
        return;
    if(! ctx->log_func)
        return;
    ctx->log_func(ctx->log_user_data, DNDC_NODELESS_MESSAGE, "", 0, 0, 0, msg.text, msg.length);
}
PopDiagnostic()
#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
