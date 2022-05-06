#ifndef DNDC_FILE_CACHE_C
#define DNDC_FILE_CACHE_C
#include <string.h>
#include "dndc_file_cache.h"
#include "file_util.h"
#include "base64.h"
#include "murmur_hash.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
// A cached loaded source file path
typedef struct FileCachePath {
    uint64_t last_eight_chars;
    uint32_t length;
    uint32_t hash;
    const char* text;
} FileCachePath;

// Same as above, but text is a borrowed, not
// nul-terminated string. This gives a bit of type safety.
typedef struct FileCacheLookupKey {
    uint64_t last_eight_chars;
    uint32_t length;
    uint32_t hash;
    const char* text;
} FileCacheLookupKey;


// A cached loaded file.
typedef struct LoadedSource {
    FileCachePath sourcepath; // doesn't have to be a filename
    LongString sourcetext; // the actual source text
} LoadedSource;


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#define MARRAY_T LoadedSource
#include "Marray.h"
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
StringResult
read_and_base64_bin_file(Allocator scratch, Allocator outallocator, const char* filepath);

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
    result.hash = murmur3_32((const uint8_t*)sv.text, sv.length, 0xd9d870de);
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
FileCachePath
FileCache_alloc_path(FileCache* cache, FileCacheLookupKey key){
    char* path = Allocator_strndup(cache->allocator, key.text, key.length);
    return (FileCachePath){
            .text = path,
            .length = key.length,
            .hash = key.hash,
            .last_eight_chars = key.last_eight_chars,
        };
}

static inline
warn_unused
StringResult
FileCache_read_file_(FileCache* cache, FileCachePath path){
    TextFileResult fr = read_file(path.text, cache->allocator);
    if(!fr.errored){
        LoadedSource* ls = Marray_alloc(LoadedSource)(&cache->_files, cache->allocator);
        ls->sourcepath = path;
        ls->sourcetext = fr.result;
    }
    return (StringResult){.result = fr.result, .errored=fr.errored};
}

static inline
warn_unused
StringResult
FileCache_read_file(FileCache* cache, StringView spath, bool cached_only){
    FileCacheLookupKey key = FileCache_make_key(spath);
    MARRAY_FOR_EACH(LoadedSource, src, cache->_files){
        if(FileCache_key_eq(key, src->sourcepath)){
            return (StringResult){
                .result = src->sourcetext,
            };
        }
    }
    if(unlikely(cached_only)){
        return (StringResult){.errored = DNDC_ERROR_FILE_READ};
    }
    FileCachePath path = FileCache_alloc_path(cache, key);
    StringResult result = FileCache_read_file_(cache, path);
    if(result.errored){
        FileCache_free_path(cache, path);
    }
    return result;
}

static inline
warn_unused
StringResult
FileCache_read_and_b64_file(FileCache* cache, StringView spath, bool cached_only){
    FileCacheLookupKey key = FileCache_make_key(spath);
    MARRAY_FOR_EACH(LoadedSource, src, cache->_files){
        if(FileCache_key_eq(key, src->sourcepath)){
            return (StringResult){
                .result = src->sourcetext,
            };
        }
    }
    if(unlikely(cached_only)){
        return (StringResult){.errored = DNDC_ERROR_FILE_READ};
    }
    FileCachePath path = FileCache_alloc_path(cache, key);
    StringResult base64ed_e = read_and_base64_bin_file(cache->scratch, cache->allocator, path.text);
    if(unlikely(base64ed_e.errored)){
        FileCache_free_path(cache, path);
        return (StringResult){.errored=base64ed_e.errored};
    }
    else {
        LoadedSource* ls = Marray_alloc(LoadedSource)(&cache->_files, cache->allocator);
        ls->sourcepath = path;
        ls->sourcetext = base64ed_e.result;
        return (StringResult){.result=base64ed_e.result};
    }
}

static
void
FileCache_preload_b64_files(FileCache* cache, StringView* spaths, size_t count){
    for(size_t i = 0; i < count; i++){
        StringView spath = spaths[i];
        StringResult e = FileCache_read_and_b64_file(cache, spath, false);
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
StringResult
read_and_base64_bin_file(Allocator scratch, Allocator outallocator, const char* filepath){
    StringResult result = {0};
    BinaryFileResult bfr = read_bin_file(filepath, scratch);
    if(bfr.errored){
        result.errored = bfr.errored;
        return result;
    }
    ByteBuffer buff = bfr.result;
    MStringBuilder sb = {.allocator=outallocator};
    msb_write_b64(&sb, buff.buff, buff.n_bytes);
    Allocator_free(scratch, buff.buff, buff.n_bytes);
    result.result = msb_detach_ls(&sb);
    return result;
}

static inline
int
FileCache_store_text_file(FileCache* cache, StringView spath, StringView data, bool overwrite){
    FileCacheLookupKey key = FileCache_make_key(spath);
    char* d = Allocator_strndup(cache->allocator, data.text, data.length);
    LongString ds = {.text=d, .length=data.length};
    MARRAY_FOR_EACH(LoadedSource, src, cache->_files){
        if(FileCache_key_eq(key, src->sourcepath)){
            if(!overwrite) return DNDC_ERROR_FILE_READ;
            Allocator_free(cache->allocator, src->sourcetext.text, src->sourcetext.length+1);
            src->sourcetext = ds;
            return 0;
        }
    }
    FileCachePath path = FileCache_alloc_path(cache, key);
    LoadedSource* ls = Marray_alloc(LoadedSource)(&cache->_files, cache->allocator);
    ls->sourcepath = path;
    ls->sourcetext = ds;
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
