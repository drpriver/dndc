#define DNDC_API static inline
#include "dndc.h"
#include "dndc_long_string.h"
#include "dndc_funcs.h"
#include "dndc_types.h"
#include "dndc_node_types.h"
#include "argument_parsing.h"
#include "term_util.h"
#include "file_util.h"
#include "MStringBuilder.h"
#define GET_INPUT_API static inline
#include "get_input.h"

static
void
dndc_print_out_syntax(LongString source_path);

static
int
dndc_write_depends_file(void* user_data, size_t npaths, StringView* paths);

static
int
dndc_main_ast_func(void*_Nullable user_data, DndcContext*_Nonnull ctx);

static int depends_print_callback(void*_Nullable, size_t, StringView*);

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
main(int argc, char**argv){
    LongString source_path = {};
    LongString output_path = LS("");
    DndcDependencyFunc* dependency_func = NULL;
    LongString dependency_path = LS("");
    struct DependencyUserData dependency_user_data = {};
    LongString base_dir = LS("");
    uint64_t ast_func_flags = DNDC_MAIN_NONE;
    uint64_t flags = DNDC_FLAGS_NONE ;
    bool print_syntax = false;
    bool print_depends = false;
    bool cleanup = false;
    {
        ArgToParse pos_args[] = {
            [0] = {
                .name = SV("source"),
                .max_num = 1,
                .dest = ARGDEST(&source_path),
                .help = "Source file (.dnd file) to read from.\n"
                        "If not given, will read from stdin.",
                },
            };
        ArgToParse kw_args[] = {
            {
                .name = SV("-o"),
                .altname1 = SV("--output"),
                .max_num = 1,
                .dest = ARGDEST(&output_path),
                .help = "Output path (.html file) to write to.\n"
                        "If not given, will write to stdout.",
            },
            {
                .name = SV("-d"),
                .altname1 = SV("--depends-path"),
                .max_num = 1,
                .dest = ARGDEST(&dependency_path),
                .help = "If given, where to write a make-style dependency file.",
            },
            {
                .name = SV("-C"),
                .altname1 = SV("--base-directory"),
                .max_num = 1,
                .dest = ARGDEST(&base_dir),
                .help = "Paths in source files will be relative "
                        "to the given directory.\n"
                        "If not given, everything is relative to cwd.",
            },
            {
                .name = SV("--no-python"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_NO_PYTHON),
                .help = "Don't execute python nodes.",
                .hidden = true,
            },
            {
                .name = SV("--no-js"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_NO_COMPILETIME_JS),
                .help = "Don't execute js nodes.",
                .hidden = true,
            },
            {
                .name = SV("--print-tree"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&ast_func_flags, DNDC_MAIN_PRINT_TREE),
                .help = "Print out the entire document tree.",
                .hidden = true,
            },
            {
                .name = SV("--print-links"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&ast_func_flags, DNDC_MAIN_PRINT_LINKS),
                .help = "Print out all links (and what they target) known by "
                        "the system.",
                .hidden = true,
            },
            {
                .name = SV("--print-syntax"),
                .max_num = 1,
                .dest = ARGDEST(&print_syntax),
                .help = "Print out the input document with syntax highlighting.",
                .hidden = true,
            },
            {
                .name = SV("--print-stats"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_PRINT_STATS),
                .help = "Log some informative statistics.",
                .hidden = true,
            },
            {
                .name = SV("--print-depends"),
                .max_num = 1,
                .dest = ARGDEST(&print_depends),
                .help = "Print out what paths the document depends on.",
                .hidden = true,
            },
            {
                .name = SV("--allow-bad-links"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_ALLOW_BAD_LINKS),
                .help = "Warn instead of erroring if a link can't be resolved.",
                .hidden = true,
            },
            {
                .name = SV("--suppress-warnings"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_SUPPRESS_WARNINGS),
                .help = "Don't report non-fatal errors.",
                .hidden = true,
            },
            {
                .name = SV("--dont-write"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_DONT_WRITE),
                .help = "Don't write out the document.",
                .hidden = true,
            },
            {
                .name = SV("--single-threaded"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_NO_THREADS),
                .help = "Do not create worker threads, do everything in the "
                        "same thread.",
                .hidden = true,
            },
            {
                .name = SV("--cleanup"),
                .max_num = 1,
                .dest = ARGDEST(&cleanup),
                .help = "Cleanup all resources (memory allocations, etc.).\n"
                        "Development debugging tool, useless in regular cli use.",
                .hidden = true,
            },
            {
                .name = SV("--use-site"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_PYTHON_UNISOLATED),
                .help = "Don't isolate python, import site, etc.\n"
                        "Greatly slows startup, but allows importing user "
                        "installed packages.",
                .hidden = true,
            },
            {
                .name = SV("--format"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_REFORMAT_ONLY),
                .help = "Instead of rendering to html, render to .dnd with "
                        "trailing spaces removed, text wrapped to 80 columns "
                        "(if semantically equivalent), etc." ,
            },
            {
                .name = SV("--expand"),
                .altname1 = SV("--expand-only"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_OUTPUT_EXPANDED_DND),
                .help = "After resolving imports and executing user scripts, "
                        "output as a single file .dnd file instead of html.",
                .hidden = true,
            },
            {
                .name = SV("--dont-inline-images"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_DONT_INLINE_IMAGES),
                .help = "Instead of base64ing the images, use a link.",
                .hidden = true,
            },
            {
                .name = SV("--untrusted-input"),
                .altname1 = SV("--untrusted"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_INPUT_IS_UNTRUSTED),
                .help = "Input is untrusted and thus should not be allowed to "
                        "import files, execute scripts or embed javascript in "
                        "the output.",
                .hidden = true,
            },
            {
                .name = SV("--strip-spaces"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_STRIP_WHITESPACE),
                .help = "Strip trailing and leading whitespace from all output "
                        "lines.",
                .hidden = true,
            },
            {
                .name = SV("--dont-read"),
                .max_num = 1,
                .dest = ArgBitFlagDest(&flags, DNDC_DONT_READ),
                .help = "Don't read any files (other than builtins and the "
                        "initial input file). Python blocks can bypass this.",
                .hidden = true,
            },
            };
        enum {HELP, VERSION, HIDDEN_HELP};
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
        };
        const char* version = "dndc version " DNDC_VERSION ". Compiled " __TIMESTAMP__;
        ArgParser argparser = {
            .name = argv[0],
            .description = "A .dnd to .html parser and compiler.",
            .positional.args = pos_args,
            .positional.count = arrlen(pos_args),
            .keyword.args = kw_args,
            .keyword.count = arrlen(kw_args),
            .early_out.args = early_args,
            .early_out.count = arrlen(early_args),
            };
        Args args = argc?(Args){argc-1, (const char*const*)argv+1}: (Args){0, 0};
        switch(check_for_early_out_args(&argparser, &args)){
            case HELP:{
                int columns = get_terminal_size().columns;
                if(columns > 80)
                    columns = 80;
                print_argparse_help(&argparser, columns);
                putchar('\n');
                return 0;
                }
            case VERSION:
                puts(version);
                return 0;
            case HIDDEN_HELP:{
                fputs(
                    "Hidden Arguments:\n"
                    "-----------------", stdout);
                int columns = get_terminal_size().columns;
                if(columns > 80)
                    columns = 80;
                for(int i = 0; i < arrlen(kw_args); i++){
                    auto arg = &kw_args[i];
                    if(!arg->hidden){
                        continue;
                        }
                    putchar('\n');
                    print_arg_help(arg, columns);
                    }
                return 0;
                }
            default:
                break;
            }
        auto e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
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
        if(!source_path.text){
            // read from stdin
            MStringBuilder sb = {.allocator=get_mallocator()};
            if(isatty(fileno(stdin))){
                char buff[4096];
                struct LineHistory history = {};
                for(;;){
                    ssize_t len = get_input_line(&history, LS("> "), buff, sizeof(buff));
                    if(len < 0)
                        break;
                    add_line_to_history(&history, (LongString){.length=len, .text=buff});
                    msb_write_str(&sb, buff, len);
                    msb_write_char(&sb, '\n');
                    }
                puts("^D");
                }
            else {
                for(;;){
                    enum {N = 4096};
                    msb_ensure_additional(&sb, N);
                    char* buff = sb.data + sb.cursor;
                    auto numread = fread(buff, 1, N, stdin);
                    sb.cursor += numread;
                    if(numread != N)
                        break;
                    }
                }
            if(!sb.cursor)
                msb_write_char(&sb, ' ');
            source_path = msb_detach(&sb);
            }
        else {
            flags |= DNDC_SOURCE_IS_PATH_NOT_DATA;
            }
    }
    if(print_syntax){
        dndc_print_out_syntax(source_path);
        return 0;
    }

    if(print_depends){
        dependency_func = depends_print_callback;
        }
    else if(dependency_path.length){
        dependency_func = dndc_write_depends_file;
        dependency_user_data.depfile = dependency_path;
        }
    dependency_user_data.outfile = output_path;
    WorkerThread* worker = NULL;
    if(!(flags & DNDC_NO_THREADS))
        worker = (WorkerThread*)dndc_worker_thread_create();

    #ifdef BENCHMARKING
    flags &= ~DNDC_NO_CLEANUP;
    LongString output = {};
    auto e = run_the_dndc(flags,
                base_dir,
                source_path,
                output_path,
                &output,
                NULL, NULL,
                dndc_stderr_error_func, NULL,
                dependency_func, &dependency_user_data,
                dndc_main_ast_func, (void*)(uintptr_t)ast_func_flags,
                worker);

    assert(!e.errored);
    dndc_free_string(output);
    flags |= DNDC_PYTHON_IS_INIT;
    for(int i = 0; i < BENCHMARKITERS; i++){
        e = run_the_dndc(flags,
                base_dir,
                source_path,
                output_path,
                &output,
                NULL, NULL,
                dndc_stderr_error_func, NULL,
                dependency_func, &dependency_user_data,
                dndc_main_ast_func, (void*)(uintptr_t)ast_func_flags,
                worker);
        assert(!e.errored);
        }
    dndc_free_string(output);
    end_interpreter();
    return 0;
    #else
    LongString output;
    auto e = run_the_dndc(flags,
                 base_dir,
                 source_path,
                 output_path,
                 &output,
                 NULL, NULL,
                 dndc_stderr_error_func, NULL,
                 dependency_func, &dependency_user_data,
                 dndc_main_ast_func, (void*)(uintptr_t)ast_func_flags,
                 worker
                 );
    if(e.errored) return e.errored;
    if(flags & DNDC_DONT_WRITE)
        return 0;
    if(output_path.length){
        auto write_err = write_file(output_path.text, output.text, output.length);
        if(write_err){
            // TODO: retrieve platform specific error message.
            fprintf(stderr, "Failed to write to output path: %s\n", output_path.text);
            return write_err;
            }
        }
    else {
        puts(output.text);
        }
    return 0;
    #endif
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
        auto dep = &paths[i];
        msb_write_char(&msb, ' ');
        msb_write_str(&msb, dep->text, dep->length);
        }
    msb_write_char(&msb, '\n');
    // generate empty rules so deleted files don't fail the build
    for(size_t i = 0; i < npaths; i++){
        auto dep = &paths[i];
        msb_write_str(&msb, dep->text, dep->length);
        msb_write_literal(&msb, ":\n");
        }
    auto deptext = msb_borrow(&msb);
    auto write_err = write_file(ud->depfile.text, deptext.text, deptext.length);
    msb_destroy(&msb);
    if(write_err){
        perror("Error on write");
        return write_err;
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
        for(size_t i = 0; i < ctx->links.count; i++){
            auto li = &ctx->links.data[i];
            fprintf(stderr, "[%zu] key: '%.*s', value: '%.*s'\n", i, (int)li->key.length, li->key.text, (int)li->value.length, li->value.text);
            }
        }
    return 0;
    }

static inline
void
print_node_and_children(DndcContext* ctx, NodeHandle handle, int depth){
    auto node = get_node(ctx, handle);
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
        case NODE_RAW:
        case NODE_PRE:
        case NODE_PYTHON:
        case NODE_JS:
        case NODE_BULLETS:
        case NODE_STYLESHEETS:
        case NODE_DEPENDENCIES:
        case NODE_LINKS:
        case NODE_SCRIPTS:
        case NODE_IMPORT:
        case NODE_IMAGE:
        case NODE_TABLE:
        case NODE_TEXT:
        case NODE_TITLE:
        case NODE_HEADING:
        case NODE_LIST:
        case NODE_COMMENT:
        case NODE_DATA:
        case NODE_NAV:
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
            RARRAY_FOR_EACH(c, node->classes){
                printf(".%.*s ", (int)c->length, c->text);
                }
            RARRAY_FOR_EACH(a, node->attributes){
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
        fwrite(*where, 1, begin - *where, stdout);
        }
    const char* gray    = "\033[97m";
    const char* blue    = "\033[94m";
    const char* green   = "\033[92m";
    const char* red     = "\033[91m";
    // const char* yellow  = "\033[93m";
    const char* magenta = "\033[95m";
    const char* cyan    = "\033[96m";
    const char* white   = "\033[37m";
    const char* reset   = "\033[39;49m";
    switch((enum DndcSyntax)type){
        // case DNDC_SYNTAX_NONE:
            // break;
        case DNDC_SYNTAX_DOUBLE_COLON:
            fputs(gray, stdout);
            break;
        case DNDC_SYNTAX_HEADER:
            fputs(blue, stdout);
            break;
        case DNDC_SYNTAX_NODE_TYPE:
            fputs(red, stdout);
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
dndc_print_out_syntax(LongString source_path){
    LongString source_text;
    Allocator allocator = get_mallocator();
    if(!source_path.length){
        MStringBuilder sb = {.allocator=allocator};
        for(;;){
            enum {N = 4096};
            msb_ensure_additional(&sb, N);
            char* buff = sb.data + sb.cursor;
            auto numread = fread(buff, 1, N, stdin);
            sb.cursor += numread;
            if(numread != N)
                break;
            }
        source_text = msb_detach(&sb);
        }
    else {
        auto load_err = read_file( source_path.text, allocator);
        if(load_err.errored){
            fprintf(stderr, "Unable to read: '%s'\n", source_path.text);
            return;
            }
        source_text = load_err.result;
        }
    const char* where = source_text.text;
    dndc_analyze_syntax(LS_to_SV(source_text), dndc_syntax_func, &where);
    if(where != source_text.text+source_text.length){
        fwrite(where, 1, (source_text.text+source_text.length) - where, stdout);
        }
    }

#include "dndc.c"
#include "get_input.c"
