#import "filewatchcache.h"
#import <dispatch/dispatch.h>

static DndcFileCache*_Nonnull BASE64CACHE = NULL;
static DndcFileCache*_Nonnull TEXTCACHE = NULL;

#pragma clang assume_nonnull begin
typedef struct FileWatchItem FileWatchItem;
struct FileWatchItem {
    uint64_t hash;
    uint64_t last_eight_chars;
    LongString fullpath;
    int fd;
    bool tomb;
};
typedef struct FileWatchCache FileWatchCache;
struct FileWatchCache {
    size_t capacity;
    size_t count;
    FileWatchItem* items;
};

static
void
cache_watch_file(void* cache_, StringView path){
    // path is not necessarily valid - javascript blocks can add dependencies.
    FileWatchCache* cache = cache_;
    if(!path.length || !path.text){
        NSLog(@"Not watching invalid path");
        return;
    }
    uint64_t hash = hash_align1(path.text, path.length);
    uint64_t last_eight = 0;
    const char* end = path.text + path.length;
    size_t length = path.length >= 8? 8 : path.length;
    memcpy(&last_eight, end-length, length);

    size_t first_tomb = -1;
    for(size_t i = 0; i < cache->count; i++){
        FileWatchItem* it = &cache->items[i];
        if(it->tomb){
            if(first_tomb == (size_t)-1){
                first_tomb = i;
            }
            continue;
        }
        if(it->hash == hash){
            if(it->last_eight_chars == last_eight){
                if(LS_SV_equals(it->fullpath, path)){
                    // already watching it.
                    return;
                }
            }
        }
    }
    if(cache->count >= cache->capacity){
        size_t capacity = cache->capacity * 2;
        if(!capacity)
            capacity = 8;
        FileWatchItem* items = realloc(cache->items, capacity*sizeof(*items));
        if(!items){
            NSLog(@"Resizing filewatchcache to %zu items failed", capacity);
            return;
        }
        cache->items = items;
        cache->capacity = capacity;
    }
    size_t item_index = (first_tomb != (size_t)-1)?first_tomb:cache->count++;
    FileWatchItem* item = &cache->items[item_index];
    item->tomb = false;
    item->hash = hash;
    item->last_eight_chars = last_eight;
    item->fullpath.text = strndup(path.text, path.length);
    if(!item->fullpath.text){
        NSLog(@"strndup failed");
        item->tomb = true;
        return;
    }
    item->fullpath.length = path.length;
    item->fd = open(item->fullpath.text, O_EVTONLY);
    if(item->fd < 0){
        // NSLog(@"open call for '%s' failed: %s", item->fullpath.text, strerror(errno));
        item->tomb = true;
        const_free(item->fullpath.text);
        return;
    }
    // NSLog(@"watching '%s'", item->fullpath.text);
    dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, item->fd, DISPATCH_VNODE_WRITE | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME | DISPATCH_VNODE_EXTEND | DISPATCH_VNODE_ATTRIB, dispatch_get_main_queue());
    dispatch_source_set_event_handler(source, ^{
        FileWatchItem* bitem = &cache->items[item_index];
        dndc_filecache_remove(TEXTCACHE, LS_to_SV(bitem->fullpath));
        dndc_filecache_remove(BASE64CACHE, LS_to_SV(bitem->fullpath));
        // NSLog(@"'%s' changed", bitem->fullpath.text);
#if 0
        uint64_t mask = dispatch_source_get_data(source);
        if(mask & DISPATCH_VNODE_DELETE){
            NSLog(@"VNode for '%s' was deleted", bitem->fullpath.text);
        }
        if(mask & DISPATCH_VNODE_RENAME){
            NSLog(@"VNode for '%s' was renamed", bitem->fullpath.text);
        }
        if(mask & DISPATCH_VNODE_WRITE) {
            NSLog(@"VNode for '%s' was written", bitem->fullpath.text);
        }
        if(mask & DISPATCH_VNODE_EXTEND) {
            NSLog(@"VNode for '%s' was extended", bitem->fullpath.text);
        }
        if(mask & DISPATCH_VNODE_ATTRIB) {
            NSLog(@"VNode for '%s' metadata changed", bitem->fullpath.text);
        }
#endif
        dispatch_source_cancel(source);
    });
    dispatch_source_set_cancel_handler(source, ^{
        FileWatchItem* bitem = &cache->items[item_index];
        // NSLog(@"VNode for '%s' was canceled", bitem->fullpath.text);
        bitem->tomb = true;
        close(bitem->fd);
        const_free(bitem->fullpath.text);
    });
    dispatch_resume(source);
}
static FileWatchCache FILE_WATCH_CACHE = {0};
static
int
cache_watch_files(void* unused, size_t npaths, StringView*paths){
    (void)unused;
    for(size_t i = 0; i < npaths; i++){
        cache_watch_file(&FILE_WATCH_CACHE, paths[i]);
    }
    return 0;
}

#pragma clang assume_nonnull end
