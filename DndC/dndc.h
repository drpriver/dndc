#ifndef DNDC_H
#define DNDC_H
#include "long_string_type.h"

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
// On Windows and if loaded from a dll, you should use dndc_free_string.
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
dndc_make_html(StringView base_directory, LongString source_text, LongString*_Nonnull output, ErrorFunc*_Nullable error_func, void*_Nullable error_user_data);

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
// On Windows and if loaded from a dll, you should use dndc_free_string.
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
//    A function for reporting errors. See `ErrorFunc` above. If NULL, errors
//    will not be printed. Use `dndc_stderr_error_func` for a function that just
//    prints to stderr.
//
// error_user_data:
//    A pointer that will be passed to the error_func. For
//    `dndc_stderr_error_func`, this should be NULL. For a function you've
//    defined, pass an appropriate pointer!
//
// Returns
// -------
// Returns 0 on success, a non-zero error code otherwise.
// If non-zero, output will not be written to.
//
extern
int
dndc_format(LongString source_text, LongString* _Nonnull output, ErrorFunc* _Nullable error_func, void*_Nullable error_user_data);

//
// On windows, if you load a dll, it will have its own crt and thus its own heap.
// This function is provided so you can free the returned string with the right heap.
// On Linux or MacOS this is unnecessary as dynamic linking works differently.
//
extern
void
dndc_free_string(LongString);

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

enum DndCSyntax {
    // DNDC_SYNTAX_NONE,
    DNDC_SYNTAX_DOUBLE_COLON = 1,
    DNDC_SYNTAX_HEADER = 2,
    DNDC_SYNTAX_NODE_TYPE = 3,
    DNDC_SYNTAX_ATTRIBUTE = 4,
    DNDC_SYNTAX_ATTRIBUTE_ARGUMENT = 5,
    DNDC_SYNTAX_CLASS = 6,
    DNDC_SYNTAX_RAW_STRING = 7,
    // DNDC_SYNTAX_BULLET,
    // DNDC_SYNTAX_COMMENT,
};
enum{DNDC_SYNTAX_MAX=8};

//
// A function type for marking syntactic regions, for use with
// dndc_analyze_syntax.
//
// Arguments:
// ----------
// user_data:
//    A pointer to user-defined data. The pointer will be the same one provided
//    to dndc_analyze_syntax.
//
// type:
//    The type of the syntactic region. See DndCSyntaxType.
//
// line:
//    Which line of the file the error originated from. This is 0-based.
//    Newlines increment the line count.
//
// col:
//    The column the error occurred in, on the line specified by line.
//    This is 0-based and is a byte-offset from the beginning of the line.
//
// begin:
//    The beginning of the syntactic region. This is a pointer derived from
//    the source string.
//
// length:
//    The length of the syntactic region, in bytes.
//
typedef void SyntaxFunc(void* _Nullable user_data, int type, int line, int col, const char* _Nonnull begin, size_t length);
// Ditto, but for utf-16 code units in native endian-ness
typedef void SyntaxFuncUtf16(void* _Nullable user_data, int type, int line, int col, const uint16_t*_Nonnull begin, size_t length);

//
// Analyzes a string, identifiying the syntax of parts of the string.
//
// When the function recognizes an entire syntactic region, it will invoke
// the syntax func on that region. You can do whatever you want with the syntax.
// This syntax func will never be invoked with DNDC_SYNTAX_NONE. The function is
// not invoked on every single piece of the string - "regular" string nodes
// and such will not be called on (as implicitly everything is a string node unless
// otherwise).
//
// This function does not execute any python blocks and does not read any
// files.
//
// Note: this function is looser with parsing than the other dndc funcs. In the
// interest of providing syntax highlighting to an entire document,
// unrecognized node types will be treated as regular nodes.
//
// Arguments
// ---------
// source_text:
//    The actual source .dnd string. This string does *not* need to be
//    nul-terminated (which means substrings can be analyzed).
//    No references to this are retained.
//
// syntax_func:
//    A function for marking syntactic regions. See SyntaxFunc. Whenever
//    a syntactic region is identified, this function will be invoked on it.
//
// syntax_data:
//    A pointer that will be passed to the syntax_func. Pass an appropriate
//    pointer as defined by your syntax func.
//
// Returns
// -------
// Returns 0 on success, a non-zero error code otherwise.
//
extern
int
dndc_analyze_syntax(StringView source_text, SyntaxFunc*_Nonnull syntax_func, void*_Nullable syntax_data);
//
// ditto, but for utf-16 code units of native endianness
//
extern
int
dndc_analyze_syntax_utf16(StringViewUtf16 source_text, SyntaxFuncUtf16*_Nonnull syntax_func, void*_Nullable syntax_data);
#ifdef __cplusplus
}
#endif

#endif
