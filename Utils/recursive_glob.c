//
// Copyright © 2021-2022, David Priver
//
#ifndef RECURSIVE_GLOB_C
#define RECURSIVE_GLOB_C
#if defined(_WIN32)
#include <direct.h>
#include <assert.h>
#else
#include <fts.h>
#endif

#include "recursive_glob.h"
#include "str_util.h"

#include "Allocators/mallocator.h"
#include "MStringBuilder.h"


#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
#if defined(_WIN32)
static
void
recursive_glob_suffix_inner(StringView original, StringView directory, StringView suffix, Marray(StringView)* entries, int max_depth){
    if(max_depth <= 0) return;
    MStringBuilder sb = {.allocator = MALLOCATOR};
    msb_write_str(&sb, directory.text, directory.length);
    msb_write_char(&sb, '/');
    msb_write_char(&sb, '*');
    msb_write_str(&sb, suffix.text, suffix.length);
    msb_nul_terminate(&sb);
    LongString wildcard = msb_borrow_ls(&sb);
    WIN32_FIND_DATAA findd;
    HANDLE handle = FindFirstFileExA(wildcard.text, FindExInfoBasic, &findd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    msb_erase(&sb, suffix.length+2);
    if(handle == INVALID_HANDLE_VALUE){
    }
    else{
        do {
            size_t cursor = sb.cursor;
            msb_write_char(&sb, '/');
            msb_write_str(&sb, findd.cFileName, strlen(findd.cFileName));
            StringView text = msb_borrow_sv(&sb);
            char* s = Allocator_strndup(MALLOCATOR, text.text+original.length+1, text.length-original.length-1);
            StringView* it; int err = Marray_alloc__StringView(entries, MALLOCATOR, &it);
            assert(!err);
            *it = (StringView){.text=s, .length=text.length-original.length-1};
            sb.cursor = cursor;
        }while(FindNextFileA(handle, &findd));
        FindClose(handle);
    }
    msb_write_literal(&sb, "/*");
    msb_nul_terminate(&sb);
    LongString thisdir = msb_borrow_ls(&sb);
    handle = FindFirstFileExA(thisdir.text, FindExInfoBasic, &findd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if(handle == INVALID_HANDLE_VALUE){
        goto end;
    }
    msb_erase(&sb, sizeof("/*")-1);
    do {
        if(findd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN){
            continue;
        }
        if(!(findd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
            continue;
        }
        StringView fn = {.text = findd.cFileName, .length = strlen(findd.cFileName)};
        if(fn.text[0] == '.'){
            continue;
        }
        msb_write_char(&sb, '/');
        msb_write_str(&sb, fn.text, fn.length);
        msb_nul_terminate(&sb);
        StringView nextdir = msb_borrow_sv(&sb);
        recursive_glob_suffix_inner(original, nextdir, suffix, entries, max_depth-1);
        msb_erase(&sb, 1+fn.length);
    }while(FindNextFileA(handle, &findd));
    end:
    msb_destroy(&sb);
}
#endif

#ifndef PushDiagnostic
#if defined(__clang__)
#define PushDiagnostic()                _Pragma("clang diagnostic push")
#define PopDiagnostic()                 _Pragma("clang diagnostic pop")
#define SuppressCastQual()              _Pragma("clang diagnostic ignored \"-Wcast-qual\"")
#elif defined(__GNUC__)
#define PushDiagnostic()                _Pragma("GCC diagnostic push")
#define PopDiagnostic()                 _Pragma("GCC diagnostic pop")
#define SuppressCastQual()              _Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
#else
#define PushDiagnostic()
#define PopDiagnostic()
#define SuppressCastQual()
#endif
#endif

RECURSIVE_GLOB_API
void
recursive_glob_suffix(LongString directory, StringView suffix, Marray(StringView)* entries, int max_depth){
#if defined(__APPLE__) || defined(__linux__)
    const char* dirs[] = {directory.text, NULL};
    PushDiagnostic();
    SuppressCastQual();
    FTS* handle = fts_open((char**)dirs, FTS_LOGICAL | FTS_NOCHDIR | FTS_NOSTAT, NULL);
    PopDiagnostic();
    if(!handle) return;
    for(;;){
        FTSENT* ent = fts_read(handle);
        if(!ent) break;
        if(ent->fts_namelen > 1 && ent->fts_name[0] == '.'){
            fts_set(handle, ent, FTS_SKIP);
            continue;
        }
        if(ent->fts_level > max_depth){
            fts_set(handle, ent, FTS_SKIP);
            continue;
        }
        if(ent->fts_info & (FTS_F | FTS_NSOK)){
            StringView name = {.text = ent->fts_name, .length=ent->fts_namelen};
            if(!endswith(name, suffix))
                continue;
            char* p = ent->fts_path + directory.length+1;
            size_t len = strlen(p);
            char* t = Allocator_strndup(MALLOCATOR, p, len);
            StringView* it; int err = Marray_alloc__StringView(entries, MALLOCATOR, &it);
            assert(!err);
            *it = (StringView){.length = len, .text = t};
        }
    }
    fts_close(handle);
#elif defined(_WIN32)
    recursive_glob_suffix_inner(LS_to_SV(directory), LS_to_SV(directory), suffix, entries, max_depth);
#endif
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
