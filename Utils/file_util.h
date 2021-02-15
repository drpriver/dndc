#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "common_macros.h"
#include "long_string.h"
#include "d_memory.h"
#include "allocator.h"

#ifdef WINDOWS
#define USE_C_STDIO
#endif

// #define USE_C_STDIO
#ifdef USE_C_STDIO
static inline
force_inline
Errorable_f(size_t)
file_size_from_fp(Nonnull(FILE*) fp){
    Errorable(size_t) result = {};
    #if 1
        auto fd = fileno(fp);
        struct stat s;
        auto err = fstat(fd, &s);
        if(err == -1){
            Raise(FILE_ERROR);
            }
        result.result = s.st_size;
    #else
        // technically the portable way to do this.
        fseek(fp, 0, SEEK_END);
        result.result = s.st_size;
        fseek(fp, 0, SEEK_SET);
    #endif
    return result;
    }

static inline
Errorable_f(LongString)
read_file(Nonnull(const char*)filepath){
    Errorable(LongString) result = {};
    auto fp = fopen(filepath, "rb");
    if(not fp)
        Raise(FILE_NOT_OPENED);
    auto size_e = file_size_from_fp(fp);
    if(size_e.errored){
        result.errored = FILE_ERROR;
        goto finally;
        }
    auto nbytes = unwrap(size_e);
    char* text = malloc(nbytes+1);
    if(!text){
        result.errored = OUT_OF_SPACE;
        goto finally;
        }
    assert(text);
    auto fread_result = fread(text, 1, nbytes, fp);
    if(fread_result != nbytes){
        free(text);
        result.errored = FILE_ERROR;
        goto finally;
        }
    assert(fread_result == nbytes);
    text[nbytes] = '\0';
    result.result.text = text;
    result.result.length = nbytes;
finally:
    fclose(fp);
    return result;
    }

static inline
Errorable_f(LongString)
read_file_a(Nonnull(const Allocator*)a, Nonnull(const char*)filepath){
    Errorable(LongString) result = {};
    auto fp = fopen(filepath, "rb");
    if(not fp)
        Raise(FILE_NOT_OPENED);
    auto size_e = file_size_from_fp(fp);
    if(size_e.errored){
        fclose(fp);
        Raise(FILE_ERROR);
        }
    auto nbytes = unwrap(size_e);
    char* text = Allocator_alloc(a, nbytes+1);
    if(!text){
        result.errored = OUT_OF_SPACE;
        goto finally;
        }
    auto fread_result = fread(text, 1, nbytes, fp);
    if(fread_result != nbytes){
        result.errored = FILE_ERROR;
        Allocator_free(a, text, nbytes+1);
        goto finally;
        }
    text[nbytes] = '\0';
    result.result.text = text;
    result.result.length = nbytes;
finally:
    fclose(fp);
    return result;
    }

static inline
Errorable_f(ByteBuffer)
read_bin_file(Nonnull(const char*)filepath){
    Errorable(ByteBuffer) result = {};
    auto fp = fopen(filepath, "rb");
    if(not fp)
        Raise(FILE_NOT_OPENED);
    auto size_e = file_size_from_fp(fp);
    if(size_e.errored){
        result.errored = FILE_ERROR;
        goto finally;
        }
    auto nbytes = unwrap(size_e);
    void* data = malloc(nbytes);
    if(!data){
        result.errored = OUT_OF_SPACE;
        goto finally;
        }
    assert(data);
    auto fread_result = fread(data, 1, nbytes, fp);
    if(fread_result != nbytes){
        free(data);
        result.errored = FILE_ERROR;
        goto finally;
        }
    assert(fread_result == nbytes);
    result.result.buff = data;
    result.result.n_bytes = nbytes;
finally:
    fclose(fp);
    return result;
    }

static inline
Errorable_f(ByteBuffer)
read_bin_file_a(Nonnull(const Allocator*)a, Nonnull(const char*)filepath){
    Errorable(ByteBuffer) result = {};
    auto fp = fopen(filepath, "rb");
    if(not fp)
        Raise(FILE_NOT_OPENED);
    auto size_e = file_size_from_fp(fp);
    if(size_e.errored){
        result.errored = FILE_ERROR;
        goto finally;
        }
    auto nbytes = unwrap(size_e);
    void* data = Allocator_alloc(a, nbytes);
    if(!data){
        result.errored = OUT_OF_SPACE;
        goto finally;
        }
    assert(data);
    auto fread_result = fread(data, 1, nbytes, fp);
    if(fread_result != nbytes){
        Allocator_free(a, data, nbytes);
        result.errored = FILE_ERROR;
        goto finally;
        }
    assert(fread_result == nbytes);
    result.result.buff = data;
    result.result.n_bytes = nbytes;
finally:
    fclose(fp);
    return result;
    }

#else
#include <unistd.h>
#include <fcntl.h>
static inline
force_inline
Errorable_f(size_t)
file_size_from_fd(int fd){
    Errorable(size_t) result = {};
    struct stat s;
    auto err = fstat(fd, &s);
    if(err == -1){
        Raise(FILE_ERROR);
        }
    result.result = s.st_size;
    return result;
    }

static inline
Errorable_f(LongString)
read_file(Nonnull(const char*)filepath){
    Errorable(LongString) result = {};
    int fd = open(filepath, O_RDONLY);
    if(fd < 0)
        Raise(FILE_NOT_OPENED);
    auto size_e = file_size_from_fd(fd);
    if(size_e.errored){
        result.errored = FILE_ERROR;
        goto finally;
        }
    auto nbytes = unwrap(size_e);
    char* text = malloc(nbytes+1);
    if(!text){
        result.errored = OUT_OF_SPACE;
        goto finally;
        }
    assert(text);
    auto read_result = read(fd, text, nbytes);
    if(read_result != nbytes){
        free(text);
        result.errored = FILE_ERROR;
        goto finally;
        }
    assert(read_result == nbytes);
    text[nbytes] = '\0';
    result.result.text = text;
    result.result.length = nbytes;
finally:
    close(fd);
    return result;
    }

static inline
Errorable_f(LongString)
read_file_a(Nonnull(const Allocator*)a, Nonnull(const char*)filepath){
    Errorable(LongString) result = {};
    int fd = open(filepath, O_RDONLY);
    if(fd < 0)
        Raise(FILE_NOT_OPENED);
    auto size_e = file_size_from_fd(fd);
    if(size_e.errored){
        result.errored = FILE_ERROR;
        goto finally;
        }
    auto nbytes = unwrap(size_e);
    char* text = Allocator_alloc(a, nbytes+1);
    if(!text){
        result.errored = OUT_OF_SPACE;
        goto finally;
        }
    auto read_result = read(fd, text, nbytes);
    if(read_result != nbytes){
        Allocator_free(a, text, nbytes+1);
        result.errored = FILE_ERROR;
        goto finally;
        }
    assert(read_result == nbytes);
    text[nbytes] = '\0';
    result.result.text = text;
    result.result.length = nbytes;
finally:
    close(fd);
    return result;
    }

static inline
Errorable_f(ByteBuffer)
read_bin_file(Nonnull(const char*)filepath){
    Errorable(ByteBuffer) result = {};
    int fd = open(filepath, O_RDONLY);
    if(fd < 0)
        Raise(FILE_NOT_OPENED);
    auto size_e = file_size_from_fd(fd);
    if(size_e.errored){
        result.errored = FILE_ERROR;
        goto finally;
        }
    auto nbytes = unwrap(size_e);
    void* data = malloc(nbytes);
    if(!data){
        result.errored = OUT_OF_SPACE;
        goto finally;
        }
    assert(data);
    auto read_result = read(fd, data, nbytes);
    if(read_result != nbytes){
        free(data);
        result.errored = FILE_ERROR;
        goto finally;
        }
    assert(read_result == nbytes);
    result.result.buff = data;
    result.result.n_bytes = nbytes;
finally:
    close(fd);
    return result;
    }

static inline
Errorable_f(ByteBuffer)
read_bin_file_a(Nonnull(const Allocator*)a, Nonnull(const char*)filepath){
    Errorable(ByteBuffer) result = {};
    int fd = open(filepath, O_RDONLY);
    if(fd < 0)
        Raise(FILE_NOT_OPENED);
    auto size_e = file_size_from_fd(fd);
    if(size_e.errored){
        result.errored = FILE_ERROR;
        goto finally;
        }
    auto nbytes = unwrap(size_e);
    void* data = Allocator_alloc(a, nbytes);
    if(!data){
        result.errored = OUT_OF_SPACE;
        goto finally;
        }
    assert(data);
    auto read_result = read(fd, data, nbytes);
    if(read_result != nbytes){
        Allocator_free(a, data, nbytes);
        result.errored = FILE_ERROR;
        goto finally;
        }
    assert(read_result == nbytes);
    result.result.buff = data;
    result.result.n_bytes = nbytes;
finally:
    close(fd);
    return result;
    }

#endif

static inline
Errorable_f(uint64_t)
get_file_mtime(Nonnull(const char*) filename){
    Errorable(uint64_t) result = {};
    struct stat file_stat;
    if(stat(filename, &file_stat)){
        Raise(FILE_ERROR);
        }
    #ifdef DARWIN
        auto mtime = &file_stat.st_mtimespec;
        result.result = mtime->tv_sec * 1000000000llu + mtime->tv_nsec;
    #elif defined(LINUX)
        auto mtime = &file_stat.st_mtim;
        result.result = mtime->tv_sec * 1000000000llu + mtime->tv_nsec;
    #elif defined(WINDOWS)
    // check this is correct?
        result.result = file_stat.st_mtime;
    #else
        #error "Unrecognized os"
    #endif
    return result;
    }


// Note: this is almost never what you want, and should
// instead should just try to open the file for reading if you
// need to read it or call `open` with the appropriate exclusivity
// flag if you need to write to a file only if it doesn't exist.
//
// However, you might just be checking that a file with the same
// name exists in two directories at once or something, so this
// simplifies that.
static inline
bool
file_exists(Nonnull(const char*) filename){
    struct stat unused;
    return !stat(filename, &unused);
    }
#ifdef USE_C_STDIO
// You can probably do the same semantics as unix,
// but I don't feel like looking up the calls in mdn.
static inline
Errorable_f(void)
write_and_swap_file(Nonnull(const char*)filename, Nonnull(const char*) tempname, Nonnull(const void*)data, size_t data_length){
    Errorable(void) result = {};
    // Technically the "x" specifier is not conformant, but it is documented
    // to work on macos, windows and glibc linux.
    // "x" makes it fail if the file already exists.
    auto fp = fopen(tempname, "wxb");
    if(!fp) Raise(FILE_NOT_OPENED);

    size_t nwrit = fwrite(data, 1, data_length, fp);
    if(nwrit != data_length){
        fclose(fp);
        remove(tempname);
        Raise(FILE_ERROR);
        }
    fflush(fp);
    fclose(fp);
    auto err = rename(tempname, filename);
    if(err == -1){
        remove(tempname);
        Raise(FILE_ERROR);
        }
    return result;
    }
static inline
Errorable_f(void)
write_file(Nonnull(const char*)filename, Nonnull(const void*)data, size_t data_length){
    Errorable(void) result = {};
    auto fp = fopen(filename, "wb");
    if(!fp) Raise(FILE_NOT_OPENED);

    size_t nwrit = fwrite(data, 1, data_length, fp);
    if(nwrit != data_length){
        fclose(fp);
        Raise(FILE_ERROR);
        }
    fflush(fp);
    fclose(fp);
    return result;
    }
#else
// Write to the temporary location (exclusively) and then rename to filename
// Greatly reduces chance of data corruption for things like savefiles.
static inline
Errorable_f(void)
write_and_swap_file(Nonnull(const char*)filename, Nonnull(const char*) tempname, Nonnull(const void*)data, size_t data_length){
    Errorable(void) result = {};
    return result;
    int fd = open(
            tempname,
            O_WRONLY | O_EXCL | O_NOFOLLOW | O_CREAT,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(fd < 0)
        Raise(FILE_NOT_OPENED);
    ssize_t nwrit = write(fd, data, data_length);
    if(nwrit != data_length){
        close(fd);
        remove(tempname);
        Raise(FILE_ERROR);
        }
    // I don't think we need to fsync.
    // But I am unsure, so just to be safe.
    fsync(fd);
    close(fd);
    auto err = rename(tempname, filename);
    if(err == -1){
        remove(tempname);
        Raise(FILE_ERROR);
        }
    return result;
    }
static inline
Errorable_f(void)
write_file(Nonnull(const char*)filename, Nonnull(const void*)data, size_t data_length){
    Errorable(void) result = {};
    int fd = open(
            filename,
            O_WRONLY | O_NOFOLLOW | O_CREAT | O_TRUNC,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(fd < 0)
        Raise(FILE_NOT_OPENED);
    ssize_t nwrit = write(fd, data, data_length);
    if(nwrit != data_length){
        close(fd);
        Raise(FILE_ERROR);
        }
    // I don't think we need to fsync.
    // But I am unsure, so just to be safe.
    // fsync(fd);
    close(fd);
    return result;
    }

#endif

#endif
