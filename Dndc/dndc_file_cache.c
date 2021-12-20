#ifndef DNDC_FILE_CACHE_C
#define DNDC_FILE_CACHE_C
#include "dndc_file_cache.h"
#include "file_util.h"
#include "base64.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
StringResult
read_and_base64_bin_file(Allocator scratch, Allocator outallocator, const char* filepath);

static inline
void
FileCache_free_path(FileCache* cache, FileCachePath path){
    Allocator_free(cache->allocator, path.path.text, path.path.length+1);
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
    Allocator al = cache->allocator;
    for(size_t i = 0; i < cache->_files.count; i++){
        LoadedSource src = cache->_files.data[i];
        if(LS_SV_equals(src.sourcepath.path, path)){
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
FileCache_has_file(FileCache* cache, StringView path){
    MARRAY_FOR_EACH(LoadedSource, src, cache->_files){
        if(LS_SV_equals(src->sourcepath.path, path))
            return true;
    }
    return false;
}

static inline
FileCachePath
FileCache_alloc_path(FileCache* cache, StringView spath){
    char* path = Allocator_strndup(cache->allocator, spath.text, spath.length);
    return (FileCachePath){
        (LongString){
            .text = path,
            .length = spath.length
        },
    };
}

static inline
warn_unused
StringResult
FileCache_read_file_(FileCache* cache, FileCachePath path){
    TextFileResult fr = read_file(path.path.text, cache->allocator);
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
    MARRAY_FOR_EACH(LoadedSource, src, cache->_files){
        if(LS_SV_equals(src->sourcepath.path, spath)){
            return (StringResult){
                .result = src->sourcetext,
            };
        }
    }
    if(unlikely(cached_only)){
        return (StringResult){.errored = PARSE_ERROR};
    }
    FileCachePath path = FileCache_alloc_path(cache, spath);
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
    MARRAY_FOR_EACH(LoadedSource, src, cache->_files){
        if(LS_SV_equals(src->sourcepath.path, spath)){
            return (StringResult){
                .result = src->sourcetext,
            };
        }
    }
    if(unlikely(cached_only)){
        return (StringResult){.errored = PARSE_ERROR};
    }
    FileCachePath path = FileCache_alloc_path(cache, spath);
    StringResult base64ed_e = read_and_base64_bin_file(cache->scratch, cache->allocator, path.path.text);
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


#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
