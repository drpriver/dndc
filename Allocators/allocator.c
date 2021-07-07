#ifndef ALLOCATOR_C
#define ALLOCATOR_C
#include "allocator.h"
#include "linear_allocator.h"
#include "mallocator.h"
#include "recording_allocator.h"

typedef struct BigAllocation {
    Nullable(struct BigAllocation*) next;
}BigAllocation;
#ifndef PAGE_SIZE
enum {PAGE_SIZE=4096};
#endif
enum {ARENA_SIZE=PAGE_SIZE*64};
enum {ARENA_BUFFER_SIZE = ARENA_SIZE-sizeof(void*)-sizeof(size_t)-sizeof(size_t)};
typedef struct Arena{
    Nullable(struct Arena*) prev;
    size_t used;
    size_t last;
    char buff[ARENA_BUFFER_SIZE];
}Arena;

static inline
force_inline
size_t
round_size_up(size_t size){
    if(size & 7){
        size += 8 - (size & 7);
        }
    return size;
    }
static
Nonnull(void*)
ArenaAllocator_alloc(Nonnull(ArenaAllocator*)aa, size_t size){
    size = round_size_up(size);
    if(size > ARENA_BUFFER_SIZE){
        BigAllocation* ba = malloc(sizeof(*ba)+size);
        ba->next = aa->big_allocations;
        aa->big_allocations = ba;
        return ba+1;
        }
    if(!aa->arena){
        Arena* arena = malloc(sizeof(*arena));
        arena->prev = NULL;
        arena->used = 0;
        arena->last = 0;
        aa->arena = arena;
        }
    if(size > ARENA_BUFFER_SIZE - aa->arena->used){
        Arena* arena = malloc(sizeof(*arena));
        arena->prev = aa->arena;
        arena->used = size;
        arena->last = 0;
        aa->arena = arena;
        return arena->buff;
        }
    aa->arena->last = aa->arena->used;
    aa->arena->used += size;
    return aa->arena->buff + aa->arena->last;
    }
static
Nonnull(void*)
ArenaAllocator_zalloc(Nonnull(ArenaAllocator*)aa, size_t size){
    size = round_size_up(size);
    if(size > ARENA_SIZE/2){
        BigAllocation* ba = calloc(1, sizeof(*ba)+size);
        ba->next = aa->big_allocations;
        aa->big_allocations = ba;
        return ba+1;
        }
    if(!aa->arena){
        Arena* arena = calloc(1, sizeof(*arena));
        arena->prev = NULL;
        arena->used = 0;
        arena->last = 0;
        aa->arena = arena;
        }
    if(size > ARENA_BUFFER_SIZE - aa->arena->used){
        Arena* arena = calloc(1, sizeof(*arena));
        arena->prev = aa->arena;
        arena->used = size;
        arena->last = 0;
        aa->arena = arena;
        return arena->buff;
        }
    aa->arena->last = aa->arena->used;
    aa->arena->used += size;
    void* result =  aa->arena->buff + aa->arena->last;
    memset(result, 0, size);
    return result;
    }

static
Nullable(void*)
ArenaAllocator_realloc(Nonnull(ArenaAllocator*)aa, Nullable(void*)ptr, size_t old_size, size_t new_size){
    if(!old_size || !ptr){
        return ArenaAllocator_alloc(aa, new_size);
        }
    if(!new_size)
        return NULL;
    old_size = round_size_up(old_size);
    new_size = round_size_up(new_size);
    if(new_size > ARENA_BUFFER_SIZE){
        BigAllocation* ba = malloc(sizeof(*ba)+new_size);
        ba->next = aa->big_allocations;
        aa->big_allocations = ba;
        void* result = ba+1;
        if(old_size < new_size)
            memcpy(result, ptr, old_size);
        else
            memcpy(result, ptr, new_size);
        return result;
        }
    if(old_size > ARENA_BUFFER_SIZE){
        void* result = ArenaAllocator_alloc(aa, new_size);
        if(old_size < new_size)
            memcpy(result, ptr, old_size);
        else
            memcpy(result, ptr, new_size);
        return result;
        }
    assert(aa->arena);
    if(aa->arena->last + aa->arena->buff == ptr){
        if(new_size <= ARENA_BUFFER_SIZE - aa->arena->last){
            aa->arena->used = aa->arena->last + new_size;
            return ptr;
            }
        }
    void* result = ArenaAllocator_alloc(aa, new_size);
    if(old_size < new_size)
        memcpy(result, ptr, old_size);
    else
        memcpy(result, ptr, new_size);
    return result;
    }

static
void
ArenaAllocator_free_all(Nullable(ArenaAllocator*)aa){
    Arena* arena = aa->arena;
    while(arena){
        Arena* to_free = arena;
        arena = arena->prev;
        free(to_free);
        }
    BigAllocation* ba = aa->big_allocations;
    while(ba){
        BigAllocation* to_free = ba;
        ba = ba->next;
        free(to_free);
        }
    aa->arena = NULL;
    aa->big_allocations = NULL;
    return;
    }


static inline
void
Allocator_free_all(const Allocator a){
    switch(a.type){
        case ALLOCATOR_UNSET:
            abort();
            break;
        case ALLOCATOR_MALLOC:
            abort();
            break;
        case ALLOCATOR_LINEAR:
            linear_reset(a._data);
            break;
        case ALLOCATOR_RECORDED:
            recording_free_all(a._data);
            break;
        case ALLOCATOR_ARENA:
            ArenaAllocator_free_all(a._data);
            break;
    PushDiagnostic();
    SuppressCoveredSwitchDefault();
        default:
            abort();
            break;
    PopDiagnostic();
        }
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_alloc(const Allocator a, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            abort();
            break;
        case ALLOCATOR_LINEAR:
            return linear_alloc(a._data, size);
        case ALLOCATOR_MALLOC:
            return malloc(size);
        case ALLOCATOR_RECORDED:
            return recording_alloc(a._data, size);
        case ALLOCATOR_ARENA:
            return ArenaAllocator_alloc(a._data, size);
    PushDiagnostic();
    SuppressCoveredSwitchDefault();
        default:
            abort();
            break;
    PopDiagnostic();
        }
    unreachable();
    }

MALLOC_FUNC
ALLOCATOR_SIZE(2)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_zalloc(const Allocator a, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            abort();
            break;
        case ALLOCATOR_LINEAR:
            return linear_zalloc(a._data, size);
        case ALLOCATOR_MALLOC:
            return calloc(1, size);
        case ALLOCATOR_RECORDED:
            return recording_zalloc(a._data, size);
        case ALLOCATOR_ARENA:
            return ArenaAllocator_zalloc(a._data, size);
    PushDiagnostic();
    SuppressCoveredSwitchDefault();
        default:
            abort();
            break;
    PopDiagnostic();
        }
    unreachable();
    }

ALLOCATOR_SIZE(4)
static inline
// force_inline
warn_unused
Nonnull(void*)
Allocator_realloc(const Allocator a, Nullable(void*) data, size_t orig_size, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            abort();
            break;
        case ALLOCATOR_LINEAR:
            return linear_realloc(a._data, data, orig_size, size);
        case ALLOCATOR_MALLOC:
            return sane_realloc(data, orig_size, size);
        case ALLOCATOR_RECORDED:
            return recording_realloc(a._data, data, orig_size, size);
        case ALLOCATOR_ARENA:
            return (void*)ArenaAllocator_realloc(a._data, data, orig_size, size);
    PushDiagnostic();
    SuppressCoveredSwitchDefault();
        default:
            abort();
            break;
    PopDiagnostic();
        }
    unreachable();
    }

static inline
// force_inline
void
Allocator_free(const Allocator a, Nullable(const void*) data, size_t size){
    switch(a.type){
        case ALLOCATOR_UNSET:
            abort();
            break;
        case ALLOCATOR_LINEAR:
            linear_free(a._data, data, size);
            break;
        case ALLOCATOR_MALLOC:
            const_free(data);
            break;
        case ALLOCATOR_RECORDED:
            recording_free(a._data, data, size);
            break;
        case ALLOCATOR_ARENA:
            break;
    PushDiagnostic();
    SuppressCoveredSwitchDefault();
        default:
            abort();
            break;
    PopDiagnostic();
        }
    }

ALLOCATOR_SIZE(3)
static inline
warn_unused
// force_inline
Nonnull(void*)
Allocator_dupe(const Allocator allocator, Nonnull(const void*) data, size_t size){
    void* result = Allocator_alloc(allocator, size);
    memcpy(result, data, size);
    return result;
    }

MALLOC_FUNC
ALLOCATOR_SIZE(3)
static inline
warn_unused
Nonnull(char*)
Allocator_strndup(const Allocator allocator, Nonnull(const char*)str, size_t length){
    char* result = Allocator_alloc(allocator, length+1);
    unhandled_error_condition(!result);
    if(likely(length))
        memcpy(result, str, length);
    result[length] = '\0';
    return result;
    }
#endif
