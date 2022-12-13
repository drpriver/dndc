#ifndef DNDC_API
#define DNDC_API static inline
#endif
#import "Dndc/dndc_long_string.h"
#import "Dndc/dndc.h"

static DndcFileCache*_Nonnull BASE64CACHE;
static DndcFileCache*_Nonnull TEXTCACHE;

#pragma clang assume_nonnull begin
static int cache_watch_files(void* unused, size_t npaths, StringView* paths);
#pragma clang assume_nonnull end
