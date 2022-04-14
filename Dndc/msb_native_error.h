// TODO: use this header to report errors.
//       We would need to change our general error return type from an int
//       to a (apperror, systemerror) tuple and do that consistently.
#ifndef MSB_NATIVE_ERROR_H
#define MSB_NATIVE_ERROR_H
#if defined(_WIN32)
#include "windowsheader.h"
typedef DWORD native_error_type;
#elif defined(WASM)
typedef int native_error_type;
#else
// posix
#include <errno.h> // errno
#include <string.h> // strerror
typedef int native_error_type;
#endif

#include "MStringBuilder.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

native_error_type
get_last_system_error(void){
    #if defined(_WIN32)
        return GetLastError();
    #elif defined(WASM)
        return 0;
    #else
        return errno;
    #endif
}


// This is an extension for the string builder that will write the text of a
// system error message code into the builder.
//
static inline
void
msb_write_native_error(MStringBuilder*msb, native_error_type error){
#if defined(_WIN32)
    enum {bufflen=4092};
    msb_ensure_additional(msb, bufflen);
    char* buff = msb->data + msb->cursor;
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM
                // "when you are not in control of the message, you
                // had better pass the FORMAT_MESSAGE_IGNORE_INSERTS
                // flag"  - Raymond Chen
                | FORMAT_MESSAGE_IGNORE_INSERTS
                   ;
    DWORD written = FormatMessageA(flags, NULL, error, 0, buff, bufflen, NULL);
    if(written == 0){
        // hmm. This means we failed to write the error message, but we are
        // in the middle of reporting an error...
        msb_write_literal(mss, "(Failed to get system error message)");
    }
    else {
        msb->cursor += written;
    }
#elif defined(WASM)
    msb_write_literal(msb, "System Error");
#else
    // posix
    char* mess = strerror(error);
    msb_write_str(msb, mess, strlen(mess));
#endif
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
