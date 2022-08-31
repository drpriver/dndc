//
// Copyright © 2021-2022, David Priver
//
#ifndef DNDC_FUNCS_H
#define DNDC_FUNCS_H
#include "dndc_long_string.h"
#include "dndc.h"
#include "errorable_long_string.h"
#include "common_macros.h"
#include "dndc_types.h"
#include "Utils/MStringBuilder.h"
#include "Utils/msb_format.h"

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
typedef int(DndcPostParseAstFunc)(void*_Nullable user_data, DndcContext*);

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
// output_target:
//    What to output.
//
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
// log_func:
//    A function for reporting errors. See `DndcLogFunc` in dndc.h. If NULL,
//    errors will not be printed. Use `dndc_stderr_log_func` for a function
//    that just prints to stderr.
//
// log_user_data:
//    A pointer that will be passed to the log_func. For
//    `dndc_stderr_log_func`, this should be NULL. For a function you've
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


enum OutputTarget {
    OUTPUT_HTML     = 0,
    OUTPUT_REFORMAT = 1,
    OUTPUT_MD       = 2,
    OUTPUT_EXPAND   = 3,
};

static
warn_unused
int
run_the_dndc(
        enum OutputTarget output_target,
        uint64_t flags,
        StringView base_directory,
        StringView source_text,
        StringView source_path,
        LongString* outstring,
        FileCache*_Nullable external_b64cache,
        FileCache*_Nullable external_textcache,
        DndcLogFunc*_Nullable log_func,
        void*_Nullable log_user_data,
        DndcDependencyFunc*_Nullable dependency_func,
        void*_Nullable dependency_user_data,
        DndcPostParseAstFunc*_Nullable ast_func,
        void*_Nullable ast_func_user_data,
        WorkerThread*_Nullable  worker_thread,
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
// build_toc_block
// ---------------
// Walks the tree to construct the toc block.
// Sets the result as a string on the context.
//
static
void
build_toc_block(DndcContext*);

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
  // node_get_attribute
  // ------------------
  // Retrieves the value associate with attr key.
  //
  // If the node does not have that attribute, returns non-zero. Many attributes
  // will have empty strings. In that case, value is set to the empty string.
  //
  static inline
  int
  node_get_attribute(const Node* node, StringView attr, StringView* value);

  //
  // node_set_attribute
  // ------------------
  // Sets an attribute on the node.
  //
  static inline
  warn_unused
  int
  node_set_attribute(Node* node, Allocator allocator, StringView attr, StringView value);

  //
  // node_has_class
  // --------------
  // Checks if the node has a class or not.
  //
  static inline
  bool
  node_has_class(const Node* node, StringView class);

  //
  // node_add_class
  // --------------
  // Adds a class to the node, removing duplicates.
  //
  static inline
  warn_unused
  int
  node_add_class(DndcContext* ctx, NodeHandle handle, StringView cls);

  static inline
  void
  node_remove_class(Node* node, StringView cls);

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
  warn_unused
  int
  append_child(DndcContext*, NodeHandle parent, NodeHandle child);

  //
  // node_insert_child
  // -----------------
  // Insert the node as a child of the parent at index i.
  // If i >= count, just appends to the end.
  //
  static inline
  warn_unused
  int
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
  // ctx_note_dependency
  // -------------------
  // Marks a file as being a dependency of the document. Deduplicates.
  //
  static inline
  warn_unused
  int
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
  // render_md
  // -----------
  // Writes the document tree (starting from the context's root node)
  // as a .md file, in a best effort manner.
  //
  // Returns 0 on success.
  //
  static
  warn_unused
  int
  render_md(DndcContext*, MStringBuilder*);

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


  //
  // format_node
  // -----------
  // Writes the document from the given node. Note that it does
  // not elide the header or indent for the topmost level node.
  // That is reserved for the root.
  //
  // Returns 0 on success.

  static inline
  warn_unused
  int
  format_node(DndcContext* ctx, MStringBuilder*, Node*, int indent);

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
  warn_unused
  int
  gather_anchors(DndcContext*);


  //
  // find_link_target
  // ----------------
  // Find the target that the kebabed string view is actually a link to.
  // Can return NULL if the link can't be resolved.
  // 
  // Returns 1 if the link is missing.
  //
  static inline
  int
  find_link_target(DndcContext* ctx, StringView kebabed, StringView*);

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
  warn_unused
  int
  add_link_from_header(DndcContext* ctx, StringView str);

  static inline
  warn_unused
  int
  add_link_from_pair(DndcContext* ctx, StringView kebabed, StringView value);


#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
