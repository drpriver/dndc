//
// Copyright © 2021-2022, David Priver
//
#ifndef DNDC_LOGGING_H
#define DNDC_LOGGING_H
#include "dndc_long_string.h"
#include "dndc_types.h"
#include "Utils/msb_format.h"

#define SELECT_LOG_FORMAT_IMPL(n) LOG_FORMAT_IMPL##n
#define SELECT_LOG_FORMAT(n) SELECT_LOG_FORMAT_IMPL(n)

#define LOG_FORMAT_IMPL1(a) \
    FMT(a)
#define LOG_FORMAT_IMPL2(a, b) \
    FMT(a), FMT(b)
#define LOG_FORMAT_IMPL3(a, b, c) \
    FMT(a), FMT(b), FMT(c)
#define LOG_FORMAT_IMPL4(a, b, c, d) \
    FMT(a), FMT(b), FMT(c), FMT(d)
#define LOG_FORMAT_IMPL5(a, b, c, d, e) \
    FMT(a), FMT(b), FMT(c), FMT(d), FMT(e)
#define LOG_FORMAT_IMPL6(a, b, c, d, e, f) \
    FMT(a), FMT(b), FMT(c), FMT(d), FMT(e), FMT(f)
#define LOG_FORMAT_IMPL7(a, b, c, d, e, f, g) \
    FMT(a), FMT(b), FMT(c), FMT(d), FMT(e), FMT(f), FMT(g)
#define LOG_FORMAT_IMPL8(a, b, c, d, e, f, g, h) \
    FMT(a), FMT(b), FMT(c), FMT(d), FMT(e), FMT(f), FMT(g), FMT(h)
#define LOG_FORMAT_IMPL9(a, b, c, d, e, f, g, h, i) \
    FMT(a), FMT(b), FMT(c), FMT(d), FMT(e), FMT(f), FMT(g), FMT(h), FMT(i)

#define LOG_FORMAT(...) SELECT_LOG_FORMAT(COUNT_MACRO_ARGS(__VA_ARGS__))(__VA_ARGS__)

#define LOG_ERROR(ctx, fn, line, col, ...) do { \
    FormatArg _format_args[] = {LOG_FORMAT(__VA_ARGS__)}; \
    log_error(ctx, fn, line, col, sizeof(_format_args)/sizeof(_format_args[0]), _format_args); \
}while(0)

#define LOG_WARNING(ctx, fn, line, col, ...) do { \
    FormatArg _format_args[] = {LOG_FORMAT(__VA_ARGS__)}; \
    log_warning(ctx, fn, line, col, sizeof(_format_args)/sizeof(_format_args[0]), _format_args); \
}while(0)

#define LOG_INFO(ctx, fn, line, col, ...) do { \
    FormatArg _format_args[] = {LOG_FORMAT(__VA_ARGS__)}; \
    log_info(ctx, fn, line, col, sizeof(_format_args)/sizeof(_format_args[0]), _format_args); \
}while(0)

#define LOG_DEBUG(ctx, fn, line, col, ...) do { \
    FormatArg _format_args[] = {LOG_FORMAT(__VA_ARGS__)}; \
    log_debug(ctx, fn, line, col, sizeof(_format_args)/sizeof(_format_args[0]), _format_args); \
}while(0)

#define NODE_LOG_ERROR(ctx, node, ...) do { \
    FormatArg _format_args[] = {LOG_FORMAT(__VA_ARGS__)}; \
    node_log_error(ctx, node, sizeof(_format_args)/sizeof(_format_args[0]), _format_args); \
}while(0)

#define NODE_LOG_WARNING(ctx, node, ...) do { \
    FormatArg _format_args[] = {LOG_FORMAT(__VA_ARGS__)}; \
    node_log_warning(ctx, node, sizeof(_format_args)/sizeof(_format_args[0]), _format_args); \
}while(0)

#define NODE_LOG_INFO(ctx, node, ...) do { \
    FormatArg _format_args[] = {LOG_FORMAT(__VA_ARGS__)}; \
    node_log_info(ctx, node, sizeof(_format_args)/sizeof(_format_args[0]), _format_args); \
}while(0)

#define NODE_LOG_DEBUG(ctx, node, ...) do { \
    FormatArg _format_args[] = {LOG_FORMAT(__VA_ARGS__)}; \
    node_log_debug(ctx, node, sizeof(_format_args)/sizeof(_format_args[0]), _format_args); \
}while(0)

#define HANDLE_LOG_ERROR(ctx, node, ...) do { \
    FormatArg _format_args[] = {LOG_FORMAT(__VA_ARGS__)}; \
    handle_log_error(ctx, node, sizeof(_format_args)/sizeof(_format_args[0]), _format_args); \
}while(0)

#define HANDLE_LOG_WARNING(ctx, node, ...) do { \
    FormatArg _format_args[] = {LOG_FORMAT(__VA_ARGS__)}; \
    handle_log_warning(ctx, node, sizeof(_format_args)/sizeof(_format_args[0]), _format_args); \
}while(0)

#define HANDLE_LOG_INFO(ctx, node, ...) do { \
    FormatArg _format_args[] = {LOG_FORMAT(__VA_ARGS__)}; \
    handle_log_info(ctx, node, sizeof(_format_args)/sizeof(_format_args[0]), _format_args); \
}while(0)

#define HANDLE_LOG_DEBUG(ctx, node, ...) do { \
    FormatArg _format_args[] = {LOG_FORMAT(__VA_ARGS__)}; \
    handle_log_debug(ctx, node, sizeof(_format_args)/sizeof(_format_args[0]), _format_args); \
}while(0)

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// Should I expose these in the ast api?

static
void
log_warning(DndcContext*, StringView, int, int, size_t, FormatArg*);

static
void
log_info(DndcContext*, StringView, int, int, size_t, FormatArg*);

static
void
log_error(DndcContext*, StringView, int, int, size_t, FormatArg*);

static
void
log_debug(DndcContext*, StringView, int, int, size_t, FormatArg*);

static
void
node_log_warning(DndcContext*, const Node*, size_t, FormatArg*);

static
void
node_log_info(DndcContext*, const Node*, size_t, FormatArg*);

static
void
node_log_error(DndcContext*, const Node*, size_t, FormatArg*);

static
void
node_log_debug(DndcContext*, const Node*, size_t, FormatArg*);

static
void
handle_log_warning(DndcContext*, NodeHandle, size_t, FormatArg*);

static
void
handle_log_info(DndcContext*, NodeHandle, size_t, FormatArg*);

static
void
handle_log_error(DndcContext*, NodeHandle, size_t, FormatArg*);

static
void
handle_log_debug(DndcContext*, NodeHandle, size_t, FormatArg*);

//
// node_log_err_offset
// -------------------
// Logs an error but with an offset to the column.
// Rarely used.
//
static
void
node_log_err_offset(DndcContext* ctx, const Node*, int, LongString);

//
// handle_log_err_offset
// -------------------
// Logs an error but with an offset to the column.
// Rarely used.
//
static
void
handle_log_err_offset(DndcContext* ctx, NodeHandle, int, LongString);

//
// Error reporting functions
// -------------------------
// The following functions are for reporting errors and warnings. ONLY use
// these functions for that purpose. Do not directly use printf, fprintf or a
// log function. These functions will report the error as originating from a
// specific file, line, column and will handle suppressing them based on the
// flags given to run_the_dndc.
//

//
// report_time
// -----------
// Reports time to execute some component.
//
static
void
report_time(DndcContext*, StringView msg, uint64_t microseconds);

//
// report_size
// -----------
// Reports size of some component.
//
static
void
report_size(DndcContext*, StringView msg, uint64_t microseconds);

//
// report_system_error
// -------------------
// Reports an error that did not originate from the source text. Should only be
// called by run_the_dndc right before it returns an error.
//
static
void
report_system_error(DndcContext* ctx, StringView msg);


#ifdef __clang__
#pragma clang assume_nonnull end
#endif




#endif
