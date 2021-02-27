#ifdef LOG_LEVEL
#undef LOG_LEVEL
#endif
#define LOG_LEVEL LOG_LEVEL_INFO
#include <stdarg.h>
#include "dndc_format.c"
#include "path_util.h"
#include "linear_allocator.h"
#include "long_string.h"
#include "MStringBuilder.h"
#include "measure_time.h"
#include "argument_parsing.h"
#include "recording_allocator.h"
#include "dndc_types.h"
#include "thread_utils.h"
#include "bb_extensions.h"
#include "mallocator.h"
#include "dndc_funcs.h"
#include "dndc.h"

#define DNC_MAJOR 0
#define DNC_MINOR 2
#define DNC_MICRO 1
#define DNDC_VERSION STRINGIFY(DNC_MAJOR) "." STRINGIFY(DNC_MINOR) "." STRINGIFY(DNC_MICRO)

// Unsure of where to put this. So, just putting it here for now.
typedef struct BinaryJob{
    Marray(StringView) sourcepaths;
    Nonnull(Base64Cache*)cache;
    bool report_time;
} BinaryJob;

static
THREADFUNC(binary_worker){
    auto before = get_t();
    BinaryJob* jobp = thread_arg;
    auto cache = *jobp->cache;
    bool report_time = jobp->report_time;
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
    memcpy(jobp->cache, &cache, sizeof(cache));
    // *jobp->cache = cache;
    auto after = get_t();
    if(report_time)
        fprintf(stderr, "Info: Binary worker took %.3fms\n", (after-before)/1000.);
    return 0;
    }

// NOTE: we can have larger scope than this if we want.
// Slicing the work here this way is not inherent.
static
Errorable_f(void)
do_python_and_load_images(Nonnull(DndcContext*)ctx){
    Errorable(void) result = {};
    // Setup for the worker thread.
    auto flags = ctx->flags;
    BinaryJob job = {
        .cache = &ctx->b64cache,
        .report_time = !!(flags & DNDC_PRINT_STATS),
        };
    {
        Marray(NodeHandle)* img_nodes[] = {
            &ctx->img_nodes,
            &ctx->imglinks_nodes,
            };
        for(size_t n = 0; n < arrlen(img_nodes); n++){
            auto nodes = img_nodes[n];
            for(size_t i = 0; i < nodes->count; i++){
                auto node = get_node(ctx, nodes->data[i]);
                if(!node->children.count)
                    continue;
                auto child = get_node(ctx, node->children.data[0]);
                if(!child->header.length)
                    continue;
                auto sv = Marray_alloc(StringView)(&job.sourcepaths, ctx->allocator);
                if(!ctx->base_directory.length){
                    *sv = child->header;
                    }
                else {
                    MStringBuilder path_builder = {};
                    msb_write_str(&path_builder, ctx->allocator, ctx->base_directory.text, ctx->base_directory.length);
                    msb_append_path(&path_builder, ctx->allocator, child->header.text, child->header.length);
                    *sv = LS_to_SV(msb_detach(&path_builder, ctx->allocator));
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
            auto before = get_t();
            create_thread(&worker, &binary_worker, &job);
            auto after = get_t();
            report_stat(flags, "Launching binary data processing took: %.3fms", (after-before)/1000.);
            }
        }
    // Execute the python blocks.
    if(!(flags & DNDC_NO_PYTHON) and ctx->python_nodes.count){
        auto before = get_t();
        // init_python_docparser handles the DNDC_PYTHON_IS_INIT flag.
        auto e = init_python_docparser(flags);
        if(e.errored) {
            report_error(flags, "Failed to initialize python\n");
            result.errored = e.errored;
            goto cleanup;
            }
        auto after = get_t();
        report_stat(flags, "Python startup took: %.3fms", (after-before)/1000.);
        for(size_t i = 0; i < ctx->python_nodes.count; i++){
            auto handle = ctx->python_nodes.data[i];
            {
            auto node = get_node(ctx, handle);
            if(node->type != NODE_PYTHON)
                continue;
            MStringBuilder msb = {};
            for(auto j = 0; j < node->children.count; j++){
                auto child = node->children.data[j];
                auto child_node = get_node(ctx, child);
                msb_write_str(&msb, ctx->allocator, child_node->header.text, child_node->header.length);
                msb_write_char(&msb, ctx->allocator, '\n');
                }
            if(!msb.cursor)
                continue;
            auto str = msb_detach(&msb, ctx->allocator);
            auto py_err = execute_python_string(ctx, str.text, handle);
            if(py_err.errored){
                report_error(flags, "%s", ctx->error_message.text);
                result.errored = py_err.errored;
                goto cleanup;
                }
            }
            auto node = get_node(ctx, handle);
            // unsure if this is right, but doing it for now.
            auto parent = get_node(ctx, node->parent);
            node->parent = INVALID_NODE_HANDLE;
            for(size_t j = 0; j < parent->children.count; j++){
                if(NodeHandle_eq(handle, parent->children.data[j])){
                    Marray_remove__NodeHandle(&parent->children, j);
                    goto after;
                    }
                }
            // don't both warning here, but leave the scaffolding in case I want to.
            after:;
            }
        auto after_python = get_t();
        report_stat(flags, "Python scripts took: %.3fms", (after_python-after)/1000.);
        report_stat(flags, "Python total took: %.3fms", (after_python-before)/1000.);
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
            report_stat(flags, "Joining took: %.3fms", (after-before)/1000.);
            }
        Marray_cleanup(StringView)(&job.sourcepaths, ctx->allocator);
        }
    return result;
    }

#ifndef NOMAIN
int main(int argc, char**argv){
    auto t0 = get_t();
    LongString source_path = {};
    LongString output_path = {};
    LongString depends_dir = {};
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
    {
    ArgToParse pos_args[] = {
        [0] = {
            .name = SV("source"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&source_path),
            .help = "Source file (.dnd file) to read from.\nIf not given, reads from stdin.",
            },
        [1] = {
            .name = SV("output"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&output_path),
            .help = "output path (.html file) to write to.\n If not given, writes to stdout.",
            .hide_default = true,
            },
        };
    ArgToParse kw_args[] = {
        {
            .name = SV("-d"),
            .altname1 = SV("--depends-dir"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&depends_dir),
            .help = "If given, what directory to write a corresponding make-style .dep file.",
            .hide_default = true,
        },
        {
            .name = SV("--report-orphans"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&report_orphans),
            .help = "Report orphaned nodes (for debugging scripts).",
        },
        {
            .name = SV("--no-python"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&no_python),
            .help = "Don't execute python nodes.",
        },
        {
            .name = SV("--print-tree"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&print_tree),
            .help = "Print out the entire document tree.",
        },
        {
            .name = SV("--print-links"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&print_links),
            .help = "Print out all links (and what they target) known by the system.",
        },
        {
            .name = SV("--print-stats"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&print_stats),
            .help = "Log some informative statistics.",
        },
        {
            .name = SV("--allow-bad-links"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&allow_bad_links),
            .help = "Warn instead of erroring if a link can't be resolved.",
        },
        {
            .name = SV("--suppress-warnings"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&suppress_warnings),
            .help = "Don't report non-fatal errors.",
        },
        {
            .name = SV("--dont-write"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&dont_write),
            .help = "Don't write out the document.\n"
                "    Outputfile is exposed to scripts so that must still be given.",
        },
        {
            .name = SV("--no-threads"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&no_threads),
            .help = "Do not create worker threads, do everything in the same thread.",
        },
        {
            .name = SV("--cleanup"),
            .min_num = 0,
            .max_num = 1,
            .dest = ARGDEST(&cleanup),
            .help = "Cleanup all resources (memory allocations, etc.).\n"
                "    Development debugging tool, useless in regular cli use."
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
                    "equivelant), etc. Imports will not be resolved - only the "
                    "given input file will be imported.\n"
                    "Implies --no-python."
                    ,
        }
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
    auto after_parse_args = get_t();
    // this one has to be done manually as we don't have a ctx yet.
    report_stat(print_stats?DNDC_PRINT_STATS:0, "Parsing args took: %.3fms", (after_parse_args-t0)/1000.);
    }

    uint64_t flags = DNDC_FLAGS_NONE;
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

    #ifdef BENCHMARKING
    if(!source_path.length){
        source_path = LS(BENCHMARKINPUTPATH);
        }
    if(!output_path.length){
        output_path = LS(BENCHMARKOUTPUTPATH);
        }
    flags &= ~DNDC_NO_CLEANUP;
    auto e = run_the_dndc(flags, SV(BENCHMARKDIRECTORY), source_path, &output_path, depends_dir, NULL);
    assert(!e.errored);
    flags |= DNDC_PYTHON_IS_INIT;
    for(int i = 0; i < BENCHMARKITERS;i++){
        e = run_the_dndc(flags, SV(BENCHMARKDIRECTORY), source_path, &output_path, depends_dir, NULL);
        assert(!e.errored);
        }
    end_interpreter();
    return 0;
    #else
    auto e = run_the_dndc(flags, SV(""), source_path, output_path.length? &output_path : NULL, depends_dir, NULL);
    return e.errored;
    #endif
    }
#endif

static
Errorable_f(void)
run_the_dndc(uint64_t flags, StringView base_directory, LongString source_path, Nullable(LongString*) output_path, LongString depends_dir, Nullable(Base64Cache*)external_b64cache){
    if(flags & DNDC_REFORMAT_ONLY)
        flags |= DNDC_NO_PYTHON;
    auto t0 = get_t();
    MStringBuilder msb = {};
    Errorable(void) result = {};
    StringView path;
    if(flags & DNDC_SOURCE_PATH_IS_DATA_NOT_PATH)
        path = SV("(string input)");
    else
        path = LS_to_SV(source_path);
    LongString outpath;
    if(!output_path){
        outpath = LS("");
        }
    else if(flags & DNDC_OUTPUT_PATH_IS_OUT_PARAM){
        outpath = LS("this.html");
        }
    else {
        outpath = *output_path;
        }
    const Allocator allocator = flags & DNDC_NO_CLEANUP?get_mallocator():new_recorded_mallocator();
    // The linear allocator is very useful for temporary allocations, like
    // when we need to turn a string into its kebabed form and then look it up
    // in the link map. We do this a lot and throw away the temporary string
    // constantly - this means we don't have to keep hitting malloc
    // just for temporary strings of arbitrary size.
    LinearAllocator la_ = new_linear_storage(1024*1024, "temp storage");
    auto la = allocator_from_la(&la_);
    DndcContext ctx = {
        .flags = flags,
        .allocator = allocator,
        .temp_allocator = la,
        .titlenode = INVALID_NODE_HANDLE,
        .navnode = INVALID_NODE_HANDLE,
        .outputfile = outpath,
        .base_directory = base_directory,
        .b64cache = external_b64cache? *external_b64cache : ({
            const Allocator cache_allocator = flags & DNDC_NO_CLEANUP?get_mallocator():new_recorded_mallocator();
            (Base64Cache){.allocator = cache_allocator};
            }),
        };
    LongString source;
    if(flags & DNDC_SOURCE_PATH_IS_DATA_NOT_PATH){
        source = source_path;
        }
    else if(!path.length){
        // read from stdin
        MStringBuilder sb = {};
        for(;;){
            enum {N = 4096};
            msb_reserve(&sb, ctx.allocator, N);
            char* buff = sb.data + sb.cursor;
            auto numread = fread(buff, 1, N, stdin);
            sb.cursor += numread;
            if(numread != N)
                break;
            }
        source = msb_detach(&sb, ctx.allocator);
        }
    else {
        auto source_err = ctx_load_source_file(&ctx, path);
        if(source_err.errored){
            report_error(flags, "Unable to open %.*s", (int)path.length, path.text);
            result.errored = source_err.errored;
            goto cleanup;
            }
        source = unwrap(source_err);
        }
    Marray_reserve(Node)(&ctx.nodes, ctx.allocator, source.length/10+1);

    // Setup the root node.
    {
        auto root_handle = alloc_handle(&ctx);
        ctx.root_handle = root_handle;
        auto root = get_node(&ctx, root_handle);
        root->col = 0;
        root->row = 0;
        root->filename = path;
        // root->type = NODE_ROOT;
        root->type = NODE_MD;
        root->parent = root_handle;
    }
    // Parse the initial document.
    {
        auto before_parse = get_t();
        auto e = dndc_parse(&ctx, ctx.root_handle, path, source.text);
        auto after_parse = get_t();
        report_stat(ctx.flags, "Initial parsing took: %.3fms", (after_parse-before_parse)/1000.);
        if(e.errored){
            report_error(flags, "%s", ctx.error_message.text);
            result.errored = e.errored;
            goto cleanup;
            }
    }
    if(flags & DNDC_REFORMAT_ONLY){
        msb_reset(&msb);
        auto before = get_t();
        format_tree(&ctx, &msb);
        auto after = get_t();
        report_stat(ctx.flags, "Formatting took: %.3fms", (after-before)/1000.);

        auto str = msb_borrow(&msb, ctx.allocator);
        auto before_write = get_t();
        if(flags & DNDC_OUTPUT_PATH_IS_OUT_PARAM){
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
        else if(!output_path || outpath.length == 0){
            fputs(str.text, stdout);
            goto success;
            }
        else {
            auto write_err = write_file(outpath.text, str.text, str.length);
            if(write_err.errored){
                ERROR("Error on write: %s", get_error_name(write_err));
                perror("Error on write");
                result.errored = write_err.errored;
                goto cleanup;
                }
            }
        auto after_write = get_t();
        report_stat(ctx.flags, "Writing took: %.3fms", (after_write-before_write)/1000.);
        report_stat(ctx.flags, "Total output size: %zu bytes", str.length);
        goto success;
        }
    // Handle imports. Imports can import more imports.
    {
        auto before_imports = get_t();
        for(size_t i = 0; i < ctx.imports.count; i++){
            auto handle = ctx.imports.data[i];
            auto node = get_node(&ctx, handle);
            for(size_t j = 0; j < node->children.count; j++, node=get_node(&ctx, handle)){
                auto child_handle = node->children.data[j];
                auto child = get_node(&ctx, child_handle);
                if(child->type != NODE_STRING){
                    node_print_err(&ctx, child, "import child is not a string");
                    result.errored = PARSE_ERROR;
                    goto cleanup;
                    }
                StringView filename = child->header;
                child->type = NODE_CONTAINER;
                child->header = SV("");
                auto imp_e = ctx_load_source_file(&ctx, filename);
                if(imp_e.errored){
                    node_print_err(&ctx, child, "Unable to open '%.*s'", (int)filename.length, filename.text);
                    result.errored = imp_e.errored;
                    goto cleanup;
                    }
                LongString imp_text = unwrap(imp_e);
                auto parse_e = dndc_parse(&ctx, child_handle, filename, imp_text.text);
                if(parse_e.errored){
                    report_error(flags, "%s", ctx.error_message.text);
                    result.errored = parse_e.errored;
                    goto cleanup;
                    }
                }
            }
        auto after_imports = get_t();
        report_stat(ctx.flags, "Resolving imports took: %.3fms", (after_imports-before_imports)/1000.);
    }

    // Speculatively load imgs and imglinks and preprocess them.
    // Do this as the same time we execute the Python nodes.
    // Python blocks can add imgs or change the paths of the img nodes,
    // but they usually don't, so doing these in parallel is a win as
    // Python startup is very slow.
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
    report_stat(ctx.flags, "ctx.nodes.count = %zu", ctx.nodes.count);
    report_stat(ctx.flags, "ctx.python_nodes.count = %zu", ctx.python_nodes.count);
    report_stat(ctx.flags, "ctx.imports.count = %zu", ctx.imports.count);
    report_stat(ctx.flags, "ctx.script_nodes.count = %zu", ctx.script_nodes.count);
    report_stat(ctx.flags, "ctx.dependencies.count = %zu", ctx.dependencies_nodes.count);
    report_stat(ctx.flags, "ctx.link_nodes.count = %zu", ctx.link_nodes.count);
    if(flags & DNDC_REPORT_ORPHANS){
        for(size_t i = 0; i < ctx.nodes.count; i++){
            auto node = &ctx.nodes.data[i];
            // python nodes get orphaned after execution
            if(node->type == NODE_PYTHON)
                continue;
            if(NodeHandle_eq(node->parent, INVALID_NODE_HANDLE)){
                node_print_warning(&ctx, node, "Orphaned node (invalid parent node handle)");
                }
            }
        }
    if(NodeHandle_eq(ctx.root_handle, INVALID_NODE_HANDLE)){
        report_error(flags, "ctx has no root Node.");
        result.errored = PARSE_ERROR;
        goto cleanup;
        }
    // Check that the tree is not too deep!
    {
        auto before = get_t();
        auto e = check_depth(&ctx);
        if(e.errored){
            report_error(flags, "%s", ctx.error_message.text);
            result.errored = e.errored;
            goto cleanup;
            }
        auto after = get_t();
        report_stat(ctx.flags, "Checking depth took %.3fms", (after-before)/1000.);
    }
    // Create links from headers.
    {
        auto before = get_t();
        gather_anchors(&ctx);
        auto after = get_t();
        report_stat(ctx.flags, "Link resolving took: %.3fms", (after-before)/1000.);
        if(ctx.error_message.length){
            report_error(flags, "%s", ctx.error_message.text);
            result.errored = PARSE_ERROR;
            goto cleanup;
            }
    }

    // Maybe should remove this option as it clogs up the cli and was just for debugging
    // before rendering was off the ground.
    if(flags & DNDC_PRINT_TREE)
        print_node_and_children(&ctx, ctx.root_handle, 0);

    // Render the nav block if we have one.
    {
        auto before = get_t();
        if(!NodeHandle_eq(ctx.navnode, INVALID_NODE_HANDLE))
            build_nav_block(&ctx);
        auto after =  get_t();
        report_stat(ctx.flags, "Nav block building took: %.3fms", (after-before)/1000.);
    }

    // Add in the links from explicit link blocks.
    {
        auto link_node_count = ctx.link_nodes.count;
        auto link_handles = ctx.link_nodes.data;
        for(size_t ln = 0; ln < link_node_count; ln++){
            auto link_node = get_node(&ctx, link_handles[ln]);
            for(size_t i = 0; i < link_node->children.count; i++){
                auto link_str_node = get_node(&ctx, link_node->children.data[i]);
                auto str = link_str_node->header;
                auto e = add_link_from_sv(&ctx, str, /*check_valid=*/true);
                if(e.errored){
                    // This looks weird, but I am formatting the error.
                    // FIXME: pass the node into the add_link_from_sv function?
                    // That way it can properly format the error itself?
                    node_set_err(&ctx, link_str_node, "%s", ctx.error_message.text);
                    report_error(flags, "%s", ctx.error_message.text);
                    result.errored = e.errored;
                    goto cleanup;
                    }
                }
            }
        // Sort so we can do a binary search.
        if(ctx.links.count)
            qsort(ctx.links.data, ctx.links.count, sizeof(ctx.links.data[0]), StringView_cmp);
        if(flags & DNDC_PRINT_LINKS){
            for(size_t i = 0; i < ctx.links.count; i++){
                auto li = &ctx.links.data[i];
                printf("[%zu] key: '%.*s', value: '%.*s'\n", i, (int)li->key.length, li->key.text, (int)li->value.length, li->value.text);
                }
            }
        report_stat(ctx.flags, "ctx.links.count = %zu", ctx.links.count);
    }

    if(unlikely(flags & DNDC_DONT_WRITE))
        goto success;

    // Render data nodes into the data blob.
    {
        auto before_data = get_t();
        MStringBuilder sb = {};
        for(size_t i = 0; i < ctx.data_nodes.count; i++){
            auto handle = ctx.data_nodes.data[i];
            auto data_node = get_node(&ctx, handle);
            // Node could've been mutated after being registered.
            if(data_node->type != NODE_DATA)
                continue;
            for(size_t j = 0; j < data_node->children.count; j++){
                auto child = get_node(&ctx, data_node->children.data[j]);
                if(!child->header.length){
                    node_print_warning(&ctx, child, "Missing header from data child?");
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
                    report_error(flags, "%s", ctx.error_message.text);
                    result.errored = e.errored;
                    goto cleanup;
                    }
                }
                if(!sb.cursor){
                    node_print_warning(&ctx, child, "Rendered a data node with no data. Not outputting it.");
                    continue;
                    }
                auto text = msb_detach(&sb, ctx.allocator);
                auto di = Marray_alloc(DataItem)(&ctx.rendered_data, ctx.allocator);
                di->key = child->header;
                di->value = text;
                }
            }
        auto after_data = get_t();
        report_stat(ctx.flags, "Data blob rendering took: %.3fms", (after_data-before_data)/1000.);
        report_stat(ctx.flags, "ctx.rendered_data.count = %zu", ctx.rendered_data.count);
    }
    // Render the actual document into a string as html.
    {
        msb_reset(&msb);
        auto before_render = get_t();
        auto e = render_tree(&ctx, &msb);
        auto after_render = get_t();
        report_stat(ctx.flags, "Rendering took: %.3fms", (after_render-before_render)/1000.);

        if(e.errored){
            report_error(flags, "%s", ctx.error_message.text);
            result.errored = e.errored;
            goto cleanup;
            }
        auto str = msb_borrow(&msb, ctx.allocator);
        auto before_write = get_t();
        if(flags & DNDC_OUTPUT_PATH_IS_OUT_PARAM){
            assert(output_path);
            // We don't use the allocator as this needs to outlive the recording
            // allocator.
            //
            // We could do this without the extra copy, but this will work for now.
            char* html_text = malloc(str.length+1);
            memcpy(html_text, str.text, str.length);
            html_text[str.length] = '\0';
            output_path->text = html_text;
            output_path->length = str.length;
            }
        else if(!output_path || outpath.length == 0){
            fputs(str.text, stdout);
            goto success;
            }
        else {
            auto write_err = write_file(outpath.text, str.text, str.length);
            if(write_err.errored){
                ERROR("Error on write: %s", get_error_name(write_err));
                perror("Error on write");
                result.errored = write_err.errored;
                goto cleanup;
                }
            }
        auto after_write = get_t();
        report_stat(ctx.flags, "Writing took: %.3fms", (after_write-before_write)/1000.);
        report_stat(ctx.flags, "Total output size: %zu bytes", str.length);
    }
    // Write the make-style dependency file to the Dependency directory.
    if(depends_dir.length){
        for(size_t i = 0; i < ctx.loaded_files.count; i++){
            Marray_push(StringView)(&ctx.dependencies, ctx.allocator, LS_to_SV(ctx.loaded_files.data[i].sourcepath));
            }
        for(size_t i = 0; i < ctx.dependencies_nodes.count; i++){
            auto handle = ctx.dependencies_nodes.data[i];
            auto node = get_node(&ctx, handle);
            for(size_t j = 0; j < node->children.count; j++){
                auto child_handle = node->children.data[j];
                auto child = get_node(&ctx, child_handle);
                if(child->type != NODE_STRING){
                    // just warn, don't want to fail the build
                    node_print_warning(&ctx, child, "Non-string node found as a child node: %s", nodenames[child->type].text);
                    continue;
                    }
                Marray_push(StringView)(&ctx.dependencies, ctx.allocator, child->header);
                }
            }
        msb_reset(&msb);
        MStringBuilder depb = {};
        msb_write_str(&depb, ctx.temp_allocator, depends_dir.text, depends_dir.length);
        auto out = LS_to_SV(outpath);
        auto basename = path_basename(out);
        auto stripped = path_strip_extension(basename);
        msb_append_path(&depb, ctx.temp_allocator, stripped.text, stripped.length);
        msb_write_literal(&depb, ctx.temp_allocator, ".dep");
        auto depfilename = msb_borrow(&depb, ctx.temp_allocator);
        msb_write_str(&msb, ctx.allocator, outpath.text, outpath.length);
        msb_write_char(&msb, ctx.allocator, ':');
        for(size_t i = 0; i < ctx.dependencies.count; i++){
            auto dep = &ctx.dependencies.data[i];
            msb_write_char(&msb, ctx.allocator, ' ');
            msb_write_str(&msb, ctx.allocator, dep->text, dep->length);
            }
        msb_write_char(&msb, ctx.allocator, '\n');
        // generate empty rules so deleted files don't fail the build
        for(size_t i = 0; i < ctx.dependencies.count; i++){
            auto dep = &ctx.dependencies.data[i];
            msb_write_str(&msb, ctx.allocator, dep->text, dep->length);
            msb_write_literal(&msb, ctx.allocator, ":\n");
            }
        auto deptext = msb_borrow(&msb, ctx.allocator);
        auto write_err = write_file(depfilename.text, deptext.text, deptext.length);
        msb_destroy(&depb, ctx.temp_allocator);
        if(write_err.errored){
            ERROR("Error on write: %s", get_error_name(write_err));
            perror("Error on write");
            result.errored = write_err.errored;
            goto cleanup;
            }
        }
    success:;
    cleanup:;
    msb_destroy(&msb, ctx.allocator);
    report_stat(ctx.flags, "la_.high_water = %zu", la_.high_water);
    if(!(flags & DNDC_NO_CLEANUP)){
        auto before = get_t();
        if(ctx.flags & DNDC_PRINT_STATS){
            RecordingAllocator* recorder = allocator._data;
            report_stat(ctx.flags, "There were %zu allocations.", recorder->recorded.count);
            size_t total = 0;
            for(size_t i = 0; i < recorder->recorded.count; i++){
                total += recorder->recorded.allocation_sizes[i];
                }
            report_stat(ctx.flags, "Allocations outstanding total: %zu", total);
            }
        Allocator_free_all(allocator);
        shallow_free_recorded_mallocator(allocator);
        destroy_linear_storage(&la_);
        if(!external_b64cache){
            Allocator_free_all(ctx.b64cache.allocator);
            shallow_free_recorded_mallocator(ctx.b64cache.allocator);
            }
        // TEMP: move ownership to caller
        auto after = get_t();
        report_stat(ctx.flags, "Cleaning up memory took: %.3fms", (after-before)/1000.);
        }
    if(external_b64cache){
        DBG("Copying back");
        memcpy(external_b64cache, &ctx.b64cache, sizeof(ctx.b64cache));
        }
    auto t1 = get_t();
    report_stat(ctx.flags, "Execution took: %.3fms", (t1-t0)/1000.);
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
    printf("[%-8s]", nodenames[node->type].text);
    switch((NodeType)node->type){
        case NODE_ROOT:
        case NODE_PARA:
        case NODE_TABLE_ROW:
        case NODE_BULLET:
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
        case NODE_DIV:{
            printf(" '%.*s' ", (int)node->header.length, node->header.text);
            for(size_t i = 0; i < node->classes.count; i++){
                auto c = &node->classes.data[i];
                printf(".%.*s ", (int)c->length, c->text);
                }
            for(size_t i = 0; i < node->attributes.count;i++){
                auto a = &node->attributes.data[i];
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
    for(size_t i = 0; i < node->children.count; i++){
        print_node_and_children(ctx, node->children.data[i], depth+1);
        }
    }

#include "dndc_python.c"
#include "dndc_htmlgen.c"
#include "dndc_parser.c"
#include "dndc_context.c"


extern
int
dndc_make_html(StringView base_directory, LongString source_text, Nonnull(LongString*)output){
    uint64_t flags = 0;
    flags |= DNDC_SOURCE_PATH_IS_DATA_NOT_PATH;
    flags |= DNDC_OUTPUT_PATH_IS_OUT_PARAM;
    flags |= DNDC_PYTHON_IS_INIT;
    // flags |= DNDC_DONT_PRINT_ERRORS;
    flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    // gross, move to caller.
    static Base64Cache cache = {.allocator._vtable = &MallocVtable};
    auto e = run_the_dndc(flags, base_directory, source_text, output, LS(""), &cache);
    return e.errored;
    }
extern
int
dndc_format(LongString source_text, Nonnull(LongString*)output){
    uint64_t flags = 0;
    flags |= DNDC_SOURCE_PATH_IS_DATA_NOT_PATH;
    flags |= DNDC_OUTPUT_PATH_IS_OUT_PARAM;
    flags |= DNDC_PYTHON_IS_INIT;
    flags |= DNDC_DONT_PRINT_ERRORS;
    flags |= DNDC_SUPPRESS_WARNINGS;
    flags |= DNDC_ALLOW_BAD_LINKS;
    flags |= DNDC_REFORMAT_ONLY;
    auto e = run_the_dndc(flags, SV(""), source_text, output, LS(""), NULL);
    return e.errored;
    }

extern
int
dndc_init_python(void){
    auto err = init_python_docparser(0);
    return err.errored;
    }


