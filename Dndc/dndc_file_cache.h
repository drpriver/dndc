#ifndef DNDC_FILE_CACHE_H
#define DNDC_FILE_CACHE_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dndc_long_string.h"
#include "errorable_long_string.h"
#include "allocator.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct DndcFileCache FileCache;

static inline
void
FileCache_clear(FileCache* cache);

static inline
int
FileCache_maybe_remove(FileCache* cache, StringView path);

static inline
bool
FileCache_has_file(const FileCache* cache, StringView path);

static inline
warn_unused
StringResult
FileCache_read_file(FileCache* cache, StringView spath, bool cached_only);

static inline
warn_unused
StringResult
FileCache_read_and_b64_file(FileCache* cache, StringView spath, bool cached_only);

static 
void // preload only so no error code
FileCache_preload_b64_files(FileCache* cache, StringView* spath, size_t count);

// 
// Outputs what paths are cached in this filecache.
// Returns how many paths were written to buff.
// Cookie is a pointer to integer for tracking the iterator state.
// Initialize it to 0 and then pass it to each call to this function.
static
size_t
FileCache_cached_paths(const FileCache* cache, StringView* buff, size_t bufflen, size_t* cookie);

static inline
size_t
FileCache_n_paths(const FileCache* cache);


// A cached loaded source file path
typedef struct FileCachePath {
    uint64_t last_eight_chars;
    uint32_t length;
    uint32_t hash;
    const char* text;
} FileCachePath;


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

struct DndcFileCache {
    Allocator allocator;
    Allocator scratch;
    // TODO: use an adaptive table. Paths can get long and slow.
    Marray(LoadedSource) _files;
};

#endif
