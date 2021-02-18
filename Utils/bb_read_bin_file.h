#ifndef BB_READ_BIN_FILE_H
#define BB_READ_BIN_FILE_H
#include "ByteBuilder.h"
#include "file_util.h"

//
// Reads an entire file into a ByteBuilder.
// Returns an error if the data could not be read for the file for
// whatever reason (file doesn't exist, some random file error, whatever).
// The byte builder handles allocating enough data to hold the contents
// of the file.
static inline
Errorable_f(void)
bb_read_bin_file(Nonnull(ByteBuilder*)bb, Nonnull(const char*)filename);

#ifdef USE_C_STDIO

static inline
Errorable_f(void)
bb_read_bin_file(Nonnull(ByteBuilder*)bb, Nonnull(const char*)filename){
    Errorable(void) result = {};
    auto fp = fopen(filename, "rb");
    if(not fp)
        Raise(FILE_NOT_OPENED);
    auto size_e = file_size_from_fp(fp);
    if(size_e.errored){
        result.errored = FILE_ERROR;
        goto finally;
        }
    auto nbytes = unwrap(size_e);
    bb_reserve(bb, nbytes);
    void* data = bb->data + bb->cursor;
    auto fread_result = fread(data, 1, nbytes, fp);
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

#elif defined(LINUX) || defined(DARWIN)
static inline
Errorable_f(void)
bb_read_bin_file(Nonnull(ByteBuilder*)bb, Nonnull(const char*)filename){
    Errorable(void) result = {};
    int fd = open(filename, O_RDONLY);
    if(fd < 0)
        Raise(FILE_NOT_OPENED);
    auto size_e = file_size_from_fd(fd);
    if(size_e.errored){
        result.errored = FILE_ERROR;
        goto finally;
        }
    auto nbytes = unwrap(size_e);
    bb_reserve(bb, nbytes);
    void* data = bb->data + bb->cursor;
    auto read_result = read(fd, data, nbytes);
    if(read_result != nbytes){
        result.errored = FILE_ERROR;
        goto finally;
        }
    assert(read_result == nbytes);
    bb->cursor += nbytes;
finally:
    close(fd);
    return result;
    }

#elif defined(WINDOWS)
static inline
Errorable_f(void)
bb_read_bin_file(Nonnull(ByteBuilder*)bb, Nonnull(const char*)filename){
    Errorable(void) result = {};
    PushDiagnostic();
    SuppressDiscardQualifiers();
    auto handle = CreateFile(
            filename,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
            );
    PopDiagnostic();
    if(handle == INVALID_HANDLE_VALUE){
        Raise(FILE_NOT_OPENED);
        }
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

#endif



#endif
