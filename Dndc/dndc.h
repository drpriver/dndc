#ifndef DNDC_H
#define DNDC_H
//
// dndc.h
// ------
// This documents the external API.
// For the internal API, see dndc_funcs.h.
//

//
// DNDC_VERSION
// ------------
#define DNDC_VERSION DNDC_STRINGIFY(DNDC_MAJOR) "." DNDC_STRINGIFY(DNDC_MINOR) "." DNDC_STRINGIFY(DNDC_MICRO)

#define DNDC_MAJOR 0
#define DNDC_MINOR 11
#define DNDC_MICRO 0
#define DNDC_STRINGIFY_IMPL(x) #x
#define DNDC_STRINGIFY(x) DNDC_STRINGIFY_IMPL(x)
//
// DNDC_INT_VERSION
// ----------------
// The version as a single number. Comparable.
// 11 bits for major
// 10 bits for minor
// 10 bits for micro
//
#define DNDC_INT_VERSION(major, minor, micro) ((major) << 20 | (minor) << 10 | (micro))
enum {DNDC_NUMERIC_VERSION = DNDC_INT_VERSION(DNDC_MAJOR, DNDC_MINOR, DNDC_MICRO)};

#include <stddef.h> // for size_t

#ifdef __clang__
#pragma clang assume_nonnull begin // Unless marked, pointers are nonnull.

//
// DNDC_NULLABLE
// -------------
// This pointer may be null
#define DNDC_NULLABLE(x) x _Nullable

//
// DNDC_NULLDEP
// ------------
// This pointer's nullability depends on something else and can be assumed null
// or nonnull depending on that state. For example, a buffer with length 0 can
// have a null pointer.
#define DNDC_NULLDEP(x) x _Null_unspecified

#else
#define DNDC_NULLABLE(x) x
#define DNDC_NULLDEP(x) x
#endif

// DNDC_API
// --------
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
#ifndef _Static_assert
#define _Static_assert static_assert
#endif
#endif

//
// String Types
// ------------
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

  //
  // DNDC_LONGSTRING_DEFINED
  // -----------------------
  // For ease of integration, you can define this macro and the corresponding
  // string types yourself. The stringtypes should be a typedef of an existing
  // length + pointer structure. The definition should match this one exactly.
  #ifndef DNDC_LONGSTRING_DEFINED

  //
  // DndcLongString
  // --------------
  typedef struct DndcLongString {
      size_t length; // excludes the terminating NUL
      DNDC_NULLDEP(const char*) text; // utf-8 encoded text
  } DndcLongString;

  //
  // DndcStringView
  // --------------
  typedef struct DndcStringView {
      size_t length;
      DNDC_NULLDEP(const char*) text; // utf-8, might not be nul-terminated
  } DndcStringView;

  //
  // DndcStringViewUtf16
  // -------------------
  // Avoiding including <stdint.h> in public header.
  _Static_assert(sizeof(unsigned short) == 2, "unsigned short is not uint16_t");
  typedef struct DndcStringViewUtf16 {
      size_t length; // in code units
      DNDC_NULLDEP(const unsigned short*) text; // utf-16, native endianness
  } DndcStringViewUtf16;

  #define DNDC_LONGSTRING_DEFINED 1

#endif
//
// Syntax Analysis
// ---------------

  //
  // DndcSyntax
  // ----------
  // The kind of syntactic region, for `DndcSyntaxFunc`.
  enum DndcSyntax {
      DNDC_SYNTAX_NONE = 0,
      DNDC_SYNTAX_DOUBLE_COLON = 1,
      DNDC_SYNTAX_HEADER = 2,
      DNDC_SYNTAX_NODE_TYPE = 3,
      DNDC_SYNTAX_ATTRIBUTE = 4,
      DNDC_SYNTAX_DIRECTIVE = 5,
      DNDC_SYNTAX_ATTRIBUTE_ARGUMENT = 6,
      DNDC_SYNTAX_CLASS = 7,
      DNDC_SYNTAX_RAW_STRING = 8,
      // Javascript syntax
      // -----------------
      DNDC_SYNTAX_JS_COMMENT = 9,
      DNDC_SYNTAX_JS_STRING = 10,
      DNDC_SYNTAX_JS_REGEX = 11,
      DNDC_SYNTAX_JS_NUMBER = 12,
      DNDC_SYNTAX_JS_KEYWORD = 13,
      DNDC_SYNTAX_JS_KEYWORD_VALUE = 14,
      DNDC_SYNTAX_JS_VAR = 15,
      DNDC_SYNTAX_JS_IDENTIFIER = 16,
      DNDC_SYNTAX_JS_BUILTIN = 17,
      DNDC_SYNTAX_JS_NODETYPE = 18,
      DNDC_SYNTAX_JS_BRACE = 19,
  };
  enum{DNDC_SYNTAX_MAX=20};
  _Static_assert((int)DNDC_SYNTAX_MAX > (int)DNDC_SYNTAX_JS_BRACE, "");

  //
  // DndcSyntaxFunc
  // --------------
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
  //    The type of the syntactic region. See `DndcSyntax`.
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
  // DndcSyntaxFuncUtf16
  // -------------------
  //
  // A function type for marking syntactic regions, for use with
  // `dndc_analyze_syntax_utf16`. This is very similar to `DndcSyntaxFunc`, but
  // for utf16 code units.
  //
  // Arguments:
  // ----------
  // user_data:
  //    A pointer to user-defined data. The pointer will be the same one provided
  //    to `dndc_analyze_syntax`.
  //
  // type:
  //    The type of the syntactic region. See `DndcSyntax`.
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
  // dndc_analyze_syntax
  // -------------------
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
  // This function does not execute any javascript blocks and does not read any
  // files.
  //
  // Note: this function is looser with parsing than the other dndc funcs. In the
  // interest of providing syntax highlighting to an entire document as you edit
  // it, unrecognized node types will be treated as regular nodes.
  //
  // Arguments:
  // ----------
  // source_text:
  //    The actual source .dnd string. This string does *not* need to be
  //    nul-terminated (which means substrings can be analyzed).  No references
  //    to this are retained.
  //
  // syntax_func:
  //    A function for marking syntactic regions. See `DndcSyntaxFunc`. Whenever
  //    a syntactic region is identified, this function will be invoked on it.
  //
  // syntax_data:
  //    A pointer that will be passed to the syntax_func. Pass an appropriate
  //    pointer as defined by your syntax func.
  //
  // Returns:
  // --------
  // Returns 0 on success, a non-zero error code otherwise.
  //
  DNDC_API
  int
  dndc_analyze_syntax(DndcStringView source_text,
          DndcSyntaxFunc* syntax_func,
          DNDC_NULLABLE(void*) syntax_data);

  //
  // dndc_analyze_syntax_utf16
  // -------------------------
  //
  // Ditto, but for utf-16 code units of native endianness.
  //
  DNDC_API
  int
  dndc_analyze_syntax_utf16(DndcStringViewUtf16 source_text,
          DndcSyntaxFuncUtf16* syntax_func,
          DNDC_NULLABLE(void*) syntax_data);


//
// Error Reporting
// ---------------

  //
  // DndcErrorMessageType
  // --------------------
  // The type of the error message.
  //
  enum DndcErrorMessageType {
      //
      // DNDC_ERROR_MESSAGE
      // ------------------
      // An error that is not possible to recover from.
      DNDC_ERROR_MESSAGE = 0,
      //
      // DNDC_WARNING_MESSAGE
      // ------------------
      // A warning that valid output can still be produced for.
      DNDC_WARNING_MESSAGE = 1,
      //
      // DNDC_NODELESS_MESSAGE
      // ---------------------
      // The error did not originate from any specific node. Rather, it ocurred
      // for another reason.
      //
      // Filename will be "", line, col, etc will be 0, etc.
      DNDC_NODELESS_MESSAGE = 2,
      //
      // DNDC_STATISTIC_MESSAGE
      // ----------------------
      // The message is just a report of some statistic. It does not originate
      // from the source text.
      //
      // Filename will be "", line, col, etc will be 0, etc.
      DNDC_STATISTIC_MESSAGE = 3,
      //
      // DNDC_DEBUG_MESSAGE
      // ------------------
      // Message is a debugging message, as requested by the user.
      // May or may not have a valid filename, line, col.
      DNDC_DEBUG_MESSAGE = 4,
  };

  //
  // DndcErrorFunc
  // -------------
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
  //    The type of the message. See `DndcErrorMessageType`.
  //
  // filename:
  //    Which file the error occurred in. This pointer is NOT nul-terminated.
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
  // dndc_stderr_error_func
  // ----------------------
  //
  // An error reporting function that prints to stderr. For use with the
  // `dndc_compile_dnd_file`.
  //
  DNDC_API void dndc_stderr_error_func(DNDC_NULLABLE(void*) error_user_data,
          int type, const char* filename, int filename_len,
          int line, int col, const char* message, int message_len);

//
// DndcDependencyFunc
// ------------------
//
// A function type for reporting dependencies. For use with
// `dndc_compile_dnd_file`.
//
// Arguments:
// ----------
// dependency_user_data:
//    A pointer to user-defined data. The pointer will be the same one provided
//    to `dndc_compile_dnd_file`.
//
// dependency_paths_count:
//    The length of the array dependency_paths points to.
//
// dependency_paths:
//    A pointer to an array of string views of the paths to the files that the
//    file depends on. Note these are string views and so not guaranteed to be
//    nul-terminated. Files that were loaded in the usual way will have the
//    base dir prepended, but javascript blocks can introduce arbitrary strings
//    as dependencies, which may or may not be absolute paths, or valid paths
//    at all.
//
// Returns:
// --------
// 0 on success and non-zero on failure. The value you return will be returned
// from dndc_compile_dnd_file if non-zero.
//
typedef int DndcDependencyFunc(DNDC_NULLABLE(void*) dependency_user_data,
        size_t dependency_paths_count,
        DndcStringView* dependency_paths);

//
// DndcFileCache
// -------------
// A cache for storing files across repeated invocations.
// Opaque structure (PIMPL).
//
typedef struct DndcFileCache DndcFileCache;

  //
  // dndc_create_filecache
  // ---------------------
  //
  // Allocate a new file cache.
  //
  DNDC_API
  DndcFileCache*
  dndc_create_filecache(void);

  //
  // dndc_filecache_destroy
  // ----------------------
  //
  // Cleanup all allocated resources and deallocate the filecache.
  //
  DNDC_API
  void
  dndc_filecache_destroy(DndcFileCache* cache);

  //
  // dndc_filecache_remove
  // ---------------------
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
  dndc_filecache_remove(DndcFileCache* cache, DndcStringView path);

  //
  // dndc_filecache_clear
  // -------------------
  //
  // Remove all paths from the filecache.
  //
  DNDC_API
  void
  dndc_filecache_clear(DndcFileCache* cache);

  //
  // dndc_filecache_has_path
  // -----------------------
  //
  // Check if a path is in the filecache.
  //
  // Returns:
  // --------
  // Returns 1 if the path is in the cache, 0 otherwise.
  //
  DNDC_API
  int
  dndc_filecache_has_path(DndcFileCache*, DndcStringView path);

  //
  // dndc_filecache_n_paths
  // ----------------------
  //
  // Returns the number of paths cached in the file cache.
  //
  DNDC_API
  size_t
  dndc_filecache_n_paths(DndcFileCache*);

  //
  // dndc_filecache_cached_paths
  // ---------------------------
  //
  // Reads a number of cached paths into a buffer. These paths are unstable -
  // further interaction with the filecache may invalidate them. Call this
  // function multiple times with the cookie to get all of the paths out of the
  // cache.
  //
  // The return value is the number of paths written to buff. This may be fewer
  // than the number of paths in the filecache. Call this function until it
  // returns 0.
  //
  // Args:
  // -----
  //
  // cache:
  //   The cache to read the paths from.
  //
  // buff:
  //   A pointer to an array of `DndcStringView`s to store the results in.
  //
  // bufflen:
  //   The length of the array pointed to by buff.
  //
  // cookie:
  //   A pointer to an opaque value to record the internal position of the
  //   filecache. Initialize this to 0 and pass it to this function as you call
  //   it in a loop.
  //
  // Returns:
  // --------
  // The number of paths written to buff. When all paths have been written or an
  // invalid argument is given, returns 0.
  //
  // Example:
  // --------
  //  enum {EXAMPLE_LENGTH=100};
  //  DndcStringView buff[EXAMPLE_LENGTH];
  //  for(size_t cookie = 0,
  //             n = dndc_filecache_cached_paths(cache, buff, EXAMPLE_LENGTH, &cookie);
  //      n != 0;
  //      n = dndc_filecache_cached_paths(cache, buff, EXAMPLE_LENGTH, &cookie)
  //  ){
  //      for(size_t i = 0; i < n; i++){
  //          DndcStringView path = buff[i];
  //          printf("%.*s\n", (int)path.length, path.text);
  //      }
  //  }
  //
  DNDC_API
  size_t
  dndc_filecache_cached_paths(DndcFileCache*cache, DndcStringView* buff, size_t bufflen, size_t* cookie);

  //
  // dndc_filecache_store_text
  // ---------------------
  //
  // Stores the given text in the file cache under the given path. The path and
  // data are copied out.
  //
  // Args:
  // -----
  // cache:
  //   The cache to store the data in.
  //
  // path:
  //   The path to store the data under. This path is copied.
  //
  // data:
  //   The data to store. This data is copied.
  //
  // overwrite:
  //   If true, if the path is already in the filecache, it is overwritten.
  //   If false, then an error is returned instead.
  //
  // Returns:
  // --------
  // Returns 0 if it successfully stored the path and non-zero if it failed to
  // store the data.
  //
  DNDC_API
  int
  dndc_filecache_store_text(DndcFileCache* cache, DndcStringView path, DndcStringView data, int overwrite);

// DndcWorkerThread
// ----------------
typedef struct DndcWorkerThread DndcWorkerThread;

  //
  // dndc_worker_thread_create
  // -------------------------
  // Creates a new worker thread that can be passed to `dndc_compile_dnd_file`,
  // to avoid creating an excessive number of threads on repeated invocation.
  DNDC_API
  DndcWorkerThread*
  dndc_worker_thread_create(void);

  //
  // dndc_worker_thread_destroy
  // --------------------------
  DNDC_API
  void
  dndc_worker_thread_destroy(DndcWorkerThread*);

//
// DndcFlags
// ---------
// The flags that can be passed as the flags argument to
// `dndc_compile_dnd_file`. Combine them together via bitwise-or.
//
enum DndcFlags {
    DNDC_FLAGS_NONE = 0x0,

    // DNDC_FRAGMENT_ONLY
    // ------------------
    // Instead of a complete document, only produce the html fragment.
    // If scripts and styles are included, they will also be produced.
    DNDC_FRAGMENT_ONLY = 0x1,

    // DNDC_DONT_WRITE
    // ---------------
    // Don't write out the final result.
    DNDC_DONT_WRITE = 0x2,

    // DNDC_DONT_READ
    // --------------
    // Don't read any files not already in the file cache (with the exception
    // of the initial input file). Additionally, prevent access to the
    // filesystem (like checking if a file exists).
    DNDC_DONT_READ = 0x4,

    // DNDC_INPUT_IS_UNTRUSTED
    // -----------------------
    // Input is untrusted and thus should not be allowed to read files, execute
    // javascript blocks or embed javascript in the output. As raw nodes are
    // inserted literally, raw nodes are ignored.
    DNDC_INPUT_IS_UNTRUSTED  = 0x8,

    // DNDC_REFORMAT_ONLY
    // ------------------
    // Instead of rendering to html, render to .dnd with trailing spaces
    // removed, text aligned to 79 columns (if semantically equivelant) etc.
    DNDC_REFORMAT_ONLY = 0x10,

    // DNDC_SUPPRESS_WARNINGS
    // ----------------------
    // Don't report any non-fatal errors via the `error_func`.
    DNDC_SUPPRESS_WARNINGS = 0x20,

    // DNDC_DONT_PRINT_ERRORS
    // ----------------------
    // Don't report errors via the `error_func`.
    DNDC_DONT_PRINT_ERRORS = 0x40,

    // DNDC_PRINT_STATS
    // ----------------
    // Log stats during execution of timings and counts.
    DNDC_PRINT_STATS = 0x80,

    // DNDC_ALLOW_BAD_LINKS
    // --------------------
    // Don't error on bad links.
    DNDC_ALLOW_BAD_LINKS = 0x100,

    // DNDC_NO_COMPILETIME_JS
    // ----------------------
    // Don't execute js blocks.
    DNDC_NO_COMPILETIME_JS = 0x200,

    // DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
    // -----------------------------------------
    // Attributes and directives are in separate namespaces, but that can be
    // confusing.  It's generally bad practice to use an attribute that is the
    // same as a directive as that is really confusing and error-prone.
    // However, to allow for future changes we do not error on that. Set this
    // flag to turn that into an error so you can migrate your collisions.
    DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP = 0x400,

    // DNDC_ENABLE_JS_WRITE
    // --------------------
    // Allow JavaScript to write files.
    DNDC_ENABLE_JS_WRITE = 0x800,

    // DNDC_DONT_IMPORT
    // ----------------
    // Don't import files (via #import or from import nodes), instead leaving
    // them as is in the document. This is useful for breaking circular
    // dependencies when bootstrapping a document that relies on introspection.
    DNDC_DONT_IMPORT = 0x1000,

    // DNDC_NO_THREADS
    // ---------------
    // Don't spawn any worker threads. No parallelism.
    DNDC_NO_THREADS = 0x2000,

    // DNDC_NO_CLEANUP
    // ---------------
    // Don't cleanup allocations or anything.
    // Appropriate for batch use.
    DNDC_NO_CLEANUP = 0x4000,

    // DNDC_STRIP_WHITESPACE
    // ---------------------
    // Strip trailing and leading whitespace from all output lines.
    DNDC_STRIP_WHITESPACE = 0x8000,

    // DNDC_DONT_INLINE_IMAGES
    // -----------------------
    // Instead of base64-ing the image, use a link.
    DNDC_DONT_INLINE_IMAGES = 0x10000,

    // DNDC_USE_DND_URL_SCHEME
    // -----------------------
    // For imgs, don't base64 them and don't use regular links. Instead, use a
    // dnd:///absolute/path/to/img url instead. Applications can then
    // implement custom url handlers for this url scheme.
    DNDC_USE_DND_URL_SCHEME = 0x20000,

    // DNDC_OUTPUT_EXPANDED_DND
    // ------------------------
    // After resolving imports and executing user scripts, output as a single
    // file .dnd file instead of html.
    DNDC_OUTPUT_EXPANDED_DND = 0x40000,

    // Unused Flags
    // ------------
    // Flags currently uses 19 bits, 2 of those are unused.
};

//
// DndcCompilation
// ---------------


  //
  // dndc_compile_dnd_file
  // ---------------------
  //
  // Low-level function to compile a dnd source file. The behavior of this
  // function is complex and is greatly controlled by the flags argument. See
  // `DndcFlags` for details on the exact meaning of the flags.
  //
  // In its default mode, this function will parse the given source text, resolve
  // imports, execute javascript blocks, spawn a thread to base64 referenced
  // images, load referenced files such as js files and css files, and render the
  // result into an html file at the given location.
  //
  // Args:
  // ----
  //
  // flags:
  //    A bitwise-or combination of `DndcFlags`.
  //
  // base_directory:
  //    May be a zero-length string. For relative filepaths referenced in the
  //    document, what those paths are relative to. Defaults to the current
  //    directory for a zero length string view.
  //
  //    Specifically, this string plus a directory separator will be prepended to
  //    all paths for the purposes of opening those paths.
  //
  // source_text:
  //    The string to be parsed and compiled.
  //
  // source_path:
  //    The filepath that the source path was loaded from. This is mostly used
  //    for reporting errors.
  //
  // outpath:
  //    Several features depend on knowing what the ultimate name of the file
  //    will be. APIs such as ctx.outpath etc. in js blocks for example. Note
  //    that we do not actually write to this path.
  //
  //    This path is *NOT* adjusted by the base_directory argument.
  //
  // outstring:
  //    A pointer to a string structure to write the data to. The text will be
  //    allocated via malloc. You can call `dndc_free_string` on the text if you
  //    are on a platform where each dynamic library has its own heap (aka
  //    Windows).
  //
  // base64filecache:
  //    A pointer to a filecache (created with dndc_create_filecache) that is
  //    used to cache files across invocations of this function. This may be
  //    null, in which case no caching is done.
  //
  //    This cache is used to cache the results of loading and base64-ing binary
  //    files.
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
  // worker_thread:
  //   A thread created with `dndc_worker_create`.
  //
  // jsargs:
  //   A json string literal that will be available to JS blocks as Args. May be
  //   the empty string. Should be an object literal or an array literal. An
  //   empty string will be treated as "null".
  //
  // Returns:
  // --------
  // Returns 0 on success, a non-zero error code otherwise.
  //
  DNDC_API
  int
  dndc_compile_dnd_file(
      unsigned long long flags,
      DndcStringView base_directory,
      DndcStringView source_text,
      DndcStringView source_path,
      DndcStringView outpath,
      DndcLongString* outstring,
      DNDC_NULLABLE(DndcFileCache*) base64cache,
      DNDC_NULLABLE(DndcFileCache*) textcache,
      DNDC_NULLABLE(DndcErrorFunc*) error_func,
      DNDC_NULLABLE(void*) error_user_data,
      DNDC_NULLABLE(DndcDependencyFunc*) dependency_func,
      DNDC_NULLABLE(void*) dependency_user_data,
      DNDC_NULLABLE(DndcWorkerThread*) worker_thread,
      DndcLongString jsargs
  );


  //
  // dndc_format
  // -----------
  //
  // Turns the given .dnd string into another .dnd string, but formatted such
  // that lines do not exceed 79 characters if it is possible to semantically do
  // so, lines are right-stripped, redundant blank lines are merged, etc.  The
  // resulting string is stored in output.
  //
  // This function does not execute any javascript blocks and does not read any
  // files.
  //
  // The output is allocated by malloc. You take ownership of the result.  If on
  // Windows and if loaded from a dll, you should use `dndc_free_string`.
  //
  // Arguments:
  // ----------
  // source_text:
  //    The actual source .dnd string. This string does need to be
  //    nul-terminated. No references to this are retained afterwards.
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
  // Returns:
  // --------
  // Returns 0 on success, a non-zero error code otherwise.
  // If non-zero, output will not be written to.
  //
  DNDC_API
  int
  dndc_format(DndcStringView source_text,
          DndcLongString* output,
          DNDC_NULLABLE(DndcErrorFunc*) error_func,
          DNDC_NULLABLE(void*) error_user_data);

  //
  // dndc_free_string
  // ----------------
  //
  // On windows, if you load a dll, it will have its own crt and thus its own
  // heap. This function is provided so you can free the returned string with the
  // right heap. On Linux or MacOS this is unnecessary as dynamic linking works
  // differently.
  //
  DNDC_API
  void
  dndc_free_string(DndcLongString);




#ifdef __cplusplus
}
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
