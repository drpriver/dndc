#ifndef DNDC_H
#define DNDC_H
#include "long_string.h"

#ifdef __cplusplus
extern "C" {
#endif

//
// This documents the external API.
// For the internal API, see dndc_funcs.h.
//

//
// The type of the error message.
//
enum DndCErrorMessageType {
    // An error that it is not possible to recover from.
    DNDC_ERROR_MESSAGE = 0,
    // A warning that valid output can still be produced for.
    DNDC_WARNING_MESSAGE = 1,
    // The error originated from the system (not from the source text).
    // filename will be "", line, col, etc will be 0, etc.
    DNDC_SYSTEM_MESSAGE = 2,
    // The message is just a report of some statistic. It does not originate
    // from the source text.
    // filename will be "", line, col, etc will be 0, etc.
    DNDC_STATISTIC_MESSAGE = 3,
};

//
// A function type for reporting errors. For use with one of the dndc entry
// point functions, as declared in this file.
//
// Arguments:
// ----------
// error_user_data:
//    A pointer to user-defined data. The pointer will be the same one
//    provided to one of the dndc entry point functions.
//
// type:
//    The type of the message. See DndCErrorMessageType.
//
// filename:
//    Which file the error occurred in. This pointer is not nul-terminated.
//
// filename_len:
//    The length of the character array pointed to by file.
//
// line:
//    Which line of the file the error originated from. This is 0-based.
//    Newlines increment the line count.
//
// col:
//    The column the error occurred in, on the line specified by line.
//    This is 0-based and is a byte-offset from the beginning of the line.
//
// message:
//    The error message. This string is nul-terminated, but a length is
//    provided for convenience.
//
// message_len:
//    The length of the error message (excluding the terminating nul character)
//
typedef void ErrorFunc(void* _Nullable error_user_data, int type, const char* _Nonnull filename, int filename_len, int line, int col, const char* _Nonnull message, int message_len);

//
// An error reporting function that prints to stderr. For use with the dndc
//
extern ErrorFunc dndc_stderr_error_func;

//
// You *must* call dndc_init_python before calling this function.
//
// Turns the given .dnd string into html, storing the result in output.
// The output is allocated by malloc. You take ownership of the result.
//
// Arguments
// ---------
// base_directory:
//    The base directory from which imports and other file references are
//    relative to. This can be the empty string, in which case it will not be
//    prepended to anything. Absolute paths in the source will never be
//    adjusted. This allows you to use relative filepaths without having to
//    chdir, as chdir sucks. This string does not need to be nul-terminated.
//    No references to this are retained afterwards.
//
// source_text:
//    The actual source .dnd string. This string does need to be
//    nul-terminated. No references to this are retained afterwards.
//
// output:
//    A pointer to a string to store the html into. The output is allocated by
//    malloc. You take ownership of the result. Must be non-null.
//    If there is an error, the output is not written to.
//
// error_func:
//   A function for reporting errors. See `ErrorFunc` above. If NULL, errors
//   will not be printed. Use `dndc_stderr_error_func` for a function that just
//   prints to stderr.
//
// error_user_data:
//   A pointer that will be passed to the error_func. For
//   `dndc_stderr_error_func`, this should be NULL. For a function you've
//   defined, pass an appropriate pointer!
//
// Returns
// -------
// Returns 0 on success, a non-zero error code otherwise.
// If non-zero, output will not be written to.
//
extern
int
dndc_make_html(StringView base_directory, LongString source_text, Nonnull(LongString*)output, Nullable(ErrorFunc*) error_func, Nullable(void*) error_user_data);

//
// You do *not* need to call dndc_init_python before calling this function.
//
// Turns the given .dnd string into another .dnd string, but formatted such
// that lines do not exceed 80 characters if it is possible to semantically do
// so, lines are right-stripped, redundant blank lines are merged, etc.
// The resulting string is stored in output.
//
// This function does not execute any python blocks and does not read any
// files.
//
// The output is allocated by malloc. You take ownership of the result.
//
// Arguments
// ---------
// source_text:
//    The actual source .dnd string. This string does need to be nul-terminated.
//    No references to this are retained afterwards.
//
// output:
//    A pointer to a string to store the formatted string into. The output is
//    allocated by malloc. You take ownership of the result. Must be non-null.
//    If there is an error, the output is not written to.
//
// error_func:
//   A function for reporting errors. See `ErrorFunc` above. If NULL, errors
//   will not be printed. Use `dndc_stderr_error_func` for a function that just
//   prints to stderr.
//
// error_user_data:
//   A pointer that will be passed to the error_func. For
//   `dndc_stderr_error_func`, this should be NULL. For a function you've
//   defined, pass an appropriate pointer!
//
// Returns
// -------
// Returns 0 on success, a non-zero error code otherwise.
// If non-zero, output will not be written to.
//
extern
int
dndc_format(LongString source_text, Nonnull(LongString*)output, Nullable(ErrorFunc*)error_func, Nullable(void*) error_user_data);

//
// Initializes the python interpreter and imports the dndc types.
//
// This should be called before dndc_make_html. If you already have a python
// interpreter, call dndc_init_python_types instead.
//
// Returns
// -------
// Returns 0 on success, a non-zero error code otherwise.
//
extern
int
dndc_init_python(void);

//
// Initializes and imports the dndc types.
//
// A Python interpreter should have been initialized by you beforehand. This
// should be called before dndc_make_html if you already have a python
// interpreter. If you do not already have a python interpreter, call
// dndc_init_python instead.
//
// Returns
// -------
// Returns 0 on success, a non-zero error code otherwise.
//
extern
int
dndc_init_python_types(void);

#ifdef __cplusplus
}
#endif

#endif
