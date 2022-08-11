//
// Copyright © 2021-2022, David Priver
//
#ifndef LINK_TABLE_H
#define LINK_TABLE_H
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

typedef struct StringTable StringTable;
struct StringTable {
    Allocator allocator;
    uint32_t count_;
    uint32_t capacity_;
    StringView*_Null_unspecified keys;
};

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
warn_unused
int
string_table_set(StringTable* table, StringView key, StringView value){
    if(!key.length) return 1;
    if(table->count_ *2 >= table->capacity_){
        size_t old_cap = table->capacity_;
        size_t new_cap = old_cap?old_cap*2:128;
        StringView* new_keys = Allocator_zalloc(table->allocator, sizeof(*new_keys) * new_cap*2);
        if(unlikely(!new_keys)) return 1;
        StringView* new_values = new_keys+new_cap;
        if(old_cap){
            StringView* old_keys = table->keys;
            StringView* old_values = table->keys + old_cap;
            for(size_t i = 0; i < old_cap; i++){
                if(old_keys[i].length){
                    StringView k = old_keys[i];
                    StringView v = old_values[i];
                    uint32_t hash = hash_align1(k.text, k.length);
                    uint32_t idx = fast_reduce32(hash, new_cap);
                    // We know that none of the keys are equal, so just find an empty slot.
                    while(new_keys[idx].length){
                        idx++;
                        if(idx >= new_cap) idx = 0;
                    }
                    new_keys[idx] = k;
                    new_values[idx] = v;
                }
            }
            assert(old_keys);
            Allocator_free(table->allocator, old_keys, sizeof(*old_keys)*old_cap*2);
        }
        table->keys = new_keys;
        table->capacity_ = new_cap;
    }
    size_t cap = table->capacity_;
    uint32_t hash = hash_align1(key.text, key.length);
    StringView* keys = table->keys;
    StringView* values = keys + cap;
    uint32_t idx = fast_reduce32(hash, cap);
    for(;;){
        if(!keys[idx].length){
            keys[idx] = key;
            values[idx] = value;
            table->count_++;
            return 0;
        }
        else {
            if(SV_equals(key, keys[idx])){
                values[idx] = value;
                return 0;
            }
        }
        idx++;
        if(idx >= cap) idx = 0;
    }

}

static
const StringView* _Nullable
string_table_get(StringTable* table, StringView key){
    if(!table->count_) return NULL;
    size_t cap = table->capacity_;
    uint32_t hash = hash_align1(key.text, key.length);
    const StringView* keys = table->keys;
    const StringView* values = keys + cap;
    uint32_t idx = fast_reduce32(hash, cap);
    while(keys[idx].length){
        if(SV_equals(key, keys[idx]))
            return &values[idx];
        idx++;
        if(idx >= cap) idx = 0;
    }
    return NULL;
}

static inline
void
string_table_destroy(StringTable* table){
    if(table->keys)
        Allocator_free(table->allocator, table->keys, sizeof(*table->keys)*2*table->capacity_);
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
