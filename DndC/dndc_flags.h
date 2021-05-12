#ifndef DNDC_FLAGS_H
#define DNDC_FLAGS_H
// I wish C offered a typesafe bitflags type, but we resort
// to just making it a uint64 instead.
#ifndef _WIN32
#include "common_macros.h"
enum DndcFlags {
    DNDC_FLAGS_NONE        = 0x0,
    // Don't error on bad links.
    DNDC_ALLOW_BAD_LINKS   = 0x0001,
    // Don't report any non-fatal errors.
    DNDC_SUPPRESS_WARNINGS = 0x0002,
    // Log stats during execution of timings and counts.
    DNDC_PRINT_STATS       = 0x0004,
    // Log orphaned nodes.
    DNDC_REPORT_ORPHANS    = 0x0008,
    // Don't execute python blocks.
    DNDC_NO_PYTHON         = 0x0010,
    // The python interpreter and docparser types have already been initialized.
    DNDC_PYTHON_IS_INIT    = 0x0020,
    // Print out the document tree
    DNDC_PRINT_TREE        = 0x0040,
    // Print out all links and what they resolve to
    DNDC_PRINT_LINKS       = 0x0080,
    // Don't spawn any worker threads. No parallelism.
    DNDC_NO_THREADS        = 0x0100,
    // Don't write out the final result.
    DNDC_DONT_WRITE        = 0x0200,
    // Don't cleanup allocations or anything
    DNDC_NO_CLEANUP        = 0x0400,
    // The source_path argument is actually a string containing the data, not a path.
    DNDC_SOURCE_PATH_IS_DATA_NOT_PATH = 0x0800,
    // Don't print errors to stderr
    DNDC_DONT_PRINT_ERRORS = 0x01000,
    // Don't bother isolating python from site
    // Greatly slows startup, but allows importing user-installed
    // libraries.
    DNDC_PYTHON_UNISOLATED = 0x02000,
    // The output path is actually a string to output data to.
    DNDC_OUTPUT_PATH_IS_OUT_PARAM = 0x04000,
    // Instead of rendering to html, render to .dnd with trailing
    // spaces removed, text aligned to 80 columns (if semantically equivelant)
    // etc.
    DNDC_REFORMAT_ONLY      = 0x08000,
    // Instead of base64-ing the image, use a link.
    DNDC_DONT_INLINE_IMAGES =  0x10000,
    // The depends argument of dndc is a callback to be called with each
    // of the document's dependencies.
    DNDC_DEPENDS_IS_CALLBACK = 0x20000,
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
};
#else
//
// See above for documentation.
#define DNDC_FLAGS_NONE                   0x000000
#define DNDC_ALLOW_BAD_LINKS              0x000001
#define DNDC_SUPPRESS_WARNINGS            0x000002
#define DNDC_PRINT_STATS                  0x000004
#define DNDC_REPORT_ORPHANS               0x000008
#define DNDC_NO_PYTHON                    0x000010
#define DNDC_PYTHON_IS_INIT               0x000020
#define DNDC_PRINT_TREE                   0x000040
#define DNDC_PRINT_LINKS                  0x000080
#define DNDC_NO_THREADS                   0x000100
#define DNDC_DONT_WRITE                   0x000200
#define DNDC_NO_CLEANUP                   0x000400
#define DNDC_SOURCE_PATH_IS_DATA_NOT_PATH 0x000800
#define DNDC_DONT_PRINT_ERRORS            0x001000
#define DNDC_PYTHON_UNISOLATED            0x002000
#define DNDC_OUTPUT_PATH_IS_OUT_PARAM     0x004000
#define DNDC_REFORMAT_ONLY                0x008000
#define DNDC_DONT_INLINE_IMAGES           0x010000
#define DNDC_DEPENDS_IS_CALLBACK          0x020000
#define DNDC_USE_DND_URL_SCHEME           0x040000
#define DNDC_INPUT_IS_UNTRUSTED           0x080000
#define DNDC_STRIP_WHITESPACE             0x100000
#endif


#endif
