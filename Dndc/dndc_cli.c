//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#include "compiler_warnings.h"
#if !defined(_WIN32)
#include <errno.h> // For reporting write file erors
#endif
#define DNDC_API static inline
#include "dndc.h"
#include "dndc_long_string.h"
#include "dndc_funcs.h"
#include "dndc_types.h"
#include "dndc_node_types.h"
#include "dndc_credits.h"
#include "Utils/argument_parsing.h"
#include "Utils/term_util.h"
#include "Utils/file_util.h"
#include "Utils/path_util.h"
#include "Utils/MStringBuilder.h"
#include "Utils/msb_extensions.h"
#include "Allocators/mallocator.h"
#include "Utils/gi_indent_completer.h"
#define GET_INPUT_API static inline
#include "Utils/get_input.h"
#ifdef _WIN32
#include "Platform/Windows/wincli.h"
#endif
#include <stdio.h>

// Print out syntax-highlighted version of the file to stdout.
static
void
dndc_print_out_syntax(StringView source);

// Writes out the make-style dependency file.
static
int
dndc_write_depends_file(void* user_data, size_t npaths, StringView* paths);

// Print out the dependencies instead of writing to file.
static int depends_print_callback(void*_Nullable, size_t, StringView*);


struct MainAstData {
    uint64_t flags;
    LongString output_path;
};
//
// Peek into the private ctx so we can print out links and print
// out the tree.
static
int
dndc_main_ast_func(void*_Nullable user_data, DndcContext*_Nonnull ctx);

static
void
print_file_writing_error(const char* filename, FileError err);

//
// Prints out a representation of the final document tree.
// I might remove this later, it's mostly for debugging.
// Calls itself recursively, thus the depth argument.
//
static inline
void
print_node_and_children(DndcContext*_Nonnull ctx, NodeHandle handle, int depth, FILE*);

struct DependencyUserData {
    LongString outfile;
    LongString depfile;
};

enum DndcMainFlags {
    DNDC_MAIN_NONE = 0x0,
    DNDC_MAIN_PRINT_TREE = 0x1,
    DNDC_MAIN_PRINT_LINKS = 0x2,
    DNDC_MAIN_DUMP_JSON = 0x4,
};

static
int
append_arg(void* msb_, const void* arg_){
    MStringBuilder* msb = msb_;
    const StringView* sv = arg_;
    msb_write_char(msb, msb->cursor?',':'[');
    msb_write_char(msb, '"');
    msb_write_json_escaped_str(msb, sv->text, sv->length);
    msb_write_char(msb, '"');
    return 0;
}
int
main(int argc, char**argv){
#ifdef _WIN32
    // unclear if this is actually needed or if argv is utf8.
    if(get_main_args(&argc, &argv) != 0)
        return 1;
#endif

    StringView source_path = {0};
    StringView original_source_path = {0};
    StringView source_text = {0};
    StringView output_path = SV("");
    DndcDependencyFunc* dependency_func = NULL;
    LongString dependency_path = LS("");
    struct DependencyUserData dependency_user_data = {0};
    StringView base_dir = {0};
    uint64_t ast_func_flags = DNDC_MAIN_NONE;
    uint64_t flags = DNDC_FLAGS_NONE ;
    _Bool print_syntax = 0;
    _Bool print_depends = 0;
    _Bool cleanup = 0;
    int bench_iters = 0;
    _Bool bench_cache_files = 0;
    _Bool no_null_terminator = 0;
    _Bool dont_write = 0;
    _Bool format_only = 0;
    _Bool markdown = 0;
    _Bool expand = 0;
    enum OutputTarget output_target = OUTPUT_HTML;
    LongString jsargs = LS("");
    MStringBuilder argbuilder = {.allocator = MALLOCATOR};
    {
        ArgToParse pos_args[] = {
            [0] = {
                .name = SV("source"),
                .dest = ARGDEST(&source_path),
                .help = "Source file (.dnd file) to read from. "
                        "This is not adjusted by --base-directory.\n"
                        "If not given, will read from stdin until EOF.",
                },
        };
        ArgToParse kw_args[] = {
            {
                .name = SV("-o"),
                .altname1 = SV("--output"),
                .dest = ARGDEST(&output_path),
                .help = "Output path (.html file) to write to.\n"
                        "If not given, will write to stdout.",
            },
            {
                .name = SV("-d"),
                .altname1 = SV("--depends-path"),
                .dest = ARGDEST(&dependency_path),
                .help = "If given, where to write a make-style dependency file.",
            },
            {
                .name = SV("-C"),
                .altname1 = SV("--base-directory"),
                .dest = ARGDEST(&base_dir),
                .help = "Paths in source files will be relative "
                        "to the given directory."
                        "\n"
                        "If not given, but source_path is given, "
                        "then everything is relative to the directory "
                        "that the source path is in. If that is also "
                        "not given, then everything is relative to the "
                        "current working directory."
                        "\n"
                        "NOTE: This does not affect the source "
                        "argument."
                        ,
            },
            {
                .name = SV("--no-js"),
                .dest = ArgBitFlagDest(&flags, DNDC_NO_COMPILETIME_JS),
                .help = "Don't execute js nodes.",
                .hidden = 1,
            },
            {
                .name = SV("--print-tree"),
                .dest = ArgBitFlagDest(&ast_func_flags, DNDC_MAIN_PRINT_TREE),
                .help = "Print out the entire document tree.",
                .hidden = 1,
            },
            {
                .name = SV("--json"),
                .dest = ArgBitFlagDest(&ast_func_flags, DNDC_MAIN_DUMP_JSON),
                .help = "Dump out the state of the context as json to stdout.",
                .hidden = 1,
            },
            {
                .name = SV("--print-links"),
                .dest = ArgBitFlagDest(&ast_func_flags, DNDC_MAIN_PRINT_LINKS),
                .help = "Print out all links (and what they target) known by "
                        "the system.",
                .hidden = 1,
            },
            {
                .name = SV("--print-syntax"),
                .dest = ARGDEST(&print_syntax),
                .help = "Print out the input document with syntax highlighting.",
                .hidden = 1,
            },
            {
                .name = SV("--print-stats"),
                .dest = ArgBitFlagDest(&flags, DNDC_PRINT_STATS),
                .help = "Log some informative statistics.",
                .hidden = 1,
            },
            {
                .name = SV("--print-depends"),
                .dest = ARGDEST(&print_depends),
                .help = "Print out what paths the document depends on.",
                .hidden = 1,
            },
            {
                .name = SV("--allow-bad-links"),
                .dest = ArgBitFlagDest(&flags, DNDC_ALLOW_BAD_LINKS),
                .help = "Warn instead of erroring if a link can't be resolved.",
                .hidden = 1,
            },
            {
                .name = SV("--suppress-warnings"),
                .dest = ArgBitFlagDest(&flags, DNDC_SUPPRESS_WARNINGS),
                .help = "Don't report non-fatal errors.",
                .hidden = 1,
            },
            {
                .name = SV("--dont-write"),
                .dest = ARGDEST(&dont_write),
                .help = "Don't write out the document.",
                .hidden = 1,
            },
            {
                .name = SV("--dont-import"),
                .dest = ArgBitFlagDest(&flags, DNDC_DONT_IMPORT),
                .help = "Don't import files (via #import or from import nodes), "
                        "instead leaving them as-is in the document. "
                        "Useful for breaking circular dependencies during "
                        "bootstrapping. Can also speed up introspection-only "
                        "runs.",
                .hidden = 1,
            },
            {
                .name = SV("--single-threaded"),
                .dest = ArgBitFlagDest(&flags, DNDC_NO_THREADS),
                .help = "Do not create worker threads, do everything in the "
                        "same thread.",
                .hidden = 1,
            },
            {
                .name = SV("--cleanup"),
                .dest = ARGDEST(&cleanup),
                .help = "Cleanup all resources (memory allocations, etc.).\n"
                        "Development debugging tool, useless in regular cli "
                        "use.",
                .hidden = 1,
            },
            {
                .name = SV("--format"),
                .dest = ARGDEST(&format_only),
                .help = "Instead of rendering to html, render to .dnd\n"
                        "Trailing spaces are removed, text wrapped to 80 "
                        "columns (if semantically equivalent), etc." ,
            },
            {
                .name = SV("--expand"),
                .altname1 = SV("--expand-only"),
                .dest = ARGDEST(&expand),
                .help = "Output as a single .dnd file instead of html.\n"
                        "Expansion is after resolving imports and executing "
                        "user scripts.",
                .hidden = 1,
            },
            {
                .name = SV("--md"),
                .altname1 = SV("--markdown"),
                .dest = ARGDEST(&markdown),
                .help = "Output as a single .md file instead of html.\n"
                        "Expansion is after resolving imports and executing "
                        "user scripts. This is a best effort attempt to "
                        "translate to markdown, some things will be dropped."
                        ,
                .hidden = 1,
            },
            {
                .name = SV("--dont-inline-images"),
                .dest = ArgBitFlagDest(&flags, DNDC_DONT_INLINE_IMAGES),
                .help = "Instead of base64ing the images, use a link.",
                .hidden = 1,
            },
            {
                .name = SV("--untrusted-input"),
                .altname1 = SV("--untrusted"),
                .dest = ArgBitFlagDest(&flags, DNDC_INPUT_IS_UNTRUSTED),
                .help = "Input is untrusted, so disallow some vectors.\n"
                        "Input thus should not be allowed to "
                        "import files, execute scripts or embed javascript in "
                        "the output.",
                .hidden = 1,
            },
            {
                .name = SV("--strip-spaces"),
                .dest = ArgBitFlagDest(&flags, DNDC_STRIP_WHITESPACE),
                .help = "Strip trailing and leading whitespace from all output "
                        "lines.",
                .hidden = 1,
            },
            {
                .name = SV("--dont-read"),
                .dest = ArgBitFlagDest(&flags, DNDC_DONT_READ),
                .help = "Don't read any files other than the "
                        "initial input file.",
                .hidden = 1,
            },
            {
                .name = SV("--bench-iters"),
                .dest = ARGDEST(&bench_iters),
                .help = "Execute in a repeated loop this many times.",
                .hidden = 1,
            },
            {
                .name = SV("--bench-cache-files"),
                .dest = ARGDEST(&bench_cache_files),
                .help = "Cache files while benchmarking",
                .hidden = 1,
            },
            {
                .name = SV("--fragment"),
                .altname1 = SV("--fragment-only"),
                .dest = ArgBitFlagDest(&flags, DNDC_FRAGMENT_ONLY),
                .help = "Produce an html fragment instead of a full html "
                        "document.",
                .hidden = 0,
            },
            {
                .name = SV("--disallow-attribute-directive-overlap"),
                .altname1 = SV("--dado"),
                .dest = ArgBitFlagDest(&flags, DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP),
                .help = "Error if an attribute name overlaps with a directive "
                        "name.",
                .hidden = 1,
            },
            {
                .name = SV("--allow-js-write"),
                .dest = ArgBitFlagDest(&flags, DNDC_ENABLE_JS_WRITE),
                .help = "Allow compiletime javascript to write files.",
                .hidden = 1,
            },
            {
                .name = SV("--jsargs"),
                .altname1 = SV("-J"),
                .dest = ARGDEST(&jsargs),
                .help = "A json literal that will be exposed to javascript as "
                        "Args.",
                .hidden = 1,
            },
            {
                .name = SV("--args"),
                .dest = {
                    .type = ARG_USER_DEFINED,
                    .pointer = &argbuilder,
                    // This is safe in C, but not in C++, beware.
                    .user_pointer = &(ArgParseUserDefinedType){
                        .type_name = LS("string"),
                    },
                },
                .help = "The following arguments will be appened to a js array "
                        "that will be available as Args. This overwrites any "
                        "argument given by --jsargs. Use one or the other.",
                .hidden = 1,
                .max_num = 0xffff,
                .append_proc = &append_arg,
            },
            {
                .name = SV("--no-nul-terminator"),
                .hidden = 1,
                .dest = ARGDEST(&no_null_terminator),
                .help = "Don't have a trailing nul after the input text. "
                        " For reproducing fuzzing bugs.",
            },
            {
                .name = SV("--no-css"),
                .hidden = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_NO_CSS),
                .help = "Don't output css",
            },
        };
        enum {HELP, VERSION, HIDDEN_HELP, OPEN_SOURCE, FISH};
        ArgToParse early_args[] = {
            [HELP] = {
                .name = SV("-h"),
                .altname1 = SV("--help"),
                .help = "Print this help and exit.",
            },
            [VERSION] = {
                .name = SV("-v"),
                .altname1 = SV("--version"),
                .help = "Print version information and exit.",
            },
            [HIDDEN_HELP] = {
                .name = SV("-H"),
                .altname1 = SV("--hidden-help"),
                .help = "Print out help for the hidden arguments and exit.",
            },
            [OPEN_SOURCE] = {
                .name = SV("--open-source-credits"),
                .help = "Print out attribution of open source libraries used "
                        "and exit.",
            },
            [FISH] = {
                .name = SV("--fish-completions"),
                .help = "Print out commands for fish shell completions.",
                .hidden = 1,
            },
        };
        const char* version = "dndc version " DNDC_VERSION ". Compiled "
                              __DATE__ " " __TIME__ ".";
        ArgParser argparser = {
            .name = argc? argv[0]: "dndc",
            .description = "A .dnd to .html parser and compiler.",
            .positional.args = pos_args,
            .positional.count = arrlen(pos_args),
            .keyword.args = kw_args,
            .keyword.count = arrlen(kw_args),
            .early_out.args = early_args,
            .early_out.count = arrlen(early_args),
            .styling.plain = !isatty(fileno(stdout)),
        };
        Args args = argc?(Args){argc-1, (const char*const*)argv+1}:(Args){0, 0};
        switch(check_for_early_out_args(&argparser, &args)){
            case HELP:{
                int columns = get_terminal_size().columns;
                if(columns > 80) columns = 80;
                print_argparse_help(&argparser, columns);
                return 0;
            }
            case VERSION:
                puts(version);
                return 0;
            case HIDDEN_HELP:{
                int columns = get_terminal_size().columns;
                if(columns > 80) columns = 80;
                print_argparse_hidden_help(&argparser, columns);
                return 0;
            }
            case OPEN_SOURCE:{
                int columns = get_terminal_size().columns;
                if(columns > 80) columns = 80;
                print_wrapped(DNDC_OPEN_SOURCE_CREDITS, columns);
                return 0;
            }
            case FISH:{
                print_argparse_fish_completions(&argparser);
                return 0;
            }
            default:
                break;
        }
        enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
        if(e){
            print_argparse_error(&argparser, e);
            fprintf(stderr, "Use --help to see usage.\n");
            return e;
        }
        if(expand && format_only){
            fprintf(stderr, "Do not specify both --expand and --format. "
                            "Only one is allowed\n");
            return 1;
        }
        if(expand && markdown){
            fprintf(stderr, "Do not specify both --expand and --md. "
                            "Only one is allowed\n");
            return 1;
        }
        if(format_only && markdown){
            fprintf(stderr, "Do not specify both --format and --md. "
                            "Only one is allowed\n");
            return 1;
        }
        if(expand) output_target = OUTPUT_EXPAND;
        if(markdown) output_target = OUTPUT_MD;
        if(format_only) output_target = OUTPUT_REFORMAT;
        if(!cleanup)
            flags |= DNDC_NO_CLEANUP;
        original_source_path = source_path;
        if(!base_dir.text){
            if(source_path.text){
                base_dir = path_dirname(source_path);
                source_path = path_basename(source_path);
            }
            else
                base_dir = SV("");
        }
        if(!original_source_path.text){
            source_path = SV("(stdin)");
            // read from stdin
            MStringBuilder sb = {.allocator=MALLOCATOR};
            if(isatty(fileno(stdin))){
                size_t indent = 0;
                GetInputCtx history = {.prompt = SV("> ")};
                history.tab_completion_func = indent_completer;
                for(;;){
                    ssize_t len = gi_get_input2(&history, indent);
                    if(len < 0)
                        break;
                    gi_add_line_to_history_len(&history, history.buff, len);
                    indent = 0;
                    for(ssize_t i = 0; i < len; i++){
                        if(history.buff[i] != ' ') break;
                        indent++;
                    }
                    msb_write_str(&sb, history.buff, len);
                    msb_write_char(&sb, '\n');
                }
            }
            else {
                for(;;){
                    enum {N = 4096};
                    int err = msb_ensure_additional(&sb, N);
                    if(unlikely(err)) {
                        fprintf(stderr, "OOM when allocating input buffer\n");
                        return 1;
                    }
                    char* buff = sb.data + sb.cursor;
                    size_t numread = fread(buff, 1, N, stdin);
                    sb.cursor += numread;
                    if(numread != N)
                        break;
                }
            }
            if(!sb.cursor)
                msb_write_char(&sb, ' ');
            source_text = msb_detach_sv(&sb);
        }
        else {
            Allocator allocator = MALLOCATOR;
            FileError load_err = read_file(original_source_path.text, allocator, (LongString*)&source_text);
            if(load_err.errored == FILE_IS_NOT_A_FILE){
                MStringBuilder sb = {.allocator=MALLOCATOR};
                // Use FILE until we add "read_entire_file_streamed" native api.
                FILE* fp = fopen(original_source_path.text, "rb");
                if(!fp){
                    fprintf(stderr, "Unable to read: '%s'\n", original_source_path.text);
                    return 1;
                }
                for(;;){
                    enum {N=4096};
                    int err = msb_ensure_additional(&sb, N);
                    if(unlikely(err)){
                        fprintf(stderr, "OOM when allocating input buffer\n");
                        return 1;
                    }
                    char* buff = sb.data + sb.cursor;
                    size_t numread = fread(buff, 1, N, fp);
                    sb.cursor += numread;
                    if(numread != N)
                        break;
                }
                if(!sb.cursor)
                    msb_write_char(&sb, ' ');
                source_text = msb_detach_sv(&sb);
                fclose(fp);
                load_err.errored = 0;
            }
            if(load_err.errored){
                fprintf(stderr, "Unable to read: '%s'\n", original_source_path.text);
                return 1;
            }
            if(no_null_terminator){
                // make a copy so we can remove the nul-terminator (for repro-ing
                // fuzz crashes)
                char* t = malloc(source_text.length);
                if(!t) return 1;
                memcpy(t, source_text.text, source_text.length);
                source_text.text = t;
            }
        }
        if(argbuilder.cursor){
            msb_write_char(&argbuilder, ']');
            jsargs = msb_detach_ls(&argbuilder);
        }
    }
    if(print_syntax){
        dndc_print_out_syntax(source_text);
        return 0;
    }

    if(print_depends){
        dependency_func = depends_print_callback;
    }
    else if(dependency_path.length){
        dependency_func = dndc_write_depends_file;
        dependency_user_data.depfile = dependency_path;
    }
    // We know output_path is nul-terminated as it came from argv or from a string literal
    dependency_user_data.outfile = (LongString){.text=output_path.text, .length=output_path.length};
    WorkerThread* worker = NULL;
    if(!(flags & DNDC_NO_THREADS))
        worker = (WorkerThread*)dndc_worker_thread_create();

    struct MainAstData mad = {
        .flags = ast_func_flags,
        // output_path is really a LongString, so this is fine.
        .output_path = {output_path.length, output_path.text},
    };

    if(bench_iters){
        LongString output = {0};
        flags &= ~DNDC_NO_CLEANUP;
        FileCache* b64cache = bench_cache_files?dndc_create_filecache():NULL;
        FileCache* textcache = bench_cache_files?dndc_create_filecache():NULL;
        for(int i = 0; i < bench_iters; i++){
            int e = run_the_dndc(
                output_target,
                flags,
                base_dir,
                source_text,
                source_path,
                &output,
                b64cache, textcache,
                dndc_stderr_log_func, NULL,
                dependency_func, &dependency_user_data,
                dndc_main_ast_func, &mad,
                worker,
                jsargs);
            if(e) return 1;
            assert(!e);
            if(!dont_write){
                if(output_path.length){
                    FileError write_err = write_file(output_path.text, output.text, output.length);
                    print_file_writing_error(output_path.text, write_err);
                    if(write_err.errored) return write_err.errored;
                }
                else {
                    fwrite(output.text, output.length, 1, stdout);
                }
            }
            dndc_free_string(output);
        }
        if(worker)
            dndc_worker_thread_destroy((DndcWorkerThread*)worker);
        return 0;
    }
    else {
        LongString output;
        int e = run_the_dndc(
            output_target,
            flags,
            base_dir,
            source_text,
            source_path,
            &output,
            NULL, NULL,
            dndc_stderr_log_func, NULL,
            dependency_func, &dependency_user_data,
            dndc_main_ast_func, &mad,
            worker,
            jsargs
        );
        if(e == -1) return 0;
        if(e) return e;
        if(dont_write)
            return 0;
        if(output_path.length){
            FileError write_err = write_file(output_path.text, output.text, output.length);
            print_file_writing_error(output_path.text, write_err);
            return write_err.errored;
        }
        else {
            fwrite(output.text, output.length, 1, stdout);
        }
        return 0;
    }
}


static
int
depends_print_callback(void*_Nullable unused, size_t npaths, StringView* paths){
    (void)unused;
    for(size_t i = 0; i < npaths; i++){
        StringView path = paths[i];
        printf("%.*s\n", (int)path.length, path.text);
    }
    return 0;
}

static
int
dndc_write_depends_file(void* user_data, size_t npaths, StringView* paths){
    struct DependencyUserData* ud = user_data;
    if(!ud->depfile.length || !ud->outfile.length)
        return 0;
    MStringBuilder msb = {.allocator=MALLOCATOR};
    msb_reset(&msb);
    msb_write_str(&msb, ud->outfile.text, ud->outfile.length);
    msb_write_char(&msb, ':');
    for(size_t i = 0; i < npaths; i++){
        StringView* dep = &paths[i];
        msb_write_char(&msb, ' ');
        msb_write_str(&msb, dep->text, dep->length);
    }
    msb_write_char(&msb, '\n');
    // generate empty rules so deleted files don't fail the build
    for(size_t i = 0; i < npaths; i++){
        StringView* dep = &paths[i];
        msb_write_str(&msb, dep->text, dep->length);
        msb_write_literal(&msb, ":\n");
    }
    StringView deptext = msb_borrow_sv(&msb);
    FileError write_err = write_file(ud->depfile.text, deptext.text, deptext.length);
    msb_destroy(&msb);
    if(write_err.errored){
        print_file_writing_error(ud->depfile.text, write_err);
        return write_err.errored;
    }
    return 0;
}
static inline
void
ctx_to_json(DndcContext*, MStringBuilder*);

static
int
dndc_main_ast_func(void*_Nullable user_data, DndcContext*_Nonnull ctx){
    struct MainAstData* data = user_data;
    uint64_t flags = data->flags;
    if(!flags) return 0;
    FILE* fp;
    if(data->output_path.length){
        fp = fopen(data->output_path.text, "wb");
        if(!fp) return DNDC_ERROR_USER_ERROR;
    }
    else
        fp = stdout;
    if(flags & DNDC_MAIN_PRINT_TREE){
        print_node_and_children(ctx, ctx->root_handle, 0, fp);
        return -1;
    }
    if(flags & DNDC_MAIN_PRINT_LINKS){
        size_t print_idx = 0;
        StringView2* items = string_table_items(&ctx->links);
        for(size_t i = 0; i < ctx->links.count_; i++){
            StringView k = items[i].key;
            StringView v = items[i].value;
            fprintf(fp, "[%zu] key: '%.*s', value: '%.*s'\n", print_idx++, (int)k.length, k.text, (int)v.length, v.text);
        }
        if(data->output_path.length) fclose(fp);
        return -1;
    }
    if(flags & DNDC_MAIN_DUMP_JSON){
        MStringBuilder sb = {.allocator=MALLOCATOR};
        ctx_to_json(ctx, &sb);
        fputs(msb_borrow_ls(&sb).text, fp);
        msb_destroy(&sb);
        if(data->output_path.length) fclose(fp);
        return -1;
    }
    return 0;
}

static inline
void
print_node_and_children(DndcContext* ctx, NodeHandle handle, int depth, FILE* out){
    Node* node = get_node(ctx, handle);
    for(int i = 0 ; i < depth*2; i++){
        fputc(' ', out);
    }
    fprintf(out, "[%-8s]", NODENAMES[node->type].text);
    switch((NodeType)node->type){
        case NODE_PARA:
        case NODE_TABLE_ROW:
        case NODE_LIST_ITEM:
        case NODE_KEYVALUEPAIR:
            break;
        case NODE_META:
        case NODE_HEAD:
        case NODE_RAW:
        case NODE_PRE:
        case NODE_JS:
        case NODE_BULLETS:
        case NODE_STYLESHEETS:
        case NODE_LINKS:
        case NODE_SCRIPTS:
        case NODE_IMPORT:
        case NODE_IMAGE:
        case NODE_TABLE:
        case NODE_TITLE:
        case NODE_HEADING:
        case NODE_LIST:
        case NODE_COMMENT:
        case NODE_TOC:
        case NODE_KEYVALUE:
        case NODE_IMGLINKS:
        case NODE_DEFLIST:
        case NODE_DEF:
        case NODE_DETAILS:
        case NODE_MD:
        case NODE_CONTAINER:
        case NODE_INVALID:
        case NODE_QUOTE:
        case NODE_DIV:{
            fprintf(out," '%.*s' ", (int)node->header.length, node->header.text);
            RARRAY_FOR_EACH(StringView, c, node->classes){
                fprintf(out, ".%.*s ", (int)c->length, c->text);
            }
            if(node->attributes){
                StringView2* items = AttrTable_items(node->attributes);
                size_t count = node->attributes->count;
                for(size_t i = 0; i < count; i++){
                    StringView2* a = items+i;
                    if(!a->key.length) continue;
                    fprintf(out, "@%.*s", (int)a->key.length, a->key.text);
                    if(a->value.length)
                        fprintf(out, "(%.*s) ", (int)a->value.length, a->value.text);
                    else
                        fputc(' ', out);
                }
            }
        }break;
        case NODE_STRING:{
            fprintf(out, " '%.*s'", (int)node->header.length, node->header.text);
        }break;
        case NODE_SHEBANG:{
            fprintf(out, "%.*s", (int)node->header.length, node->header.text);
        }break;
    }
    fputc('\n', out);
    NODE_CHILDREN_FOR_EACH(it, node){
        print_node_and_children(ctx, *it, depth+1, out);
    }
}

static
void
dndc_syntax_func(void* _Nullable data, int type, int line, int col, const char* begin, size_t length){
    (void)line;
    (void)col;
    const char** where = data;
    if(begin != *where){
        assert(*where < begin);
        fwrite(*where, 1, begin - *where, stdout);
    }
    const char* gray    = "\033[97m";
    const char* blue    = "\033[94m";
    const char* green   = "\033[92m";
    const char* red     = "\033[91m";
    const char* yellow  = "\033[93m";
    const char* magenta = "\033[95m";
    const char* cyan    = "\033[96m";
    const char* white   = "\033[37m";
    const char* reset   = "\033[0;23;39;49m";
    const char* bold    = "\033[1m";
    const char* italic  = "\033[3m";
    const char* brightgreen = "\033[38;5;121m";
    switch((enum DndcSyntax)type){
        case DNDC_SYNTAX_NONE:
            break;
        case DNDC_SYNTAX_DOUBLE_COLON:
            fputs(gray, stdout);
            break;
        case DNDC_SYNTAX_HEADER:
            fputs(blue, stdout);
            break;
        case DNDC_SYNTAX_NODE_TYPE:
            fputs(red, stdout);
            break;
        case DNDC_SYNTAX_DIRECTIVE:
            fputs(yellow, stdout);
            break;
        case DNDC_SYNTAX_ATTRIBUTE:
            fputs(white, stdout);
            break;
        case DNDC_SYNTAX_ATTRIBUTE_ARGUMENT:
            fputs(magenta, stdout);
            break;
        case DNDC_SYNTAX_CLASS:
            fputs(cyan, stdout);
            break;
        case DNDC_SYNTAX_JS_COMMENT:
            fputs(gray, stdout);
            break;
        case DNDC_SYNTAX_JS_STRING:
            fputs(italic, stdout);
            break;
        case DNDC_SYNTAX_JS_REGEX:
            fputs(red, stdout);
            fputs(italic, stdout);
            break;
        case DNDC_SYNTAX_JS_KEYWORD_VALUE:
        case DNDC_SYNTAX_JS_NUMBER:
            fputs(green, stdout);
            break;
        case DNDC_SYNTAX_JS_KEYWORD:
            fputs(cyan, stdout);
            break;
        case DNDC_SYNTAX_JS_IDENTIFIER:
            // fputs(magenta, stdout);
            break;
        case DNDC_SYNTAX_JS_BUILTIN:
            fputs(bold, stdout);
            // fputs(blue, stdout);
            break;
        case DNDC_SYNTAX_JS_NODETYPE:
            fputs(bold, stdout);
            break;
        case DNDC_SYNTAX_JS_BRACE:
            fputs(blue, stdout);
            break;
        case DNDC_SYNTAX_JS_VAR:
            fputs(brightgreen, stdout);
            break;
        // case DNDC_SYNTAX_BULLET:
            // break;
        // case DNDC_SYNTAX_COMMENT:
            // break;
        case DNDC_SYNTAX_RAW_STRING:
            fputs(green, stdout);
            break;
    }
    fwrite(begin, 1, length, stdout);
    *where = begin + length;
    fputs(reset, stdout);
}

static
void
dndc_print_out_syntax(StringView source_text){
    const char* where = source_text.text;
    dndc_analyze_syntax(source_text, dndc_syntax_func, &where);
    if(where != source_text.text+source_text.length){
        fwrite(where, 1, (source_text.text+source_text.length) - where, stdout);
    }
}

static
void
print_file_writing_error(const char* filename, FileError err){
    #if !defined(_WIN32)
    switch(err.errored){
        case FILE_NOT_OPENED:
            fprintf(stderr, "Failed to open '%s' for writing: %s\n", filename, strerror(err.native_error));
            return;
        case FILE_ERROR:
            fprintf(stderr, "Error when writing to  '%s': %s\n", filename, strerror(err.native_error));
            return;
        default:
            return;
    }
    #else
    char errbuff[4096];
    if(err.errored){
        DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM
                       // "when you are not in control of the message, you
                       // had better pass the FORMAT_MESSAGE_IGNORE_INSERTS
                       // flag"  - Raymond Chen
                       | FORMAT_MESSAGE_IGNORE_INSERTS
                       ;
        FormatMessageA(flags, NULL, err.native_error, 0, errbuff, sizeof errbuff, NULL);
    }
    switch(err.errored){
        case FILE_NOT_OPENED:
            fprintf(stderr, "Failed to open '%s' for writing: %s\n", filename, errbuff);
            return;
        case FILE_ERROR:
            fprintf(stderr, "Error when writing to  '%s': %s\n", filename, errbuff);
            return;
        default:
            return;
    }
    #endif
}

#include "dndc.c"
#include "Utils/get_input.c"
