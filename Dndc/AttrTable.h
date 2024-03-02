//
// Copyright © 2022-2024, David Priver <david@davidpriver.com>
//
#ifndef ATTRTABLE_H
#define ATTRTABLE_H
//
// This file implements an adaptive associative array type for strings to
// strings.
//
// Features:
//   - Single allocation
//   - Realloc-friendly
//   - Remembers order of insertion
//   - Efficient lookup by key.
//   - Empty tables are just a null pointer, saving size.
//   - Deletion of keys
//   - Generic over allocator
//   - Fast iteration over key/value items
//
// Implementation decisions:
//   - can only hold 2**31 items.
//   - in hashtable mode:
//     - hashes are not cached.
//     - resizing requires hashing every key.
//     - tombstones are removed by shifting all elements forward.
//     - tombstones are only removed on resize
//     - never shrinks
//     - key/values items are at the front of the allocation rather than
//       the back, so indexing is likely to cause a cache miss.
//     - hashes are only 32 bits.
//       They probably should be 64, but it is so easy to just use crc32.
//  - in array mode:
//     - does a linear scan of all of the keys and effectively strcmps the
//       ones of matching size.
//       - in practice this is fine as ATTRIBUTE_THRESH is 8
//     - doesn't clear tombs (although it probably should).
//
// For small numbers of items it is just a dynamic array and does a linear
// scan to ensure uniqueness.
//
// If you can afford the size, it's probably worth having a fatter data
// structure so count, capacity, etc.  don't have to be accessed through a
// pointer.
//
// If you expect a larger amount of data, you can skip the "start as an array"
// stage.  Lots of things to tweak in hash table design.
//
// Note that this file doesn't #include any Dndc files, so it is easy
// to use in another project using the Allocators and Utils.
//
// TODO:
//   - test ATTRIBUTE_THRESH for optimal threshold



#include "Utils/hash_func.h"
#include "Utils/long_string.h"
#include "Allocators/allocator.h"

// Branch hinting.
// Makes returning error codes much cheaper.

#if !defined(likely) && !defined(unlikely)
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif

// Warn unused.
// Used for return values that *should* be handled, like an error
// code from `get`. Ignoring those return values could lead to reading
// uninitalize memory, etc.
#ifndef warn_unused
#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif
#endif

// Clang nullability stuff
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#endif


typedef struct AttrTable AttrTable;
struct AttrTable {
    uint32_t count; // This includes tombstones.
                    // To get number of live slots, do (count - tombs).
    uint32_t tombs; // number of deleted keys.
    uint32_t capacity; // capacity <= ATTRIBUTE_THRESH means array, else hash table.
    uint32_t pad; // So buff is properly aligned.
    char buff[]; // This gets cast to the right thing.
    // In array mode, this is just a buffer of `StringView2` of length
    // capacity.
    // In hash mode, it is also a buffer of `StringView2` of length capacity,
    // but then after it there is also a buffer of `uint32_t` of length
    // capacity that are the indexes back into the `StringView2` buffer.
    // UINT32_MAX is the sentinel meaning an unused index.

    // This type has to be dynamically allocated (actually you can
    // stack allocate it with GCC extensions. If you do, then pass
    // a `NULLACATOR` so that resizing fails.)
};

// TODO: Tune this number.
#ifndef ATTRIBUTE_THRESH
enum {ATTRIBUTE_THRESH=8}; // capacity > this means hashtable
#endif

// Does the internal cast of buff for you.
// Array mode and hash table mode are the same for just getting the items.
static inline
StringView2*_Nullable
AttrTable_items(AttrTable*_Nullable table){
    if(!table) return NULL;
    return (StringView2*)table->buff;
}

static inline
size_t
AttrTable_alloc_size(AttrTable*_Nullable table){
    if(!table) return 0;
    size_t cap = table->capacity;
    size_t size;
    if(cap > ATTRIBUTE_THRESH)
        size = cap*sizeof(StringView2) + cap*sizeof(uint32_t) + sizeof(AttrTable);
    else
        size = cap*sizeof(StringView2) + sizeof(AttrTable);
    return size;
}

static inline
void
AttrTable_cleanup(AttrTable*_Nullable table, Allocator a){
    if(!table) return;
    Allocator_free(a, table, AttrTable_alloc_size(table));
}

// This is semi-private and should only be called in hash mode.
// Doubles the capacity and clears tombstones.
static inline
warn_unused
int
AttrTable_resize_hash(AttrTable*_Nonnull*_Nonnull t, Allocator a){
    AttrTable* table = *t;
    size_t old_cap = table->capacity;
    size_t new_cap = old_cap*2;
    if(unlikely(new_cap >= UINT32_MAX)) return 1;
    size_t new_size = new_cap * sizeof(StringView2)+new_cap*sizeof(uint32_t)+sizeof(AttrTable);
    size_t old_size = old_cap * sizeof(StringView2)+old_cap*sizeof(uint32_t)+sizeof(AttrTable);
    table = Allocator_realloc(a, table, old_size, new_size);
    if(unlikely(!table)) return 1;
    StringView2* items = (StringView2*)table->buff;
    uint32_t* indexes = (uint32_t*)(table->buff + new_cap*sizeof(StringView2));
    memset(indexes, 0xff, new_cap*sizeof(*indexes));
    size_t old_count = table->count;
    size_t count = 0;
    for(size_t i = 0; i < old_count; i++){
        StringView2* pair = items + i;
        if(!pair->key.length) continue;
        uint32_t hash = hash_align1(pair->key.text, pair->key.length);
        uint32_t idx = fast_reduce32(hash, (uint32_t)new_cap);
        if(count != i)
            items[count] = *pair;
        while(indexes[idx] != UINT32_MAX){
            idx++;
            if(unlikely(idx >= new_cap)) idx = 0;
        }
        indexes[idx] = count;
        count++;
    }
    table->tombs = 0;
    table->count = count;
    table->capacity = new_cap;
    *t = table;
    return 0;
}


// Doubles the capacity. Can be used in both hash mode and array mode.
static inline
warn_unused
int
AttrTable_resize_array(AttrTable*_Nonnull*_Nonnull t, Allocator a){
    AttrTable* table = *t;
    size_t old_cap = table->capacity;
    size_t new_cap = old_cap*2;
    if(unlikely(new_cap > ATTRIBUTE_THRESH)){
        size_t new_size = new_cap * sizeof(StringView2)+new_cap*sizeof(uint32_t)+sizeof(AttrTable);
        size_t old_size = old_cap * sizeof(StringView2)+sizeof(AttrTable);
        table = Allocator_realloc(a, table, old_size, new_size);
        if(unlikely(!table)) return 1;
        StringView2* items = (StringView2*)table->buff;
        uint32_t* indexes = (uint32_t*)(table->buff + new_cap*sizeof(StringView2));
        memset(indexes, 0xff, new_cap*sizeof(*indexes));
        size_t old_count = table->count;
        size_t count = 0;
        for(size_t i = 0; i < old_count; i++){
            StringView2* pair = items + i;
            if(!pair->key.length) continue;
            uint32_t hash = hash_align1(pair->key.text, pair->key.length);
            uint32_t idx = fast_reduce32(hash, (uint32_t)new_cap);
            if(count != i)
                items[count] = *pair;
            while(indexes[idx] != UINT32_MAX){
                idx++;
                if(unlikely(idx >= new_cap)) idx = 0;
            }
            indexes[idx] = count;
            count++;
        }
        table->tombs = 0;
        table->count = count;
        table->capacity = new_cap;
        *t = table;
        return 0;
    }
    size_t new_size = new_cap * sizeof(StringView2)+new_cap*sizeof(uint32_t)+sizeof(AttrTable);
    size_t old_size = old_cap * sizeof(StringView2)+sizeof(AttrTable);
    table = Allocator_realloc(a, table, old_size, new_size);
    if(unlikely(!table)) return 1;
    // just leave the tombs in whatever.
    table->capacity = new_cap;
    *t = table;
    return 0;
}

// Makes sure there's space for one more item.
static inline
warn_unused
int
AttrTable_ensure_one(AttrTable*_Nullable*_Nonnull t, Allocator a){
    AttrTable* table = *t;
    if(!table){
        enum {INITIAL_CAPACITY=2};
        enum {INITIAL_SIZE=INITIAL_CAPACITY*sizeof(StringView2)+sizeof(AttrTable)};
        table = Allocator_alloc(a, INITIAL_SIZE);
        if(!table) return 1;
        table->count = 0;
        table->capacity = INITIAL_CAPACITY;
        table->tombs = 0;
        *t = table;
        return 0;
    }
    if(table->capacity > ATTRIBUTE_THRESH){
        if((table->count)*2 >= table->capacity){
            int err = AttrTable_resize_hash((AttrTable*_Nonnull*_Nonnull)t, a);
            return err;
        }
        return 0;
    }
    if(table->count >= table->capacity){
        int err = AttrTable_resize_array((AttrTable*_Nonnull*_Nonnull)t, a);
        return err;
    }
    return 0;
}

// Implement set for hash mode. Only should be called in hash mode and with
// enough space.
//
// Basically always returns 0.
static inline
warn_unused
int
AttrTable_set_hash(AttrTable* table, StringView key, StringView value){
    uint32_t hash = hash_align1(key.text, key.length);
    size_t cap = table->capacity;
    StringView2* items = (StringView2*)table->buff;
    uint32_t* indexes = (uint32_t*)(table->buff + cap*sizeof(StringView2));
    uint32_t idx = fast_reduce32(hash, (uint32_t)cap);
    for(;;){
        uint32_t i = indexes[idx];
        if(i == UINT32_MAX){ // empty slot
            indexes[idx] = table->count;
            items[table->count] = (StringView2){key, value};
            table->count++;
            return 0;
        }
        if(SV_equals(items[i].key, key)){
            items[i].value = value;
            return 0;
        }
        idx++;
        if(unlikely(idx >= cap)) idx = 0;
    }
    return 1; // unreachable
}

// Implement set for both array and hash mode. Ensures enough space itself.
//
// Returns 1 on OOM and 0 otherwise.
static inline
warn_unused
int
AttrTable_set(AttrTable*_Nullable*_Nonnull t, Allocator a, StringView key, StringView value){
    if(!key.length) return 1;
    int err = AttrTable_ensure_one(t, a);
    if(unlikely(err)) return err;
    AttrTable* table = *t;
    if(unlikely(table->capacity > ATTRIBUTE_THRESH)){
        return AttrTable_set_hash(table, key, value);
    }
    StringView2* items = (StringView2*)table->buff;
    size_t count = table->count;
    for(size_t i = 0; i < count; i++){
        if(SV_equals(items[i].key, key)){
            items[i].value = value;
            return 0;
        }
    }
    items[table->count++] = (StringView2){key, value};
    return 0;
}

// Like set_hash, but "returns" the pointer to the value of the item after
// initializing it to the empty string.
static inline
warn_unused
int
AttrTable_alloc_hash(AttrTable* table, StringView key, StringView*_Nullable*_Nonnull value){
    uint32_t hash = hash_align1(key.text, key.length);
    size_t cap = table->capacity;
    StringView2* items = (StringView2*)table->buff;
    uint32_t* indexes = (uint32_t*)(table->buff + cap*sizeof(StringView2));
    uint32_t idx = fast_reduce32(hash, (uint32_t)cap);
    for(;;){
        uint32_t i = indexes[idx];
        if(i == UINT32_MAX){ // empty slot
            indexes[idx] = table->count;
            items[table->count] = (StringView2){key, SV("")};
            *value = &items[table->count].value;
            table->count++;
            return 0;
        }
        if(SV_equals(items[i].key, key)){
            *value = &items[i].value;
            return 0;
        }
        idx++;
        if(unlikely(idx >= cap)) idx = 0;
    }
    return 1; // unreachable
}

//
// Like set, but "returns" the pointer to the value of the item after
// initializing it to the empty string.
// Works in both hash and array mode and ensures space itself.
//
// Returns 1 on OOM and 0 otherwise.
static inline
warn_unused
int
AttrTable_alloc(AttrTable*_Nullable*_Nonnull t, Allocator a, StringView key, StringView*_Nullable*_Nonnull value){
    if(!key.length) return 1;
    int err = AttrTable_ensure_one(t, a);
    if(unlikely(err)) return err;
    AttrTable* table = *t;
    if(unlikely(table->capacity > ATTRIBUTE_THRESH)){
        return AttrTable_alloc_hash(table, key, value);
    }
    StringView2* items = (StringView2*)table->buff;
    size_t count = table->count;
    for(size_t i = 0; i < count; i++){
        if(SV_equals(items[i].key, key)){
            *value = &items[i].value;
            return 0;
        }
    }
    items[table->count] = (StringView2){key, SV("")};
    *value = &items[table->count].value;
    table->count++;
    return 0;
}

// Only call in hash mode. Implements the getting by key operation, "returning"
// through the outparam.
//
// Returns 1 if key is missing and 0 otherwise.
static inline
warn_unused
int
AttrTable_get_hash(AttrTable* table, StringView key, StringView* value){
    uint32_t hash = hash_align1(key.text, key.length);
    size_t cap = table->capacity;
    StringView2* items = (StringView2*)table->buff;
    uint32_t* indexes = (uint32_t*)(table->buff + cap*sizeof(StringView2));
    uint32_t idx = fast_reduce32(hash, (uint32_t)cap);
    for(;;){
        uint32_t i = indexes[idx];
        if(i == UINT32_MAX) // empty slot
            return 1;
        if(SV_equals(items[i].key, key)){
            *value = items[i].value;
            return 0;
        }
        idx++;
        if(unlikely(idx >= cap)) idx = 0;
    }
    return 1; // unreachable
}

// Call in either mode. Implements the getting by key operation, "returning"
// through the outparam.
//
// Returns 1 if key is missing and 0 otherwise.
static inline
warn_unused
int
AttrTable_get(AttrTable*_Nullable table, StringView key, StringView* value){
    if(!key.length) return 1;
    if(!table) return 1;
    if(unlikely(table->capacity > ATTRIBUTE_THRESH)){
        return AttrTable_get_hash((AttrTable*)table, key, value);
    }
    StringView2* items = (StringView2*)table->buff;
    size_t count = table->count;
    for(size_t i = 0; i < count; i++){
        if(SV_equals(items[i].key, key)){
            *value = items[i].value;
            return 0;
        }
    }
    return 1;
}

// Only call in hash mode.
//
// Returns 1 if key is present and 0 if missing.
static inline
int
AttrTable_has_hash(AttrTable* table, StringView key){
    uint32_t hash = hash_align1(key.text, key.length);
    size_t cap = table->capacity;
    StringView2* items = (StringView2*)table->buff;
    uint32_t* indexes = (uint32_t*)(table->buff + cap*sizeof(StringView2));
    uint32_t idx = fast_reduce32(hash, (uint32_t)cap);
    for(;;){
        uint32_t i = indexes[idx];
        if(i == UINT32_MAX) // empty slot
            return 0;
        if(SV_equals(items[i].key, key)){
            return 1;
        }
        idx++;
        if(unlikely(idx >= cap)) idx = 0;
    }
    return 0; // unreachable
}

// Call in either mode.
//
// Returns 1 if key is present and 0 if missing.
static inline
int
AttrTable_has(AttrTable*_Nullable table, StringView key){
    if(unlikely(!key.length)) return 0;
    if(!table) return 0;
    if(unlikely(table->capacity > ATTRIBUTE_THRESH)){
        return AttrTable_has_hash((AttrTable*)table, key);
    }
    StringView2* items = (StringView2*)table->buff;
    size_t count = table->count;
    for(size_t i = 0; i < count; i++){
        if(SV_equals(items[i].key, key)){
            return 1;
        }
    }
    return 0;
}

// Call in hash mode.
//
// Returns 1 if key was present and 0 if missing.
static inline
int
AttrTable_del_hash(AttrTable* table, StringView key){
    uint32_t hash = hash_align1(key.text, key.length);
    size_t cap = table->capacity;
    StringView2* items = (StringView2*)table->buff;
    uint32_t* indexes = (uint32_t*)(table->buff + cap*sizeof(StringView2));
    uint32_t idx = fast_reduce32(hash, (uint32_t)cap);
    for(;;){
        uint32_t i = indexes[idx];
        if(i == UINT32_MAX) // empty slot
            return 0;
        if(SV_equals(items[i].key, key)){
            items[i].key = SV("");
            table->tombs++;
            return 1;
        }
        idx++;
        if(unlikely(idx >= cap)) idx = 0;
    }
    return 0; // unreachable
}

// Call in either mode.
//
// Returns 1 if key was present and 0 if missing.
static inline
int
AttrTable_del(AttrTable*_Nullable table, StringView key){
    if(!key.length) return 0;
    if(!table) return 0;
    if(unlikely(table->capacity > ATTRIBUTE_THRESH)){
        return AttrTable_del_hash((AttrTable*)table, key);
    }
    StringView2* items = (StringView2*)table->buff;
    if(!table->count) return 0;
    size_t count = table->count;
    for(size_t i = 0; i < count; i++){
        if(SV_equals(items[i].key, key)){
            items[i].key = SV("");
            table->tombs++;
            return 1;
        }
    }
    return 0;
}

//
// Makes a clone of the table, "returning" through the outparam.
//
// Copy is a shallow copy (the underlying strings are not dup'd).
//
// Returns 1 on OOM and 0 otherwise.
static inline
warn_unused
int
AttrTable_dup(AttrTable*_Nullable table, Allocator a, AttrTable*_Nullable*_Nonnull outtable){
    if(!table){
        *outtable = NULL;
        return 0;
    }
    AttrTable* copy = Allocator_dupe(a, (AttrTable*)table, AttrTable_alloc_size(table));
    if(!copy) return 1;
    *outtable = copy;
    return 0;
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
