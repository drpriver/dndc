//
// Copyright © 2021-2022, David Priver
//
#ifndef ALLSTD_H
#define ALLSTD_H
// Just put everything in one header and have
// the other headers just include this one.
typedef __builtin_va_list va_list;
#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_end(ap)          __builtin_va_end(ap)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
extern unsigned char __heap_base[];
static unsigned char*_Nonnull _base_ptr = __heap_base;
typedef typeof(sizeof(1)) size_t;
_Static_assert(sizeof(size_t) == 4, "");
typedef long ssize_t;
_Static_assert(sizeof(size_t) == sizeof(ssize_t), "");
enum {SIZE_T_SIZE=sizeof(size_t)};
typedef short int16_t;
typedef unsigned short uint16_t;
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef long ptrdiff_t;
typedef unsigned long uintptr_t;
typedef long intptr_t;
#define UINTPTR_MAX 0xFFFFFFFF
#define INT32_MAX 2147483647
#define UINT32_MAX 4294967295u
#define UINT64_MAX 18446744073709551615llu

#define INT32_MIN  (-INT32_MAX-1)
_Static_assert(sizeof(int32_t) == sizeof(int), "");
#define INT_MAX INT32_MAX
#define INT_MIN INT32_MIN
#define INT64_MAX 9223372036854775807ll
#define INT64_MIN (-INT64_MAX-1)
_Static_assert(sizeof(uint8_t)==1, "");
_Static_assert(sizeof(int8_t)==1, "");
_Static_assert(sizeof(uint16_t)==2, "");
_Static_assert(sizeof(int16_t)==2, "");
_Static_assert(sizeof(uint32_t)==4, "");
_Static_assert(sizeof(int32_t)==4, "");
_Static_assert(sizeof(int64_t)==8, "");
_Static_assert(sizeof(uint64_t)==8, "");

#define bool _Bool
#define true 1
#define false 0

#define assert(x) (__builtin_expect(!(x), 0), (void)0)

#define NULL ((void*)0)

static inline
void* _Nullable
memchr(const void*_Nonnull pointer, int c, size_t nbytes){
    unsigned char*p  = (unsigned char*)pointer;
    while(nbytes--){
        if(*p == c)
            return p;
        p++;
    }
    return NULL;
}

size_t strlen(const char* p){
    for(size_t i = 0; ; i++){
        if(p[i] == 0)
            return i;
    }
}

static
void*_Nullable
strchr(const char*_Nonnull pointer, int c){
    while(*pointer!= 0){
        if(*pointer == c)
            return pointer;
        pointer++;
    }
    return NULL;
}

// static inline
void*_Null_unspecified
memcpy(void*_Nonnull dst, const void*_Nonnull src, size_t nbytes){
    char* d = dst;
    const char* s = src;
    while(nbytes--)
        *d++ = *s++;
    return dst;
}

// static inline
void
bzero(void*_Nonnull s, size_t nbytes){
    uint64_t* dl = s;
    while((nbytes-=8) > 7)
        *dl++ = 0;
    char* d = (char*)dl;
    while(nbytes--)
        *d++ = 0;
}

void*
memset(void*_Nonnull s, int c, size_t n){
    unsigned char* str = s;
    unsigned char ch = c;
    while(n--)
        *str++ = ch;
    return s;
}


static inline
void*_Nonnull
alloc(size_t size, size_t alignment){
    if(alignment > SIZE_T_SIZE)
        alignment = SIZE_T_SIZE;
    size_t b = (size_t)_base_ptr;
    if(b & (alignment-1)){
        b += alignment - (b & (alignment-1));
        _base_ptr = (unsigned char*)b;
    }
    void* result = _base_ptr;
    _base_ptr += size;
    return result;
}

static inline
void
dealloc(const void*_Nonnull pointer, size_t size){
    size_t p = (size_t)pointer;
    size_t b = (size_t)_base_ptr;
    if(p + size == b){
        _base_ptr = (unsigned char*)p;
    }
}

extern
void
reset_memory(void){
    _base_ptr = __heap_base;
}

void*_Nonnull
malloc(size_t size){
    return alloc(size, 8);
}

void*
calloc(size_t n_items, size_t item_size){
    void* result = malloc(n_items*item_size);
    memset(result, 0, n_items*item_size);
    return result;
}

void
free(void* p){
    (void)p;
}

int printf(const char*_Nonnull fmt, ...){
    (void)fmt;
    return 0;
}
typedef struct FILE FILE;

int puts(const char* str){
    (void)str;
    return 0;
}
int fputs(const char* str, FILE*stream){
    (void)str;
    (void)stream;
    return 0;
}
int putchar(int c){
    (void)c;
    return c;
}

int
fprintf(FILE*_Nonnull fp, const char*_Nonnull fmt, ...){
    (void)fp;
    (void)fmt;
    return 0;
}
size_t fread(void* ptr, size_t size, size_t nitems, FILE* stream){
    (void)ptr, (void)size, (void)nitems, (void)stream;
    return 0;
}

void
perror(const char* s){
    (void)s;
}

static
void*
sane_realloc(void* p, size_t orig_size, size_t size){
    void* result = malloc(size);
    if(orig_size){
        memcpy(result, p, orig_size);
        free(p);
    }
    return result;
}

extern
void __attribute__((noreturn))
abort(void);

int
memcmp(const void* s1, const void* s2, size_t n){
    const unsigned char* pa = s1;
    const unsigned char* pb = s2;
    while(n--){
        unsigned char a = *pa++;
        unsigned char b = *pb++;
        int diff = a - b;
        if(diff)
            return diff;
    }
    return 0;
}

int
strcmp(const char* s1, const char* s2){
    const unsigned char* pa = s1;
    const unsigned char* pb = s2;
    for(;;){
        unsigned char a = *pa++;
        unsigned char b = *pb++;
        int diff = a - b;
        if(diff)
            return diff;
        if(!a) // both are zero by the above comparison.
            return 0;
    }
    return 0;
}

void*
memmove(void* dst, const void* src, size_t len){
    if(src == dst)
        return dst;
    if(src < dst){
        char* d_end = dst + len;
        const char* s_end = src+len;
        while(len--)
            *--d_end = *--s_end;
        return dst;
    }
    else
        return memcpy(dst, src, len);
}

int abs(int i){
    return i < 0 ? -i : i;
}

#define stdout (FILE*)0xcafebabe
#define stdin (FILE*)0xcafebabe
#define stderr (FILE*)0xcafebabe

#endif
