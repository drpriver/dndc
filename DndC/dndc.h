#ifndef DNDC_H
#define DNDC_H
#include "long_string.h"

//
// This documents the external API.
// For the internal API, see dndc_funcs.h.
//

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
//    prepended to anything. Absolute paths in the source will never be adjusted.
//    This allows you to use relative filepaths without having to chdir, as chdir
//    sucks. This string does not need to be nul-terminated.
//    No references to this are retained afterwards.
//
// source_text:
//    The actual source .dnd string. This string does need to be nul-terminated.
//    No references to this are retained afterwards.
//
// output:
//    A pointer to a string to store the html into. The output is allocated by
//    malloc. You take ownership of the result. Must be non-null.
//    If there is an error, the output is not written to.
//
// Returns
// -------
// Returns 0 on success, a non-zero error code otherwise.
// If non-zero, output will not be written to.
//
extern
int
dndc_make_html(StringView base_directory, LongString source_text, Nonnull(LongString*)output);

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
// Returns
// -------
// Returns 0 on success, a non-zero error code otherwise.
// If non-zero, output will not be written to.
//
extern
int
dndc_format(LongString source_text, Nonnull(LongString*)output);

//
// Initializes the python interpreter and imports the dndc types.
// This should be called before dndc_make_html.
// If you already have a python interpreter, call dndc_init_python_types
// instead.
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
// A Python interpreter should have been initialized by you beforehand.
// This should be called before dndc_make_html if you already have a python
// interpreter.
// If you do not already have a python interpreter, call dndc_init_python
// instead.
//
// Returns
// -------
// Returns 0 on success, a non-zero error code otherwise.
//
extern
int
dndc_init_python_types(void);

#endif
