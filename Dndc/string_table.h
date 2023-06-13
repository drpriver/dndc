//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
#ifndef STRING_TABLE_H
#define STRING_TABLE_H
// This is a basic hash table of sv to sv.
// However! We only need to support set and get, which simplifies things
// greatly (no tombs, easy to tell when you need to expand).
// We can also use zero length strings as empty keys.
#include <stdint.h>
#include "Utils/long_string.h"
#include "Utils/hash_func.h"
#include "Allocators/allocator.h"

#if !defined(likely) && !defined(unlikely)
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif

#ifndef warn_unused
#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct StringTable StringTable;
struct StringTable {
    uint32_t count_;
    uint32_t capacity_;
    char*_Null_unspecified data;
};

static inline
size_t
string_table_alloc_size(size_t capacity){
    return capacity * sizeof(StringView2) + capacity * sizeof(uint32_t);
}

static inline
StringView2*
string_table_items(const StringTable* table){
    return (StringView2*)table->data;
}

static inline
uint32_t*
string_table_indexes(const StringTable* table){
    return (uint32_t*)(table->data + table->capacity_ * sizeof(StringView2));
}


static
warn_unused
int
string_table_set(StringTable* table, Allocator a, StringView key, StringView value){
    if(unlikely(!key.length)) return 1;
    if(unlikely(table->count_ *2 >= table->capacity_)){
        size_t old_cap = table->capacity_;
        size_t new_cap = old_cap?old_cap*2:128;
        char* new_data = Allocator_realloc(a, table->data, string_table_alloc_size(old_cap), string_table_alloc_size(new_cap));
        if(unlikely(!new_data)) return 1;
        table->data = new_data;
        table->capacity_ = new_cap;
        uint32_t* indexes = string_table_indexes(table);
        memset(indexes, 0xff, sizeof(*indexes)*new_cap);
        StringView2* items = string_table_items(table);
        for(size_t i = 0; i < table->count_; i++){
            StringView k = items[i].key;
            uint32_t hash = hash_align1(k.text, k.length);
            uint32_t idx = fast_reduce32(hash, (uint32_t)new_cap);
            while(indexes[idx] != UINT32_MAX){
                idx++;
                if(unlikely(idx >= new_cap)) idx = 0;
            }
            indexes[idx] = i;
        }
    }
    size_t cap = table->capacity_;
    uint32_t hash = hash_align1(key.text, key.length);
    StringView2* items = string_table_items(table);
    uint32_t* indexes = string_table_indexes(table);
    uint32_t idx = fast_reduce32(hash, cap);
    for(;;){
        uint32_t i = indexes[idx];
        if(i == UINT32_MAX){ // empty slot
            indexes[idx] = table->count_;
            items[table->count_] = (StringView2){.key=key, .value=value};
            table->count_++;
            return 0;
        }
        if(SV_equals(items[i].key, key)){
            items[i].value = value;
            return 0;
        }
        idx++;
        if(unlikely(idx >= cap)) idx = 0;
    }

}

static
warn_unused
int
string_table_get(StringTable* table, StringView key, StringView* outvalue){
    if(!table->count_) return 1;
    size_t cap = table->capacity_;
    uint32_t hash = hash_align1(key.text, key.length);
    const StringView2* items = string_table_items(table);
    const uint32_t* indexes = string_table_indexes(table);
    uint32_t idx = fast_reduce32(hash, cap);
    for(;;){
        uint32_t i = indexes[idx];
        if(i == UINT32_MAX) return 1;
        if(SV_equals(items[i].key, key)){
            *outvalue = items[i].value;
            return 0;
        }
        idx++;
        if(unlikely(idx >= cap)) idx = 0;
    }
    return 1; // unreachable
}

static inline
void
string_table_destroy(StringTable* table, Allocator a){
    if(table->data)
        Allocator_free(a, table->data, string_table_alloc_size(table->capacity_));
    memset(table, 0, sizeof *table);
}

static inline
warn_unused
int
string_table_dup(const StringTable* table, Allocator a, StringTable*outtable){
    char* dupkeys = NULL;
    if(table->data){
        dupkeys = Allocator_dupe(a, table->data, string_table_alloc_size(table->capacity_));
        if(unlikely(!dupkeys))
            return 1;
    }
    *outtable = *table;
    outtable->data = dupkeys;
    return 0;
}



#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
