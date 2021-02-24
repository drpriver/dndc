#ifndef DNDC_FLAGS_H
#define DNDC_FLAGS_H
// I wish C offered a typesafe bitflags type, but we resort
// to just making it a uint64 instead.
#ifndef WINDOWS
#include "common_macros.h"
FlagEnum DndcFlags {
    DNDC_FLAGS_NONE        = 0x0,
    // Don't error on bad links.
    DNDC_ALLOW_BAD_LINKS   = 0x001,
    // Don't report any non-fatal errors.
    DNDC_SUPPRESS_WARNINGS = 0x002,
    // Log stats during execution of timings and counts.
    DNDC_PRINT_STATS       = 0x004,
    // Log orphaned nodes.
    DNDC_REPORT_ORPHANS    = 0x008,
    // Don't execute python blocks.
    DNDC_NO_PYTHON         = 0x010,
    // The python interpreter and docparser types have already been initialized.
    DNDC_PYTHON_IS_INIT    = 0x020,
    // Print out the document tree
    DNDC_PRINT_TREE        = 0x040,
    // Print out all links and what they resolve to
    DNDC_PRINT_LINKS       = 0x080,
    // Don't spawn any worker threads. No parallelism.
    DNDC_NO_THREADS        = 0x100,
    // Don't write out the final result.
    DNDC_DONT_WRITE        = 0x200,
    // Don't cleanup allocations or anything
    DNDC_NO_CLEANUP        = 0x400,
    // The source_path argument is actually a string containing the data, not a path.
    DNDC_SOURCE_PATH_IS_DATA_NOT_PATH = 0x800,
    // Don't print errors to stderr
    DNDC_DONT_PRINT_ERRORS = 0x1000,
    // Don't bother isolating python from site
    // Greatly slows startup, but allows importing user-installed
    // libraries.
    DNDC_PYTHON_UNISOLATED = 0x2000,
    // The output path is actually a string to output data to.
    DNDC_OUTPUT_PATH_IS_OUT_PARAM = 0x4000,
    // Instead of rendering to html, render to .dnd with trailing
    // spaces removed, text aligned to 80 columns (if semantically equivelant)
    // etc.
    DNDC_REFORMAT_ONLY            = 0x8000,
};
#else
#define DNDC_FLAGS_NONE                   0x0000
#define DNDC_ALLOW_BAD_LINKS              0x0001
#define DNDC_SUPPRESS_WARNINGS            0x0002
#define DNDC_PRINT_STATS                  0x0004
#define DNDC_REPORT_ORPHANS               0x0008
#define DNDC_NO_PYTHON                    0x0010
#define DNDC_PYTHON_IS_INIT               0x0020
#define DNDC_PRINT_TREE                   0x0040
#define DNDC_PRINT_LINKS                  0x0080
#define DNDC_NO_THREADS                   0x0100
#define DNDC_DONT_WRITE                   0x0200
#define DNDC_NO_CLEANUP                   0x0400
#define DNDC_SOURCE_PATH_IS_DATA_NOT_PATH 0x0800
#define DNDC_DONT_PRINT_ERRORS            0x1000
#define DNDC_PYTHON_UNISOLATED            0x2000
#define DNDC_OUTPUT_PATH_IS_OUT_PARAM     0x4000
#define DNDC_REFORMAT_ONLY                0x8000
#endif


#endif
