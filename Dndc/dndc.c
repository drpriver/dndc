// define DNDC_API before including dndc.h
#ifdef LOG_LEVEL
#undef LOG_LEVEL
#endif
#define LOG_LEVEL LOG_LEVEL_INFO

#include "dndc_api_def.h"
#include "dndc.h"
#include "dndc_node_types.h"
#include "dndc_format.c"
#include "dndc_types.h"
#include "dndc_funcs.h"

#include "path_util.h"
#include "allocator.h"
#include "mallocator.h"
#include "linear_allocator.h"
#include "recording_allocator.h"
#include "long_string.h"
#include "MStringBuilder.h"
#include "measure_time.h"
#include "thread_utils.h"
#include "bb_extensions.h"

#define DSORT_T LinkItem
#define DSORT_CMP StringView_cmp
#include "dsort.h"

#ifdef DNDCMAIN
#include "argument_parsing.h"
#endif


// Unsure of where to put this. So, just putting it here for now.
typedef struct BinaryJob{
    Marray(StringView) sourcepaths;
    Nonnull(FileCache*)b64cache;
} BinaryJob;

static
THREADFUNC(binary_worker){
    BinaryJob* jobp = thread_arg;
    FileCache cache = *jobp->b64cache;
    size_t count = jobp->sourcepaths.count;
    StringView* data = jobp->sourcepaths.data;
    ByteBuilder bb = {.allocator=cache.allocator};
    for(size_t i = 0; i < count; i++){
        auto sv = data[i];
        auto e = load_processed_binary_file(&cache, sv, &bb);
        // We'll let the renderer report the error when it tries
        // to load it.
        (void)e;
        bb_reset(&bb);
        }
    bb_destroy(&bb);
    memcpy(jobp->b64cache, &cache, sizeof(cache));
    return 0;
    }

// NOTE: we can have larger scope than this if we want.
// Slicing the work here this way is not inherent.
// However, care must be taken that the spawned thread is
// joined by the time we exit.
static
Errorable_f(void)
do_python_and_load_images(Nonnull(DndcContext*)ctx){
    Errorable(void) result = {};
    // Setup for the worker thread.
    auto flags = ctx->flags;
    BinaryJob job = {
        .b64cache = &ctx->b64cache,
        };
    if(not (ctx->flags & (DNDC_DONT_INLINE_IMAGES | DNDC_USE_DND_URL_SCHEME | DNDC_DONT_READ))){
        Marray(NodeHandle)* img_nodes[] = {
            &ctx->img_nodes,
            &ctx->imglinks_nodes,
            };
        for(size_t n = 0; n < arrlen(img_nodes); n++){
            auto nodes = img_nodes[n];
            MARRAY_FOR_EACH(it, *nodes){
                auto node = get_node(ctx, *it);
                if(!node->children.count)
                    continue;
                auto child = get_node(ctx, node_children(node)[0]);
                if(!child->header.length)
                    continue;
                if(path_is_abspath(child->header) or !ctx->base_directory.length){
                    if(not FileCache_has_file(job.b64cache, child->header)){
                        auto sv = Marray_alloc(StringView)(&job.sourcepaths, ctx->allocator);
                        *sv = child->header;
                        }
                    }
                else {
                    MStringBuilder path_builder = {.allocator=ctx->string_allocator};
                    msb_write_str(&path_builder, ctx->base_directory.text, ctx->base_directory.length);
                    msb_append_path(&path_builder, child->header.text, child->header.length);
                    auto path = msb_borrow(&path_builder);
                    if(not FileCache_has_file(job.b64cache, path)){
                        auto sv = Marray_alloc(StringView)(&job.sourcepaths, ctx->allocator);
                        *sv = LS_to_SV(msb_detach(&path_builder));
                        }
                    else {
                        msb_destroy(&path_builder);
                        }
                    }
                }
            }
    }
    ThreadHandle worker = {};
    bool binary_work_to_be_done = !!job.sourcepaths.count;
    if(binary_work_to_be_done){
        if(flags & DNDC_NO_THREADS){
            // Do it ourselves in this thread.
            binary_worker(&job);
            }
        else{
            create_thread(&worker, &binary_worker, &job);
            }
        }
    // Execute the python blocks.
    if(!(flags & DNDC_NO_PYTHON) and ctx->python_nodes.count){
        auto before = get_t();

        #ifndef PYTHONMODULE
        // This call handles the DNDC_PYTHON_IS_INIT flag.
        auto e = internal_init_dndc_python_interpreter(flags);
        if(e.errored){
            report_system_error(ctx, SV("Failed to initialize python"));
            result.errored = e.errored;
            goto cleanup;
            }
        #endif

        auto after = get_t();
        report_time(ctx, SV("Python startup took: "), after-before);
        for(size_t i = 0; i < ctx->python_nodes.count; i++){
            auto handle = ctx->python_nodes.data[i];
            {
            auto node = get_node(ctx, handle);
            if(node->type != NODE_PYTHON)
                continue;
            MStringBuilder msb = {.allocator=ctx->string_allocator};
            NODE_CHILDREN_FOR_EACH(it, node){
                auto child = *it;
                auto child_node = get_node(ctx, child);
                msb_write_str(&msb, child_node->header.text, child_node->header.length);
                msb_write_char(&msb, '\n');
                }
            if(!msb.cursor)
                continue;
            auto str = msb_detach(&msb);
            auto py_err = execute_python_string(ctx, str.text, handle);
            if(py_err.errored){
                report_set_error(ctx);
                result.errored = py_err.errored;
                goto cleanup;
                }
            }
            auto node = get_node(ctx, handle);
            // unsure if this is right, but doing it for now.
            auto parent = get_node(ctx, node->parent);
            node->parent = INVALID_NODE_HANDLE;
            for(size_t j = 0; j < parent->children.count; j++){
                if(NodeHandle_eq(handle, node_children(parent)[j])){
                    node_remove_child(parent, j, ctx->allocator);
                    goto after;
                    }
                }
            // don't both warning here, but leave the scaffolding in case I want to.
            after:;
            }
        auto after_python = get_t();
        report_time(ctx, SV("Python scripts took: "), after_python-after);
        report_time(ctx, SV("Python total took: "), after_python-before);
        }
    // Python blocks are done, join with the base64 thread.
    // We could actually join later now that I think about it.
    // We only need to join by the time we start rendering any of the nodes into html.
    // Well, todo, we almost always finish by the time python is done.
    cleanup:
    if(binary_work_to_be_done){
        if(!(flags & DNDC_NO_THREADS)){
            auto before = get_t();
            join_thread(worker);
            auto after = get_t();
            // This is usually very fast as the worker thread finished before python.
            report_time(ctx, SV("Joining took: "), after-before);
            }
        Marray_cleanup(StringView)(&job.sourcepaths, ctx->allocator);
        }
    else {
        report_info(ctx, SV("No binary work was to be done."));
        }
    return result;
    }


#ifdef DNDCMAIN
static
void
dndc_print_out_syntax(LongString source_path);
static
int
dndc_write_depends_file(Nonnull(void*)user_data, size_t npaths, Nonnull(StringView*) paths);

struct DependencyUserData {
    LongString outfile;
    LongString depfile;
};

static int depends_print_callback(void*_Nullable, size_t, Nonnull(StringView*));
int main(int argc, char**argv){
    LongString source_path = LS("");
    LongString output_path = LS("");
    DndcDependencyFunc*dependency_func = NULL;
    LongString dependency_path = LS("");
    struct DependencyUserData dependency_user_data = {};
    LongString base_dir = LS("");
    bool report_orphans = false;
    bool no_python = false;
    bool print_tree = false;
    bool print_links = false;
    bool print_stats = false;
    bool allow_bad_links = false;
    bool suppress_warnings = false;
    bool dont_write = false;
    bool no_threads = false;
    bool cleanup = false;
    bool use_site = false;
    bool reformat_only = false;
    bool hidden_help = false;
    bool dont_inline_images = false;
    bool print_syntax = false;
    bool print_depends = false;
    bool untrusted = false;
    bool strip_whitespace = false;
    bool dont_read = false;
    {
        ArgToParse pos_args[] = {
            [0] = {
                .name = SV("source"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&source_path),
                .help = "Source file (.dnd file) to read from.\nIf not given, reads from stdin.",
                },
            };
        ArgToParse kw_args[] = {
            {
                .name = SV("-o"),
                .altname1 = SV("--output"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&output_path),
                .help = "output path (.html file) to write to.\n If not given, writes to stdout.",
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
                .help = "Relative filepaths in source files will be relative to the given directory.\n"
                        "If not given, everything is relative to cwd.",
                .hide_default = true,
            },
            {
                .name = SV("--report-orphans"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&report_orphans),
                .help = "Report orphaned nodes (for debugging scripts).",
                .hidden = true,
            },
            {
                .name = SV("--no-python"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&no_python),
                .help = "Don't execute python nodes.",
                .hidden = true,
            },
            {
                .name = SV("--print-tree"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&print_tree),
                .help = "Print out the entire document tree.",
                .hidden = true,
            },
            {
                .name = SV("--print-links"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&print_links),
                .help = "Print out all links (and what they target) known by the system.",
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
                .dest = ARGDEST(&print_stats),
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
                .dest = ARGDEST(&allow_bad_links),
                .help = "Warn instead of erroring if a link can't be resolved.",
                .hidden = true,
            },
            {
                .name = SV("--suppress-warnings"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&suppress_warnings),
                .help = "Don't report non-fatal errors.",
                .hidden = true,
            },
            {
                .name = SV("--dont-write"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&dont_write),
                .help = "Don't write out the document.",
                .hidden = true,
            },
            {
                .name = SV("--single-threaded"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&no_threads),
                .help = "Do not create worker threads, do everything in the same thread.",
                .hidden = true,
            },
            {
                .name = SV("--cleanup"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&cleanup),
                .help = "Cleanup all resources (memory allocations, etc.).\n"
                    "    Development debugging tool, useless in regular cli use.",
                .hidden = true,
            },
            {
                .name = SV("--use-site"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&use_site),
                .help = "Don't isolate python, import site, etc.\n"
                    "   Greatly slows startup, but allows importing user installed packages.",
            },
            {
                .name = SV("--format"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&reformat_only),
                .help = "Instead of rendering to html, render to .dnd with trailing  "
                        "spaces removed, text wrapped to 80 columns (if semantically "
                        "equivalent), etc. Imports will not be resolved - only the "
                        "given input file will be imported."
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
                .dest = ARGDEST(&dont_inline_images),
                .help = "Instead of base64ing the images, use a link.",
                .hidden = true,
            },
            {
                .name = SV("--untrusted-input"),
                .altname1 = SV("--untrusted"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&untrusted),
                .help = "Input is untrusted and thus should not be allowed to import files, execute scripts or embed javascript in the output.",
                .hidden = true,
            },
            {
                .name = SV("--strip-spaces"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&strip_whitespace),
                .help = "Strip trailing and leading whitespace from all output lines",
                .hidden = false,
            },
            {
                .name = SV("--dont-read"),
                .min_num = 0,
                .max_num = 1,
                .dest = ARGDEST(&dont_read),
                .help = "Don't read any files (other than builtins and the initial input file). Python blocks can bypass this.",
                .hidden = true,
            },
            };
        ArgParser argparser = {
            .name = argv[0],
            .description = "A .dnd to .html parser and compiler.",
            .version = "dndc version " DNDC_VERSION ". Compiled " __TIMESTAMP__,
            .positional.args = pos_args,
            .positional.count = arrlen(pos_args),
            .keyword.args = kw_args,
            .keyword.count = arrlen(kw_args),
            };
        Args args = argc?(Args){argc-1, (const char*const*)argv+1}: (Args){0, 0};
        if(check_for_help(&args)){
            print_help(&argparser);
            return 0;
            }
        if(check_for_version(&args)){
            print_version(&argparser);
            return 0;
            }
        auto e = parse_args(&argparser, &args);
        if(e.errored){
            fprintf(stderr, "Error when parsing arguments.\n");
            print_help(&argparser);
            return e.errored;
            }
        if(hidden_help){
            fputs(
                "Hidden Arguments:\n"
                "-----------------", stdout);
            auto term_size = get_terminal_size();
            if(term_size.columns > 80)
                term_size.columns = 80;
            for(int i = 0; i < arrlen(kw_args); i++){
                auto arg = &kw_args[i];
                if(!arg->hidden){
                    continue;
                    }
                putchar('\n');
                print_arg_help(arg, term_size);
                }
            return 0;
            }
    }
    if(print_syntax){
        dndc_print_out_syntax(source_path);
        return 0;
    }

    uint64_t flags = DNDC_FLAGS_NONE
        | DNDC_SOURCE_IS_PATH_NOT_DATA
        | DNDC_OUTPUT_IS_FILE_PATH_NOT_OUT_PARAM
        ;
    if(allow_bad_links)
        flags |= DNDC_ALLOW_BAD_LINKS;
    if(suppress_warnings)
        flags |= DNDC_SUPPRESS_WARNINGS;
    if(print_stats)
        flags |= DNDC_PRINT_STATS;
    if(report_orphans)
        flags |= DNDC_REPORT_ORPHANS;
    if(no_python)
        flags |= DNDC_NO_PYTHON;
    if(print_tree)
        flags |= DNDC_PRINT_TREE;
    if(print_links)
        flags |= DNDC_PRINT_LINKS;
    if(print_depends){
        dependency_func = depends_print_callback;
        }
    else if(dependency_path.length){
        dependency_func = dndc_write_depends_file;
        dependency_user_data.depfile = dependency_path;
        }
    if(no_threads)
        flags |= DNDC_NO_THREADS;
    if(dont_write)
        flags |= DNDC_DONT_WRITE;
    if(not cleanup)
        flags |= DNDC_NO_CLEANUP;
    if(use_site)
        flags |= DNDC_PYTHON_UNISOLATED;
    if(reformat_only)
        flags |= DNDC_REFORMAT_ONLY;
    if(dont_inline_images)
        flags |= DNDC_DONT_INLINE_IMAGES;
    if(untrusted)
        flags |= DNDC_INPUT_IS_UNTRUSTED;
    if(strip_whitespace)
        flags |= DNDC_STRIP_WHITESPACE;
    if(dont_read)
        flags |= DNDC_DONT_READ;
    dependency_user_data.outfile = output_path;

    #ifdef BENCHMARKING
    flags &= ~DNDC_NO_CLEANUP;
    auto e = run_the_dndc(flags, LS_to_SV(base_dir), source_path, &output_path, NULL, NULL, dndc_stderr_error_func, NULL, dependency_func, &dependency_user_data);
    assert(!e.errored);
    flags |= DNDC_PYTHON_IS_INIT;
    for(int i = 0; i < BENCHMARKITERS;i++){
        e = run_the_dndc(flags, LS_to_SV(base_dir), source_path, &output_path, NULL, NULL, dndc_stderr_error_func, NULL, dependency_func, &dependency_user_data);
        assert(!e.errored);
        }
    end_interpreter();
    return 0;
    #else
    auto e = run_the_dndc(flags, LS_to_SV(base_dir), source_path, output_path.length? &output_path : NULL, NULL, NULL, dndc_stderr_error_func, NULL, dependency_func, &dependency_user_data);
    return e.errored;
    #endif
    }
static
int
depends_print_callback(void*_Nullable unused, size_t npaths, Nonnull(StringView*) paths){
    (void)unused;
    for(size_t i = 0; i < npaths; i++){
        StringView path = paths[i];
        printf("%.*s\n", (int)path.length, path.text);
        }
    return 0;
    }
static
int
dndc_write_depends_file(Nonnull(void*)user_data, size_t npaths, Nonnull(StringView*) paths){
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
    if(write_err.errored){
        perror("Error on write");
        return write_err.errored;
        }
    return 0;
    }
#endif

static
Errorable_f(void)
run_the_dndc(uint64_t flags, StringView base_directory, LongString source_or_path,
        Nullable(LongString*) output_path,
        Nullable(FileCache*)external_b64cache,
        Nullable(FileCache*)external_textcache,
        Nullable(DndcErrorFunc*)error_func, Nullable(void*)error_user_data,
        Nullable(DndcDependencyFunc*)dependency_func,
        Nullable(void*)dependency_user_data){
    if(flags & DNDC_REFORMAT_ONLY)
        flags |= DNDC_NO_PYTHON;
    if(flags & DNDC_INPUT_IS_UNTRUSTED){
        flags |= DNDC_NO_PYTHON;
        flags |= DNDC_NO_THREADS;
        flags |= DNDC_DONT_INLINE_IMAGES;
        flags |= DNDC_DONT_READ;
        }
    auto t0 = get_t();
    Errorable(void) result = {};
    StringView path;
    if(flags & DNDC_SOURCE_IS_PATH_NOT_DATA)
        path = LS_to_SV(source_or_path);
    else
        path = SV("(string input)");
    LongString outpath;
    if(!output_path){
        outpath = LS("");
        }
    else if(flags & DNDC_OUTPUT_IS_FILE_PATH_NOT_OUT_PARAM){
        outpath = *output_path;
        }
    else {
        outpath = LS("this.html");
        }
    ArenaAllocator arena_allocator = {};
    const Allocator string_allocator = {.type=ALLOCATOR_ARENA, ._data=&arena_allocator};
    const Allocator allocator = flags & DNDC_NO_CLEANUP?get_mallocator():new_recorded_mallocator();
    // The linear allocator is very useful for temporary allocations, like
    // when we need to turn a string into its kebabed form and then look it up
    // in the link map. We do this a lot and throw away the temporary string
    // constantly - this means we don't have to keep hitting malloc
    // just for temporary strings of arbitrary size.
    LinearAllocator la_ = new_linear_storage(1024*1024, "temp storage");
    Allocator la = allocator_from_la(&la_);
    DndcContext ctx = {
        .flags = flags,
        .allocator = allocator,
        .temp_allocator = la,
        .string_allocator = string_allocator,
        .titlenode = INVALID_NODE_HANDLE,
        .navnode = INVALID_NODE_HANDLE,
        .outputfile = outpath,
        .base_directory = base_directory,
        // The base64 cache is moved to another thread and then moved back, so
        // it needs an independent allocator so it can run concurrently.
        .b64cache = external_b64cache? *external_b64cache :
            ((FileCache){.allocator = flags & DNDC_NO_CLEANUP?get_mallocator():new_recorded_mallocator()}),
        // The text cache only runs on this thread so we can just use the general allocator.
        .textcache = external_textcache?*external_textcache:((FileCache){.allocator=allocator}),
        .error_func = error_func,
        .error_user_data = error_user_data,
        };
    MStringBuilder msb = {.allocator=ctx.allocator};
    ctx_add_builtins(&ctx);
    LongString source;
    if(flags & DNDC_SOURCE_IS_PATH_NOT_DATA){
        if(!path.length){
            // read from stdin
            MStringBuilder sb = {.allocator=ctx.allocator};
            for(;;){
                enum {N = 4096};
                msb_reserve(&sb, N);
                char* buff = sb.data + sb.cursor;
                auto numread = fread(buff, 1, N, stdin);
                sb.cursor += numread;
                if(numread != N)
                    break;
                }
            source = msb_detach(&sb);
            path = SV("(stdin)");
            ctx_store_builtin_file(&ctx, LS("(stdin)"), source);
            }
        else {
            // Temporarily clear the DONT_READ flag.
            auto old_flags = ctx.flags;
            ctx.flags &= ~DNDC_DONT_READ;
            auto source_err = ctx_load_source_file(&ctx, path);
            ctx.flags = old_flags;
            if(source_err.errored){
                if(ctx.base_directory.length){
                    MStringBuilder err_builder = {.allocator = ctx.temp_allocator};
                    MSB_FORMAT(&err_builder, "Unable to open '", ctx.base_directory, "/", path, "'");
                    report_system_error(&ctx, msb_borrow(&err_builder));
                    msb_destroy(&err_builder);
                    }
                else{
                    MStringBuilder err_builder = {.allocator = ctx.temp_allocator};
                    MSB_FORMAT(&err_builder, "Unable to open '", path, "'");
                    report_system_error(&ctx, msb_borrow(&err_builder));
                    msb_destroy(&err_builder);
                    }
                result.errored = source_err.errored;
                goto cleanup;
                }
            source = source_err.result;
            }
        }
    else {
        source = source_or_path;
        if(!source.length){
            report_system_error(&ctx, SV("String with no data given as input"));
            result.errored = UNEXPECTED_END;
            goto cleanup;
            }
        ctx_store_builtin_file(&ctx, LS("(string input)"), source);
        }
    // Quick and dirty estimate of how many nodes we will need.
    Marray_reserve(Node)(&ctx.nodes, ctx.allocator, source.length/10+1);

    // Setup the root node.
    {
        auto root_handle = alloc_handle(&ctx);
        ctx.root_handle = root_handle;
        auto root = get_node(&ctx, root_handle);
        root->col = 0;
        root->row = 0;
        root->filename = path;
        root->type = NODE_MD;
        root->parent = root_handle;
    }
    // Parse the initial document.
    {
        auto before_parse = get_t();
        auto e = dndc_parse(&ctx, ctx.root_handle, path, source.text);
        auto after_parse = get_t();
        report_time(&ctx, SV("Initial parsing took: "), after_parse-before_parse);
        if(e.errored){
            report_set_error(&ctx);
            result.errored = e.errored;
            goto cleanup;
            }
    }
    if(flags & DNDC_REFORMAT_ONLY){
        msb_reset(&msb);
        auto before = get_t();
        auto format_error = format_tree(&ctx, &msb);
        auto after = get_t();
        report_time(&ctx, SV("Formatting took: "), after-before);
        if(format_error.errored){
            result.errored = format_error.errored;
            goto cleanup;
            }

        auto str = msb_borrow(&msb);
        auto before_write = get_t();
        if(flags & DNDC_DONT_WRITE){
            goto success;
            }
        else if(!output_path || outpath.length == 0){
            fputs(str.text, stdout);
            goto success;
            }
        else if(!(flags & DNDC_OUTPUT_IS_FILE_PATH_NOT_OUT_PARAM)){
            assert(output_path);
            // We don't use the allocator as this needs to outlive the recording
            // allocator.
            //
            // We could do this without the extra copy, but this will work for now.
            char* text = malloc(str.length+1);
            memcpy(text, str.text, str.length);
            text[str.length] = '\0';
            output_path->text = text;
            output_path->length = str.length;
            }
        else {
            auto write_err = write_file(outpath.text, str.text, str.length);
            if(write_err.errored){
                perror("Error on write");
                result.errored = write_err.errored;
                goto cleanup;
                }
            }
        auto after_write = get_t();
        report_time(&ctx, SV("Writing took: "), after_write-before_write);
        report_size(&ctx, SV("Total output size (bytes): "), str.length);
        goto success;
        }
    if(unlikely(flags & DNDC_INPUT_IS_UNTRUSTED)){
        if(ctx.imports.count){
            auto handle = ctx.imports.data[0];
            auto node = get_node(&ctx, handle);
            node_print_err(&ctx, node, SV("Imports are illegal for untrusted input."));
            result.errored = PARSE_ERROR;
            goto cleanup;
            }
        if(ctx.python_nodes.count){
            auto handle = ctx.python_nodes.data[0];
            auto node = get_node(&ctx, handle);
            node_print_err(&ctx, node, SV("Python blocks are illegal for untrusted input."));
            result.errored = PARSE_ERROR;
            goto cleanup;
            }
        if(ctx.script_nodes.count){
            auto handle = ctx.script_nodes.data[0];
            auto node = get_node(&ctx, handle);
            node_print_err(&ctx, node, SV("JS blocks are illegal for untrusted input."));
            result.errored = PARSE_ERROR;
            goto cleanup;
            }
        }
    else {
        // Handle imports. Imports can import more imports, so don't use a FOR_EACH.
        auto before_imports = get_t();
        for(size_t i = 0; i < ctx.imports.count; i++){
            auto handle = ctx.imports.data[i];
            auto node = get_node(&ctx, handle);
            if(ctx.imports.count > 1000){
                node_print_err(&ctx, node, SV("More than 1000 imports. Aborting parsing (did you accidentally create an import cycle?)"));
                result.errored = PARSE_ERROR;
                goto cleanup;
                }
            // NOTE: re-get the node every loop as the pointer is invalidated.
            for(size_t j = 0; j < node->children.count; j++, node=get_node(&ctx, handle)){
                auto child_handle = node_children(node)[j];
                auto child = get_node(&ctx, child_handle);
                if(child->type != NODE_STRING){
                    node_print_err(&ctx, child, SV("import child is not a string"));
                    result.errored = PARSE_ERROR;
                    goto cleanup;
                    }
                StringView filename = child->header;
                // set to MD so we can parse as md
                child->type = NODE_MD;
                child->header = SV("");
                auto imp_e = ctx_load_source_file(&ctx, filename);
                if(imp_e.errored){
                    MStringBuilder err_builder = {.allocator = ctx.temp_allocator};
                    if(ctx.base_directory.length){
                        MSB_FORMAT(&err_builder, "Unable to open '", ctx.base_directory, "/", filename, "'");
                        }
                    else{
                        MSB_FORMAT(&err_builder, "Unable to open '", filename, "'");
                        }
                    node_print_err(&ctx, child, msb_borrow(&err_builder));
                    msb_destroy(&err_builder);
                    result.errored = imp_e.errored;
                    goto cleanup;
                    }
                LongString imp_text = imp_e.result;
                auto parse_e = dndc_parse(&ctx, child_handle, filename, imp_text.text);
                if(parse_e.errored){
                    report_set_error(&ctx);
                    result.errored = parse_e.errored;
                    goto cleanup;
                    }
                child = get_node(&ctx, child_handle);
                // change to container
                child->type = NODE_CONTAINER;
                }
            }
        auto after_imports = get_t();
        report_time(&ctx, SV("Resolving imports took: "), after_imports-before_imports);
    }

    // Speculatively load imgs and imglinks and preprocess them.
    // Do this at the same time as we execute the Python nodes.
    // Python blocks can add imgs or change the paths of the img nodes,
    // but they usually don't, so doing these in parallel is a win as
    // Python startup (and execution) is very slow.
    {
        // This is shoved in its own function as we need to guarantee
        // the worker has joined before continuing beyond this point.
        // Putting it in its own function with single-point-of-exit style
        // makes that easier to do.
        auto e = do_python_and_load_images(&ctx);
        if(e.errored){
            result.errored = e.errored;
            goto cleanup;
            }
    }
    // Do some reporting as we don't add any nodes after this.
    report_size(&ctx, SV("ctx.nodes.count = "), ctx.nodes.count);
    report_size(&ctx, SV("ctx.python_nodes.count = "), ctx.python_nodes.count);
    report_size(&ctx, SV("ctx.imports.count = "), ctx.imports.count);
    report_size(&ctx, SV("ctx.script_nodes.count = "), ctx.script_nodes.count);
    report_size(&ctx, SV("ctx.dependencies_nodes.count = "), ctx.dependencies_nodes.count);
    report_size(&ctx, SV("ctx.link_nodes.count = "), ctx.link_nodes.count);
    if(flags & DNDC_REPORT_ORPHANS){
        MARRAY_FOR_EACH(node, ctx.nodes){
            // python nodes get orphaned after execution
            if(node->type == NODE_PYTHON)
                continue;
            if(NodeHandle_eq(node->parent, INVALID_NODE_HANDLE)){
                node_print_warning(&ctx, node, SV("Orphaned node (invalid parent node handle)"));
                }
            }
        }
    // Python blocks can detach the root node and then forget to attach a new
    // one.
    if(NodeHandle_eq(ctx.root_handle, INVALID_NODE_HANDLE)){
        report_system_error(&ctx, SV("ctx has no root Node."));
        result.errored = PARSE_ERROR;
        goto cleanup;
        }
    // Check that the tree is not too deep!
    {
        auto before = get_t();
        auto e = check_depth(&ctx);
        if(e.errored){
            report_set_error(&ctx);
            result.errored = e.errored;
            goto cleanup;
            }
        auto after = get_t();
        report_time(&ctx, SV("Checking depth took "), after-before);
    }
    // Create links from headers.
    {
        auto before = get_t();
        gather_anchors(&ctx);
        auto after = get_t();
        report_time(&ctx, SV("Link resolving took: "), after-before);
        // FIXME: if an error can be set while gathering anchors, we should
        // return an error!
        if(ctx.error.message.length){
            report_set_error(&ctx);
            result.errored = PARSE_ERROR;
            goto cleanup;
            }
    }

    // Maybe should remove this option as it clogs up the cli and was just for
    // debugging before rendering was off the ground.
    if(flags & DNDC_PRINT_TREE)
        print_node_and_children(&ctx, ctx.root_handle, 0);

    // Render the nav block if we have one.
    {
        auto before = get_t();
        if(not NodeHandle_eq(ctx.navnode, INVALID_NODE_HANDLE))
            build_nav_block(&ctx);
        auto after =  get_t();
        report_time(&ctx, SV("Nav block building took: "), after-before);
    }

    // Add in the links from explicit link blocks.
    {
        MARRAY_FOR_EACH(link_handle, ctx.link_nodes){
            auto link_node = get_node(&ctx, *link_handle);
            NODE_CHILDREN_FOR_EACH(it, link_node){
                auto link_str_node = get_node(&ctx, *it);
                auto e = add_link_from_sv(&ctx, link_str_node);
                if(e.errored){
                    result.errored = e.errored;
                    goto cleanup;
                    }
                }
            }
        // Sort so we can do a binary search.
        if(ctx.links.count){
            #if 1
                LinkItem__array_sort(ctx.links.data, ctx.links.count);
            #else
                qsort(ctx.links.data, ctx.links.count, sizeof(ctx.links.data[0]), StringView_cmp);
            #endif
            }
        if(flags & DNDC_PRINT_LINKS){
            if(!ctx.error_func){
                result.errored = PARSE_ERROR;
                goto cleanup;
                }
            MStringBuilder temp = {.allocator = ctx.temp_allocator};
            for(size_t i = 0; i < ctx.links.count; i++){
                auto li = &ctx.links.data[i];
                MSB_FORMAT(&temp, "[", i, "] key: '", li->key, "', value: '", li->value, "'");
                auto msg = msb_borrow(&temp);
                ctx.error_func(ctx.error_user_data, DNDC_DEBUG_MESSAGE, "", 0, 0, 0, msg.text, msg.length);
                msb_reset(&temp);
                }
            msb_destroy(&temp);
            }
        report_size(&ctx, SV("ctx.links.count = "), ctx.links.count);
    }

    // Render data nodes into the data blob.
    {
        auto before_data = get_t();
        MStringBuilder sb = {.allocator=ctx.allocator};
        MARRAY_FOR_EACH(handle, ctx.data_nodes){
            auto data_node = get_node(&ctx, *handle);
            // Node could've been mutated after being registered.
            if(data_node->type != NODE_DATA)
                continue;
            NODE_CHILDREN_FOR_EACH(it, data_node){
                auto child = get_node(&ctx, *it);
                if(!child->header.length){
                    node_print_warning(&ctx, child, SV("Missing header from data child?"));
                    }
                // FIXME:
                // A maliciously crafted python block could bypass our depth check
                // up above by detaching the data node and making one too deep,
                // thus making us vulnerable to stack exhaustion during this
                // recursive call.
                //
                // However, our code execution is totally unsandboxed right now and
                // a malicious python block can just do anything. Crashing this
                // main program is the least of your worries when it can just do an
                // os.system('rm -rf /'). We bother guarding at all as you could
                // run this in no-python mode.
                {
                msb_reset(&sb);
                auto e = render_node(&ctx, &sb, child, 1);
                if(e.errored){
                    report_set_error(&ctx);
                    result.errored = e.errored;
                    goto cleanup;
                    }
                }
                if(!sb.cursor){
                    node_print_warning(&ctx, child, SV("Rendered a data node with no data. Not outputting it."));
                    continue;
                    }
                auto text = msb_detach(&sb);
                auto di = Marray_alloc(DataItem)(&ctx.rendered_data, ctx.allocator);
                di->key = child->header;
                di->value = text;
                }
            }
        auto after_data = get_t();
        report_time(&ctx, SV("Data blob rendering took: "), after_data-before_data);
        report_size(&ctx, SV("ctx.rendered_data.count = "), ctx.rendered_data.count);
    }
    // Render the actual document into a string as html.
    {
        // msb_reset(&msb);
        MStringBuilder output_sb = {.allocator = get_mallocator()};
        auto before_render = get_t();
        auto e = render_tree(&ctx, &output_sb);
        auto after_render = get_t();
        report_time(&ctx, SV("Rendering took: "), after_render-before_render);

        if(e.errored){
            report_set_error(&ctx);
            result.errored = e.errored;
            goto cleanup;
            }
        auto before_write = get_t();
        if(flags & DNDC_DONT_WRITE){
            msb_destroy(&output_sb);
            goto success;
            }
        else if(!output_path || outpath.length == 0){
            auto str = msb_borrow(&output_sb);
            fputs(str.text, stdout);
            msb_destroy(&output_sb);
            goto skip_report;
            }
        else if(!(flags & DNDC_OUTPUT_IS_FILE_PATH_NOT_OUT_PARAM)){
            assert(output_path);
            // We don't use the allocator as this needs to outlive the recording
            // allocator.
            //
            // We could do this without the extra copy, but this will work for now.
            *output_path = msb_detach(&output_sb);
            }
        else {
            auto str = msb_borrow(&output_sb);
            auto write_err = write_file(outpath.text, str.text, str.length);
            msb_destroy(&output_sb);
            if(write_err.errored){
                perror("Error on write");
                result.errored = write_err.errored;
                goto cleanup;
                }
            report_size(&ctx, SV("Total output size (bytes): "), str.length);
            }
        auto after_write = get_t();
        report_time(&ctx, SV("Writing took: "), after_write-before_write);
        skip_report:;
    }
    // Write the make-style dependency file to the Dependency directory.
    if(dependency_func){
        MARRAY_FOR_EACH(handle, ctx.dependencies_nodes){
            auto node = get_node(&ctx, *handle);
            NODE_CHILDREN_FOR_EACH(it, node){
                auto child = get_node(&ctx, *it);
                if(child->type != NODE_STRING){
                    // just warn, don't want to fail the build
                    node_print_warning2(&ctx, child, SV("Non-string node found as a child node: "), LS_to_SV(NODENAMES[child->type]));
                    continue;
                    }
                Marray_push(StringView)(&ctx.dependencies, ctx.allocator, child->header);
                }
            }
        int err = dependency_func(dependency_user_data, ctx.dependencies.count, ctx.dependencies.data);
        if(err){
            result.errored = err;
            goto cleanup;
            }
        }
    success:;
    cleanup:;
    msb_destroy(&msb);
    report_size(&ctx, SV("la_.high_water = "), la_.high_water);
    if(!(flags & DNDC_NO_CLEANUP)){
        auto before = get_t();
        if(ctx.flags & DNDC_PRINT_STATS){
            RecordingAllocator* recorder = allocator._data;
            report_size(&ctx, SV("N allocations: "), recorder->count);
            size_t total = 0;
            size_t alloced = 0;
            for(size_t i = 0; i < recorder->count; i++){
                size_t size = recorder->allocation_sizes[i];
                total += size;
                alloced += size > 0;
                }
            report_size(&ctx, SV("N existing allocations: "), alloced);
            report_size(&ctx, SV("Allocations outstanding total (bytes): "), total);
            }
        Allocator_free_all(string_allocator);
        Allocator_free_all(allocator);
        shallow_free_recorded_mallocator(allocator);
        if(!external_b64cache){
            Allocator_free_all(ctx.b64cache.allocator);
            shallow_free_recorded_mallocator(ctx.b64cache.allocator);
            }
        auto after = get_t();
        report_time(&ctx, SV("Cleaning up memory took: "), after-before);
        }
    if(external_b64cache){
        memcpy(external_b64cache, &ctx.b64cache, sizeof(ctx.b64cache));
        }
    if(external_textcache){
        memcpy(external_textcache, &ctx.textcache, sizeof(ctx.textcache));
        }
    auto t1 = get_t();
    report_time(&ctx, SV("Execution took: "), t1-t0);
    if(!(flags & DNDC_NO_CLEANUP))
        destroy_linear_storage(&la_);
    return result;
    }

// Idk where to put this.
static
void
print_node_and_children(Nonnull(DndcContext*)ctx, NodeHandle handle, int depth){
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

#include "dndc_htmlgen.c"
#include "dndc_parser.c"
#include "dndc_context.c"
#include "allocator.c"
#ifndef WASM
#include "dndc_python.c"
#else
static
Errorable_f(void)
internal_init_dndc_python_interpreter(uint64_t flags){
    Errorable(void) result = {};
    if(flags & DNDC_PYTHON_IS_INIT)
        return result;
    result.errored = OS_ERROR;
    return result;
}
static
Errorable_f(void)
execute_python_string(Nonnull(DndcContext*)ctx, Nonnull(const char*)str, NodeHandle node){
    (void)ctx, (void)str, (void)node;
    return (Errorable(void)){.errored=OS_ERROR};
    }

#endif

#ifndef PYTHONMODULE
#ifndef WASM

DNDC_API
int
dndc_format(LongString source_text, Nonnull(LongString*)output, Nullable(DndcErrorFunc*)error_func, Nullable(void*)error_user_data){
    uint64_t flags = 0
        | DNDC_PYTHON_IS_INIT
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_REFORMAT_ONLY
        ;
    auto e = run_the_dndc(flags, SV(""), source_text, output, NULL, NULL, error_func, error_user_data, NULL, NULL);
    return e.errored;
    }
DNDC_API
int
dndc_init_python(void){
    auto err = internal_init_dndc_python_interpreter(0);
    return err.errored;
    }

DNDC_API
int
dndc_init_python_types(void){
    auto err = internal_dndc_python_init_types();
    return err.errored;
    }

DNDC_API
void
dndc_free_string(LongString str){
    const_free(str.text);
}

DNDC_API
void
dndc_stderr_error_func(Nullable(void*)unused, int type, const char*_Nonnull filename, int filename_len, int line, int col, const char*_Nonnull message, int message_len){
    (void)unused;
    (void)message_len;
    switch((enum DndcErrorMessageType)type){
        case DNDC_NODELESS_MESSAGE:
            fprintf(stderr, "Error: %s\n", message);
            return;
        case DNDC_STATISTIC_MESSAGE:
            fprintf(stderr, "Info: %s\n", message);
            return;
        case DNDC_DEBUG_MESSAGE:
            if(filename_len)
                fprintf(stderr, "%.*s:%d:%d: %s\n", filename_len, filename, line+1, col+1, message);
            else
                fprintf(stderr, "%s\n", message);
            return;
        default:
            fprintf(stderr, "%.*s:%d:%d: %s\n", filename_len, filename, line+1, col+1, message);
            return;
        }
    }
#endif
#endif


static
Nullable(const char*)
find_double_colon(Nonnull(const char*) haystack, size_t length){
    if(length < 2)
        return NULL;
    const char* end = haystack + length;
    for(;;){
        const char* first = memchr(haystack, ':', end - haystack);
        if(!first)
            return NULL;
        if(end - first < 2)
            return NULL;
        if(first[1] == ':')
            return first;
        haystack = first+2;
        }
    }

#ifdef DNDCMAIN
static
void
dndc_syntax_func(void* _Nullable data, int type, int line, int col, Nonnull(const char*)begin, size_t length){
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
            msb_reserve(&sb, N);
            char* buff = sb.data + sb.cursor;
            auto numread = fread(buff, 1, N, stdin);
            sb.cursor += numread;
            if(numread != N)
                break;
            }
        source_text = msb_detach(&sb);
        }
    else {
        auto load_err = read_file(allocator, source_path.text);
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
#endif

static inline
force_inline
const uint16_t* _Nullable
mem_utf16(const uint16_t* _Nonnull haystack, uint16_t needle, size_t ncode_units){
    // A 1 in each utf-16 code unit slot.
    const uint64_t ones = 0x0001000100010001;
    const uint64_t needle_ = needle; // Basically a cast.
    // Repeat the needle in the 4 utf-16 code unit slots.
    const uint64_t needles = (needle_ << 48) | (needle_ << 32) | (needle_ << 16) | needle_;
    // Skip 4 code units at a time if the needle is not present.
    while(ncode_units > 4){
        uint64_t tmp1;
        // Pray that efficient code is generated.
        (memcpy)(&tmp1, haystack, 4*sizeof(*haystack));
        // After this, each code unit is all zeros iff that code unit == needle
        const uint64_t tmp2 = tmp1 ^ needles;
        // Subtract one from all of the code units,
        // Thus those that are zeros become all ones.
        const uint64_t tmp3 = tmp2 - ones;
        // Apply a mask to the high bit. The only way the highest bit in each
        // code point can be set is if it was a zero and we subtracted one.
        const uint64_t tmp4 = tmp3 & (ones << 15);
        // Check if any are set, thus a match.
        if(tmp4)
            break;
        haystack += 4;
        ncode_units -= 4;
    }
    // We're either at the tail end of the haystack or this block of
    // 4 units has the needle.
    // Just do a unit at a time search for it.
    for(;ncode_units > 0; --ncode_units, ++haystack){
        if(*haystack == needle)
            return haystack;
    }
    return NULL;
}

static inline
force_inline
Nullable(const uint16_t*)
find_double_colon_utf16(Nonnull(const uint16_t*) haystack, size_t ncode_units){
    if(ncode_units < 2)
        return NULL;
    const uint16_t* end = haystack + ncode_units;
    for(;;){
        const uint16_t* first = mem_utf16(haystack, u':', end - haystack);
        if(!first)
            return NULL;
        if(end - first < 2)
            return NULL;
        if(first[1] == u':')
            return first;
        haystack = first+2;
        }
    }

DNDC_API
int
dndc_analyze_syntax(StringView source_text, Nonnull(DndcSyntaxFunc*) syntax_func, Nullable(void*)syntax_data){
    // this is only needed for raw nodes
    ptrdiff_t raw_indentation = 0;
    int line = 0;
    const char* begin = source_text.text;
    const char* const end = begin + source_text.length;
    enum WhichNode {
        RAW,
        GENERIC,
        };
    enum WhichNode which = GENERIC;
    for(;begin != end;line++){
        const char* endline = memchr(begin, '\n', end-begin);
        if(not endline)
            endline = end;
        StringView stripped = lstripped_view(begin, endline-begin);
        ptrdiff_t indent = stripped.text - begin;
        if(stripped.length and indent <= raw_indentation)
            which = GENERIC;
        if(which == RAW){
            syntax_func(syntax_data, DNDC_SYNTAX_RAW_STRING, line, indent, stripped.text, stripped.length);
            }
        else {
            const char* doublecolon = find_double_colon(stripped.text, stripped.length);
            if(not doublecolon){
                }
            else {
                StringView header = lstripped_view(stripped.text, doublecolon - stripped.text);
                if(header.length){
                    syntax_func(syntax_data, DNDC_SYNTAX_HEADER, line, header.text - begin, header.text, header.length);
                    }
                syntax_func(syntax_data, DNDC_SYNTAX_DOUBLE_COLON, line, doublecolon-begin, doublecolon, 2);
                StringView aftercolon = lstripped_view(doublecolon+2, endline-(doublecolon+2));
                const char* nodenameend = aftercolon.text;
                for(;nodenameend != aftercolon.text+aftercolon.length;nodenameend++){
                    switch(*nodenameend){
                        case 'a' ... 'z':
                            continue;
                        default:
                            break;
                        }
                    break;
                    }
                if(nodenameend != aftercolon.text){
                    StringView nodename = {.text=aftercolon.text, .length=nodenameend-aftercolon.text};
                    syntax_func(syntax_data, DNDC_SYNTAX_NODE_TYPE, line, nodename.text-begin, nodename.text, nodename.length);
                    for(size_t i = 0; i < arrlen(RAW_NODES); i++){
                        if(SV_equals(nodename, RAW_NODES[i])){
                            which = RAW;
                            raw_indentation = stripped.text - begin;
                            break;
                            }
                        }
                    }
                const char* postnodename = nodenameend;
                for(;postnodename != endline;){
                    switch(*postnodename){
                        case '@':{
                            const char* attrfirst = postnodename;
                            postnodename++;
                            for(;postnodename != endline;postnodename++){
                                char c = *postnodename;
                                switch(c){
                                    case 'a' ... 'z':
                                    case 'A' ... 'Z':
                                    case '0' ... '9':
                                    case '-': case '_':
                                        continue;
                                    default:
                                        break;
                                    }
                                break;
                                }
                            syntax_func(syntax_data, DNDC_SYNTAX_ATTRIBUTE, line, attrfirst-begin, attrfirst, postnodename-attrfirst);
                            if(postnodename != endline and *postnodename == '('){
                                int parens = 1;
                                postnodename++;
                                const char* argfirst = postnodename;
                                for(;postnodename != endline;postnodename++){
                                    if(*postnodename == '(')
                                        parens++;
                                    if(*postnodename == ')')
                                        parens--;
                                    if(!parens)
                                        break;
                                    }
                                syntax_func(syntax_data, DNDC_SYNTAX_ATTRIBUTE_ARGUMENT, line, argfirst-begin, argfirst, postnodename-argfirst);
                                if(postnodename != endline)
                                    postnodename++;
                                }
                            }break;
                        case '.':{
                            const char* classfirst = postnodename;
                            postnodename++;
                            for(;postnodename != endline;postnodename++){
                                char c = *postnodename;
                                switch(c){
                                    case 'a' ... 'z':
                                    case 'A' ... 'Z':
                                    case '0' ... '9':
                                    case '-': case '_':
                                        continue;
                                    default:
                                        break;
                                    }
                                break;
                                }
                            syntax_func(syntax_data, DNDC_SYNTAX_CLASS, line, classfirst-begin, classfirst, postnodename-classfirst);
                            }break;
                        default:
                            postnodename++;
                            continue;
                        }
                    }
                }
            }
        if(endline == end)
            break;
        begin = endline+1;
        }
    return 0;
}

//
// copy-paste and slight alterations from dndc_analyze_syntax
// Will need to keep these in sync. This is where static if would come in handy.
//
DNDC_API
int
dndc_analyze_syntax_utf16(StringViewUtf16 source_text, Nonnull(DndcSyntaxFuncUtf16*) syntax_func, Nullable(void*)syntax_data){
    // this is only needed for raw nodes
    ptrdiff_t raw_indentation = 0;
    int line = 0;
    const uint16_t* begin = source_text.text;
    const uint16_t* const end = begin + source_text.length;
    enum WhichNode {
        RAW,
        GENERIC,
        };
    enum WhichNode which = GENERIC;
    for(;begin != end;line++){
        const uint16_t* endline = mem_utf16(begin, u'\n', end-begin);
        if(not endline)
            endline = end;
        StringViewUtf16 stripped = lstripped_view_utf16(begin, endline-begin);
        ptrdiff_t indent = stripped.text - begin;
        if(stripped.length and indent <= raw_indentation)
            which = GENERIC;
        if(which == RAW){
            syntax_func(syntax_data, DNDC_SYNTAX_RAW_STRING, line, indent, stripped.text, stripped.length);
            }
        else {
            const uint16_t* doublecolon = find_double_colon_utf16(stripped.text, stripped.length);
            if(not doublecolon){
                }
            else {
                StringViewUtf16 header = lstripped_view_utf16(stripped.text, doublecolon - stripped.text);
                if(header.length){
                    syntax_func(syntax_data, DNDC_SYNTAX_HEADER, line, header.text - begin, header.text, header.length);
                    }
                syntax_func(syntax_data, DNDC_SYNTAX_DOUBLE_COLON, line, doublecolon-begin, doublecolon, 2);
                StringViewUtf16 aftercolon = lstripped_view_utf16(doublecolon+2, endline-(doublecolon+2));
                const uint16_t* nodenameend = aftercolon.text;
                for(;nodenameend != aftercolon.text+aftercolon.length;nodenameend++){
                    switch(*nodenameend){
                        case u'a' ... u'z':
                            continue;
                        default:
                            break;
                        }
                    break;
                    }
                if(nodenameend != aftercolon.text){
                    StringViewUtf16 nodename = {.text=aftercolon.text, .length=nodenameend-aftercolon.text};
                    syntax_func(syntax_data, DNDC_SYNTAX_NODE_TYPE, line, nodename.text-begin, nodename.text, nodename.length);
                    for(size_t i = 0; i < arrlen(RAW_NODES); i++){
                        if(SV_utf16_equals(nodename, RAW_NODES_UTF16[i])){
                            which = RAW;
                            raw_indentation = stripped.text - begin;
                            break;
                            }
                        }
                    }
                const uint16_t* postnodename = nodenameend;
                for(;postnodename != endline;){
                    switch(*postnodename){
                        case u'@':{
                            const uint16_t* attrfirst = postnodename;
                            postnodename++;
                            for(;postnodename != endline;postnodename++){
                                uint16_t c = *postnodename;
                                switch(c){
                                    case u'a' ... u'z':
                                    case u'A' ... u'Z':
                                    case u'0' ... u'9':
                                    case u'-': case u'_':
                                        continue;
                                    default:
                                        break;
                                    }
                                break;
                                }
                            syntax_func(syntax_data, DNDC_SYNTAX_ATTRIBUTE, line, attrfirst-begin, attrfirst, postnodename-attrfirst);
                            if(postnodename != endline and *postnodename == u'('){
                                int parens = 1;
                                postnodename++;
                                const uint16_t* argfirst = postnodename;
                                for(;postnodename != endline;postnodename++){
                                    if(*postnodename == u'(')
                                        parens++;
                                    if(*postnodename == u')')
                                        parens--;
                                    if(!parens)
                                        break;
                                    }
                                syntax_func(syntax_data, DNDC_SYNTAX_ATTRIBUTE_ARGUMENT, line, argfirst-begin, argfirst, postnodename-argfirst);
                                if(postnodename != endline)
                                    postnodename++;
                                }
                            }break;
                        case u'.':{
                            const uint16_t* classfirst = postnodename;
                            postnodename++;
                            for(;postnodename != endline;postnodename++){
                                uint16_t c = *postnodename;
                                switch(c){
                                    case u'a' ... u'z':
                                    case u'A' ... u'Z':
                                    case u'0' ... u'9':
                                    case u'-': case u'_':
                                        continue;
                                    default:
                                        break;
                                    }
                                break;
                                }
                            syntax_func(syntax_data, DNDC_SYNTAX_CLASS, line, classfirst-begin, classfirst, postnodename-classfirst);
                            }break;
                        default:
                            postnodename++;
                            continue;
                        }
                    }
                }
            }
        if(endline == end)
            break;
        begin = endline+1;
        }
    return 0;
}

DNDC_API
Nonnull(struct DndcFileCache*)
dndc_create_filecache(void){
    struct DndcFileCache* result = malloc(sizeof(*result));
    Allocator al = get_mallocator();
    *result = (struct DndcFileCache){.allocator = al};
    return result;
    }
DNDC_API
void
dndc_filecache_destroy(Nonnull(struct DndcFileCache*)cache){
    FileCache_clear(cache);
    free(cache);
}

DNDC_API
int
dndc_filecache_remove(Nonnull(struct DndcFileCache*)cache, StringView path){
    return FileCache_maybe_remove(cache, path);
    }

DNDC_API
void
dndc_filecache_clear(Nonnull(struct DndcFileCache*)cache){
    FileCache_clear(cache);
    }

DNDC_API
int
dndc_filecache_has_path(Nonnull(struct DndcFileCache*)cache, struct DndcStringView path){
    return FileCache_has_file(cache, path);
    }

DNDC_API
int
dndc_compile_dnd_file(unsigned long long flags, struct DndcStringView base_directory,
    struct DndcLongString source_path,
    struct DndcLongString*_Nullable output_path,
    struct DndcFileCache*_Nullable base64cache,
    struct DndcFileCache*_Nullable textcache,
    DndcErrorFunc*_Nullable error_func,
    void*_Nullable error_user_data,
    DndcDependencyFunc*_Nullable dependency_func,
    void*_Nullable dependency_user_data

){
    enum {
        // All the valid flags.
        DNDC_VALID_FLAGS = 0
            | DNDC_ALLOW_BAD_LINKS
            | DNDC_SUPPRESS_WARNINGS
            | DNDC_PRINT_STATS
            | DNDC_REPORT_ORPHANS
            | DNDC_NO_PYTHON
            | DNDC_PYTHON_IS_INIT
            | DNDC_PRINT_TREE
            | DNDC_PRINT_LINKS
            | DNDC_NO_THREADS
            | DNDC_DONT_WRITE
            | DNDC_NO_CLEANUP
            | DNDC_SOURCE_IS_PATH_NOT_DATA
            | DNDC_DONT_PRINT_ERRORS
            | DNDC_PYTHON_UNISOLATED
            | DNDC_OUTPUT_IS_FILE_PATH_NOT_OUT_PARAM
            | DNDC_REFORMAT_ONLY
            | DNDC_DONT_INLINE_IMAGES
            | DNDC_USE_DND_URL_SCHEME
            | DNDC_INPUT_IS_UNTRUSTED
            | DNDC_STRIP_WHITESPACE
            | DNDC_DONT_READ
    };
    uint64_t new_flags = flags & DNDC_VALID_FLAGS;
    if(new_flags != flags)
        return GENERIC_ERROR;
    auto err = run_the_dndc(flags, base_directory, source_path, output_path, base64cache, textcache, error_func, error_user_data, dependency_func, dependency_user_data);
    return err.errored;
    }
