#if !defined(_WIN32)
#include <errno.h> // For reporting write file erors
#endif
#define NO_DNDC_AST_API
#define DNDC_API static inline
#include "dndc.h"
#include "dndc_long_string.h"
#include "dndc_funcs.h"
#include "dndc_types.h"
#include "dndc_node_types.h"
#include "dndc_credits.h"
#include "argument_parsing.h"
#include "term_util.h"
#include "file_util.h"
#include "path_util.h"
#include "MStringBuilder.h"
#include "msb_extensions.h"
#include "mallocator.h"
#include "gi_indent_completer.h"
#define GET_INPUT_API static inline
#include "get_input.h"

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

//
// Peek into the private ctx so we can print out links and print
// out the tree.
static
int
dndc_main_ast_func(void*_Nullable user_data, DndcContext*_Nonnull ctx);

static
void
print_file_writing_error(const char* filename, FileWriteResult err);

//
// Prints out a representation of the final document tree.
// I might remove this later, it's mostly for debugging.
// Calls itself recursively, thus the depth argument.
//
static inline
void
print_node_and_children(Nonnull(DndcContext*), NodeHandle handle, int depth);

struct DependencyUserData {
    LongString outfile;
    LongString depfile;
};

enum DndcMainFlags {
    DNDC_MAIN_NONE = 0x0,
    DNDC_MAIN_PRINT_TREE = 0x1,
    DNDC_MAIN_PRINT_LINKS = 0x2,
};

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
    StringView source_path = {0};
    StringView source_text = {0};
    StringView output_path = SV("");
    DndcDependencyFunc* dependency_func = NULL;
    LongString dependency_path = LS("");
    struct DependencyUserData dependency_user_data = {0};
    StringView base_dir = {0};
    uint64_t ast_func_flags = DNDC_MAIN_NONE;
    uint64_t flags = DNDC_FLAGS_NONE ;
    bool print_syntax = false;
    bool print_depends = false;
    bool cleanup = false;
    int bench_iters = 0;
    bool bench_cache_files = false;
    LongString jsargs = LS("");
    MStringBuilder argbuilder = {.allocator = get_mallocator()};
    {
        ArgToParse pos_args[] = {
            [0] = {
                .name = SV("source"),
                .dest = ARGDEST(&source_path),
                .help = "Source file (.dnd file) to read from.\n"
                        "If not given, will read from stdin.",
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
                        "to the given directory.\n"
                        "If not given, but source_path is given, "
                        "then everything is relative to the directory "
                        "that the source path is in. If that is also "
                        "not given, then everything is relative to the "
                        "current working directory.",
            },
            {
                .name = SV("--no-js"),
                .dest = ArgBitFlagDest(&flags, DNDC_NO_COMPILETIME_JS),
                .help = "Don't execute js nodes.",
                .hidden = true,
            },
            {
                .name = SV("--print-tree"),
                .dest = ArgBitFlagDest(&ast_func_flags, DNDC_MAIN_PRINT_TREE),
                .help = "Print out the entire document tree.",
                .hidden = true,
            },
            {
                .name = SV("--print-links"),
                .dest = ArgBitFlagDest(&ast_func_flags, DNDC_MAIN_PRINT_LINKS),
                .help = "Print out all links (and what they target) known by "
                        "the system.",
                .hidden = true,
            },
            {
                .name = SV("--print-syntax"),
                .dest = ARGDEST(&print_syntax),
                .help = "Print out the input document with syntax highlighting.",
                .hidden = true,
            },
            {
                .name = SV("--print-stats"),
                .dest = ArgBitFlagDest(&flags, DNDC_PRINT_STATS),
                .help = "Log some informative statistics.",
                .hidden = true,
            },
            {
                .name = SV("--print-depends"),
                .dest = ARGDEST(&print_depends),
                .help = "Print out what paths the document depends on.",
                .hidden = true,
            },
            {
                .name = SV("--allow-bad-links"),
                .dest = ArgBitFlagDest(&flags, DNDC_ALLOW_BAD_LINKS),
                .help = "Warn instead of erroring if a link can't be resolved.",
                .hidden = true,
            },
            {
                .name = SV("--suppress-warnings"),
                .dest = ArgBitFlagDest(&flags, DNDC_SUPPRESS_WARNINGS),
                .help = "Don't report non-fatal errors.",
                .hidden = true,
            },
            {
                .name = SV("--dont-write"),
                .dest = ArgBitFlagDest(&flags, DNDC_DONT_WRITE),
                .help = "Don't write out the document.",
                .hidden = true,
            },
            {
                .name = SV("--dont-import"),
                .dest = ArgBitFlagDest(&flags, DNDC_DONT_IMPORT),
                .help = "Don't import files (via #import or from import nodes), "
                        "instead leaving them as-is in the document. "
                        "Useful for breaking circular dependencies during bootstrapping. "
                        "Can also speed up introspection-only runs.",
                .hidden = true,
            },
            {
                .name = SV("--single-threaded"),
                .dest = ArgBitFlagDest(&flags, DNDC_NO_THREADS),
                .help = "Do not create worker threads, do everything in the "
                        "same thread.",
                .hidden = true,
            },
            {
                .name = SV("--cleanup"),
                .dest = ARGDEST(&cleanup),
                .help = "Cleanup all resources (memory allocations, etc.).\n"
                        "Development debugging tool, useless in regular cli use.",
                .hidden = true,
            },
            {
                .name = SV("--format"),
                .dest = ArgBitFlagDest(&flags, DNDC_REFORMAT_ONLY),
                .help = "Instead of rendering to html, render to .dnd\n"
                        "Trailing spaces are removed, text wrapped to 80 columns "
                        "(if semantically equivalent), etc." ,
            },
            {
                .name = SV("--expand"),
                .altname1 = SV("--expand-only"),
                .dest = ArgBitFlagDest(&flags, DNDC_OUTPUT_EXPANDED_DND),
                .help = "Output as a single .dnd file instead of html.\n"
                        "Expansion is after resolving imports and executing user  "
                        "scripts.",
                .hidden = true,
            },
            {
                .name = SV("--dont-inline-images"),
                .dest = ArgBitFlagDest(&flags, DNDC_DONT_INLINE_IMAGES),
                .help = "Instead of base64ing the images, use a link.",
                .hidden = true,
            },
            {
                .name = SV("--untrusted-input"),
                .altname1 = SV("--untrusted"),
                .dest = ArgBitFlagDest(&flags, DNDC_INPUT_IS_UNTRUSTED),
                .help = "Input is untrusted, so disallow some vectors.\n"
                        "Input thus should not be allowed to "
                        "import files, execute scripts or embed javascript in "
                        "the output.",
                .hidden = true,
            },
            {
                .name = SV("--strip-spaces"),
                .dest = ArgBitFlagDest(&flags, DNDC_STRIP_WHITESPACE),
                .help = "Strip trailing and leading whitespace from all output "
                        "lines.",
                .hidden = true,
            },
            {
                .name = SV("--dont-read"),
                .dest = ArgBitFlagDest(&flags, DNDC_DONT_READ),
                .help = "Don't read any files other than the "
                        "initial input file.",
                .hidden = true,
            },
            {
                .name = SV("--bench-iters"),
                .dest = ARGDEST(&bench_iters),
                .help = "Execute in a repeated loop this many times.",
                .hidden = true,
            },
            {
                .name = SV("--bench-cache-files"),
                .dest = ARGDEST(&bench_cache_files),
                .help = "Cache files while benchmarking",
                .hidden = true,
            },
            {
                .name = SV("--fragment"),
                .altname1 = SV("--fragment-only"),
                .dest = ArgBitFlagDest(&flags, DNDC_FRAGMENT_ONLY),
                .help = "Produce an html fragment instead of a full html document.",
                .hidden = false,
            },
            {
                .name = SV("--disallow-attribute-directive-overlap"),
                .altname1 = SV("--dado"),
                .dest = ArgBitFlagDest(&flags, DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP),
                .help = "Error if an attribute name overlaps with a directive name.",
                .hidden = true,
            },
            {
                .name = SV("--allow-js-write"),
                .dest = ArgBitFlagDest(&flags, DNDC_ENABLE_JS_WRITE),
                .help = "Allow compiletime javascript to write files.",
                .hidden = true,
            },
            {
                .name = SV("--jsargs"),
                .altname1 = SV("-J"),
                .dest = ARGDEST(&jsargs),
                .help = "A json literal that will be exposed to javascript as "
                        "Args.",
                .hidden = true,
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
                .help = "The following arguments will be appened to a js array that will "
                        "be available as Args. This overwrites any argument given by --jsargs. "
                        "Use one or the other.",
                .hidden = true,
                .max_num = 0xffff,
                .append_proc = &append_arg,
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
                .help = "Print out attribution of open source libraries used and exit.",
            },
            [FISH] = {
                .name = SV("--fish-completions"),
                .help = "Print out commands for fish shell completions.",
                .hidden = true,
            },
        };
        const char* version = "dndc version " DNDC_VERSION ". Compiled " __DATE__ " " __TIME__ ".";
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
        Args args = argc?(Args){argc-1, (const char*const*)argv+1}: (Args){0, 0};
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
        if((flags & DNDC_OUTPUT_EXPANDED_DND) && (flags & DNDC_REFORMAT_ONLY)){
            fprintf(stderr, "Do not specify both --expand and --format. Only one is allowed\n");
            return 1;
        }
        if(!cleanup)
            flags |= DNDC_NO_CLEANUP;
        if(!base_dir.text){
            if(source_path.text)
                base_dir = path_dirname(source_path);
            else
                base_dir = SV("");
        }
        if(!source_path.text){
            source_path = SV("(stdin)");
            // read from stdin
            MStringBuilder sb = {.allocator=get_mallocator()};
            if(isatty(fileno(stdin))){
                GetInputCtx history = {.prompt = SV("> ")};
                history.tab_completion_func = indent_completer;
                for(;;){
                    ssize_t len = gi_get_input(&history);
                    if(len < 0)
                        break;
                    gi_add_line_to_history_len(&history, history.buff, len);
                    msb_write_str(&sb, history.buff, len);
                    msb_write_char(&sb, '\n');
                }
                puts("^D");
            }
            else {
                for(;;){
                    enum {N = 4096};
                    msb_ensure_additional(&sb, N);
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
            Allocator allocator = get_mallocator();
            TextFileResult load_err = read_file(source_path.text, allocator);
            if(load_err.errored){
                fprintf(stderr, "Unable to read: '%s'\n", source_path.text);
                return 1;
            }
            source_text = LS_to_SV(load_err.result);
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

    if(bench_iters){
        LongString output = {0};
        flags &= ~DNDC_NO_CLEANUP;
        FileCache* b64cache = bench_cache_files?dndc_create_filecache():NULL;
        FileCache* textcache = bench_cache_files?dndc_create_filecache():NULL;
        for(int i = 0; i < bench_iters; i++){
            int e = run_the_dndc(
                flags,
                base_dir,
                source_text,
                source_path,
                &output,
                b64cache, textcache,
                dndc_stderr_error_func, NULL,
                dependency_func, &dependency_user_data,
                dndc_main_ast_func, (void*)(uintptr_t)ast_func_flags,
                worker,
                jsargs);
            assert(!e);
            dndc_free_string(output);
        }
        if(worker)
            dndc_worker_thread_destroy((DndcWorkerThread*)worker);
        return 0;
    }
    else {
        LongString output;
        int e = run_the_dndc(
            flags,
            base_dir,
            source_text,
            source_path,
            &output,
            NULL, NULL,
            dndc_stderr_error_func, NULL,
            dependency_func, &dependency_user_data,
            dndc_main_ast_func, (void*)(uintptr_t)ast_func_flags,
            worker,
            jsargs
            );
        if(e) return e;
        if(flags & DNDC_DONT_WRITE)
            return 0;
        if(output_path.length){
            FileWriteResult write_err = write_file(output_path.text, output.text, output.length);
            print_file_writing_error(output_path.text, write_err);
            return write_err.errored;
        }
        else {
            puts(output.text);
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
    MStringBuilder msb = {.allocator=get_mallocator()};
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
    FileWriteResult write_err = write_file(ud->depfile.text, deptext.text, deptext.length);
    msb_destroy(&msb);
    if(write_err.errored){
        print_file_writing_error(ud->depfile.text, write_err);
        return write_err.errored;
    }
    return 0;
}

static
int
dndc_main_ast_func(void*_Nullable user_data, DndcContext*_Nonnull ctx){
    uint64_t flags = (uintptr_t)user_data;
    if(flags & DNDC_MAIN_PRINT_TREE){
        print_node_and_children(ctx, ctx->root_handle, 0);
    }
    if(flags & DNDC_MAIN_PRINT_LINKS){
        size_t print_idx = 0;
        for(size_t i = 0; i < ctx->links.capacity_; i++){
            StringView k = ctx->links.keys[i];
            if(!k.length) continue;
            StringView v = ctx->links.keys[i+ctx->links.capacity_];
            fprintf(stderr, "[%zu] key: '%.*s', value: '%.*s'\n", print_idx++, (int)k.length, k.text, (int)v.length, v.text);
        }
    }
    return 0;
}

static inline
void
print_node_and_children(DndcContext* ctx, NodeHandle handle, int depth){
    Node* node = get_node(ctx, handle);
    for(int i = 0 ; i < depth*2; i++){
        putchar(' ');
    }
    printf("[%-8s]", NODENAMES[node->type].text);
    switch((NodeType)node->type){
        case NODE_PARA:
        case NODE_TABLE_ROW:
        case NODE_LIST_ITEM:
        case NODE_KEYVALUEPAIR:
            break;
        case NODE_META:
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
        case NODE_DATA:
        case NODE_TOC:
        case NODE_KEYVALUE:
        case NODE_IMGLINKS:
        case NODE_DETAILS:
        case NODE_MD:
        case NODE_CONTAINER:
        case NODE_INVALID:
        case NODE_QUOTE:
        case NODE_HR:
        case NODE_DIV:{
            printf(" '%.*s' ", (int)node->header.length, node->header.text);
            RARRAY_FOR_EACH(StringView, c, node->classes){
                printf(".%.*s ", (int)c->length, c->text);
            }
            RARRAY_FOR_EACH(Attribute, a, node->attributes){
                printf("@%.*s", (int)a->key.length, a->key.text);
                if(a->value.length)
                    printf("(%.*s) ", (int)a->value.length, a->value.text);
                else
                    putchar(' ');
            }
        }break;
        case NODE_STRING:{
            printf(" '%.*s'", (int)node->header.length, node->header.text);
        }break;
    }
    putchar('\n');
    NODE_CHILDREN_FOR_EACH(it, node){
        print_node_and_children(ctx, *it, depth+1);
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
print_file_writing_error(const char* filename, FileWriteResult err){
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
    char errbuff[4192];
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
#include "get_input.c"
