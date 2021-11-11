#ifdef LOG_LEVEL
#undef LOG_LEVEL
#endif
#define LOG_LEVEL LOG_LEVEL_INFO

// define DNDC_API before including dndc.h
#include "dndc_api_def.h"
#include "dndc.h"
#include "dndc_long_string.h"
#include "dndc_node_types.h"
#include "dndc_format.c"
#include "dndc_types.h"
#include "dndc_funcs.h"
#include "dndc_qjs.h"

#include "path_util.h"
#include "MStringBuilder.h"
#include "msb_extensions.h"
#include "allocator.h"
#include "mallocator.h"
#include "linear_allocator.h"
#include "recording_allocator.h"
#include "arena_allocator.h"
#include "measure_time.h"
#include "thread_utils.h"
#include "ByteBuilder.h"
#include "bb_extensions.h"

#define DSORT_T LinkItem
#define DSORT_CMP StringView_cmp
#include "dsort.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#if defined(_WIN32) || defined(WASM)
// provide our own version
static
const void*_Nullable
memmem(const void* hay_, size_t haysz, const void* needle_, size_t needlesz){
    if(!hay_ || !haysz || !needle_ || !needlesz) return NULL;
    const char* hay = hay_;
    const char* needle = needle_;
    char first = *needle;
    const char* hayend = hay+haysz;
    for(;;){
        const char* c = memchr(hay, first, hayend-hay);
        if(!first) return NULL;
        if(hayend-c < needlesz) return NULL;
        if(memcmp(c, needle, needlesz) == 0)
            return c;
        hay = c+1;
        }
    }
#else
void*_Nullable memmem(const void*, size_t, const void*, size_t);
#endif

// Unsure of where to put this. So, just putting it here for now.
typedef struct BinaryJob{
    Marray(StringView) sourcepaths;
    FileCache* b64cache;
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
    // auto after = get_t();
    // fprintf(stderr, "binary worker: %.3fms\n", (after-before)/1000.);
    return 0;
    }

static
Errorable_f(void)
execute_user_scripts(DndcContext* ctx){
    Errorable(void) result = {};
    auto flags = ctx->flags;
    ArenaAllocator aa = {};
    // The rt and python are lazily initialized as they are pretty expensive
    // if not actually used.
    QJSRuntime* rt = NULL;
    QJSContext* jsctx = NULL;
#ifdef PYTHONMODULE
    bool python_is_initialized = true;
#else
    bool python_is_initialized = !!(flags & DNDC_PYTHON_IS_INIT);
#endif
    auto before = get_t();
    // Count must be re-read each time through the loop as more scripts
    // can be added by scripts.
    for(size_t i = 0; i < ctx->user_script_nodes.count; i++){
        NodeHandle handle = ctx->user_script_nodes.data[i];
        NodeType type;
        LongString str;
        NodeHandle firstchild;
        {
            Node* node = get_node(ctx, handle);
            type = node->type;
            if(type != NODE_JS && type != NODE_PYTHON)
                continue;
            if(type == NODE_PYTHON && (flags & DNDC_NO_PYTHON))
                continue;
            if(type == NODE_JS && (flags & DNDC_NO_COMPILETIME_JS))
                continue;
            if(!node_children_count(node))
                continue;
            firstchild = node_children(node)[0];
            // HACK: turn the filename into the text of the script.
            if(node_has_attribute(node, SV("import"))){
                if(node_children_count(node) != 1){
                    node_print_err(ctx, node, SV("Only 1 child of imported node allowed"));
                    result.errored = PARSE_ERROR;
                    goto cleanup;
                    }
                // NodeHandle childhandle
                Node* firstchild_node = get_node(ctx, firstchild);
                auto e = ctx_load_source_file(ctx, firstchild_node->header);
                if(e.errored){
                    node_print_err(ctx, firstchild_node, SV("Unable to load file"));
                    result.errored = e.errored;
                    goto cleanup;
                    }
                Marray_push(StringView)(&ctx->filenames, ctx->allocator, firstchild_node->header);
                firstchild_node->filename_idx = ctx->filenames.count-1;
                firstchild_node->col = 0;
                firstchild_node->row = 1;
                firstchild_node->header = LS_to_SV(e.result);
                }
            MStringBuilder msb = (MStringBuilder){.allocator=ctx->string_allocator};
            if(type == NODE_JS){
                msb_write_literal(&msb, "\"use strict\";\n");
                msb_write_nchar(&msb, '\n', node->row);
                }
            NODE_CHILDREN_FOR_EACH(it, node){
                auto child_node = get_node(ctx, *it);
                msb_write_str(&msb, child_node->header.text, child_node->header.length);
                msb_write_char(&msb, '\n');
                }
            if(!msb.cursor) // empty script block.
                continue;
            str = msb_detach(&msb);
        }
        if(type == NODE_PYTHON){
            if(!python_is_initialized){
                auto before_init = get_t();
                auto e = internal_init_dndc_python_interpreter(flags);
                if(e.errored){
                    report_system_error(ctx, SV("Failed to initialize python"));
                    result.errored = e.errored;
                    goto cleanup;
                    }
                python_is_initialized = true;
                auto after_init = get_t();
                report_time(ctx, SV("Python init took: "), after_init-before_init);
                }
            auto py_err = execute_python_string(ctx, str.text, handle, firstchild);
            if(py_err.errored){
                report_set_error(ctx);
                result.errored = py_err.errored;
                goto cleanup;
                }
            }
        else {
            assert(type == NODE_JS);
            if(!rt){
                auto before_init = get_t();
                rt = new_qjs_rt(&aa);
                assert(!jsctx);
                jsctx = new_qjs_ctx(rt, ctx);
                if(!jsctx){
                    report_system_error(ctx, SV("Failed to initialize javascript context"));
                    result.errored = GENERIC_ERROR;
                    goto cleanup;
                    }
                auto after_init = get_t();
                report_time(ctx, SV("qjs init took: "), after_init-before_init);
                }
            auto js_err = execute_qjs_string(jsctx, ctx, str.text, str.length, handle, firstchild);
            if(js_err.errored){
                report_set_error(ctx);
                result.errored = js_err.errored;
                goto cleanup;
                }
            }
        Node* node = get_node(ctx, handle);
        if(!NodeHandle_eq(node->parent, INVALID_NODE_HANDLE)){
            Node* parent = get_node(ctx, node->parent);
            node->parent = INVALID_NODE_HANDLE;
            for(size_t j = 0; j < node_children_count(parent); j++){
                if(NodeHandle_eq(handle, node_children(parent)[j])){
                    node_remove_child(parent, j, ctx->allocator);
                    goto after;
                    }
                }
            // don't both warning here, but leave the scaffolding in case I want to.
            after:;
            }
        }
    auto after_scripts = get_t();
    report_time(ctx, SV("user scripts took: "), after_scripts-before);
    cleanup:
    if(rt){
        free_qjs_rt(rt, &aa);
        }
    return result;
    }

// NOTE: we can have larger scope than this if we want.
// Slicing the work here this way is not inherent.
// However, care must be taken that the spawned thread is
// joined by the time we exit.
static
Errorable_f(void)
execute_user_scripts_and_load_images(DndcContext* ctx, Nullable(WorkerThread*) worker){
    Errorable(void) result = {};
    auto flags = ctx->flags;
    // Setup the worker thread.
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
                if(!node_children_count(node))
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
    ThreadHandle thread_worker = {};
    bool binary_work_to_be_done = !!job.sourcepaths.count;
    bool thread_created = false;
    if(binary_work_to_be_done){
        if(flags & DNDC_NO_THREADS){
            // Do it ourselves in this thread.
            binary_worker(&job);
            }
        else{
            if(worker){
                worker_submit((WorkerThread*)worker, &job);
                }
            else{
                create_thread(&thread_worker, &binary_worker, &job);
                }
            thread_created = true;
            }
        }

    result = execute_user_scripts(ctx);

    if(thread_created){
        auto before = get_t();
        if(worker){
            worker_wait((WorkerThread*)worker);
            }
        else
            join_thread(thread_worker);
        auto after = get_t();
        // This is usually very fast as the worker thread finished before python.
        report_time(ctx, SV("Joining took: "), after-before);
        }
    if(binary_work_to_be_done){
        Marray_cleanup(StringView)(&job.sourcepaths, ctx->allocator);
        }
    else {
        report_info(ctx, SV("No binary work was to be done."));
        }
    return result;
    }

static
Errorable_f(void)
run_the_dndc(uint64_t flags,
        LongString base_directory,
        LongString source_or_path,
        LongString outpath,
        Nonnull(LongString*) outstring,
        Nullable(FileCache*)external_b64cache,
        Nullable(FileCache*)external_textcache,
        Nullable(DndcErrorFunc*)error_func,
        Nullable(void*)error_user_data,
        Nullable(DndcDependencyFunc*)dependency_func,
        Nullable(void*)dependency_user_data,
        Nullable(DndcPostParseAstFunc*)ast_func,
        Nullable(void*)ast_func_user_data,
        Nullable(WorkerThread*)worker
        ){
    if(flags & DNDC_REFORMAT_ONLY){
        flags |= DNDC_NO_PYTHON;
        flags |= DNDC_NO_COMPILETIME_JS;
        }
    if(flags & DNDC_OUTPUT_EXPANDED_DND)
        flags |= DNDC_DONT_INLINE_IMAGES;
    if(flags & DNDC_INPUT_IS_UNTRUSTED){
        flags |= DNDC_NO_PYTHON;
        flags |= DNDC_NO_COMPILETIME_JS;
        flags |= DNDC_NO_THREADS;
        flags |= DNDC_DONT_INLINE_IMAGES;
        flags |= DNDC_DONT_READ;
        }
#ifdef WASM
    const bool wasm = true;
#else
    const bool wasm = false;
#endif
    auto t0 = get_t();
    Errorable(void) result = {};
    StringView path;
    if(flags & DNDC_SOURCE_IS_PATH_NOT_DATA)
        path = LS_to_SV(source_or_path);
    else
        path = SV("(string input)");
    ArenaAllocator arena_allocator = {};
    const Allocator string_allocator = {.type=ALLOCATOR_ARENA, ._data=&arena_allocator};
    ArenaAllocator main_arena = {};
    const Allocator allocator = {.type=ALLOCATOR_ARENA, ._data=&main_arena};
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
        .b64cache = external_b64cache? *external_b64cache:
            (FileCache){
                .allocator = (flags & DNDC_NO_CLEANUP)?
                    get_mallocator()
                    : new_recorded_mallocator()
                },
        // The text cache only runs on this thread so we can just use the
        // general allocator.
        .textcache = external_textcache?
            *external_textcache
            : (FileCache){.allocator=allocator},
        .error_func = error_func,
        .error_user_data = error_user_data,
        };
    MStringBuilder msb = {.allocator=ctx.allocator};
    ctx_add_builtins(&ctx);
    LongString source;
    if(!wasm && (flags & DNDC_SOURCE_IS_PATH_NOT_DATA)){
        // Temporarily clear the DONT_READ flag.
        auto old_flags = ctx.flags;
        ctx.flags &= ~DNDC_DONT_READ;
        auto source_err = ctx_load_source_file(&ctx, path);
        ctx.flags = old_flags;
        if(source_err.errored){
            MStringBuilder err_builder = {.allocator = ctx.temp_allocator};
            if(ctx.base_directory.length){
                MSB_FORMAT(&err_builder, "Unable to open '", ctx.base_directory, "/", path, "'");
                }
            else{
                MSB_FORMAT(&err_builder, "Unable to open '", path, "'");
                }
            report_system_error(&ctx, msb_borrow(&err_builder));
            msb_destroy(&err_builder);
            result.errored = source_err.errored;
            goto cleanup;
            }
            #if 0
                // Forces the allocation to be the right size and via malloc.
                // Useful for asan.
                char* text = malloc(source_err.result.length+1);
                memcpy(text, source_err.result.text, source_err.result.length);
                text[source_err.result.length] = 0;
                source.length = source_err.result.length;
                source.text = text;
            #else
                source = source_err.result;
            #endif
        }
    else {
        source = source_or_path;
        if(!source.text){
            report_system_error(&ctx, SV("String with no data given as input"));
            result.errored = UNEXPECTED_END;
            goto cleanup;
            }
        ctx_store_builtin_file(&ctx, LS("(string input)"), source);
        }
    // Quick and dirty estimate of how many nodes we will need.
    Marray_ensure_total(Node)(&ctx.nodes, ctx.allocator, source.length/10+1);

    // Setup the root node.
    {
        auto root_handle = alloc_handle(&ctx);
        ctx.root_handle = root_handle;
        auto root = get_node(&ctx, root_handle);
        root->col = 0;
        root->row = 0;
        Marray_push(StringView)(&ctx.filenames, ctx.allocator, path);
        root->filename_idx = ctx.filenames.count-1;
        root->type = NODE_MD;
        root->parent = root_handle;
    }
    // Parse the initial document.
    {
        auto before_parse = get_t();
        auto e = dndc_parse(&ctx, ctx.root_handle, path, source.text, source.length);
        auto after_parse = get_t();
        report_time(&ctx, SV("Initial parsing took: "), after_parse-before_parse);
        if(e.errored){
            report_set_error(&ctx);
            result.errored = e.errored;
            goto cleanup;
            }
    }
    if(!wasm && (flags & DNDC_REFORMAT_ONLY)){
        MStringBuilder outsb = {.allocator = get_mallocator()};
        auto before = get_t();
        auto format_error = format_tree(&ctx, &outsb);
        if(format_error.errored){
            msb_destroy(&outsb);
            result.errored = format_error.errored;
            goto cleanup;
            }
        auto after = get_t();
        report_time(&ctx, SV("Formatting took: "), after-before);
        report_size(&ctx, SV("Total output size (bytes): "), outsb.cursor);

        if(flags & DNDC_DONT_WRITE){
            msb_destroy(&outsb);
            }
        else {
            assert(outstring);
            *outstring = msb_detach(&outsb);
            }
        goto success;
        }
    if(wasm || unlikely(flags & DNDC_INPUT_IS_UNTRUSTED)){
        if(ctx.imports.count){
            auto handle = ctx.imports.data[0];
            auto node = get_node(&ctx, handle);
            node_print_err(&ctx, node, SV("Imports are illegal for untrusted input."));
            result.errored = PARSE_ERROR;
            goto cleanup;
            }
        if(ctx.user_script_nodes.count){
            auto handle = ctx.user_script_nodes.data[0];
            auto node = get_node(&ctx, handle);
            node_print_err(&ctx, node, SV("Python blocks and JS blocks are illegal for untrusted input."));
            result.errored = PARSE_ERROR;
            goto cleanup;
            }
        if(ctx.script_nodes.count){
            auto handle = ctx.script_nodes.data[0];
            auto node = get_node(&ctx, handle);
            node_print_err(&ctx, node, SV("Script blocks are illegal for untrusted input."));
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
            for(size_t j = 0; j < node_children_count(node); j++, node=get_node(&ctx, handle)){
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
                auto parse_e = dndc_parse(&ctx, child_handle, filename, imp_text.text, imp_text.length);
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
    // Do this at the same time as we execute the Python and js nodes.
    // Python/js blocks can add imgs or change the paths of the img nodes,
    // but they usually don't, so doing these in parallel is a win as
    // Python startup (and execution) is very slow. JS startup is quicker, but not
    // free.
    if(!wasm){
        // This is shoved in its own function as we need to guarantee
        // the worker has joined before continuing beyond this point.
        // Putting it in its own function with single-point-of-exit style
        // makes that easier to do.
        auto e = execute_user_scripts_and_load_images(&ctx, worker);
        if(e.errored){
            result.errored = e.errored;
            goto cleanup;
            }
    }
    // Do some reporting as we don't add any nodes after this.
    if(!wasm){
        report_size(&ctx, SV("ctx.nodes.count = "), ctx.nodes.count);
        report_size(&ctx, SV("ctx.user_script_nodes.count = "), ctx.user_script_nodes.count);
        report_size(&ctx, SV("ctx.imports.count = "), ctx.imports.count);
        report_size(&ctx, SV("ctx.script_nodes.count = "), ctx.script_nodes.count);
        report_size(&ctx, SV("ctx.dependencies_nodes.count = "), ctx.dependencies_nodes.count);
        report_size(&ctx, SV("ctx.link_nodes.count = "), ctx.link_nodes.count);
        }
    // Python blocks can detach the root node and then forget to attach a new
    // one.
    if(NodeHandle_eq(ctx.root_handle, INVALID_NODE_HANDLE)){
        report_system_error(&ctx, SV("ctx has no root Node."));
        result.errored = PARSE_ERROR;
        goto cleanup;
        }
    // Check that the tree is not too deep!
    if(!wasm){
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
        bool autoindexed = false;
        MARRAY_FOR_EACH(link_handle, ctx.link_nodes){
            auto link_node = get_node(&ctx, *link_handle);
            if(!wasm && !autoindexed && node_has_attribute(link_node, SV("autoindex"))){
                autoindexed = true;
                auto e = ctx_add_auto_index_links(&ctx);
                if(e.errored){
                    result.errored = e.errored;
                    goto cleanup;
                    }
                }
            NODE_CHILDREN_FOR_EACH(it, link_node){
                auto link_str_node = get_node(&ctx, *it);
                if(link_str_node->type != NODE_STRING)
                    continue;
                auto e = add_link_from_sv(&ctx, link_str_node);
                if(e.errored){
                    result.errored = e.errored;
                    goto cleanup;
                    }
                }
            }
        // Sort so we can do a binary search.
        if(ctx.links.count){
            auto before_sort = get_t();
            #if 1
                LinkItem__array_sort(ctx.links.data, ctx.links.count);
            #else
                qsort(ctx.links.data, ctx.links.count, sizeof(ctx.links.data[0]), StringView_cmp);
            #endif
            auto after_sort = get_t();
            report_time(&ctx, SV("Sorting links took: "), (after_sort-before_sort));
            }
        report_size(&ctx, SV("ctx.links.count = "), ctx.links.count);
    }

    // Render data nodes into the data blob.
    if(!wasm){
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
    if(!wasm && ast_func){
        int err = ast_func(ast_func_user_data, &ctx);
        if(err){
            report_system_error(&ctx, SV("Error during user defined ast func"));
            goto cleanup;
            }
        }
    if(!wasm && (flags & DNDC_OUTPUT_EXPANDED_DND)){
        MStringBuilder output_sb = {.allocator = get_mallocator()};
        auto before_render = get_t();
        auto e = expand_to_dnd(&ctx, &output_sb);
        if(e.errored){
            report_set_error(&ctx);
            msb_destroy(&output_sb);
            result.errored = e.errored;
            goto cleanup;
            }
        auto after_render = get_t();
        report_time(&ctx, SV("Expanding to .dnd took: "), after_render - before_render);
        if(flags & DNDC_DONT_WRITE){
            msb_destroy(&output_sb);
            goto success;
            }
        else {
            assert(outstring);
            *outstring = msb_detach(&output_sb);
            }
    }
    // Render the actual document into a string as html.
    else {
        MStringBuilder output_sb = {.allocator = get_mallocator()};
        auto before_render = get_t();
        auto e = render_tree(&ctx, &output_sb);

        if(e.errored){
            report_set_error(&ctx);
            msb_destroy(&output_sb);
            result.errored = e.errored;
            goto cleanup;
            }
        auto after_render = get_t();
        report_time(&ctx, SV("Rendering took: "), after_render-before_render);

        if(flags & DNDC_DONT_WRITE){
            msb_destroy(&output_sb);
            goto success;
            }
        else {
            assert(outstring);
            *outstring = msb_detach(&output_sb);
            }
    }
    // Write the make-style dependency file to the Dependency directory.
    if(!wasm && dependency_func){
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
    report_size(&ctx, SV("source.length = "), source.length);
    report_size(&ctx, SV("la_.high_water = "), la_.high_water);
    if(!wasm && !(flags & DNDC_NO_CLEANUP)){
        auto before_cleanup = get_t();
        if(ctx.flags & DNDC_PRINT_STATS){
            auto before = get_t();
#if 0
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
#else
            Arena* arena = main_arena.arena;
            while(arena){
                report_size(&ctx, SV("Arena used: "), arena->used);
                arena = arena->prev;
                }
            BigAllocation* ba = main_arena.big_allocations;
            while(ba){
                report_size(&ctx, SV("Big allocation: "), ba->size);
                ba = ba->next;
                }
#endif
            auto after = get_t();
            report_time(&ctx, SV("Reporting sizes: "), after-before);
            }
        {
            auto before = get_t();
            Allocator_free_all(string_allocator);
            auto after = get_t();
            report_time(&ctx, SV("Cleaning string allocator: "), after-before);
        }
        {
            auto before = get_t();
            Allocator_free_all(allocator);
            // shallow_free_recorded_mallocator(allocator);
            auto after = get_t();
            report_time(&ctx, SV("Cleaning allocator: "), after-before);
        }
        {
            auto before = get_t();
            if(!external_b64cache){
                Allocator_free_all(ctx.b64cache.allocator);
                shallow_free_recorded_mallocator(ctx.b64cache.allocator);
                }
            auto after = get_t();
            report_time(&ctx, SV("Cleaning b64 cache: "), after-before);
        }
        auto after = get_t();
        report_time(&ctx, SV("Cleaning up memory took: "), after-before_cleanup);
        }
    if(!wasm && external_b64cache){
        memcpy(external_b64cache, &ctx.b64cache, sizeof(ctx.b64cache));
        }
    if(!wasm && external_textcache){
        memcpy(external_textcache, &ctx.textcache, sizeof(ctx.textcache));
        }
    auto t1 = get_t();
    report_time(&ctx, SV("Execution took: "), t1-t0);
    if(!wasm && !(flags & DNDC_NO_CLEANUP))
        destroy_linear_storage(&la_);
    return result;
    }

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "dndc_expand.c"
#include "dndc_htmlgen.c"
#include "dndc_parser.c"
#include "dndc_context.c"
#include "allocator.c"

#ifndef WASM

#include "dndc_python.c"
#include "dndc_qjs.c"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#else

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

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
execute_python_string(DndcContext* ctx, const char* str, NodeHandle node, NodeHandle child){
    (void)ctx, (void)str, (void)node, (void)child;
    return (Errorable(void)){.errored=OS_ERROR};
    }

static
Errorable_f(void)
execute_qjs_string(QJSContext*jsctx, DndcContext*ctx, const char* str, size_t length, NodeHandle handle, NodeHandle firstline){
    (void)jsctx, (void)ctx, (void)str, (void)length, (void)handle, (void)firstline;
    return (Errorable(void)){.errored=OS_ERROR};
    }

static
QJSRuntime*
new_qjs_rt(ArenaAllocator*aa){
    (void)aa;
    return (void*_Nonnull)NULL;
    }

static
QJSContext*_Nullable
new_qjs_ctx(QJSRuntime*rt, DndcContext*ctx){
    (void)rt, (void)ctx;
    return NULL;
    }

static
void
free_qjs_rt(QJSRuntime*rt, ArenaAllocator*aa){
    (void)rt, (void)aa;
    }

#endif


#ifndef PYTHONMODULE
#ifndef WASM

DNDC_API
int
dndc_format(LongString source_text, LongString* output, Nullable(DndcErrorFunc*)error_func, Nullable(void*)error_user_data){
    uint64_t flags = 0
        | DNDC_PYTHON_IS_INIT
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_REFORMAT_ONLY
        ;
    auto e = run_the_dndc(flags, LS(""), source_text, LS(""), output, NULL, NULL, error_func, error_user_data, NULL, NULL, NULL, NULL, NULL);
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
dndc_stderr_error_func(Nullable(void*)unused, int type, const char* filename, int filename_len, int line, int col, const char*_Nonnull message, int message_len){
    (void)unused;
    (void)message_len;
    switch((enum DndcErrorMessageType)type){
        case DNDC_NODELESS_MESSAGE:
            fprintf(stderr, "[ERROR]: %s\n", message);
            return;
        case DNDC_STATISTIC_MESSAGE:
            fprintf(stderr, "[INFO] %s\n", message);
            return;
        case DNDC_DEBUG_MESSAGE:
            if(filename_len){
                if(col >= 0){
                    fprintf(stderr, "[DEBUG] %.*s:%d:%d: %s\n", filename_len, filename, line+1, col+1, message);
                    }
                else {
                    fprintf(stderr, "[DEBUG] %.*s:%d: %s\n", filename_len, filename, line+1, message);
                    }
                }
            else
                fprintf(stderr, "[DEBUG] %s\n", message);
            return;
        case DNDC_ERROR_MESSAGE:
            if(col >= 0){
                fprintf(stderr, "[ERROR] %.*s:%d:%d: %s\n", filename_len, filename, line+1, col+1, message);
                }
            else {
                fprintf(stderr, "[ERROR] %.*s:%d: %s\n", filename_len, filename, line+1, message);
                }
            return;
        case DNDC_WARNING_MESSAGE:
            if(col >= 0){
                fprintf(stderr, "[WARN] %.*s:%d:%d: %s\n", filename_len, filename, line+1, col+1, message);
                }
            else {
                fprintf(stderr, "[WARN] %.*s:%d: %s\n", filename_len, filename, line+1, message);
                }
            return;
        }
    // default
    if(col >= 0){
        fprintf(stderr, "%.*s:%d:%d: %s\n", filename_len, filename, line+1, col+1, message);
        }
    else {
        fprintf(stderr, "%.*s:%d: %s\n", filename_len, filename, line+1, message);
        }
    return;
    }
#endif
#endif


static
Nullable(const char*)
find_double_colon(const char* haystack, size_t length){
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

static inline
force_inline
const uint16_t* _Nullable
mem_utf16(const uint16_t* haystack, uint16_t needle, size_t ncode_units){
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
find_double_colon_utf16(const uint16_t* haystack, size_t ncode_units){
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
dndc_analyze_syntax(StringView source_text, DndcSyntaxFunc* syntax_func, Nullable(void*)syntax_data){
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
                if(memmem(stripped.text, stripped.length, "@inline", sizeof("@inline")-1)){
                    which = RAW;
                    raw_indentation = stripped.text - begin;
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
dndc_analyze_syntax_utf16(StringViewUtf16 source_text, DndcSyntaxFuncUtf16* syntax_func, Nullable(void*)syntax_data){
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
                if(memmem(stripped.text, stripped.length*2, u"@inline", sizeof(u"@inline")-2)){
                    which = RAW;
                    raw_indentation = stripped.text - begin;
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
struct DndcFileCache*
dndc_create_filecache(void){
    struct DndcFileCache* result = malloc(sizeof(*result));
    Allocator al = get_mallocator();
    *result = (struct DndcFileCache){.allocator = al};
    return result;
    }
DNDC_API
void
dndc_filecache_destroy(struct DndcFileCache* cache){
    FileCache_clear(cache);
    free(cache);
}

DNDC_API
int
dndc_filecache_remove(struct DndcFileCache* cache, StringView path){
    return FileCache_maybe_remove(cache, path);
    }

DNDC_API
void
dndc_filecache_clear(struct DndcFileCache* cache){
    FileCache_clear(cache);
    }

DNDC_API
int
dndc_filecache_has_path(struct DndcFileCache* cache, StringView path){
    return FileCache_has_file(cache, path);
    }

DNDC_API
DndcWorkerThread*
dndc_worker_thread_create(void){
    return (DndcWorkerThread*)worker_create(binary_worker);
    }

DNDC_API
void
dndc_worker_thread_destroy(DndcWorkerThread* w){
    worker_destroy((WorkerThread*)w);
    }

DNDC_API
int
dndc_compile_dnd_file(
    unsigned long long flags,
    DndcLongString base_directory,
    DndcLongString source_or_path,
    DndcLongString outpath,
    DndcLongString* outstring,
    DNDC_NULLABLE(DndcFileCache*) base64cache,
    DNDC_NULLABLE(DndcFileCache*) textcache,
    DNDC_NULLABLE(DndcErrorFunc*) error_func,
    DNDC_NULLABLE(void*) error_user_data,
    DNDC_NULLABLE(DndcDependencyFunc*) dependency_func,
    DNDC_NULLABLE(void*) dependency_user_data,
    DNDC_NULLABLE(DndcWorkerThread*) worker_thread
){
    enum {
        // All the valid flags.
        DNDC_VALID_FLAGS = 0
            | DNDC_ALLOW_BAD_LINKS
            | DNDC_SUPPRESS_WARNINGS
            | DNDC_PRINT_STATS
            | DNDC_NO_PYTHON
            | DNDC_NO_COMPILETIME_JS
            | DNDC_PYTHON_IS_INIT
            | DNDC_NO_THREADS
            | DNDC_DONT_WRITE
            | DNDC_NO_CLEANUP
            | DNDC_SOURCE_IS_PATH_NOT_DATA
            | DNDC_DONT_PRINT_ERRORS
            | DNDC_PYTHON_UNISOLATED
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
    if(!outstring)
        return GENERIC_ERROR;
    auto err = run_the_dndc(flags, base_directory, source_or_path, outpath, outstring, base64cache, textcache, error_func, error_user_data, dependency_func, dependency_user_data, NULL, NULL, (WorkerThread*)worker_thread);
    return err.errored;
    }

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
