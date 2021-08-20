#ifndef DNDC_FUNCS_H
#define DNDC_FUNCS_H
#include "dndc.h"
#include "errorable_long_string.h"
#include "MStringBuilder.h"
#include "ByteBuilder.h"
#include "dndc_types.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

//
// Forward declarations of functions that are shared between components,
// including the main run function.
// This documents the internal API. For the external API, see dndc.h
//

//
// The money function.  Basically executes the whole thing from end to end.
// Parses the data referenced by source and converts it into html, formatted,
// .dnd, etc.
//
// Arguments
// ---------
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
// source:
//    As controlled by the flags, this can be the data to parse, a path to a
//    file to parse, or if .length is 0, stdin will be read instead. This path
//    is adjusted by the base_directory argument.
//
// output_path:
//    A filepath to write the generated html to. If NULL, the html will be
//    printed to stdout instead. Printing at all can be suppressed by flags.
//
//    Flags can make this an out param instead, in which case a malloced string
//    that is the generated html will be stored in this instead.
//
//    This path is *NOT* adjusted by the base_directory argument.
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
//    An optional pointer to an external cache for text files. If NULL,
//    will internally create and then destroy one. Allows saving on io and
//    computation if being run repeatedly.
//
// error_func:
//   A function for reporting errors. See `DndcErrorFunc` in dndc.h. If NULL,
//   errors will not be printed. Use `dndc_stderr_error_func` for a function
//   that just prints to stderr.
//
// error_user_data:
//   A pointer that will be passed to the error_func. For
//   `dndc_stderr_error_func`, this should be NULL. For a function you've
//   defined, pass an appropriate pointer!
//
// dependency_func:
//    A function for reporting the dependencies of the generated file. See
//    `DndcDependencyFunc`.
//
// dependency_user_data:
//   A pointer that will be passed to the dependency_func.
//
// ast_func:
//   A function that will be called right before html generation. This exposes
//   the ctx to the caller.
//
// ast_func_user_data:
//   A pointer that will be passed to the ast_func.
//
// Returns
// -------
// Nothing is returned upon success (.errored == NO_ERROR).
// On failure, an error will be indicated.
//

typedef int (DndcPostParseAstFunc)(Nullable(void*)user_data, Nonnull(DndcContext*));
static
Errorable_f(void)
run_the_dndc(uint64_t flags, LongString base_directory, LongString source_or_path,
        Nullable(LongString*) output_path,
        Nullable(FileCache*)external_b64cache,
        Nullable(FileCache*)external_textcache,
        Nullable(DndcErrorFunc*)error_func, Nullable(void*)error_user_data,
        Nullable(DndcDependencyFunc*)dependency_func,
        Nullable(void*)dependency_user_data,
        // TEMPORARY HACK
        // Might want to have an API that yields a parse context + ast?
        Nullable(DndcPostParseAstFunc*)ast_func,
        Nullable(void*)ast_func_user_data
        );

//
// The following functions are for reporting errors and warnings. ONLY use
// these functions for that purpose. Do not directly use printf, fprintf or a
// log function. These functions will report the error as originating from a
// specific file, line, column and will handle suppressing them based on the
// flags given to run_the_dndc.
//

//
// Sets an error message on the context during parsing. errchar is a pointer
// to the first character where the error occurred. Pointer arithmetic is then
// used to determine the column of the error (file and line are implicit).
//
//
static
void
parse_set_err(DndcContext* ctx, NullUnspec(const char*) errchar, LongString);

static
void
parse_set_err_q(DndcContext* ctx, NullUnspec(const char*) errchar, StringView, StringView);

//
// Sets an error message originating from the source location that corresponds
// to the given node.
//
static
void
node_set_err(DndcContext* ctx, const Node*, LongString);

static
void
node_set_err_q(DndcContext* ctx, const Node* node, StringView msg, StringView quoted);

//
// Like node_set_err, but with an offset to the column.
//
// Sets an error message originating from the source location that corresponds
// to the given node.
//
static
void
node_set_err_offset(DndcContext* ctx, const Node*, int, LongString);

//
// Like node_set_err, but immediately prints the message instead of setting
// a string.
//
// Only use this in the body of run_the_dndc.
//
static
void
node_print_err(DndcContext* ctx, const Node*, StringView);

//
// Like node_set_err, but immediately prints the message instead of setting a
// string and is intended for non-fatal warning messages.
//
static
void
node_print_warning(DndcContext* ctx, const Node* node, StringView msg);
static
void
node_print_warning2(DndcContext* ctx, const Node* node, StringView, StringView);

//
// Reports time to execute some component.
//
static
void
report_time(DndcContext*, StringView msg, uint64_t microseconds);

//
// Reports size of some component.
//
static
void
report_size(DndcContext*, StringView msg, uint64_t microseconds);

//
// Reports some information.
//
static
void
report_info(DndcContext*, StringView msg);

//
// Reports an error that was set by a different part of the system.
//
static
void
report_set_error(DndcContext*);

//
// Reports an error that did not originate from the source text. Should only be
// called by run_the_dndc right before it returns an error.
//
static
void
report_system_error(DndcContext* ctx, StringView msg);

//
// Getter function to turn a node handle to an acual pointer to a Node.
// Keep the scopes on these pointers as tight as possible as allocating new
// nodes can trigger pointer invalidation.
//
static inline
Node*
get_node(DndcContext*, NodeHandle);

//
// Like get_node, but will be available in the debugger as it is extern.
// Don't use this, use get_node, this is purely for the debugger.
//
DNDC_API
Node*
get_node_e(DndcContext*, NodeHandle);


//
// Checks if the node has an attribute or not.
//
// For example:
//
//    if(node_has_attribute(mynode, SV("noid"))){
//       ... Do something based on the fact that the node is noid ...
//    }
//
static inline
bool
node_has_attribute(const Node* node, StringView attr);

//
// Checks if the node has a class or not.
//
static inline
bool
node_has_class(const Node* node, StringView class);

//
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
// Retrieves the id of the node. Handles the id being set via attribute. If
// the node has empty header, returns NULL.
//
// Note that the pointer returned by this function is unstable.
// Adding attributes can invalidate the pointer.
//
static inline
Nullable(const StringView*)
node_get_id(const Node* node);

//
// Loads the text of a sourcefile given by sourcepath, or an error if
// something went wrong.
//
// This function provides caching and management of the text. Do not
// read files directly, use this function instead.
//
// sourcepath will be adjusted by the context's base directory.
//
static
Errorable_f(LongString)
ctx_load_source_file(DndcContext* ctx, StringView sourcepath);

//
// Load a binary file as base64 text, or an error if something went wrong.
//
// This function provides caching and management of the text. Do not
// read files directly, use this function instead.
//
// binarypath will be adjusted by the context's base directory.
//
static
Errorable_f(LongString)
ctx_load_processed_binary_file(DndcContext* ctx, StringView binarypath);

//
// Load a binary file as base64 text, or an error if something went wrong.
//
// This function provides caching and management of the text.
// This is a lower level version of ctx_load_processed_binary_file. It allows
// better control over re-using memory (in the ByteBuilder).
//
// binarypath is not adjusted by any implicit base directory. Thus, this function
// should be called with a pre-adjusted path.
//
static
Errorable_f(LongString)
load_processed_binary_file(FileCache* cache, StringView binarypath, ByteBuilder* bb);

//
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
ctx_store_builtin_file(DndcContext* ctx, LongString sourcepath, LongString text);

//
// Marks a file as being a dependency of the document. Deduplicates.
//
static inline
void
ctx_note_dependency(DndcContext* ctx, StringView path);

//
// Parses the nul-terminated source text;
// Resulting nodes will be added as children of the node indicated by
// the given NodeHandle.
//
static
Errorable_f(void)
dndc_parse(DndcContext*, NodeHandle root, StringView filename, const char* text);


//
// Prints out a representation of the final document tree.
// I might remove this later, it's mostly for debugging.
// Calls itself recursively, thus the depth argument.
//
static
void
print_node_and_children(Nonnull(DndcContext*), NodeHandle handle, int depth);

//
// Writes the document tree (starting from the context's root node)
// as html into the given builder. The result is a fully valid html
// document including head tags, etc.
//
static
Errorable_f(void)
render_tree(DndcContext*, MStringBuilder*);

//
// Writes the tree originating from the given node into the builder.
// header_depth controls whether a header is an h2, h3, etc.
// Pass 1 to have titles be h1s, top level headers be h2 etc.
// Increase this to get h3s or whatever instead.
//
// The result is an html fragment.
//
static inline
force_inline
Errorable_f(void)
render_node(DndcContext*, MStringBuilder* restrict, Node*, int header_depth);

//
// Writes the document tree (starting from the context's root node)
// as a .dnd file, formatted to remove trailing spaces, wrap to 80 columns, etc.
//
static
Errorable_f(void)
format_tree(DndcContext*, MStringBuilder*);

//
// Traverses the tree to find all the link targets
// that result from the header targets in the document.
// Populates the link-target mapping.
//
static
void
gather_anchors(DndcContext*);

//
// Call this before any function that traverses the tree.
// Ensures that the tree is not so deep that there is a danger of exhausting
// the stack.
//
static
Errorable_f(void)
check_depth(DndcContext*);

//
// Walks the tree to construct the nav block.
// Sets the result as a string on the context.
//
static
void
build_nav_block(DndcContext*);

//
// Allocate a new node and return its handle.
//
static inline
NodeHandle
alloc_handle(DndcContext*);

//
// Execute a string representing python code. The string should be nul
// terminated. The NodeHandle will be present in the locals of the executed
// python code as "node".
//
static
Errorable_f(void)
execute_python_string(DndcContext*, Nonnull(const char*), NodeHandle);
//
// Initialize the python interpreter and the dndc python data types. Takes a
// flags argument, which is the same flags passed to run_the_dndc. This handles
// the PYTHON_IS_INIT flag.
//
static
Errorable_f(void)
internal_init_dndc_python_interpreter(uint64_t flags);

#ifndef PYTHONMODULE
//
// Shutdown the python interpreter, mostly freeing any resources it allocated.
// This is mostly for detecting memory leaks. There's some reference leaks
// somewhere it seems.
//
static
void
end_interpreter(void);
#endif

//
// Add a node to be a child of another node. The child will have its parent
// node set to this parent node and the child will be appended to the parent's
// children array.
//
static
void
append_child(DndcContext* , NodeHandle parent, NodeHandle child);

//
// Find the target that the kebabed string view is actually a link to.
// Can return NULL if the link can't be resolved.
//
static inline
Nullable(StringView*)
find_link_target(DndcContext* ctx, StringView kebabed);

//
// Parses a line of a ::links block, which is of the form "link = target"
//
static inline
Errorable_f(void)
add_link_from_sv(DndcContext* ctx, Nonnull(Node*)node);

//
// Adds the link to the link map as derived from the header
// The transmutation is:
//   kebabed(str) = #kebabed(str)
//
static inline
void
add_link_from_header(DndcContext* ctx, StringView str);


//
// Makes a copy of the indicated node, turns the original node into a container
// node and then adds the copy to the container.
//
static inline
void
convert_node_to_container_containing_clone_of_former_self(DndcContext* ctx, NodeHandle handle);

//
// Adds special builtin-scripts and stylesheets to the source cache.
//
static inline
void
ctx_add_builtins(DndcContext* ctx);

//
// Scans the base directory for files ending with .dnd
// and adds them as links.
//
static inline
Errorable_f(void)
ctx_add_auto_index_links(DndcContext* ctx);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
