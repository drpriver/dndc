//
// Copyright © 2021-2022, David Priver
//
#ifndef GI_INDENT_COMPLETER_H
#define GI_INDENT_COMPLETER_H

#include "get_input.h"
#include "mem_util.h"
static GiTabCompletionFunc indent_completer;

static inline
int
indent_completer(GetInputCtx* ctx, size_t original_curr_pos, size_t original_used_len, int n_tabs){
    (void)original_curr_pos;
    (void)original_used_len;
    (void)n_tabs;
    int err = meminsert(ctx->buff_cursor, ctx->buff, GI_BUFF_SIZE, ctx->buff_count+1, "  ", 2);
    if(err) return -1;
    ctx->buff_cursor += 2;
    ctx->buff_count += 2;
    return 0;
}


#endif
