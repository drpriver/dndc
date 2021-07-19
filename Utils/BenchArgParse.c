#include <stdio.h>
#include "argument_parsing.h"
static inline
void
clear_parser(Nonnull(ArgParser*) parser){
    for(size_t i = 0; i < parser->positional.count; i++){
        auto arg = &parser->positional.args[i];
        arg->num_parsed = 0;
        arg->visited = false;
        }
    for(size_t i = 0; i < parser->keyword.count; i++){
        auto arg = &parser->keyword.args[i];
        arg->num_parsed = 0;
        arg->visited = false;
        }
    memset(&parser->failed, 0, sizeof(parser->failed));
    }
int main(int argc, char**argv){
    LongString source_path = LS("");
    LongString output_path = LS("");
    LongString dependency_path = LS("");
    LongString base_dir = LS("");
    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SOURCE_IS_PATH_NOT_DATA
        | DNDC_OUTPUT_IS_FILE_PATH_NOT_OUT_PARAM
        ;
    bool hidden_help = false;
    bool print_syntax = false;
    bool print_depends = false;
    ArgToParse pos_args[] = {
        [0] = {
            .name = SV("source"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&source_path),
            .help = "Source file (.dnd file) to read from.\n"
                    "If not given, reads from stdin.",
            },
        };
    ArgToParse kw_args[] = {
        {
            .name = SV("-o"),
            .altname1 = SV("--output"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&output_path),
            .help = "output path (.html file) to write to.\n"
                    "If not given, writes to stdout.",
            .hide_default = true,
        },
        {
            .name = SV("-d"),
            .altname1 = SV("--depends-path"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&dependency_path),
            .help = "If given, where to write a make-style dependency file.",
            .hide_default = true,
        },
        {
            .name = SV("-C"),
            .altname1 = SV("--base-directory"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&base_dir),
            .help = "Relative filepaths in source files will be relative "
                    "to the given directory.\n"
                    "If not given, everything is relative to cwd.",
            .hide_default = true,
        },
        {
            .name = SV("--report-orphans"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_REPORT_ORPHANS),
            .help = "Report orphaned nodes (for debugging scripts).",
            .hidden = true,
        },
        {
            .name = SV("--no-python"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_NO_PYTHON),
            .help = "Don't execute python nodes.",
            .hidden = true,
        },
        {
            .name = SV("--print-tree"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_PRINT_TREE),
            .help = "Print out the entire document tree.",
            .hidden = true,
        },
        {
            .name = SV("--print-links"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_PRINT_LINKS),
            .help = "Print out all links (and what they target) known by "
                    "the system.",
            .hidden = true,
        },
        {
            .name = SV("--print-syntax"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&print_syntax),
            .help = "Print out the input document with syntax highlighting.",
            .hidden = true,
        },
        {
            .name = SV("--print-stats"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_PRINT_STATS),
            .help = "Log some informative statistics.",
            .hidden = true,
        },
        {
            .name = SV("--print-depends"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&print_depends),
            .help = "Print out what paths the document depends on.",
            .hidden = true,
        },
        {
            .name = SV("--allow-bad-links"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_ALLOW_BAD_LINKS),
            .help = "Warn instead of erroring if a link can't be resolved.",
            .hidden = true,
        },
        {
            .name = SV("--suppress-warnings"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_SUPPRESS_WARNINGS),
            .help = "Don't report non-fatal errors.",
            .hidden = true,
        },
        {
            .name = SV("--dont-write"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_DONT_WRITE),
            .help = "Don't write out the document.",
            .hidden = true,
        },
        {
            .name = SV("--single-threaded"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_NO_THREADS),
            .help = "Do not create worker threads, do everything in the "
                    "same thread.",
            .hidden = true,
        },
        {
            .name = SV("--cleanup"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_NO_CLEANUP),
            .help = "Cleanup all resources (memory allocations, etc.).\n"
                    "Development debugging tool, useless in regular cli use.",
            .hidden = true,
        },
        {
            .name = SV("--use-site"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_PYTHON_UNISOLATED),
            .help = "Don't isolate python, import site, etc.\n"
                    "Greatly slows startup, but allows importing user "
                    "installed packages.",
        },
        {
            .name = SV("--format"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_REFORMAT_ONLY),
            .help = "Instead of rendering to html, render to .dnd with "
                    "trailing spaces removed, text wrapped to 80 columns "
                    "(if semantically equivalent), etc. Imports will not "
                    "be resolved - only the given input file will be "
                    "imported."
                    ,
        },
        {
            .name = SV("-H"),
            .altname1 = SV("--hidden-help"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&hidden_help),
            .help = "Print out help for the hidden arguments.",
        },
        {
            .name = SV("--dont-inline-images"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_DONT_INLINE_IMAGES),
            .help = "Instead of base64ing the images, use a link.",
            .hidden = true,
        },
        {
            .name = SV("--untrusted-input"),
            .altname1 = SV("--untrusted"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_INPUT_IS_UNTRUSTED),
            .help = "Input is untrusted and thus should not be allowed to "
                    "import files, execute scripts or embed javascript in "
                    "the output.",
            .hidden = true,
        },
        {
            .name = SV("--strip-spaces"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_STRIP_WHITESPACE),
            .help = "Strip trailing and leading whitespace from all output "
                    "lines",
            .hidden = false,
        },
        {
            .name = SV("--dont-read"),
            .min_num = 0,
            .max_num = 1,
            .dest = ArgBitFlagDest(&flags, DNDC_DONT_READ),
            .help = "Don't read any files (other than builtins and the "
                    "initial input file). Python blocks can bypass this.",
            .hidden = true,
        },
        };
    uint64_t result = 0;
    for(int i = 0; i < 10000000; i++){
        source_path = LS("");
        output_path = LS("");
        dependency_path = LS("");
        base_dir = LS("");
        flags = DNDC_FLAGS_NONE
            | DNDC_SOURCE_IS_PATH_NOT_DATA
            | DNDC_OUTPUT_IS_FILE_PATH_NOT_OUT_PARAM
            ;
        hidden_help = false;
        print_syntax = false;
        print_depends = false;
        ArgParser argparser = {
            .name = argv[0],
            .description = "A .dnd to .html parser and compiler.",
            .version = "dndc version " DNDC_VERSION ". Compiled " __TIMESTAMP__,
            .positional.args = pos_args,
            .positional.count = arrlen(pos_args),
            .keyword.args = kw_args,
            .keyword.count = arrlen(kw_args),
            };
        clear_parser(&argparser);
        Args args = argc?(Args){argc-1, (const char*const*)argv+1}: (Args){0, 0};
        if(check_for_help(&args)){
            print_help(&argparser);
            continue;
            }
        if(check_for_version(&args)){
            print_version(&argparser);
            continue;
            }
        auto e = parse_args(&argparser, &args);
        if(e){
            print_argparse_error(&argparser, e);
            // fprintf(stderr, "Use --help to see usage.\n");
            // continue;
            return e;
            }
        result += flags;
        continue;
    }
    return result;
    }
