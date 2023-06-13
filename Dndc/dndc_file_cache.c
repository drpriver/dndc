//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
#ifndef DNDC_FILE_CACHE_C
#define DNDC_FILE_CACHE_C
#include <string.h>
#include "dndc_file_cache.h"
#include "Utils/file_util.h"
#include "Utils/base64.h"
#include "Utils/hash_func.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
// A cached loaded source file path
typedef struct FileCachePath FileCachePath;
struct FileCachePath {
    uint64_t last_eight_chars;
    uint32_t length;
    uint32_t hash;
    const char* text;
};

// Same as above, but text is a borrowed, not
// nul-terminated string. This gives a bit of type safety.
typedef struct FileCacheLookupKey FileCacheLookupKey;
struct FileCacheLookupKey {
    uint64_t last_eight_chars;
    uint32_t length;
    uint32_t hash;
    const char* text;
};


// A cached loaded file.
typedef struct LoadedSource LoadedSource;
struct LoadedSource {
    FileCachePath sourcepath; // doesn't have to be a filename
    LongString sourcetext; // the actual source text
};


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#define MARRAY_T LoadedSource
#include "Utils/Marray.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

struct DndcFileCache {
    Allocator allocator;
    Allocator scratch;
    // TODO: use an adaptive table. Paths can get long and slow.
    Marray(LoadedSource) _files;
};


static
warn_unused
int
read_and_base64_bin_file(Allocator scratch, Allocator outallocator, const char* filepath, LongString* outsv);

static inline
void
FileCache_free_path(FileCache* cache, FileCachePath path){
    Allocator_free(cache->allocator, path.text, path.length+1);
}



static inline
bool
FileCache_key_eq(FileCacheLookupKey key, FileCachePath p){
    if(key.last_eight_chars != p.last_eight_chars)
        return false;
    if(key.length != p.length)
        return false;
    if(key.hash != p.hash)
        return false;
    return memcmp(key.text, p.text, key.length) == 0;
}

static inline
FileCacheLookupKey
FileCache_make_key(StringView sv){
    FileCacheLookupKey result;
    result.text = sv.text;
    result.length = sv.length;
    result.last_eight_chars = 0;
    memcpy(&result.last_eight_chars, sv.text, sv.length >= 8? 8 : sv.length);
    result.hash = hash_align1(sv.text, sv.length);
    return result;
}

static inline
void
FileCache_clear(FileCache* cache){
    Allocator al = cache->allocator;
    MARRAY_FOR_EACH(LoadedSource, src, cache->_files){
        FileCache_free_path(cache, src->sourcepath);
        Allocator_free(al, src->sourcetext.text, src->sourcetext.length+1);
    }
    Marray_cleanup(LoadedSource)(&cache->_files, al);
}

static inline
int
FileCache_maybe_remove(FileCache* cache, StringView path){
    FileCacheLookupKey key = FileCache_make_key(path);
    Allocator al = cache->allocator;
    for(size_t i = 0; i < cache->_files.count; i++){
        LoadedSource src = cache->_files.data[i];
        if(FileCache_key_eq(key, src.sourcepath)){
            Marray_remove(LoadedSource)(&cache->_files, i);
            FileCache_free_path(cache, src.sourcepath);
            Allocator_free(al, src.sourcetext.text, src.sourcetext.length+1);
            return 1;
        }
    }
    return 0;
}

static inline
bool
FileCache_has_file(const FileCache* cache, StringView path){
    FileCacheLookupKey key = FileCache_make_key(path);
    MARRAY_FOR_EACH(LoadedSource, src, cache->_files){
        if(FileCache_key_eq(key, src->sourcepath))
            return true;
    }
    return false;
}

static inline
warn_unused
int
FileCache_alloc_path(FileCache* cache, FileCacheLookupKey key, FileCachePath* out){
    char* path = Allocator_strndup(cache->allocator, key.text, key.length);
    if(!path) return DNDC_ERROR_OOM;
    *out = (FileCachePath){
            .text = path,
            .length = key.length,
            .hash = key.hash,
            .last_eight_chars = key.last_eight_chars,
        };
    return 0;
}

static inline
warn_unused
int
FileCache_read_file_(FileCache* cache, FileCachePath path, LongString* outstr){
    LongString text;
    FileError fr = read_file(path.text, cache->allocator, &text);
    if(fr.errored){
        return DNDC_ERROR_FILE_READ;
    }
    if(!fr.errored){
        LoadedSource* ls; int err = Marray_alloc(LoadedSource)(&cache->_files, cache->allocator, &ls);
        if(unlikely(err)){
            Allocator_free(cache->allocator, text.text, text.length+1);
            return DNDC_ERROR_OOM;
        }
        ls->sourcepath = path;
        ls->sourcetext = text;
    }
    *outstr = text;
    return 0;
}

static inline
warn_unused
int
FileCache_read_file(FileCache* cache, StringView spath, bool cached_only, LongString* outstr){
    FileCacheLookupKey key = FileCache_make_key(spath);
    MARRAY_FOR_EACH(LoadedSource, src, cache->_files){
        if(FileCache_key_eq(key, src->sourcepath)){
            *outstr = src->sourcetext;
            return 0;
        }
    }
    if(unlikely(cached_only)){
        return DNDC_ERROR_FILE_READ;
    }
    FileCachePath path;
    int err = FileCache_alloc_path(cache, key, &path);
    if(unlikely(err))
        return err;
    int result = FileCache_read_file_(cache, path, outstr);
    if(result){
        FileCache_free_path(cache, path);
    }
    return result;
}

static inline
warn_unused
int
FileCache_read_and_b64_file(FileCache* cache, StringView spath, bool cached_only, LongString* outstr){
    FileCacheLookupKey key = FileCache_make_key(spath);
    MARRAY_FOR_EACH(LoadedSource, src, cache->_files){
        if(FileCache_key_eq(key, src->sourcepath)){
            *outstr = src->sourcetext;
            return 0;
        }
    }
    if(unlikely(cached_only))
        return DNDC_ERROR_FILE_READ;
    FileCachePath path;
    int err = FileCache_alloc_path(cache, key, &path);
    if(unlikely(err))
        return err;
    LongString base64ed;
    int base64ed_e = read_and_base64_bin_file(cache->scratch, cache->allocator, path.text, &base64ed);
    if(unlikely(base64ed_e)){
        FileCache_free_path(cache, path);
        return base64ed_e;
    }
    LoadedSource* ls; err = Marray_alloc(LoadedSource)(&cache->_files, cache->allocator, &ls);
    if(unlikely(err)){
        FileCache_free_path(cache, path);
        Allocator_free(cache->allocator, base64ed.text, base64ed.length+1);
        return DNDC_ERROR_OOM;
    }
    ls->sourcepath = path;
    ls->sourcetext = base64ed;
    *outstr = base64ed;
    return 0;
}

static
void
FileCache_preload_b64_files(FileCache* cache, StringView* spaths, size_t count){
    for(size_t i = 0; i < count; i++){
        StringView spath = spaths[i];
        LongString unused;
        int e = FileCache_read_and_b64_file(cache, spath, false, &unused);
        (void)unused;
        (void)e;
    }
}

static
size_t
FileCache_cached_paths(const FileCache* cache, StringView* buff, size_t bufflen, size_t* cookie){
    if(!cookie) return 0;
    size_t start = *cookie;
    if(start >= cache->_files.count)
        return 0;
    if(!bufflen) return 0;
    if(!buff) return 0;
    size_t n = cache->_files.count - start;
    if(n > bufflen)
        n = bufflen;
    for(size_t i = start; i < start + n; i++){
        buff[i].text = cache->_files.data[i].sourcepath.text;
        buff[i].length = cache->_files.data[i].sourcepath.length;
    }
    *cookie = start + n;
    return n;
}

static inline
size_t
FileCache_n_paths(const FileCache* cache){
    return cache->_files.count;
}

// TODO: figure out where to put this. It was previously in bb_extensions.

//
// Reads in a file as binary and base64-ifies it.
//
// All failures are file-related errors.
// The base64-ifying can't fail.
//
static
warn_unused
int
read_and_base64_bin_file(Allocator scratch, Allocator outallocator, const char* filepath, LongString* outstr){
    ByteBuffer buff;
    FileError bfr = read_bin_file(filepath, scratch, &buff);
    if(bfr.errored){
        return DNDC_ERROR_FILE_READ;
    }
    MStringBuilder sb = {.allocator=outallocator};
    msb_write_b64(&sb, buff.buff, buff.n_bytes);
    Allocator_free(scratch, buff.buff, buff.n_bytes);
    msb_nul_terminate(&sb);
    if(sb.errored){
        msb_destroy(&sb);
        return DNDC_ERROR_OOM;
    }
    *outstr = msb_detach_ls(&sb);
    return 0;
}

static inline
int
FileCache_store_text_file(FileCache* cache, StringView spath, StringView data, bool overwrite){
    FileCacheLookupKey key = FileCache_make_key(spath);
    char* d = Allocator_strndup(cache->allocator, data.text, data.length);
    if(!d)
        return DNDC_ERROR_OOM;

    LongString ds = {.text=d, .length=data.length};
    MARRAY_FOR_EACH(LoadedSource, src, cache->_files){
        if(FileCache_key_eq(key, src->sourcepath)){
            if(!overwrite) return DNDC_ERROR_FILE_READ;
            Allocator_free(cache->allocator, src->sourcetext.text, src->sourcetext.length+1);
            src->sourcetext = ds;
            return 0;
        }
    }
    FileCachePath path;
    int err = FileCache_alloc_path(cache, key, &path);
    if(unlikely(err)){
        Allocator_free(cache->allocator, d, data.length+1);
        return err;
    }
    LoadedSource* ls; err = Marray_alloc(LoadedSource)(&cache->_files, cache->allocator, &ls);
    if(unlikely(err)){
        Allocator_free(cache->allocator, d, data.length+1);
        FileCache_free_path(cache, path);
        return DNDC_ERROR_OOM;
    }
    ls->sourcepath = path;
    ls->sourcetext = ds;
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
