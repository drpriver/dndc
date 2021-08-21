#ifndef DIRECTORY_UTIL_H
#define DIRECTORY_UTIL_H
#include <string.h>
#ifndef _WIN32
#include <errno.h>
#include <dirent.h>

#else
#include "windowsheader.h"
// windows stuff

#endif
#include "long_string.h"
#include "error_handling.h"
#include "str_util.h"

#ifndef HAVE_MARRY_LONGSTRING
#define MARRAY_T LongString
#include "Marray.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
Errorable_f(void)
directory_gather_files_ending_with(LongString dirpath, StringView suffix, Marray(LongString)* results, Allocator array_allocator, Allocator string_allocator);

static
const char* get_directory_error(void);
static
void free_directory_error(const char*);
#ifndef _WIN32
static
const char*
get_directory_error(void){
    return strerror(errno);
    }
static
void
free_directory_error(const char* err){
    void(err);
    }
static
Errorable_f(void)
directory_gather_files_ending_with(LongString dirpath, StringView suffix, Marray(LongString)* results, Allocator array_allocator, Allocator string_allocator){
    Errorable(void) result = {};
    DIR* dir = opendir(dirpath.text);
    if(!dir){
        result.errored = OS_ERROR;
        return result;
        }
    struct dirent* entry;
    while((entry = readdir(dir))){
        if(entry->d_type != DT_REG && entry->d_type != DT_LNK)
            continue;
#ifdef __APPLE__
        StringView name = {.length = entry->d_namlen, .text = entry->d_name};
#else
        StringView name = {.length = strlen(entry->d_name), .text = entry->d_name};
#endif
        if(name.length >= suffix.length){
            const char* tail = name.text+name.length-suffix.length;
            if(memcmp(tail, suffix.text, suffix.length)==0){
                char* text = Allocator_strndup(string_allocator, name.text, name.length);
                LongString filename = {.text=text, .length=name.length};
                Marray_push(LongString)(results, array_allocator, filename);
                }
            }
        }
    closedir(dir);
    return result;
    }
#else
static
const char*
get_directory_error(void){
    DWORD err = GetLastError();
    char* msg;
    FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (char*)&msg,
            0, NULL);
    return msg;
    }
static
void
free_directory_error(const char* err){
    LocalFree((void*)err);
    }
static
Errorable_f(void)
directory_gather_files_ending_with(LongString dirpath, StringView suffix, Marray(LongString)* results, Allocator array_allocator, Allocator string_allocator){
    Errorable(void) result = {};
    char buff[1024];
    snprintf("%s*%.*s", sizeof(buff), dirpath.text, (int)suffix.length, suffix.text);
    WIN32_FIND_DATAA finddata;
    HANDLE h = FindFirstFileA(buff, &finddata);
    if(h == INVALID_HANDLE_VALUE){
        result.errored = OS_ERROR;
        return result;
        }
    do {
        StringView name = {.length = strlen(finddata.cFileName), .text = finddata.cFileName};
        char* text = Allocator_strndup(string_allocator, name.text, name.length);
        LongString filename = {.text=text, .length=name.length};
        Marray_push(LongString)(results, array_allocator, filename);
    }while(FindNextFile(h, &finddata));
    FindClose(h);
    return result;
    }
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
