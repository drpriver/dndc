#ifndef DNDC_H
#define DNDC_H
//
// This documents the external API.
// For the internal API, see dndc_funcs.h.
//
#define DNDC_STRINGIFY_IMPL(x) #x
#define DNDC_STRINGIFY(x) DNDC_STRINGIFY_IMPL(x)
#define DNDC_MAJOR 0
#define DNDC_MINOR 6
#define DNDC_MICRO 2
#define DNDC_VERSION DNDC_STRINGIFY(DNDC_MAJOR) "." DNDC_STRINGIFY(DNDC_MINOR) "." DNDC_STRINGIFY(DNDC_MICRO)

// for size_t
#include <stddef.h>

#ifdef __clang__
// Unless marked, pointers are nonnull.
#pragma clang assume_nonnull begin
// This pointer may be null
#define DNDC_NULLABLE(x) x _Nullable
// This pointer's nullability depends on something else and can be assumed null or
// nonnull depending on that state. For example, a buffer with length 0 can
// have a null pointer.
#define DNDC_NULLDEP(x) x _Null_unspecified
#else
#define DNDC_NULLABLE(x) x
#define DNDC_NULLDEP(x) x
#endif

// This can be defined before you include for your own usage.
#ifndef DNDC_API
#ifdef _WIN32
#define DNDC_API __declspec(dllimport)
#else
#define DNDC_API extern
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

//
// `DndcLongString`s and `DndcStringView`s are very similar. A `DndcLongString`
// is a `DndcStringView` with a guaranteed nul-terminator. It is unspecified if
// a `DndcStringView` has a nul-terminator (allowing it to point to
// substrings).
//
// A zero length `DndcStringView` or `DndcLongString` with a length > 0 must
// point to a valid pointer. A zero length `DndcStringView` or `DndcLongString`
// may either have a null pointer for its `text` field or be a pointer to the
// empty string (""). Thus, the length field should always be checked to see if
// it is nonzero.
//
struct DndcLongString {
    size_t length; // excludes the terminating NUL
    DNDC_NULLDEP(const char*) text; // utf-8 encoded text
};

struct DndcStringView {
    size_t length;
    // utf-8 encoded text, might not be nul-terminated
    DNDC_NULLDEP(const char*) text;
};

// Avoiding including <stdint.h> in public header.
_Static_assert(sizeof(unsigned short) == 2, "unsigned short is not uint16_t");
struct DndcStringViewUtf16 {
    size_t length; // in code units
    // utf-16 encoded code points, native endianness
    DNDC_NULLDEP(const unsigned short*) text;
};

//
// The type of the error message.
//
enum DndcErrorMessageType {
    // An error that is not possible to recover from.
    DNDC_ERROR_MESSAGE = 0,
    // A warning that valid output can still be produced for.
    DNDC_WARNING_MESSAGE = 1,
    // The error did not originate from any specific node. Rather,
    // it ocurred for another reason (for example, python may have
    // failed to initialize).
    // filename will be "", line, col, etc will be 0, etc.
    DNDC_NODELESS_MESSAGE = 2,
    // The message is just a report of some statistic. It does not originate
    // from the source text.
    // filename will be "", line, col, etc will be 0, etc.
    DNDC_STATISTIC_MESSAGE = 3,
    // Message is a debugging message, as requested by the user.
    // May or may not have a valid filename, line, col.
    DNDC_DEBUG_MESSAGE = 4,
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
//    The type of the message. See DndcErrorMessageType.
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
typedef void DndcErrorFunc(DNDC_NULLABLE(void*) error_user_data, int type,
        const char* filename, int filename_len, int line,
        int col, const char* message, int message_len);

//
// An error reporting function that prints to stderr. For use with the dndc
//
DNDC_API void dndc_stderr_error_func(DNDC_NULLABLE(void*) error_user_data,
        int type, const char* filename, int filename_len,
        int line, int col, const char* message, int message_len);

//
// A function type for reporting dependencies. For use with
// `dndc_compile_dnd_file`.
//
// Arguments:
// ----------
// dependency_user_data:
//    A pointer to user-defined data. The pointer will be the same one provided
//    to dndc_compile_dnd_file.
//
// dependency_paths_count:
//    The length of the array dependency_paths points to.
//
// dependency_paths:
//    A pointer to an array of string views of the paths to the files that the
//    file depends on. Note these are string views and so not guaranteed to be
//    nul-terminated. Files that were loaded in the usual way will have the
//    base dir prepended, but python blocks can introduce arbitrary strings as
//    dependencies, which may or may not be absolute paths, or valid paths at
//    all.
//
// Returns:
// --------
// 0 on success and non-zero on failure. The value you return will be returned
// from dndc_compile_dnd_file if non-zero.
//
typedef int DndcDependencyFunc(DNDC_NULLABLE(void*) dependency_user_data,
        size_t dependency_paths_count,
        struct DndcStringView* dependency_paths);

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
//    The actual source .dnd string. This string does need to be
//    nul-terminated.
//    No references to this are retained afterwards.
//
// output:
//    A pointer to a string to store the formatted string into. The output is
//    allocated by malloc. You take ownership of the result. Must be non-null.
//    If there is an error, the output is not written to.
//
// error_func:
//    A function for reporting errors. See `DndcErrorFunc` above. If NULL,
//    errors will not be printed. Use `dndc_stderr_error_func` for a function
//    that just prints to stderr.
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
DNDC_API
int
dndc_format(struct DndcLongString source_text,
        struct DndcLongString* output,
        DNDC_NULLABLE(DndcErrorFunc*) error_func,
        DNDC_NULLABLE(void*) error_user_data);

//
// On windows, if you load a dll, it will have its own crt and thus its own
// heap.  This function is provided so you can free the returned string with
// the right heap.  On Linux or MacOS this is unnecessary as dynamic linking
// works differently.
//
DNDC_API
void
dndc_free_string(struct DndcLongString);

//
// Initializes the python interpreter and imports the dndc types. If you do
// not call this before calling `dndc_compile_dnd_file`, that function will
// initialize a python interpreter and import the dndc types itself.
// Initializing python is slow, so being able to initialize it separately from
// compiling a dnd file gives you more control.
//
// If called at all, it should be called before `dndc_compile_dnd_file`.
// If you already have an initialized python interpreter, call
// `dndc_init_python_types` instead.
//
// Returns
// -------
// Returns 0 on success, a non-zero error code otherwise.
//
DNDC_API
int
dndc_init_python(void);

//
// Initializes and imports the dndc types for when the python interpreter has
// already been started.
//
// A Python interpreter should have been initialized by you beforehand. This
// should be called before `dndc_compile_dnd_file` if you already have a python
// interpreter. If you do not already have a python interpreter, call
// `dndc_init_python` instead.
//
// Returns
// -------
// Returns 0 on success, a non-zero error code otherwise.
//
DNDC_API
int
dndc_init_python_types(void);

enum DndcSyntax {
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
    // DNDC_SYNTAX_KEY,
    // DNDC_SYNTAX_VALUE,
};
enum{DNDC_SYNTAX_MAX=8};

//
// A function type for marking syntactic regions, for use with
// `dndc_analyze_syntax`.
//
// Arguments:
// ----------
// user_data:
//    A pointer to user-defined data. The pointer will be the same one provided
//    to `dndc_analyze_syntax`.
//
// type:
//    The type of the syntactic region. See `DndcSyntaxType`.
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
typedef void DndcSyntaxFunc(DNDC_NULLABLE(void*) user_data, int type, int line,
        int col, const char* begin, size_t length);

//
// A function type for marking syntactic regions, for use with
// `dndc_analyze_syntax_utf16`.
// This is very similar to `DndcSyntaxFunc`, but for utf16 code units.
//
// Arguments:
// ----------
// user_data:
//    A pointer to user-defined data. The pointer will be the same one provided
//    to dndc_analyze_syntax.
//
// type:
//    The type of the syntactic region. See `DndcSyntaxType`.
//
// line:
//    Which line of the file the error originated from. This is 0-based.
//    Newlines increment the line count.
//
// col:
//    The column the error occurred in, on the line specified by line.
//    This is 0-based and is a code unit offset from the beginning of the line.
//
// begin:
//    The beginning of the syntactic region. This is a pointer derived from
//    the source string.
//
// length:
//    The length of the syntactic region, in code units.
//
typedef void DndcSyntaxFuncUtf16(DNDC_NULLABLE(void*) user_data, int type,
        int line, int col, const unsigned short* begin,
        size_t length);

//
// Analyzes a string, identifiying the syntax of parts of the string.
//
// When the function recognizes an entire syntactic region, it will invoke the
// syntax func on that region. You can do whatever you want with the syntax.
// This syntax func will never be invoked with DNDC_SYNTAX_NONE. The function
// is not invoked on every single piece of the string - "regular" string nodes
// and such will not be called on (as implicitly everything is a string node
// unless otherwise).
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
//    A function for marking syntactic regions. See `DndcSyntaxFunc`. Whenever
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
DNDC_API
int
dndc_analyze_syntax(struct DndcStringView source_text,
        DndcSyntaxFunc* syntax_func,
        DNDC_NULLABLE(void*) syntax_data);

//
// Ditto, but for utf-16 code units of native endianness.
//
DNDC_API
int
dndc_analyze_syntax_utf16(struct DndcStringViewUtf16 source_text,
        DndcSyntaxFuncUtf16* syntax_func,
        DNDC_NULLABLE(void*) syntax_data);

//
// A cache for storing files across repeated invocations.
// Opaque structure (PIMPL).
//
struct DndcFileCache;
//
// Allocate a new file cache.
//
DNDC_API
struct DndcFileCache*
dndc_create_filecache(void);
//
// Cleanup all allocated resources and deallocate the filecache.
//
DNDC_API
// int // should we allow for possibility of error?
void
dndc_filecache_destroy(struct DndcFileCache* cache);
//
// Remove a given path from the filecache.
//
// Returns:
// --------
// Returns 0 if it successfully removed the path and 1 if the path was not in
// the cache.
//
DNDC_API
int
dndc_filecache_remove(struct DndcFileCache* cache, struct DndcStringView path);
//
// Remove all paths from the filecache.
//
DNDC_API
// int // should we allow for possibility of error?
void
dndc_filecache_clear(struct DndcFileCache* cache);

//
// Check if a path is in the filecache.
//
// Returns:
// --------
// Returns 1 if the path is in the cache, 0 otherwise.
//
DNDC_API
int
dndc_filecache_has_path(struct DndcFileCache*, struct DndcStringView path);

//
// Low-level function to compile a dnd source file. The behavior of this
// function is complex and is greatly controlled by the flags argument.
// See `enum DndcFlags` for details on the exact meaning of the flags. When
// arguments change meaning based on the flags that will be described below.
//
// In its default mode, this function will load the given source file, parse
// it, resolve imports, execute python blocks, spawn a thread to base64
// referenced images, load referenced files such as js files and css files,
// and render the result into an html file at the given location.
//
// Args:
// ----
//
// flags:
//    A bitwise-or combination of `enum DndcFlags`.
//
// base_directory:
//    May be a zero-length string view.  For relative filepaths referenced in
//    the document, what those paths are relative to. Defaults to the current
//    directory for a zero length string view.
//
//    Specifically, this string plus a directory separator will be prepended to
//    all paths for the purposes of opening those paths.
//
// source:
//    The string to be parsed and compiled.
//
//    Alternatively, if the flag DNDC_SOURCE_IS_PATH_NOT_DATA is set, this
//    argument is treated as a filepath to the source data. If this string's
//    length is zero, input will be read from stdin.
//
// output:
//    A pointer to a string structure to write the data to. The text will be
//    allocated via malloc. You can call `dndc_free_string` on the text if you
//    are on a platform where each dynamic library has its own heap (aka
//    Windows).
//
//    Alternatively, if the flag DNDC_OUTPUT_IS_FILE_PATH_NOT_OUT_PARAM is set,
//    the argument is a pointer to the path to write the result to. If null,
//    will write to stdout instead.
//
// base64filecache:
//    A pointer to a filecache (created with dndc_create_filecache) that is
//    used to cache files across invocations of this function. This may be
//    null, in which case no caching is done.
//
//    This cache is used to cache the results of loading and base64-ing
//    binary files.
//
// textfilecache:
//    A pointer to a filecache (created with dndc_create_filecache) that is
//    used to cache files across invocations of this function. This may be
//    null, in which case no caching is done.
//
//    This cache is used to cache the results of loading text files.
//
// error_func:
//    A function for reporting errors. See `DndcErrorFunc` above. If NULL,
//    errors will not be printed. Use `dndc_stderr_error_func` for a function
//    that just prints to stderr.
//
// error_user_data:
//    A pointer that will be passed to the error_func. For
//    `dndc_stderr_error_func`, this should be NULL. For a function you've
//    defined, pass an appropriate pointer!
//
// dependency_func:
//    A function for reporting the dependencies of the generated file. See
//    `DndcDependencyFunc` above.
//
// dependency_user_data:
//   A pointer that will be passed to the dependency_func.
//
// Returns
// -------
// Returns 0 on success, a non-zero error code otherwise.
//
DNDC_API
int
dndc_compile_dnd_file(
    unsigned long long flags,
    struct DndcStringView base_directory,
    struct DndcLongString source,
    DNDC_NULLABLE(struct DndcLongString*) output,
    DNDC_NULLABLE(struct DndcFileCache*) base64cache,
    DNDC_NULLABLE(struct DndcFileCache*) textcache,
    DNDC_NULLABLE(DndcErrorFunc*) error_func,
    DNDC_NULLABLE(void*) error_user_data,
    DNDC_NULLABLE(DndcDependencyFunc*) dependency_func,
    DNDC_NULLABLE(void*) dependency_user_data
);

//
// The flags that can be passed as the `flags` argument to
// `dndc_compile_dnd_file`. Combine them together via bitwise-or.
//
enum DndcFlags {
DNDC_FLAGS_NONE        = 0x0,

// Don't error on bad links.
DNDC_ALLOW_BAD_LINKS   = 0x0001,

// Don't report any non-fatal errors via the `error_func`.
DNDC_SUPPRESS_WARNINGS = 0x0002,

// Log stats during execution of timings and counts.
DNDC_PRINT_STATS       = 0x0004,

// Log orphaned nodes.
DNDC_REPORT_ORPHANS    = 0x0008,

// Don't execute python blocks.
DNDC_NO_PYTHON         = 0x0010,

// The python interpreter and dndc_python types have already been
// initialized.
DNDC_PYTHON_IS_INIT    = 0x0020,

// Print out the document tree
DNDC_PRINT_TREE        = 0x0040,

// Print out all links and what they resolve to
DNDC_PRINT_LINKS       = 0x0080,

// Don't spawn any worker threads. No parallelism.
DNDC_NO_THREADS        = 0x0100,

// Don't write out the final result.
DNDC_DONT_WRITE        = 0x0200,

// Don't cleanup allocations or anything.
// Appropriate for batch use.
DNDC_NO_CLEANUP        = 0x0400,

// The `source` argument is actually a filepath to the file containing the
// data.
DNDC_SOURCE_IS_PATH_NOT_DATA = 0x0800,

// Don't report errors via the `error_func`.
DNDC_DONT_PRINT_ERRORS = 0x01000,

// Don't bother isolating python from site
// Greatly slows startup, but allows importing user-installed
// libraries.
DNDC_PYTHON_UNISOLATED = 0x02000,

// The output path is actually a pointer to a filepath to write the data
// to.
DNDC_OUTPUT_IS_FILE_PATH_NOT_OUT_PARAM = 0x04000,

// Instead of rendering to html, render to .dnd with trailing
// spaces removed, text aligned to 80 columns (if semantically equivelant)
// etc.
DNDC_REFORMAT_ONLY      = 0x08000,

// Instead of base64-ing the image, use a link.
DNDC_DONT_INLINE_IMAGES =  0x10000,

// This flag is currently unused.
DNDC_UNUSED_RESERVED_FLAG_SLOT = 0x20000,

// For imgs, don't base64 them and don't use regular links.
// Instead, use a dnd:absolute/path/to/img url instead.
// Applications can then implement custom url handlers for this url scheme.
DNDC_USE_DND_URL_SCHEME  = 0x40000,

// Input is untrusted and thus should not be allowed to read files, execute
// python blocks or embed javascript in the output. As raw nodes are
// inserted literally, raw nodes are ignored.
DNDC_INPUT_IS_UNTRUSTED  = 0x80000,

// Strip trailing and leading whitespace from all output lines.
DNDC_STRIP_WHITESPACE    = 0x100000,

// Don't read any files not already in the file cache (with the exception
// of the initial input file).
// Python blocks can bypass this by calling `open` directly.
DNDC_DONT_READ           = 0x200000,
};

#ifdef __cplusplus
}
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
