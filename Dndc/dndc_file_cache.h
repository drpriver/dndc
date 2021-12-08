#ifndef DNDC_FILE_CACHE_H
#define DNDC_FILE_CACHE_H
#include "dndc_long_string.h"
#include "errorable_long_string.h"
#include "ByteBuilder.h"
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
FileCache_has_file(FileCache* cache, StringView path);

static inline
warn_unused
Errorable(LongString)
FileCache_read_file(FileCache* cache, StringView spath, bool cached_only);

static inline
warn_unused
Errorable(LongString)
FileCache_read_and_b64_file(FileCache* cache, StringView spath, bool cached_only, ByteBuilder* bb);


// A cached loaded source file path
typedef struct FileCachePath {
    LongString path;
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
    // TODO: use an adaptive table. Paths can get long and slow.
    Marray(LoadedSource) _files;
};

#endif
