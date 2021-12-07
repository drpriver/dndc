#ifndef BB_EXTENSIONS_H
#define BB_EXTENSIONS_H
#include "ByteBuilder.h"
#include "file_util.h"
#include "base64.h"
#include "errorable_long_string.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

//
// Reads an entire file into a ByteBuilder.
// Returns an error if the data could not be read for the file for
// whatever reason (file doesn't exist, some random file error, whatever).
// The byte builder handles allocating enough data to hold the contents
// of the file.
static inline
Errorable_f(void)
bb_read_bin_file(ByteBuilder* bb, const char* filename);

#ifdef USE_C_STDIO

static inline
Errorable_f(void)
bb_read_bin_file(ByteBuilder* bb, const char* filename){
    Errorable(void) result = {0};
    FILE* fp = fopen(filename, "rb");
    if(!fp){
        result.errored = FILE_NOT_OPENED;
        return result;
    }
    FileSizeResult size_e = file_size_from_fp(fp);
    if(size_e.errored){
        result.errored = FILE_ERROR;
        goto finally;
    }
    size_t nbytes = size_e.result;
    bb_reserve(bb, nbytes);
    void* data = bb->data + bb->cursor;
    size_t fread_result = fread(data, 1, nbytes, fp);
    if(fread_result != nbytes){
        result.errored = FILE_ERROR;
        goto finally;
    }
    assert(fread_result == nbytes);
    bb->cursor += nbytes;
finally:
    fclose(fp);
    return result;
}

#elif defined(__linux__) || defined(__APPLE__)
static inline
Errorable_f(void)
bb_read_bin_file(ByteBuilder* bb, const char* filename){
    Errorable(void) result = {0};
    int fd = open(filename, O_RDONLY);
    if(fd < 0)
        return (Errorable(void)){FILE_NOT_OPENED};
    FileSizeResult size_e = file_size_from_fd(fd);
    if(size_e.errored){
        result.errored = FILE_ERROR;
        goto finally;
    }
    size_t nbytes = size_e.result;
    bb_reserve(bb, nbytes);
    void* data = bb->data + bb->cursor;
    ssize_t read_result = read(fd, data, nbytes);
    if(read_result != (ssize_t)nbytes){
        result.errored = FILE_ERROR;
        goto finally;
    }
    assert(read_result == (ssize_t)nbytes);
    bb->cursor += nbytes;
finally:
    close(fd);
    return result;
}

#elif defined(_WIN32)
static inline
Errorable_f(void)
bb_read_bin_file(ByteBuilder* bb, const char* filename){
    Errorable(void) result = {0};
    PushDiagnostic();
    SuppressDiscardQualifiers();
    HANDLE handle = CreateFile(
            filename,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );
    PopDiagnostic();
    if(handle == INVALID_HANDLE_VALUE)
        return (Errorable(void)){FILE_NOT_OPENED};
    LARGE_INTEGER size;
    BOOL size_success = GetFileSizeEx(handle, &size);
    if(!size_success){
        goto finally;
    }
    size_t nbytes = size.QuadPart;
    bb_reserve(bb, nbytes);
    void* data = bb->data + bb->cursor;
    DWORD nread;
    BOOL read_success = ReadFile(handle, data, nbytes, &nread, NULL);
    if(!read_success){
        result.errored = FILE_ERROR;
        goto finally;
    }
    assert(nread == nbytes);
    bb->cursor += nbytes;
finally:
    CloseHandle(handle);
    return result;
}
#elif defined(WASM)
static inline
Errorable_f(void)
bb_read_bin_file(ByteBuilder* bb, const char* filename){
    (void)bb, (void)filename;
    Errorable(void) result = {.errored=OS_ERROR};
    return result;
}
#endif

//
// Reads in a file as binary and base64-ifies it.
// The byte builder is just as a buffer to read into, so you can
// use something like a temporary allocator for its allocator
// if yu have one.
//
// All failures are file-related errors.
// The base64-ifying can't fail.
//
static
Errorable_f(LongString)
read_and_base64_bin_file(ByteBuilder* bb, const Allocator a, const char* filepath){
    Errorable(LongString) result = {0};
    assert(bb->cursor == 0);
    Errorable(void) e = bb_read_bin_file(bb, filepath);
    if(e.errored){
        result.errored = e.errored;
        return result;
    }
    ByteBuffer buff = bb_borrow(bb);
    MStringBuilder sb = {.allocator=a};
    msb_write_b64(&sb, buff.buff, buff.n_bytes);
    result.result = msb_detach_ls(&sb);
    bb_reset(bb);
    return result;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
