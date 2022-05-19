//
// Copyright © 2021-2022, David Priver
//
#include <string.h> // memcpy
#include <stdlib.h> // malloc, free, qsort
#include "gi_byte_distance_completer.h"
#include "str_util.h"
#include "string_distances.h"
#include "Allocators/mallocator.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

struct BdcPair {
    size_t idx;
    ssize_t distance;
};

static
int
bdc_pair_cmp(const void* a, const void* b){
    const struct BdcPair* pa = a;
    const struct BdcPair* pb = b;
    if(pa->distance < pb->distance) return -1;
    if(pa->distance > pb->distance) return 1;
    return 0;
}

static
int
byte_distance_completer(GetInputCtx* ctx, size_t original_cursor, size_t original_count, int n_tabs){
    struct ByteDistanceCompleterContext* bdcctx = ctx->tab_completion_user_data;
    if(n_tabs == 1){
        StringView original = {.length=original_count, .text=ctx->altbuff};
        int strip_suff = !endswith(original, bdcctx->strip_suff);
        bdcctx->ordered.count = 0;
        struct BdcPair* distances = malloc(bdcctx->original->count * sizeof*distances);
        size_t n = 0;
        for(size_t i = 0; i < bdcctx->original->count; i++){
            StringView hay = bdcctx->original->data[i];
            ssize_t distance = byte_expansion_distance(hay.text, hay.length-bdcctx->strip_suff.length*strip_suff, ctx->altbuff, original_count);
            if(distance < 0) continue;
            distances[n].idx = i;
            distances[n].distance = distance;
            n++;
        }
        qsort(distances, n, sizeof *distances, bdc_pair_cmp);
        for(size_t i = 0; i < n; i++){
            StringView* sv = Marray_alloc__StringView(&bdcctx->ordered, get_mallocator());
            *sv = bdcctx->original->data[distances[i].idx];
        }
        free(distances);
    }
    if(ctx->tab_completion_cookie >= bdcctx->ordered.count){
        memcpy(ctx->buff, ctx->altbuff, original_count);
        ctx->buff_count = original_count;
        ctx->buff_cursor = original_cursor;
        ctx->buff[original_count] = 0;
        ctx->tab_completion_cookie = 0;
        return 0;
    }
    StringView completion = bdcctx->ordered.data[ctx->tab_completion_cookie++];
    if(completion.length >= GI_BUFF_SIZE-1)
        return 1;
    memcpy(ctx->buff, completion.text, completion.length);
    ctx->buff[completion.length] = 0;
    ctx->buff_count = completion.length;
    ctx->buff_cursor = completion.length;
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
