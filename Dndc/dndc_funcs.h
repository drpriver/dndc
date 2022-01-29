#ifndef DNDC_FUNCS_H
#define DNDC_FUNCS_H
#include "dndc.h"
#include "errorable_long_string.h"
#include "common_macros.h"
#include "MStringBuilder.h"
#include "dndc_types.h"

#ifndef warn_unused
#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused _Check_return
#else
#error "No warn unused analogue"
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

//
// dndc_funcs.h
// ------------
// Forward declarations of functions that are shared between components,
// including the main run function.
//
// This documents the internal API. For the external API, see dndc.h
//


typedef struct WorkerThread WorkerThread;

//
// DndcPostParseAstFunc
// --------------------
// Callback for working with the tree right before rendering.
// This is misleadingly named.
typedef int(DndcPostParseAstFunc)(Nullable(void*)user_data, DndcContext*);

//
// run_the_dndc
// ------------
//
// The money function.  Basically executes the whole thing from end to end.
// Parses the data referenced by source and converts it into html, formatted,
// .dnd, etc.
//
// Arguments:
// ----------
// flags:
//    Bitflags controlling behavior of execution. Consult the flags header for
//    the meanings of individual values. Is a bitwise-or combination of the
//    different options.
//
// base_directory:
//    Base directory that is the root of all relative paths. Can be the empty
//    string, in which case paths are left as is. Absolute paths in the
//    document are left unaltered.
//
// source_text:
//    The string to be parsed and compiled.
//
// source_path:
//    The filepath that the source path was loaded from. This is mostly
//    used for reporting errors.
//
// outpath:
//    Several features depend on knowing what the ultimate name of the file
//    will be.  APIs such as ctx.outpath etc. in js blocks for example.  Note
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
// external_b64cache:
//    An optional pointer to an external cache for base64 images. If NULL,
//    will internally create and then destroy one. Allows saving on io and
//    computation if being run repeatedly. Note that the cache will
//    be moved from one thread to another and used concurrently with the
//    external_textcache. So, don't do something silly like share a stateful
//    allocator between them.
//
// external_textcache:
//    An optional pointer to an external cache for text files. If NULL, will
//    internally create and then destroy one. Allows saving on computation and
//    IO if being run repeatedly.
//
// error_func:
//    A function for reporting errors. See `DndcErrorFunc` in dndc.h. If NULL,
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
//    `DndcDependencyFunc`.
//
// dependency_user_data:
//    A pointer that will be passed to the dependency_func.
//
// ast_func:
//    A function that will be called right before html generation. This exposes
//    the ctx to the caller.
//
// ast_func_user_data:
//    A pointer that will be passed to the ast_func.
//
// worker_thread:
//    A worker thread that can be reused for parallel tasks.
//
// jsargs:
//    A json literal that will be exposted to js as Args.
//
// Returns:
// --------
// Zero is returned upon success.
// On failure, a non-zero error code will be returned.
//

static
warn_unused
int
run_the_dndc(uint64_t flags,
        StringView base_directory,
        StringView source_text,
        StringView source_path,
        StringView outpath,
        LongString* outstring,
        Nullable(FileCache*)external_b64cache,
        Nullable(FileCache*)external_textcache,
        Nullable(DndcErrorFunc*)error_func,
        Nullable(void*)error_user_data,
        Nullable(DndcDependencyFunc*)dependency_func,
        Nullable(void*)dependency_user_data,
        Nullable(DndcPostParseAstFunc*)ast_func,
        Nullable(void*)ast_func_user_data,
        Nullable(WorkerThread*) worker_thread,
        LongString jsargs
    );

//
// dndc_parse
// ----------
// Parses the nul-terminated source text;
// Resulting nodes will be added as children of the node indicated by
// the given NodeHandle.
//
// Returns 0 on success.
//
static
warn_unused
int
dndc_parse(DndcContext*, NodeHandle root, StringView filename,
           const char* text, size_t length);

//
// build_nav_block
// ---------------
// Walks the tree to construct the nav block.
// Sets the result as a string on the context.
//
static
void
build_nav_block(DndcContext*);

//
// Error reporting functions
// -------------------------
// The following functions are for reporting errors and warnings. ONLY use
// these functions for that purpose. Do not directly use printf, fprintf or a
// log function. These functions will report the error as originating from a
// specific file, line, column and will handle suppressing them based on the
// flags given to run_the_dndc.
//

  //
  // parse_set_err
  // -------------
  // Sets an error message on the context during parsing. errchar is a pointer to
  // the first character where the error occurred. Pointer arithmetic is then
  // used to determine the column of the error (file and line are implicit).
  //
  static
  void
  parse_set_err(DndcContext* ctx, NullUnspec(const char*) errchar, LongString);

  // parse_set_err_q
  // ---------------
  // Ditto, but the last argument is quoted.
  static
  void
  parse_set_err_q(DndcContext* ctx, const char* errchar, StringView, StringView);

  //
  // node_set_err
  // ------------
  // Sets an error message originating from the source location that corresponds
  // to the given node.
  //
  static
  void
  node_set_err(DndcContext* ctx, const Node*, LongString);

  // node_set_err_q
  // --------------
  // Ditto, but the last argument is quoted.
  static
  void
  node_set_err_q(DndcContext* ctx, const Node* node, StringView msg, StringView quoted);

  //
  // node_set_err_offset
  // -------------------
  // Like `node_set_err`, but with an offset to the column.
  //
  // Sets an error message originating from the source location that corresponds
  // to the given node.
  //
  static
  void
  node_set_err_offset(DndcContext* ctx, const Node*, int, LongString);

  //
  // node_print_err
  // --------------
  // Like node_set_err, but immediately prints the message instead of setting
  // a string.
  //
  // Only use this in the body of run_the_dndc.
  //
  static
  void
  node_print_err(DndcContext* ctx, const Node*, LongString);

  //
  // node_print_warning
  // ------------------
  // Like node_set_err, but immediately prints the message instead of setting a
  // string and is intended for non-fatal warning messages.
  //
  static
  void
  node_print_warning(DndcContext* ctx, const Node* node, StringView msg);

  // node_print_warning2
  // -------------------
  // ditto
  static
  void
  node_print_warning2(DndcContext* ctx, const Node* node, StringView, StringView);

  //
  // report_time
  // -----------
  // Reports time to execute some component.
  //
  static
  void
  report_time(DndcContext*, StringView msg, uint64_t microseconds);

  //
  // report_size
  // -----------
  // Reports size of some component.
  //
  static
  void
  report_size(DndcContext*, StringView msg, uint64_t microseconds);

  //
  // report_info
  // -----------
  // Reports some information.
  //
  static
  void
  report_info(DndcContext*, StringView msg);

  //
  // report_set_error
  // ----------------
  // Reports an error that was set by a different part of the system.
  //
  static
  void
  report_set_error(DndcContext*);

  //
  // report_system_error
  // -------------------
  // Reports an error that did not originate from the source text. Should only be
  // called by run_the_dndc right before it returns an error.
  //
  static
  void
  report_system_error(DndcContext* ctx, StringView msg);

//
// Node Funcs
// ----------
// Functions for creating and manipulating nodes in the tree.

  //
  // alloc_handle
  // ------------
  // Allocate a new node and return its handle.
  //
  static inline
  NodeHandle
  alloc_handle(DndcContext*);


  //
  // get_node
  // --------
  // Getter function to turn a node handle to an acual pointer to a Node.
  // Keep the scopes on these pointers as tight as possible as allocating new
  // nodes can trigger pointer invalidation.
  //
  static inline
  Node*
  get_node(DndcContext*, NodeHandle);

  //
  // get_node_e
  // ----------
  // Like get_node, but will be available in the debugger as it is extern.
  // Don't use this, use get_node, this is purely for the debugger.
  //
  DNDC_API
  Node*
  get_node_e(DndcContext*, NodeHandle);


  //
  // node_has_attribute
  // ------------------
  // Checks if the node has an attribute or not.
  // You generally should not be calling this as attributes are for user scripts
  // only, but you can use this in the implementation of the bindings.
  //
  static inline
  bool
  node_has_attribute(const Node* node, StringView attr);

  //
  // node_has_class
  // --------------
  // Checks if the node has a class or not.
  //
  static inline
  bool
  node_has_class(const Node* node, StringView class);

  //
  // node_get_attribute
  // ------------------
  // Retrieves the value associate with attr key.
  //
  // If the node does not have that attribute, returns NULL. Many attributes
  // will have empty strings. In that case, a pointer to an empty string view is
  // returned.
  //
  // Note that this pointer returned by this function is unstable. Adding
  // attributes can invalidate the pointer.
  //
  static inline
  Nullable(StringView*)
  node_get_attribute(const Node* node, StringView attr);

  //
  // node_set_attribute
  // ------------------
  // Sets an attribute on the node.
  //
  static inline
  void
  node_set_attribute(Node* node, Allocator allocator, StringView attr, StringView value);

  //
  // node_get_id
  // -----------
  // Retrieves the id of the node. Handles the id being set via directive.
  // If the node has empty header, or if it's noid, returns zero length
  // string.
  //
  static inline
  StringView
  node_get_id(DndcContext*, NodeHandle);

  // node_get_explicit_id
  // --------------------
  // Don't call this function unless you really need what was explicitly set
  // by a #id() directive.
  static inline
  bool
  node_get_explicit_id(DndcContext* ctx, NodeHandle handle, StringView* out);

  //
  // node_set_id
  // -----------
  // Overrides the id of the node.
  //
  static inline
  void
  node_set_id(DndcContext* ctx, NodeHandle, StringView);

  //
  // append_child
  // ------------
  // Add a node to be a child of another node. The child will have its parent
  // node set to this parent node and the child will be appended to the parent's
  // children array.
  //
  static
  void
  append_child(DndcContext*, NodeHandle parent, NodeHandle child);

  //
  // node_insert_child
  // -----------------
  // Insert the node as a child of the parent at index i.
  // If i >= count, just appends to the end.
  //
  static inline
  void
  node_insert_child(DndcContext* ctx, NodeHandle parent, size_t i, NodeHandle child);

  // node_clone
  // ----------
  // Hacky clone function
  // Read the implementation as this has weird caveats.
  static inline
  NodeHandle
  node_clone(DndcContext* ctx, NodeHandle);

  //
  // convert_node_to_container_...
  // ---------------------------------------------------------
  // Makes a copy of the indicated node, turns the original node into a container
  // node and then adds the copy to the container.
  //
  static inline
  void
  convert_node_to_container_containing_clone_of_former_self(DndcContext* ctx, NodeHandle);

//
// File Access Functions
// ----------------------
//

  // ctx_load_source_file
  // --------------------
  // Loads the text of a sourcefile given by sourcepath, or an error if
  // something went wrong.
  //
  // This function provides caching and management of the text. Do not
  // read files directly, use this function instead.
  //
  // sourcepath will be adjusted by the context's base directory.
  //
  static
  warn_unused
  StringViewResult
  ctx_load_source_file(DndcContext* ctx, StringView sourcepath);

  //
  // ctx_load_processed_binary_file
  // ------------------------------
  // Load a binary file as base64 text, or an error if something went wrong.
  //
  // This function provides caching and management of the text. Do not
  // read files directly, use this function instead.
  //
  // binarypath will be adjusted by the context's base directory.
  //
  static
  warn_unused
  StringViewResult
  ctx_load_processed_binary_file(DndcContext* ctx, StringView binarypath);

  //
  // ctx_store_builtin_file
  // ----------------------
  // Stores a file in the context as a special builtin file.
  //
  // Does not copy the data at all, so make sure these last long enough and
  // are appropriately allocated.
  //
  // The sourcepath will not be adjusted by the base directory, but neither
  // will lookups for builtin files.
  //
  static inline
  void
  ctx_store_builtin_file(DndcContext* ctx, StringView sourcepath, StringView text);

  //
  // ctx_add_builtins
  // ----------------
  // Adds special builtin-scripts and stylesheets to the source cache.
  //
  static inline
  void
  ctx_add_builtins(DndcContext* ctx);

  //
  // ctx_note_dependency
  // -------------------
  // Marks a file as being a dependency of the document. Deduplicates.
  //
  static inline
  void
  ctx_note_dependency(DndcContext* ctx, StringView path);

// Output Functions
// ----------------
// Functions that render the tree into some format.

  //
  // expand_to_dnd
  // -------------
  // Writes the document tree (starting from the context's root node)
  // as a .dnd file into the given builder. The result is a .dnd file
  // that should parse into a tree that will render to be the same
  // as the current tree. (The exact internal representation will be
  // different as some nodes can't be created from the regular syntax,
  // like CONTAINER nodes).
  //
  // Returns 0 on success.
  //
  static
  warn_unused
  int
  expand_to_dnd(DndcContext*, MStringBuilder*);

  //
  // render_tree
  // -----------
  // Writes the document tree (starting from the context's root node)
  // as html into the given builder. The result is a fully valid html
  // document including head tags, etc.
  //
  static
  warn_unused
  int
  render_tree(DndcContext*, MStringBuilder*);

  //
  // render_node
  // -----------
  // Writes the tree originating from the given node into the builder.
  // header_depth controls whether a header is an h2, h3, etc.
  // Pass 1 to have titles be h1s, top level headers be h2 etc.
  // Increase this to get h3s or whatever instead.
  //
  // The result is an html fragment.
  //
  // Returns 0 on success.
  //
  static inline
  force_inline
  warn_unused
  int
  render_node(DndcContext*, MStringBuilder* restrict, NodeHandle, int header_depth, int node_depth);

  //
  // format_tree
  // -----------
  // Writes the document tree (starting from the context's root node)
  // as a .dnd file, formatted to remove trailing spaces, wrap to 80 columns, etc.
  //
  // Returns 0 on success.
  //
  static
  warn_unused
  int
  format_tree(DndcContext*, MStringBuilder*);

// Link Functions
// --------------

  //
  // gather_anchors
  // --------------
  // Traverses the tree to find all the link targets
  // that result from the header targets in the document.
  // Populates the link-target mapping.
  //
  static
  void
  gather_anchors(DndcContext*);


  //
  // find_link_target
  // ----------------
  // Find the target that the kebabed string view is actually a link to.
  // Can return NULL if the link can't be resolved.
  //
  static inline
  Nullable(StringView*)
  find_link_target(DndcContext* ctx, StringView kebabed);

  //
  // add_link_from_sv
  // ----------------
  // Parses a line of a ::links block, which is of the form "link = target"
  //
  // Returns 0 on success.
  //
  static inline
  warn_unused
  int
  add_link_from_sv(DndcContext* ctx, Node* node);

  //
  // add_link_from_header
  // --------------------
  // Adds the link to the link map as derived from the header
  // The transmutation is
  //   kebabed(str) = #kebabed(str)
  //
  static inline
  void
  add_link_from_header(DndcContext* ctx, StringView str);


#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
