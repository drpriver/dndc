#ifdef LOG_LEVEL
#undef LOG_LEVEL
#endif
#define LOG_LEVEL LOG_LEVEL_INFO

// define DNDC_API before including dndc.h
#include "dndc_api_def.h"
#include "dndc.h"
#include "dndc_ast.h"
#include "dndc_long_string.h"
#include "dndc_node_types.h"
#include "dndc_format.c"
#include "dndc_types.h"
#include "dndc_funcs.h"
#include "dndc_qjs.h"
#include "dndc_file_cache.h"
#include "dndc_logging.h"
#include "common_macros.h"

#include "Utils/path_util.h"
#include "Utils/MStringBuilder.h"
#include "Utils/msb_extensions.h"
#include "Utils/msb_format.h"
#include "Allocators/allocator.h"
#include "Allocators/mallocator.h"
#include "Allocators/linear_allocator.h"
#include "Allocators/recording_allocator.h"
#include "Allocators/arena_allocator.h"
#include "Utils/measure_time.h"
#include "Utils/thread_utils.h"
#ifndef WASM
#include "Utils/term_util.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

DNDC_API
int
dndc_version(void){
    return DNDC_NUMERIC_VERSION;
}

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
        if(!c) return NULL;
        if(hayend - c < (ssize_t)needlesz) return NULL;
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
    // Prepopulate the binary cache.
    BinaryJob* jobp = thread_arg;
    FileCache* cache = jobp->b64cache;
    size_t count = jobp->sourcepaths.count;
    StringView* data = jobp->sourcepaths.data;
    FileCache_preload_b64_files(cache, data, count);
    // uint64_t after = get_t();
    // fprintf(stderr, "binary worker: %.3fms\n", (after-before)/1000.);
    return 0;
}

static
warn_unused
int
execute_user_scripts(DndcContext* ctx, LongString jsargs){
    // This implementation looks a little weird as I used to have
    // both python and js scripting. It is structured to allow
    // others in the future.
    int result = 0;
    uint64_t flags = ctx->flags;
    ArenaAllocator aa = {0};
    // The rt is lazily initialized as they are pretty expensive
    // if not actually used.
    QJSRuntime* rt = NULL;
    QJSContext* jsctx = NULL;
    uint64_t before = get_t();
    // Count must be re-read each time through the loop as more scripts
    // can be added by scripts.
    for(size_t i = 0; i < ctx->user_script_nodes.count; i++){
        NodeHandle handle = ctx->user_script_nodes.data[i];
        if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
            continue;
        NodeType type;
        NodeHandle firstchild;
        MStringBuilder msb = {.allocator=string_allocator(ctx)};
        LongString str;
        {
            Node* node = get_node(ctx, handle);
            type = node->type;
            if(type != NODE_JS)
                continue;
            if(type == NODE_JS && (flags & DNDC_NO_COMPILETIME_JS))
                continue;
            if(!node_children_count(node))
                continue;
            firstchild = node_children(node)[0];
            if(type == NODE_JS){
                msb_write_literal(&msb, "\"use strict\";\n");
                msb_write_nchar(&msb, '\n', node->row);
            }
            NODE_CHILDREN_FOR_EACH(it, node){
                Node* child_node = get_node(ctx, *it);
                msb_write_str(&msb, child_node->header.text, child_node->header.length);
                msb_write_char(&msb, '\n');
            }
            if(!msb.cursor) // empty script block.
                continue;
            str = msb_borrow_ls(&msb);
        }
        {
            assert(type == NODE_JS);
            if(!rt){
                uint64_t before_init = get_t();
                rt = new_qjs_rt(&aa);
                if(!rt) {
                    report_system_error(ctx, SV("Failed to create javascript rt"));
                    result = DNDC_ERROR_JS;
                    goto cleanup;
                }
                assert(!jsctx);
                jsctx = new_qjs_ctx(rt, ctx, jsargs);
                if(!jsctx){
                    report_system_error(ctx, SV("Failed to initialize javascript context"));
                    result = DNDC_ERROR_JS;
                    goto cleanup;
                }
                uint64_t after_init = get_t();
                report_time(ctx, SV("qjs init took: "), after_init-before_init);
            }
            int js_err = execute_qjs_string(jsctx, ctx, str.text, str.length, handle, firstchild);
            msb_destroy(&msb);
            if(js_err){
                result = js_err;
                goto cleanup;
            }
        }
        Node* node = get_node(ctx, handle);
        if(!NodeHandle_eq(node->parent, INVALID_NODE_HANDLE)){
            Node* parent = get_node(ctx, node->parent);
            node->parent = INVALID_NODE_HANDLE;
            for(size_t j = 0; j < node_children_count(parent); j++){
                if(NodeHandle_eq(handle, node_children(parent)[j])){
                    node_remove_child(parent, j, main_allocator(ctx));
                    goto after;
                }
            }
            // don't bother warning here, but leave the scaffolding in case I want to.
            after:;
        }
        ctx->user_script_nodes.data[i] = INVALID_NODE_HANDLE;
    }
    uint64_t after_scripts = get_t();
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
warn_unused
int
execute_user_scripts_and_load_images(DndcContext* ctx, Nullable(WorkerThread*) worker, LongString jsargs){
    int result = 0;
    uint64_t flags = ctx->flags;
    // Setup the worker thread.
    // NOTE: a pointer to this struct is sent to the worker thread, so if
    //       you return before the thread is done, bad things will happen.
    BinaryJob job = {
        .b64cache = ctx->b64cache,
    };
    if(! (ctx->flags & (DNDC_DONT_INLINE_IMAGES | DNDC_USE_DND_URL_SCHEME | DNDC_DONT_READ))){
        // Populate a list of filepaths to load up in order
        // to pre-populate the cahce.
        Marray(NodeHandle)* img_nodes[] = {
            &ctx->img_nodes,
            &ctx->imglinks_nodes,
        };
        for(size_t n = 0; n < arrlen(img_nodes); n++){
            Marray(NodeHandle)* nodes = img_nodes[n];
            MARRAY_FOR_EACH(NodeHandle, it, *nodes){
                Node* node = get_node(ctx, *it);
                if(!node_children_count(node))
                    continue;
                Node* child = get_node(ctx, node_children(node)[0]);
                if(!child->header.length)
                    continue;
                // Already absolute or we're relative to cwd, so
                // keep it as is.
                if(path_is_abspath(child->header) || !ctx->base_directory.length){
                    if(! FileCache_has_file(job.b64cache, child->header)){
                        StringView* sv = Marray_alloc(StringView)(&job.sourcepaths, main_allocator(ctx));
                        *sv = child->header;
                    }
                }
                else {
                    // Otherwise we build the path relative to the given
                    // include directory.
                    // Get's cleaned up with the string allocator.
                    MStringBuilder path_builder = {.allocator=string_allocator(ctx)};
                    msb_write_str(&path_builder, ctx->base_directory.text, ctx->base_directory.length);
                    msb_append_path(&path_builder, child->header.text, child->header.length);
                    StringView path = msb_borrow_sv(&path_builder);
                    if(! FileCache_has_file(job.b64cache, path)){
                        StringView* sv = Marray_alloc(StringView)(&job.sourcepaths, main_allocator(ctx));
                        *sv = msb_detach_sv(&path_builder);
                    }
                    else {
                        msb_destroy(&path_builder);
                    }
                }
            }
        }
    }
    ThreadHandle thread_worker = {0};
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

    result = execute_user_scripts(ctx, jsargs);

    if(thread_created){
        uint64_t before = get_t();
        if(worker){
            worker_wait((WorkerThread*)worker);
        }
        else
            join_thread(thread_worker);
        uint64_t after = get_t();
        // This is usually very fast as the worker thread finished before scripts.
        report_time(ctx, SV("Joining took: "), after-before);
    }
    if(binary_work_to_be_done){
        Marray_cleanup(StringView)(&job.sourcepaths, main_allocator(ctx));
    }
    else {
        LOG_INFO(ctx, SV(""), 0, 0, SV("No binary work was to be done"));
        // report_info(ctx, SV("No binary work was to be done."));
    }
    return result;
}

static
int
run_the_dndc(uint64_t flags,
        StringView base_directory,
        StringView source_text,
        StringView source_path,
        Nonnull(LongString*) outstring,
        Nullable(FileCache*)external_b64cache,
        Nullable(FileCache*)external_textcache,
        Nullable(DndcLogFunc*)log_func,
        Nullable(void*)log_user_data,
        Nullable(DndcDependencyFunc*)dependency_func,
        Nullable(void*)dependency_user_data,
        Nullable(DndcPostParseAstFunc*)ast_func,
        Nullable(void*)ast_func_user_data,
        Nullable(WorkerThread*)worker,
        LongString jsargs
        ){
    // Some flags imply other flags. Set those to simplify code that
    // needs to check those conditions.
    if(flags & DNDC_REFORMAT_ONLY){
        flags |= DNDC_NO_COMPILETIME_JS;
    }
    if(flags & DNDC_OUTPUT_EXPANDED_DND)
        flags |= DNDC_DONT_INLINE_IMAGES;
    if(flags & DNDC_INPUT_IS_UNTRUSTED){
        flags |= DNDC_NO_COMPILETIME_JS;
        flags |= DNDC_NO_THREADS;
        flags |= DNDC_DONT_INLINE_IMAGES;
        flags |= DNDC_DONT_READ;
    }
    // Having const bools is easier to work with than ifdef-ing everywhere
    // and the optimizer will strip out dead code anyway.
#ifdef WASM
    const bool wasm = true;
#else
    const bool wasm = false;
#endif
    uint64_t t0 = get_t();
    // The error code returned from this function. This function has a lot of
    // resources it needs to manage, so it uses single-point-of-exit style.
    // Use `goto success` or `goto cleanup` if you need to logically early
    // return. (These are actually the same label at the moment, but it
    // signals your intent more clearly).
    int result = 0;
    if(!source_path.length)
        source_path = SV("(string input)");

    // The base64 cache is moved to another thread and then moved back, so
    // it needs an independent allocator so it can run concurrently.
    FileCache* b64cache = external_b64cache;
    if(!b64cache) b64cache = dndc_create_filecache();
    // The text cache only runs on this thread so we can just use the
    // general allocator.
    FileCache* textcache = external_textcache;
    if(!textcache) textcache = dndc_create_filecache();

    DndcContext ctx = {
        .flags = flags,
        .temp = new_linear_storage(1024*1024, "temp storage"),
        .titlenode = INVALID_NODE_HANDLE,
        .tocnode = INVALID_NODE_HANDLE,
        .base_directory = base_directory,
        .b64cache = b64cache,
        .textcache = textcache,
        .log_func = log_func,
        .log_user_data = log_user_data,
    };
    ctx.links.allocator = main_allocator(&ctx);
    if(!source_text.text){
        report_system_error(&ctx, SV("String with no data given as input"));
        result = DNDC_ERROR_PARSE;
        goto cleanup;
    }
    // Quick and dirty estimate of how many nodes we will need.
    Marray_ensure_total(Node)(&ctx.nodes, main_allocator(&ctx), source_text.length/10+1);

    // Setup the root node.
    {
        NodeHandle root_handle = alloc_handle(&ctx);
        ctx.root_handle = root_handle;
        Node* root = get_node(&ctx, root_handle);
        root->col = 0;
        root->row = 0;
        Marray_push(StringView)(&ctx.filenames, main_allocator(&ctx), source_path);
        root->filename_idx = ctx.filenames.count-1;
        root->type = NODE_MD;
        root->parent = root_handle;
    }
    // Parse the initial document.
    {
        uint64_t before_parse = get_t();
        int e = dndc_parse(&ctx, ctx.root_handle, source_path, source_text.text, source_text.length);
        uint64_t after_parse = get_t();
        report_time(&ctx, SV("Initial parsing took: "), after_parse-before_parse);
        if(e){
            result = e;
            goto cleanup;
        }
    }
    // Do reformatting if requested.
    if(unlikely(flags & DNDC_REFORMAT_ONLY)){
        MStringBuilder outsb = {.allocator = get_mallocator()};
        uint64_t before = get_t();
        int format_error = format_tree(&ctx, &outsb);
        if(format_error){
            msb_destroy(&outsb);
            result = format_error;
            goto cleanup;
        }
        uint64_t after = get_t();
        report_time(&ctx, SV("Formatting took: "), after-before);
        report_size(&ctx, SV("Total output size (bytes): "), outsb.cursor);

        if(flags & DNDC_DONT_WRITE){
            msb_destroy(&outsb);
        }
        else {
            assert(outstring);
            *outstring = msb_detach_ls(&outsb);
        }
        goto success;
    }
    // Error out on untrusted input if requested.
    if(wasm || unlikely(flags & DNDC_INPUT_IS_UNTRUSTED)){
        if(ctx.imports.count){
            NodeHandle handle = ctx.imports.data[0];
            Node* node = get_node(&ctx, handle);
            NODE_LOG_ERROR(&ctx, node, SV("Imports are illegal for untrusted input."));
            result = DNDC_ERROR_UNTRUSTED;
            goto cleanup;
        }
        if(ctx.user_script_nodes.count){
            NodeHandle handle = ctx.user_script_nodes.data[0];
            Node* node = get_node(&ctx, handle);
            NODE_LOG_ERROR(&ctx, node, SV("JS blocks are illegal for untrusted input."));
            result = DNDC_ERROR_UNTRUSTED;
            goto cleanup;
        }
        if(ctx.script_nodes.count){
            NodeHandle handle = ctx.script_nodes.data[0];
            Node* node = get_node(&ctx, handle);
            NODE_LOG_ERROR(&ctx, node, SV("Script blocks are illegal for untrusted input"));
            result = DNDC_ERROR_UNTRUSTED;
            goto cleanup;
        }
    }
    else if(flags & DNDC_DONT_IMPORT){
        // don't do imports
    }
    else {
        // Handle imports. Imports can import more imports, so don't use a FOR_EACH.
        uint64_t before_imports = get_t();
        // for(size_t i = 0; i < ctx.imports.count; progbar("Imports", &i, ctx.imports.count)){
        for(size_t i = 0; i < ctx.imports.count; i++){
            NodeHandle handle = ctx.imports.data[i];
            if(!(get_node(&ctx, handle)->flags & NODEFLAG_IMPORT))
                continue;
            // We parse into a different node and then swap the two.
            NodeHandle newhandle = alloc_handle(&ctx);
            Node* node = get_node(&ctx, handle);
            node->flags &= ~NODEFLAG_IMPORT;
            bool was_import = false;
            {
                Node* newnode = get_node(&ctx, newhandle);
                *newnode = *node;
                newnode->children.count = 0;
                if(newnode->type == NODE_IMPORT){
                    newnode->type = NODE_MD;
                    was_import = true;
                }
                newnode->attributes = Rarray_clone(Attribute)(node->attributes, main_allocator(&ctx));
            }
            if(ctx.imports.count > 1000){
                NODE_LOG_ERROR(&ctx, node, SV("More than 1000 imports. Aborting parsing (did you accidentally create an import cycle?"));
                result = DNDC_ERROR_INVALID_TREE;
                goto cleanup;
            }
            // NOTE: re-get the node every loop as the pointer is invalidated.
            for(size_t j = 0; j < node_children_count(node); j++, node=get_node(&ctx, handle)){
                NodeHandle child_handle = node_children(node)[j];
                Node* child = get_node(&ctx, child_handle);
                if(child->type != NODE_STRING){
                    NODE_LOG_ERROR(&ctx, child, SV("import child is not a STRING"));
                    result = DNDC_ERROR_INVALID_TREE;
                    goto cleanup;
                }
                StringView filename = child->header;
                StringViewResult imp_e = ctx_load_source_file(&ctx, filename);
                if(imp_e.errored){
                    if(ctx.base_directory.length)
                        NODE_LOG_ERROR(&ctx, child, "Unable to open '", ctx.base_directory, "/", filename, "'");
                    else
                        NODE_LOG_ERROR(&ctx, child, "Unable to open '", filename, "'");
                    result = imp_e.errored;
                    goto cleanup;
                }
                StringView imp_text = imp_e.result;
                int parse_e = dndc_parse(&ctx, newhandle, filename, imp_text.text, imp_text.length);
                if(parse_e){
                    result = parse_e;
                    goto cleanup;
                }
            }
            Node* newnode = get_node(&ctx, newhandle);
            if(was_import){
                // change to container
                newnode->type = NODE_CONTAINER;
            }
            Node* parent = get_node(&ctx, newnode->parent);
            NodeHandle* parentchildren = node_children(parent);
            for(size_t j = 0; j < node_children_count(parent); j++){
                if(NodeHandle_eq(parentchildren[j], handle)){
                    parentchildren[j] = newhandle;
                    break;
                }
            }
            {
                node = get_node(&ctx, handle);
                node->type = NODE_INVALID;
                node->parent = INVALID_NODE_HANDLE;
                node->children.count = 0;
            }
            Marray(NodeHandle*) handles = NULL;
            switch(newnode->type){
                case NODE_JS:
                    handles = &ctx.user_script_nodes;
                    break;
                case NODE_STYLESHEETS:
                    handles = &ctx.stylesheets_nodes;
                    break;
                case NODE_LINKS:
                    handles = &ctx.link_nodes;
                    break;
                case NODE_SCRIPTS:
                    handles = &ctx.script_nodes;
                    break;
                case NODE_IMAGE:
                    handles = &ctx.img_nodes;
                    break;
                case NODE_IMGLINKS:
                    handles = &ctx.imglinks_nodes;
                    break;
                default:
                    break;
            }
            if(handles){
                for(size_t j = 0; j < handles->count; j++){
                    if(NodeHandle_eq(handles->data[j], handle)){
                        handles->data[j] = newhandle;
                        goto foundit;
                    }
                }
                // I don't think this can happen.
                Marray_push(NodeHandle)(handles, main_allocator(&ctx), newhandle);
                foundit:;
            }
        }
        uint64_t after_imports = get_t();
        report_time(&ctx, SV("Resolving imports took: "), after_imports-before_imports);
    }

    // Speculatively load imgs and imglinks and preprocess them.
    // Do this at the same time as we execute the js nodes.
    // Js blocks can add imgs or change the paths of the img nodes,
    // but they usually don't, so doing these in parallel is a win as script execution is very slow.
    if(!wasm){
        // This is shoved in its own function as we need to guarantee
        // the worker has joined before continuing beyond this point.
        // Putting it in its own function with single-point-of-exit style
        // makes that easier to do.
        int e = execute_user_scripts_and_load_images(&ctx, worker, jsargs);
        if(e){
            result = e;
            goto cleanup;
        }
    }
    // Do some reporting as we don't add any nodes after this.
    if(!wasm){
        report_size(&ctx, SV("ctx.nodes.count = "), ctx.nodes.count);
        report_size(&ctx, SV("ctx.user_script_nodes.count = "), ctx.user_script_nodes.count);
        report_size(&ctx, SV("ctx.imports.count = "), ctx.imports.count);
        report_size(&ctx, SV("ctx.script_nodes.count = "), ctx.script_nodes.count);
        report_size(&ctx, SV("ctx.link_nodes.count = "), ctx.link_nodes.count);
    }
    // Javascript blocks can detach the root node and then forget to attach a new
    // one.
    if(NodeHandle_eq(ctx.root_handle, INVALID_NODE_HANDLE)){
        report_system_error(&ctx, SV("ctx has no root Node."));
        result = DNDC_ERROR_INVALID_TREE;
        goto cleanup;
    }
    // Create links from headers.
    {
        uint64_t before = get_t();
        gather_anchors(&ctx);
        uint64_t after = get_t();
        report_time(&ctx, SV("Link resolving took: "), after-before);
    }

    // Render the toc block if we have one.
    {
        uint64_t before = get_t();
        if(! NodeHandle_eq(ctx.tocnode, INVALID_NODE_HANDLE))
            build_toc_block(&ctx);
        uint64_t after =  get_t();
        report_time(&ctx, SV("Nav block building took: "), after-before);
    }

    // Add in the links from explicit link blocks.
    {
        MARRAY_FOR_EACH(NodeHandle, link_handle, ctx.link_nodes){
            Node* link_node = get_node(&ctx, *link_handle);
            NODE_CHILDREN_FOR_EACH(it, link_node){
                Node* link_str_node = get_node(&ctx, *it);
                if(link_str_node->type != NODE_STRING)
                    continue;
                int e = add_link_from_sv(&ctx, link_str_node);
                if(e){
                    result = e;
                    goto cleanup;
                }
            }
        }
        report_size(&ctx, SV("ctx.links.count = "), ctx.links.count_);
    }

    // User ast func
    if(!wasm && ast_func){
        int err = ast_func(ast_func_user_data, &ctx);
        if(err){
            report_system_error(&ctx, SV("Error during user defined ast func"));
            goto cleanup;
        }
    }
    // Render as a .dnd file if requested.
    if(!wasm && (flags & DNDC_OUTPUT_EXPANDED_DND)){
        MStringBuilder output_sb = {.allocator = get_mallocator()};
        uint64_t before_render = get_t();
        int e = expand_to_dnd(&ctx, &output_sb);
        if(e){
            msb_destroy(&output_sb);
            result = e;
            goto cleanup;
        }
        uint64_t after_render = get_t();
        report_time(&ctx, SV("Expanding to .dnd took: "), after_render - before_render);
        if(flags & DNDC_DONT_WRITE){
            msb_destroy(&output_sb);
            goto success;
        }
        else {
            assert(outstring);
            *outstring = msb_detach_ls(&output_sb);
        }
    }
    // Render the actual document into a string as html.
    else {
        MStringBuilder output_sb = {.allocator = get_mallocator()};
        uint64_t before_render = get_t();
        int e = render_tree(&ctx, &output_sb);

        if(e){
            msb_destroy(&output_sb);
            result = e;
            goto cleanup;
        }
        uint64_t after_render = get_t();
        report_time(&ctx, SV("Rendering took: "), after_render-before_render);

        if(flags & DNDC_DONT_WRITE){
            msb_destroy(&output_sb);
            goto success;
        }
        else {
            assert(outstring);
            *outstring = msb_detach_ls(&output_sb);
        }
    }
    // Call the user's dependency function so they can write a Makestyle
    // dependency file, or watch those files, or whatever.
    // Do this after rendering as we unfortunately read files (I think just images)
    // during render.
    if(!wasm && dependency_func){
        int err = dependency_func(dependency_user_data, ctx.dependencies.count, ctx.dependencies.data);
        if(err){
            result = err;
            goto cleanup;
        }
    }
    // It's all over!
    success:;
    cleanup:;
    report_size(&ctx, SV("source_text.length = "), source_text.length);
    report_size(&ctx, SV("la_.high_water = "), ctx.temp.high_water);
    if(!wasm && !(flags & DNDC_NO_CLEANUP)){
        uint64_t before_cleanup = get_t();
        if(ctx.flags & DNDC_PRINT_STATS){
            uint64_t before = get_t();
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
            Arena* arena = ctx.main_arena.arena;
            while(arena){
                report_size(&ctx, SV("Arena used: "), arena->used);
                report_size(&ctx, SV("Arena size: "), ARENA_BUFFER_SIZE);
                arena = arena->prev;
            }
            BigAllocation* ba = ctx.main_arena.big_allocations;
            while(ba){
                report_size(&ctx, SV("Big allocation: "), ba->size);
                ba = ba->next;
            }
            #endif
            uint64_t after = get_t();
            report_time(&ctx, SV("Reporting sizes: "), after-before);
        }
        {
            uint64_t before = get_t();
            #if 0
            Arena* arena = ctx.string_arena.arena;
            while(arena){
                report_size(&ctx, SV("String Arena used: "), arena->used);
                report_size(&ctx, SV("String Arena size: "), ARENA_BUFFER_SIZE);
                arena = arena->prev;
            }
            BigAllocation* ba = ctx.string_arena.big_allocations;
            while(ba){
                report_size(&ctx, SV("String Big allocation: "), ba->size);
                ba = ba->next;
            }
            #endif
            Allocator_free_all(string_allocator(&ctx));
            uint64_t after = get_t();
            report_time(&ctx, SV("Cleaning string allocator: "), after-before);
        }
        {
            uint64_t before = get_t();
            Allocator_free_all(main_allocator(&ctx));
            // shallow_free_recorded_mallocator(allocator);
            uint64_t after = get_t();
            report_time(&ctx, SV("Cleaning allocator: "), after-before);
        }
        uint64_t after = get_t();
        report_time(&ctx, SV("Cleaning up memory took: "), after-before_cleanup);
    }
    if(!external_b64cache)
        dndc_filecache_destroy(ctx.b64cache);
    if(!external_textcache)
        dndc_filecache_destroy(ctx.textcache);
    uint64_t t1 = get_t();
    report_time(&ctx, SV("Execution took: "), t1-t0);
    if(!wasm && !(flags & DNDC_NO_CLEANUP))
        destroy_linear_storage(&ctx.temp);
    return result;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "dndc_expand.c"
#include "dndc_htmlgen.c"
#include "dndc_parser.c"
#include "dndc_context.c"
#include "dndc_file_cache.c"
#include "dndc_logging.c"
#include "Allocators/allocator.c"

#ifndef WASM

#include "dndc_qjs.c"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#else

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

// Stubs for wasm
static
int
execute_qjs_string(QJSContext*jsctx, DndcContext*ctx, const char* str, size_t length, NodeHandle handle, NodeHandle firstline){
    (void)jsctx, (void)ctx, (void)str, (void)length, (void)handle, (void)firstline;
    return DNDC_ERROR_OS;
}

static
QJSRuntime*_Nullable
new_qjs_rt(ArenaAllocator*aa){
    (void)aa;
    return NULL;
}

static
QJSContext*_Nullable
new_qjs_ctx(QJSRuntime*rt, DndcContext*ctx, LongString args){
    (void)rt, (void)ctx, (void)args;
    return NULL;
}

static
void
free_qjs_rt(QJSRuntime*rt, ArenaAllocator*aa){
    (void)rt, (void)aa;
}

#endif


#ifndef WASM

DNDC_API
int
dndc_format(StringView source_text, LongString* output, Nullable(DndcLogFunc*)log_func, Nullable(void*)log_user_data){
    uint64_t flags = 0
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_REFORMAT_ONLY
        ;
    int e = run_the_dndc(flags, SV(""), source_text, SV(""), output, NULL, NULL, log_func, log_user_data, NULL, NULL, NULL, NULL, NULL, LS(""));
    return e;
}

DNDC_API
void
dndc_free_string(LongString str){
    const_free(str.text);
}

DNDC_API
void
dndc_stderr_log_func(Nullable(void*)unused, int type, const char* filename, int filename_len, int line, int col, const char*_Nonnull message, int message_len){
    (void)unused;
    static int interactive = -1;
    if(interactive == -1){
        interactive = isatty(fileno(stderr));
    }
    const char* Error = "ERROR";
    const char* Info = "INFO";
    const char* Debug = "DEBUG";
    const char* Warn = "WARN";
    if(interactive){
        #define RED "\033[31m"
        #define PURPLE "\033[35m"
        #define GREEN "\033[32m"
        #define RESET "\033[0m"
        #define CYAN "\033[36m"
        Error = RED "ERROR" RESET;
        Info = GREEN "INFO" RESET;
        Debug = CYAN "DEBUG" RESET;
        Warn = PURPLE "WARN" RESET;
        #undef RED
        #undef PURPLE
        #undef GREEN
        #undef RESET
    }
    switch((enum DndcLogMessageType)type){
        case DNDC_NODELESS_MESSAGE:
            fprintf(stderr, "[%s]: %.*s\n", Error, message_len, message);
            return;
        case DNDC_STATISTIC_MESSAGE:
            fprintf(stderr, "[%s] %.*s\n", Info, message_len, message);
            return;
        case DNDC_DEBUG_MESSAGE:
            if(filename_len){
                if(col >= 0){
                    fprintf(stderr, "[%s] %.*s:%d:%d: %.*s\n", Debug, filename_len, filename, line+1, col+1, message_len, message);
                }
                else {
                    fprintf(stderr, "[%s] %.*s:%d: %.*s\n", Debug, filename_len, filename, line+1, message_len, message);
                }
            }
            else
                fprintf(stderr, "[%s] %.*s\n", Debug, message_len, message);
            return;
        case DNDC_ERROR_MESSAGE:
            if(col >= 0){
                fprintf(stderr, "[%s] %.*s:%d:%d: %.*s\n", Error, filename_len, filename, line+1, col+1, message_len, message);
            }
            else {
                fprintf(stderr, "[%s] %.*s:%d: %.*s\n", Error, filename_len, filename, line+1, message_len, message);
            }
            return;
        case DNDC_WARNING_MESSAGE:
            if(col >= 0){
                fprintf(stderr, "[%s] %.*s:%d:%d: %.*s\n", Warn, filename_len, filename, line+1, col+1, message_len, message);
            }
            else {
                fprintf(stderr, "[%s] %.*s:%d: %.*s\n", Warn, filename_len, filename, line+1, message_len, message);
            }
            return;
    }
    // default
    if(col >= 0){
        fprintf(stderr, "%.*s:%d:%d: %.*s\n", filename_len, filename, line+1, col+1, message_len, message);
    }
    else {
        fprintf(stderr, "%.*s:%d: %.*s\n", filename_len, filename, line+1, message_len, message);
    }
    return;
}
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


struct JsStyleState {
    int js_parse_which;
    bool can_regex;
};
static
void
dndc_analyze_syntax_js(struct JsStyleState* state, StringView line, DndcSyntaxFunc* syntax_func, Nullable(void*)syntax_data, int lineno, int indentation);

DNDC_API
int
dndc_analyze_syntax(StringView source_text, DndcSyntaxFunc* syntax_func, Nullable(void*)syntax_data){
    // this is only needed for raw nodes
    ptrdiff_t raw_indentation = 0;
    int line = 0;
    const char* begin = source_text.text;
    const char* const end = begin + source_text.length;
    enum WhichNode {
        GENERIC = 0,
        JAVASCRIPT = 1,
        RAW = 2,
    };
    struct JsStyleState jsstyle = {0};
    enum WhichNode which = GENERIC;
    for(;begin != end;line++){
        const char* endline = memchr(begin, '\n', end-begin);
        if(!endline)
            endline = end;
        StringView stripped = lstripped_view(begin, endline-begin);
        ptrdiff_t indent = stripped.text - begin;
        if(stripped.length && indent <= raw_indentation)
            which = GENERIC;
        if(which == JAVASCRIPT){
            dndc_analyze_syntax_js(&jsstyle, stripped, syntax_func, syntax_data, line, indent);
        }
        else if(which == RAW){
            syntax_func(syntax_data, DNDC_SYNTAX_RAW_STRING, line, indent, stripped.text, stripped.length);
        }
        else {
            const char* doublecolon = find_double_colon(stripped.text, stripped.length);
            if(! doublecolon){
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
                        case CASE_a_z:
                            continue;
                        default:
                            break;
                    }
                    break;
                }
                StringView nodename = SV("");
                if(nodenameend != aftercolon.text){
                    nodename = (StringView){.text=aftercolon.text, .length=nodenameend-aftercolon.text};
                    syntax_func(syntax_data, DNDC_SYNTAX_NODE_TYPE, line, nodename.text-begin, nodename.text, nodename.length);
                    for(size_t i = 0; i < arrlen(RAW_NODES); i++){
                        if(SV_equals(nodename, RAW_NODES[i])){
                            if(i == RAW_NODE_JS_INDEX || i == RAW_NODE_SCRIPT_INDEX){
                                jsstyle = (struct JsStyleState){0};
                                which = JAVASCRIPT;
                            }
                            else {
                                which = RAW;
                            }
                            raw_indentation = stripped.text - begin;
                            break;
                        }
                    }
                }
                if(memmem(stripped.text, stripped.length, "#import", sizeof("#import")-1)){
                    which = GENERIC;
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
                                    case CASE_a_z:
                                    case CASE_A_Z:
                                    case CASE_0_9:
                                    case '-': case '_':
                                        continue;
                                    default:
                                        break;
                                }
                                break;
                            }
                            syntax_func(syntax_data, DNDC_SYNTAX_ATTRIBUTE, line, attrfirst-begin, attrfirst, postnodename-attrfirst);
                            if(postnodename != endline && *postnodename == '('){
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
                        case '#':{
                            const char* attrfirst = postnodename;
                            postnodename++;
                            for(;postnodename != endline;postnodename++){
                                char c = *postnodename;
                                switch(c){
                                    case CASE_a_z:
                                    case CASE_A_Z:
                                    case CASE_0_9:
                                    case '-': case '_':
                                        continue;
                                    default:
                                        break;
                                }
                                break;
                            }
                            syntax_func(syntax_data, DNDC_SYNTAX_DIRECTIVE, line, attrfirst-begin, attrfirst, postnodename-attrfirst);
                            if(postnodename != endline && *postnodename == '('){
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
                                    case CASE_a_z:
                                    case CASE_A_Z:
                                    case CASE_0_9:
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

static inline
_Bool
js_syntax_is_word(char c){
    switch(c){
        case CASE_a_z:
        case CASE_A_Z:
        case CASE_0_9:
        case '_':
        case '$':
            return true;
        default:
            return false;
    }
}

static inline
_Bool
js_syntax_is_keyword(StringView str){
    // keywords that are ignored:
    //   - delete
    //   - debugger
    //   - enum (javascript reserves, but doesn't have)
    #define words "|" \
        "break|case|catch|continue|default|do|" \
        "else|finally|for|function|if|in|instanceof|new|" \
        "return|switch|throw|try|typeof|while|with|" \
        "class|import|export|extends|" \
        "implements|interface|package|private|protected|" \
        "public|static|yield|" \
        "eval|" \
        "await|"
    const char* match = memmem(words, sizeof(words)-1, str.text, str.length);
    if(!match) return 0;
    return match[str.length] == '|' && match[-1] == '|';
    #undef words
}
static inline
_Bool
js_syntax_is_var(StringView str){
    #define words "|let|var|const|"
    const char* match = memmem(words, sizeof(words)-1, str.text, str.length);
    if(!match) return 0;
    return match[str.length] == '|' && match[-1] == '|';
    #undef words
}
static inline
_Bool
js_syntax_is_keyword_literal(StringView str){
    #define words "|this|super|undefined|null|true|false|Infinity|NaN|arguments|"
    const char* match = memmem(words, sizeof(words)-1, str.text, str.length);
    if(!match) return 0;
    return match[str.length] == '|' && match[-1] == '|';
    #undef words
}
static inline
_Bool
js_syntax_is_builtin(StringView str){
    #define words "|FileSystem|JSON|console|NodeType|ctx|node|"
    const char* match = memmem(words, sizeof(words)-1, str.text, str.length);
    if(!match) return 0;
    return match[str.length] == '|' && match[-1] == '|';
    #undef words
}
static inline
_Bool
js_syntax_is_node_type(StringView str){
    #define words "|MD|DIV|STRING|PARA|TITLE|HEADING|TABLE|TABLE_ROW|STYLESHEETS|LINKS|SCRIPTS|IMPORT|IMAGE|BULLETS|RAW|PRE|LIST|LIST_ITEM|KEYVALUE|KEYVALUEPAIR|IMGLINKS|TOC|COMMENT|CONTAINER|QUOTE|JS|DETAILS|"
    const char* match = memmem(words, sizeof(words)-1, str.text, str.length);
    if(!match) return 0;
    return match[str.length] == '|' && match[-1] == '|';
    #undef words
}

static
void
dndc_analyze_syntax_js(struct JsStyleState* state, StringView line, DndcSyntaxFunc* syntax_func, Nullable(void*)syntax_data, int lineno, int indentation){
    size_t n = line.length;
    const char* str = line.text;
    size_t i = 0;
    size_t start = i;
    char c;
    enum JsParseWhich {
        JS_PARSE_GENERIC = 0,
        JS_PARSE_BLOCK_COMMENT,
        JS_PARSE_MULTILINE_STRING,
    };
    switch(state->js_parse_which){
        case JS_PARSE_GENERIC:
            goto generic;
        // block comments can span multiple lines, so we need to be
        // able to resume to them.
        case JS_PARSE_BLOCK_COMMENT:
            goto blockcomment;
        // Same with multiline strings
        case JS_PARSE_MULTILINE_STRING:
            goto multilinestring;
        default:
            unreachable();
    }
    generic:
    state->js_parse_which = JS_PARSE_GENERIC;
    for(; i < n;){
        start = i;
        c = str[i++];
        switch(c){
            case ' ':
            case '\t':
            case '\r':
                continue;
            case '+':
            case '-':
                if(i < n && str[i] == c){
                    // -- or ++ token
                    i++;
                    continue;
                }
                state->can_regex = 1;
                continue;
            case '/':
                if(i < n && str[i] == '*'){
                    // record that we should continue here if the line
                    // ends.
                    state->js_parse_which = JS_PARSE_BLOCK_COMMENT;
                    blockcomment:
                    // parse block comment.
                    for(i++; i < n; i++){
                        if(str[i] == '*' && str[i+1] == '/'){
                            i += 2;
                            syntax_func(syntax_data, DNDC_SYNTAX_JS_COMMENT, lineno, indentation+start, str+start, i - start);
                            goto generic;
                        }
                    }
                    // Unterminated comment.
                    syntax_func(syntax_data, DNDC_SYNTAX_JS_COMMENT, lineno, indentation+start, str+start, i - start);
                    // We will resume to parsing block comments.
                    return;
                }
                if(i < n && str[i] == '/'){
                    // line comment
                    syntax_func(syntax_data, DNDC_SYNTAX_JS_COMMENT, lineno, indentation+start, str+start, n-start);
                    // We will resume to generic.
                    return;
                }
                if(state->can_regex){
                    state->can_regex = 0;
                    // parse regex
                    while(i < n){
                        c = str[i++];
                        if(c == '\\'){
                            if(i < n)
                                i++;
                            continue;
                        }
                        if(c == '/'){
                            while(i < n && js_syntax_is_word(str[i]))
                                i++;
                            break;
                        }
                    }
                    // Either we terminated or we have an unterminated regex
                    syntax_func(syntax_data, DNDC_SYNTAX_JS_REGEX, lineno, indentation+start, str+start, i - start);
                    continue;
                }
                state->can_regex = 1;
                continue;
            case '\'':
            case '\"':
                state->can_regex = 0;
                // parse string
                {
                    char delim = c;
                    while(i < n){
                        c = str[i++];
                        if(c == '\\'){
                            if(i >= n)
                                break;
                            // skip next character
                            i++;
                        }
                        else if(c == delim){
                            break;
                        }
                    }
                    syntax_func(syntax_data, DNDC_SYNTAX_JS_STRING, lineno, indentation+start, str+start, i - start);
                }
                break;
            case '`':
                state->can_regex = 0;
                state->js_parse_which = JS_PARSE_MULTILINE_STRING;
                // parse multiline string
                {
                    multilinestring:
                    while(i < n){
                        c = str[i++];
                        if(c == '\\'){
                            if(i >= n)
                                break;
                            // skip next character
                            i++;
                        }
                        else if(c == '`'){
                            syntax_func(syntax_data, DNDC_SYNTAX_JS_STRING, lineno, indentation+start, str+start, i - start);
                            goto generic;
                        }
                    }
                    // Didn't find delimiter, multiline string, resume
                    // here.
                    syntax_func(syntax_data, DNDC_SYNTAX_JS_STRING, lineno, indentation+start, str+start, i - start);
                    return;
                }
                break;
            case '(':
                state->can_regex = 1;
                break;
            case '[':
                state->can_regex = 1;
                break;
            case '{':
                syntax_func(syntax_data, DNDC_SYNTAX_JS_BRACE, lineno, indentation+start, str+start, 1);
                state->can_regex = 1;
                break;
            case ')':
                state->can_regex = 0;
                break;
            case ']':
                state->can_regex = 0;
                break;
            case '}':
                syntax_func(syntax_data, DNDC_SYNTAX_JS_BRACE, lineno, indentation+start, str+start, 1);
                state->can_regex = 0;
                break;
            case CASE_0_9:
                state->can_regex = 0;
                // parse number
                while(i < n && (js_syntax_is_word(str[i]) || (str[i] == '.' && (i == n - 1 || str[i+1] != '.'))))
                    i++;
                syntax_func(syntax_data, DNDC_SYNTAX_JS_NUMBER, lineno, indentation+start, str+start, i - start);
                break;
            case CASE_a_z:
            case CASE_A_Z:
            case '$': case '_':
                state->can_regex = 1;
                // parse identifier
                {
                    while(i < n && js_syntax_is_word(str[i]))
                        i++;
                    StringView substr = {.text=str+start, .length = i - start};
                    // figure out which identifier it is
                    enum DndcSyntax style = DNDC_SYNTAX_JS_IDENTIFIER;
                    if(js_syntax_is_keyword(substr)){
                        style = DNDC_SYNTAX_JS_KEYWORD;
                    }
                    else if(js_syntax_is_var(substr)){
                        style = DNDC_SYNTAX_JS_VAR;
                    }
                    else if(js_syntax_is_keyword_literal(substr)){
                        style = DNDC_SYNTAX_JS_KEYWORD_VALUE;
                        state->can_regex = 0;
                    }
                    else if(js_syntax_is_builtin(substr)){
                        style = DNDC_SYNTAX_JS_BUILTIN;
                    }
                    else if(js_syntax_is_node_type(substr)){
                        style = DNDC_SYNTAX_JS_NODETYPE;
                    }
                    else {
                        state->can_regex = 0;
                    }
                    syntax_func(syntax_data, style, lineno, indentation+start, str+start, i - start);
                }
                break;
            default:
                state->can_regex = 1;
                continue;
        }
    }
    return;

}

static inline
_Bool
js_syntax_is_word_utf16(uint16_t c){
    switch(c){
        case CASE_u16_a_z:
        case CASE_u16_A_Z:
        case CASE_u16_0_9:
        case u'_':
        case u'$':
            return true;
        default:
            return false;
    }
}

static inline
_Bool
js_syntax_is_keyword_utf16(StringViewUtf16 str){
    // keywords that are ignored:
    //   - delete
    //   - debugger
    //   - enum (javascript reserves, but doesn't have)
    #define words u"|" \
        "break|case|catch|continue|default|do|" \
        "else|finally|for|function|if|in|instanceof|new|" \
        "return|switch|throw|try|typeof|while|with|" \
        "class|import|export|extends|" \
        "implements|interface|package|private|protected|" \
        "public|static|yield|" \
        "eval|" \
        "await|"
    const uint16_t* match = memmem(words, sizeof(words)-2, str.text, str.length*2);
    if(!match) return 0;
    return match[str.length] == u'|' && match[-1] == u'|';
    #undef words
}
static inline
_Bool
js_syntax_is_var_utf16(StringViewUtf16 str){
    #define words u"|let|var|const|"
    const uint16_t* match = memmem(words, sizeof(words)-2, str.text, str.length*2);
    if(!match) return 0;
    return match[str.length] == u'|' && match[-1] == u'|';
    #undef words
}
static inline
_Bool
js_syntax_is_keyword_literal_utf16(StringViewUtf16 str){
    #define words u"|this|super|undefined|null|true|false|Infinity|NaN|arguments|"
    const uint16_t* match = memmem(words, sizeof(words)-2, str.text, str.length*2);
    if(!match) return 0;
    return match[str.length] == u'|' && match[-1] == u'|';
    #undef words
}
static inline
_Bool
js_syntax_is_builtin_utf16(StringViewUtf16 str){
    #define words u"|FileSystem|JSON|console|NodeType|ctx|node|"
    const uint16_t* match = memmem(words, sizeof(words)-2, str.text, str.length*2);
    if(!match) return 0;
    return match[str.length] == u'|' && match[-1] == u'|';
    #undef words
}
static inline
_Bool
js_syntax_is_node_type_utf16(StringViewUtf16 str){
    #define words u"|MD|DIV|STRING|PARA|TITLE|HEADING|TABLE|TABLE_ROW|STYLESHEETS|LINKS|SCRIPTS|IMPORT|IMAGE|BULLETS|RAW|PRE|LIST|LIST_ITEM|KEYVALUE|KEYVALUEPAIR|IMGLINKS|TOC|COMMENT|CONTAINER|QUOTE|JS|DETAILS|"
    const uint16_t* match = memmem(words, sizeof(words)-2, str.text, str.length*2);
    if(!match) return 0;
    return match[str.length] == u'|' && match[-1] == u'|';
    #undef words
}

static
void
dndc_analyze_syntax_js_utf16(struct JsStyleState* state, StringViewUtf16 line, DndcSyntaxFuncUtf16* syntax_func, Nullable(void*)syntax_data, int lineno, int indentation){
    size_t n = line.length;
    const uint16_t* str = line.text;
    size_t i = 0;
    size_t start = i;
    uint16_t c;
    enum JsParseWhich {
        JS_PARSE_GENERIC = 0,
        JS_PARSE_BLOCK_COMMENT,
        JS_PARSE_MULTILINE_STRING,
    };
    switch(state->js_parse_which){
        case JS_PARSE_GENERIC:
            goto generic;
        // block comments can span multiple lines, so we need to be
        // able to resume to them.
        case JS_PARSE_BLOCK_COMMENT:
            goto blockcomment;
        // Same with multiline strings
        case JS_PARSE_MULTILINE_STRING:
            goto multilinestring;
        default:
            unreachable();
    }
    generic:
    state->js_parse_which = JS_PARSE_GENERIC;
    for(; i < n;){
        start = i;
        c = str[i++];
        switch(c){
            case u' ':
            case u'\t':
            case u'\r':
                continue;
            case u'+':
            case u'-':
                if(i < n && str[i] == c){
                    // -- or ++ token
                    i++;
                    continue;
                }
                state->can_regex = 1;
                continue;
            case u'/':
                if(i < n && str[i] == u'*'){
                    // record that we should continue here if the line
                    // ends.
                    state->js_parse_which = JS_PARSE_BLOCK_COMMENT;
                    blockcomment:
                    // parse block comment.
                    for(i++; i < n; i++){
                        if(str[i] == u'*' && str[i+1] == u'/'){
                            i += 2;
                            syntax_func(syntax_data, DNDC_SYNTAX_JS_COMMENT, lineno, indentation+start, str+start, i - start);
                            goto generic;
                        }
                    }
                    // Unterminated comment.
                    syntax_func(syntax_data, DNDC_SYNTAX_JS_COMMENT, lineno, indentation+start, str+start, i - start);
                    // We will resume to parsing block comments.
                    return;
                }
                if(i < n && str[i] == u'/'){
                    // line comment
                    syntax_func(syntax_data, DNDC_SYNTAX_JS_COMMENT, lineno, indentation+start, str+start, n-start);
                    // We will resume to generic.
                    return;
                }
                if(state->can_regex){
                    state->can_regex = 0;
                    // parse regex
                    while(i < n){
                        c = str[i++];
                        if(c == u'\\'){
                            if(i < n)
                                i++;
                            continue;
                        }
                        if(c == u'/'){
                            while(i < n && js_syntax_is_word(str[i]))
                                i++;
                            break;
                        }
                    }
                    // Either we terminated or we have an unterminated regex
                    syntax_func(syntax_data, DNDC_SYNTAX_JS_REGEX, lineno, indentation+start, str+start, i - start);
                    continue;
                }
                state->can_regex = 1;
                continue;
            case u'\'':
            case u'\"':
                state->can_regex = 0;
                // parse string
                {
                    uint16_t delim = c;
                    while(i < n){
                        c = str[i++];
                        if(c == u'\\'){
                            if(i >= n)
                                break;
                            // skip next character
                            i++;
                        }
                        else if(c == delim){
                            break;
                        }
                    }
                    syntax_func(syntax_data, DNDC_SYNTAX_JS_STRING, lineno, indentation+start, str+start, i - start);
                }
                break;
            case u'`':
                state->can_regex = 0;
                state->js_parse_which = JS_PARSE_MULTILINE_STRING;
                // parse multiline string
                {
                    multilinestring:
                    while(i < n){
                        c = str[i++];
                        if(c == u'\\'){
                            if(i >= n)
                                break;
                            // skip next character
                            i++;
                        }
                        else if(c == u'`'){
                            syntax_func(syntax_data, DNDC_SYNTAX_JS_STRING, lineno, indentation+start, str+start, i - start);
                            goto generic;
                        }
                    }
                    // Didn't find delimiter, multiline string, resume
                    // here.
                    syntax_func(syntax_data, DNDC_SYNTAX_JS_STRING, lineno, indentation+start, str+start, i - start);
                    return;
                }
                break;
            case u'(':
                state->can_regex = 1;
                break;
            case u'[':
                state->can_regex = 1;
                break;
            case u'{':
                syntax_func(syntax_data, DNDC_SYNTAX_JS_BRACE, lineno, indentation+start, str+start, 1);
                state->can_regex = 1;
                break;
            case u')':
                state->can_regex = 0;
                break;
            case u']':
                state->can_regex = 0;
                break;
            case u'}':
                syntax_func(syntax_data, DNDC_SYNTAX_JS_BRACE, lineno, indentation+start, str+start, 1);
                state->can_regex = 0;
                break;
            case CASE_u16_0_9:
                state->can_regex = 0;
                // parse number
                while(i < n && (js_syntax_is_word_utf16(str[i]) || (str[i] == u'.' && (i == n - 1 || str[i+1] != u'.'))))
                    i++;
                syntax_func(syntax_data, DNDC_SYNTAX_JS_NUMBER, lineno, indentation+start, str+start, i - start);
                break;
            case CASE_u16_a_z:
            case CASE_u16_A_Z:
            case u'$': case u'_':
                state->can_regex = 1;
                // parse identifier
                {
                    while(i < n && js_syntax_is_word_utf16(str[i]))
                        i++;
                    StringViewUtf16 substr = {.text=str+start, .length = i - start};
                    // figure out which identifier it is
                    enum DndcSyntax style = DNDC_SYNTAX_JS_IDENTIFIER;
                    if(js_syntax_is_keyword_utf16(substr)){
                        style = DNDC_SYNTAX_JS_KEYWORD;
                    }
                    else if(js_syntax_is_var_utf16(substr)){
                        style = DNDC_SYNTAX_JS_VAR;
                    }
                    else if(js_syntax_is_keyword_literal_utf16(substr)){
                        style = DNDC_SYNTAX_JS_KEYWORD_VALUE;
                        state->can_regex = 0;
                    }
                    else if(js_syntax_is_builtin_utf16(substr)){
                        style = DNDC_SYNTAX_JS_BUILTIN;
                    }
                    else if(js_syntax_is_node_type_utf16(substr)){
                        style = DNDC_SYNTAX_JS_NODETYPE;
                    }
                    else {
                        state->can_regex = 0;
                    }
                    syntax_func(syntax_data, style, lineno, indentation+start, str+start, i - start);
                }
                break;
            default:
                state->can_regex = 1;
                continue;
        }
    }
    return;

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
        GENERIC = 0,
        JAVASCRIPT = 1,
        RAW = 2,
    };
    struct JsStyleState jsstyle = {0};
    enum WhichNode which = GENERIC;
    for(;begin != end;line++){
        const uint16_t* endline = mem_utf16(begin, u'\n', end-begin);
        if(!endline)
            endline = end;
        StringViewUtf16 stripped = lstripped_view_utf16(begin, endline-begin);
        ptrdiff_t indent = stripped.text - begin;
        if(stripped.length && indent <= raw_indentation)
            which = GENERIC;
        if(which == JAVASCRIPT){
            dndc_analyze_syntax_js_utf16(&jsstyle, stripped, syntax_func, syntax_data, line, indent);
        }
        else if(which == RAW){
            syntax_func(syntax_data, DNDC_SYNTAX_RAW_STRING, line, indent, stripped.text, stripped.length);
        }
        else {
            const uint16_t* doublecolon = find_double_colon_utf16(stripped.text, stripped.length);
            if(! doublecolon){
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
                        case CASE_u16_a_z:
                            continue;
                        default:
                            break;
                    }
                    break;
                }
                StringViewUtf16 nodename = SV16("");
                if(nodenameend != aftercolon.text){
                    nodename = (StringViewUtf16){.text=aftercolon.text, .length=nodenameend-aftercolon.text};
                    syntax_func(syntax_data, DNDC_SYNTAX_NODE_TYPE, line, nodename.text-begin, nodename.text, nodename.length);
                    for(size_t i = 0; i < arrlen(RAW_NODES_UTF16); i++){
                        if(SV_utf16_equals(nodename, RAW_NODES_UTF16[i])){
                            if(i == RAW_NODE_JS_INDEX || i == RAW_NODE_SCRIPT_INDEX){
                                jsstyle = (struct JsStyleState){0};
                                which = JAVASCRIPT;
                            }
                            else {
                                which = RAW;
                            }
                            raw_indentation = stripped.text - begin;
                            break;
                        }
                    }
                }
                if(memmem(stripped.text, stripped.length*2, u"#import", sizeof(u"#import")-2)){
                    which = GENERIC;
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
                                    case CASE_u16_a_z:
                                    case CASE_u16_A_Z:
                                    case CASE_u16_0_9:
                                    case u'-': case u'_':
                                        continue;
                                    default:
                                        break;
                                }
                                break;
                            }
                            syntax_func(syntax_data, DNDC_SYNTAX_ATTRIBUTE, line, attrfirst-begin, attrfirst, postnodename-attrfirst);
                            if(postnodename != endline && *postnodename == u'('){
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
                        case u'#':{
                            // copy-pasta from attribute code
                            const uint16_t* attrfirst = postnodename;
                            postnodename++;
                            for(;postnodename != endline;postnodename++){
                                uint16_t c = *postnodename;
                                switch(c){
                                    case CASE_u16_a_z:
                                    case CASE_u16_A_Z:
                                    case CASE_u16_0_9:
                                    case u'-': case u'_':
                                        continue;
                                    default:
                                        break;
                                }
                                break;
                            }
                            syntax_func(syntax_data, DNDC_SYNTAX_DIRECTIVE, line, attrfirst-begin, attrfirst, postnodename-attrfirst);
                            if(postnodename != endline && *postnodename == u'('){
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
                                    case CASE_u16_a_z:
                                    case CASE_u16_A_Z:
                                    case CASE_u16_0_9:
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
DndcFileCache*
dndc_create_filecache(void){
    struct DndcFileCache* result = malloc(sizeof(*result));
    Allocator al = get_mallocator();
    *result = (struct DndcFileCache){.allocator = al, .scratch=al};
    return result;
}
DNDC_API
void
dndc_filecache_destroy(DndcFileCache* cache){
    if(!cache) return;
    FileCache_clear(cache);
    free(cache);
}

DNDC_API
int
dndc_filecache_remove(DndcFileCache* cache, StringView path){
    return FileCache_maybe_remove(cache, path);
}

DNDC_API
void
dndc_filecache_clear(DndcFileCache* cache){
    if(cache) FileCache_clear(cache);
}

DNDC_API
int
dndc_filecache_has_path(DndcFileCache* cache, StringView path){
    return FileCache_has_file(cache, path);
}

DNDC_API
size_t
dndc_filecache_n_paths(DndcFileCache* cache){
    return FileCache_n_paths(cache);
}

DNDC_API
size_t
dndc_filecache_cached_paths(DndcFileCache* cache, DndcStringView* buff, size_t bufflen, size_t* cookie){
    return FileCache_cached_paths(cache, buff, bufflen, cookie);
}

DNDC_API
int
dndc_filecache_store_text(DndcFileCache* cache, DndcStringView path, DndcStringView data, int overwrite){
    return FileCache_store_text_file(cache, path, data, overwrite);
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
    DndcStringView base_directory,
    DndcStringView source_text,
    DndcStringView source_path,
    DndcLongString* outstring,
    DNDC_NULLABLE(DndcFileCache*) base64cache,
    DNDC_NULLABLE(DndcFileCache*) textcache,
    DNDC_NULLABLE(DndcLogFunc*) log_func,
    DNDC_NULLABLE(void*) log_user_data,
    DNDC_NULLABLE(DndcDependencyFunc*) dependency_func,
    DNDC_NULLABLE(void*) dependency_user_data,
    DNDC_NULLABLE(DndcWorkerThread*) worker_thread,
    DndcLongString jsargs
){
    enum {
        // All the valid flags.
        DNDC_VALID_FLAGS = 0
            | DNDC_FRAGMENT_ONLY
            | DNDC_DONT_WRITE
            | DNDC_DONT_IMPORT
            | DNDC_DONT_READ
            | DNDC_INPUT_IS_UNTRUSTED
            | DNDC_REFORMAT_ONLY
            | DNDC_SUPPRESS_WARNINGS
            | DNDC_DONT_PRINT_ERRORS
            | DNDC_PRINT_STATS
            | DNDC_ALLOW_BAD_LINKS
            | DNDC_NO_COMPILETIME_JS
            | DNDC_DISALLOW_ATTRIBUTE_DIRECTIVE_OVERLAP
            | DNDC_ENABLE_JS_WRITE
            | DNDC_NO_THREADS
            | DNDC_NO_CLEANUP
            | DNDC_STRIP_WHITESPACE
            | DNDC_DONT_INLINE_IMAGES
            | DNDC_USE_DND_URL_SCHEME
            | DNDC_OUTPUT_EXPANDED_DND
    };
    uint64_t new_flags = flags & DNDC_VALID_FLAGS;
    if(new_flags != flags)
        return DNDC_ERROR_VALUE;
    if(!outstring)
        return DNDC_ERROR_VALUE;
    int err = run_the_dndc(flags, base_directory, source_text, source_path, outstring, base64cache, textcache, log_func, log_user_data, dependency_func, dependency_user_data, NULL, NULL, (WorkerThread*)worker_thread, jsargs);
    return err;
}


#if defined(WASM) && !defined(NO_DNDC_AST_API)
#define NO_DNDC_AST_API
#endif

// ast API
#ifndef NO_DNDC_AST_API
DNDC_API
DndcStringView
dndc_ctx_dup_sv(DndcContext* ctx, DndcStringView text){
    return (DndcStringView){
        .text = Allocator_dupe(string_allocator(ctx), text.text, text.length),
        .length = text.length,
    };
}

static inline
LongString
ctx_dup_ls(DndcContext* ctx, LongString text){
    return (LongString){
        .text = Allocator_strndup(string_allocator(ctx), text.text, text.length),
        .length = text.length,
    };
}

DNDC_API
DndcContext*
dndc_create_ctx(unsigned long long flags, DndcFileCache*_Nullable base64cache, DndcFileCache*_Nullable textcache){
    DndcContext* ctx = calloc(1, sizeof *ctx);
    ctx->flags = flags;
    PushDiagnostic();
    SuppressNullableConversion();
    if(base64cache)
        ctx->b64cache = base64cache;
    else {
        ctx->b64cache = dndc_create_filecache();
        ctx->b64cache_allocated = 1;
    }
    if(textcache)
        ctx->textcache = textcache;
    else {
        ctx->textcache = dndc_create_filecache();
        ctx->textcache_allocated = 1;
    }
    PopDiagnostic();
    ctx->titlenode = INVALID_NODE_HANDLE;
    ctx->tocnode = INVALID_NODE_HANDLE;
    ctx->root_handle = INVALID_NODE_HANDLE;
    ctx->temp = new_linear_storage(1024*1024, "temporary storage");
    ctx->links.allocator = main_allocator(ctx);
    return ctx;
}

DNDC_API
void
dndc_ctx_set_logger(DndcContext*ctx, DndcLogFunc*_Nullable func, void*_Nullable data){
    ctx->log_func = func;
    ctx->log_user_data = data;
}


DNDC_API
void
dndc_ctx_destroy(DndcContext* ctx){
    if(ctx->textcache_allocated)
        dndc_filecache_destroy(ctx->textcache);
    if(ctx->b64cache_allocated)
        dndc_filecache_destroy(ctx->b64cache);
    Allocator_free_all(temp_allocator(ctx));
    Allocator_free_all(main_allocator(ctx));
    Allocator_free_all(string_allocator(ctx));
    destroy_linear_storage(&ctx->temp);
    free(ctx);
}

DNDC_API
DndcContext*
dndc_ctx_clone(DndcContext* ctx){
    DndcContext* result = dndc_create_ctx(
            ctx->flags,
            !ctx->b64cache_allocated?ctx->b64cache:NULL,
            !ctx->textcache_allocated?ctx->textcache:NULL);
    dndc_ctx_set_logger(result, ctx->log_func, ctx->log_user_data);
    MARRAY_FOR_EACH(StringView, fn, ctx->filenames)
        Marray_push(StringView)(&result->filenames, main_allocator(result), dndc_ctx_dup_sv(result, *fn));
    #define cp(x) \
        Marray_extend(NodeHandle)(&result->x, main_allocator(result), ctx->x.data, ctx->x.count)
    if(ctx->base_directory.length)
        result->base_directory = dndc_ctx_dup_sv(result, ctx->base_directory);

    cp(user_script_nodes);
    cp(imports);
    cp(stylesheets_nodes);
    cp(link_nodes);
    cp(script_nodes);
    cp(meta_nodes);
    cp(img_nodes);
    cp(imglinks_nodes);
    #undef cp
    result->titlenode = ctx->titlenode;
    result->tocnode = ctx->tocnode;
    result->root_handle = ctx->root_handle;

    MARRAY_FOR_EACH(StringView, s, ctx->dependencies)
        Marray_push(StringView)(&result->dependencies, main_allocator(result), dndc_ctx_dup_sv(result, *s));
    if(ctx->links.count_){
        size_t cap = ctx->links.capacity_;
        result->links.capacity_ = cap;
        result->links.count_ = ctx->links.count_;
        result->links.keys = Allocator_zalloc(result->links.allocator, sizeof(*result->links.keys)*cap*2);
        for(size_t i = 0; i < cap; i++){
            StringView k = ctx->links.keys[i];
            if(!k.length) continue;
            StringView v = ctx->links.keys[i+cap];
            result->links.keys[i] = dndc_ctx_dup_sv(result, k);
            result->links.keys[i+cap] = v.length?dndc_ctx_dup_sv(result, v):v;
        }
    }
    MARRAY_FOR_EACH(IdItem, s, ctx->explicit_node_ids)
        Marray_push(IdItem)(&result->explicit_node_ids, main_allocator(result),
            (IdItem){
                .node = s->node,
                .text = dndc_ctx_dup_sv(result, s->text),
            });
    if(ctx->renderedtoc.text)
        result->renderedtoc = ctx_dup_ls(result, ctx->renderedtoc);
    MARRAY_FOR_EACH(Node, node, ctx->nodes){
        Node* newnode = Marray_alloc(Node)(&result->nodes, main_allocator(result));
        *newnode = *node;
        if(node->header.length)
            newnode->header = dndc_ctx_dup_sv(result, newnode->header);
        if(node_children_count(node) > 4){
            memset(&newnode->children, 0, sizeof(newnode->children));
            Marray_extend(NodeHandle)(&newnode->children, main_allocator(result), node->children.data, node->children.count);
        }
        if(node->attributes){
            newnode->attributes = Rarray_clone(Attribute)(node->attributes, main_allocator(result));
            RARRAY_FOR_EACH(Attribute, attr, newnode->attributes){
                attr->key = dndc_ctx_dup_sv(result, attr->key);
                attr->value = dndc_ctx_dup_sv(result, attr->value);
            }
        }
        if(node->classes){
            newnode->classes = Rarray_clone(StringView)(node->classes, main_allocator(result));
            RARRAY_FOR_EACH(StringView, cls, newnode->classes)
                *cls = dndc_ctx_dup_sv(result, *cls);
        }
    }
    return result;
}

DNDC_API
DndcContext*
dndc_ctx_shallow_clone(DndcContext* ctx){
    DndcContext* result = dndc_create_ctx(
            ctx->flags,
            !ctx->b64cache_allocated?ctx->b64cache:NULL,
            !ctx->textcache_allocated?ctx->textcache:NULL);
    dndc_ctx_set_logger(result, ctx->log_func, ctx->log_user_data);
    if(ctx->filenames.count)
        Marray_extend(StringView)(&result->filenames, main_allocator(result), ctx->filenames.data, ctx->filenames.count);
    #define cp(x) \
        Marray_extend(NodeHandle)(&result->x, main_allocator(result), ctx->x.data, ctx->x.count)
    if(ctx->base_directory.length)
        result->base_directory = ctx->base_directory;

    cp(user_script_nodes);
    cp(imports);
    cp(stylesheets_nodes);
    cp(link_nodes);
    cp(script_nodes);
    cp(meta_nodes);
    cp(img_nodes);
    cp(imglinks_nodes);
    #undef cp
    result->titlenode = ctx->titlenode;
    result->tocnode = ctx->tocnode;
    result->root_handle = ctx->root_handle;

    if(ctx->dependencies.count)
        Marray_extend(StringView)(&result->dependencies, main_allocator(result), ctx->dependencies.data, ctx->dependencies.count);
    if(ctx->links.count_){
        size_t cap = ctx->links.capacity_;
        result->links.capacity_ = cap;
        result->links.count_ = ctx->links.count_;
        result->links.keys = Allocator_dupe(result->links.allocator, ctx->links.keys, sizeof(*result->links.keys)*cap*2);
    }
    if(ctx->explicit_node_ids.count)
        Marray_extend(IdItem)(&result->explicit_node_ids, main_allocator(result), ctx->explicit_node_ids.data, ctx->explicit_node_ids.count);
    if(ctx->renderedtoc.text)
        result->renderedtoc = ctx->renderedtoc;
    MARRAY_FOR_EACH(Node, node, ctx->nodes){
        Node* newnode = Marray_alloc(Node)(&result->nodes, main_allocator(result));
        *newnode = *node;
        if(node_children_count(node) > 4){
            memset(&newnode->children, 0, sizeof(newnode->children));
            Marray_extend(NodeHandle)(&newnode->children, main_allocator(result), node->children.data, node->children.count);
        }
        if(node->attributes)
            newnode->attributes = Rarray_clone(Attribute)(node->attributes, main_allocator(result));
        if(node->classes)
            newnode->classes = Rarray_clone(StringView)(node->classes, main_allocator(result));
    }
    return result;
}

DNDC_API
int
dndc_ctx_set_base(DndcContext* ctx, DndcStringView sv){
    ctx->base_directory = sv;
    return 0;
}

DNDC_API
int
dndc_ctx_get_base(DndcContext* ctx, DndcStringView* sv){
    *sv = ctx->base_directory;
    return 0;
}


static inline
NodeHandle
check_api_handle(DndcContext* ctx, DndcNodeHandle handle){
    if(handle >= ctx->nodes.count)
        return INVALID_NODE_HANDLE;
    return (NodeHandle){._value=handle};
}

DNDC_API
int
dndc_ctx_parse_string(DndcContext* ctx, DndcNodeHandle root, DndcStringView filename, DndcStringView contents){
    NodeHandle handle = check_api_handle(ctx, root);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE)) return DNDC_ERROR_VALUE;
    int e = dndc_parse(ctx, handle, filename, contents.text, contents.length);
    return e;
}

DNDC_API
int
dndc_ctx_parse_file(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView sourcepath){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE)) return DNDC_ERROR_VALUE;
    StringViewResult svr = ctx_load_source_file(ctx, sourcepath);
    if(svr.errored) return DNDC_ERROR_FILE_READ;
    int e = dndc_parse(ctx, handle, sourcepath, svr.result.text, svr.result.length);
    return e;
}

DNDC_API
DndcNodeHandle
dndc_ctx_make_root(DndcContext* ctx, DndcStringView filename){
    if(!NodeHandle_eq(ctx->root_handle, INVALID_NODE_HANDLE)){
        return DNDC_NODE_HANDLE_INVALID;
    }
    NodeHandle root_handle = alloc_handle(ctx);
    ctx->root_handle = root_handle;
    Node* root = get_node(ctx, root_handle);
    root->col = 0;
    root->row = 0;
    Marray_push(StringView)(&ctx->filenames, main_allocator(ctx), filename);
    root->filename_idx = ctx->filenames.count-1;
    root->type = NODE_MD;
    root->parent = root_handle;
    return root_handle._value;
}

DNDC_API
DndcNodeHandle
dndc_ctx_get_root(DndcContext* ctx){
    return ctx->root_handle._value;
}

DNDC_API
int
dndc_ctx_set_root(DndcContext* ctx, DndcNodeHandle handle){
    if(handle != DNDC_NODE_HANDLE_INVALID && handle >= ctx->nodes.count) return DNDC_ERROR_VALUE;
    if(!NodeHandle_eq(ctx->root_handle, INVALID_NODE_HANDLE)){
        Node* root = get_node(ctx, ctx->root_handle);
        root->parent = INVALID_NODE_HANDLE;
    }
    // FIXME: should fail if this node is not an orphan.
    ctx->root_handle._value = handle;
    return 0;
}

DNDC_API
int
dndc_node_set_attribute(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView key, DndcStringView value){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    Node* node = get_node(ctx, handle);
    node_set_attribute(node, main_allocator(ctx), key, value);
    return 0;
}

DNDC_API
int
dndc_node_get_attribute(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView key, DndcStringView* outvalue){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    Node* node = get_node(ctx, handle);
    StringView* value = node_get_attribute(node, key);
    if(!value) return DNDC_ERROR_NOT_FOUND;
    *outvalue = *value;
    return 0;
}

DNDC_API
int
dndc_node_has_attribute(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView key){
    NodeHandle handle = check_api_handle(ctx, dnh);
    // ambiguous - error or does it not have one?
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return 0;
    Node* node = get_node(ctx, handle);
    return node_has_attribute(node, key);
}

DNDC_API
size_t
dndc_node_attributes_count(DndcContext* ctx, DndcNodeHandle dnh){
    NodeHandle handle = check_api_handle(ctx, dnh);
    // ambiguous - error or does it not have one?
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return 0;
    Node* node = get_node(ctx, handle);
    return node->attributes? node->attributes->count:0;
}

DNDC_API
size_t
dndc_node_attributes(DndcContext* ctx, DndcNodeHandle dnh, size_t* cookie, DndcAttributePair* buff, size_t bufflen){
    NodeHandle handle = check_api_handle(ctx, dnh);
    // ambiguous - error or does it not have one?
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return 0;
    Node* node = get_node(ctx, handle);
    const Rarray(Attribute)* attributes = node->attributes;
    size_t n = attributes?attributes->count: 0;
    size_t start = *cookie;
    if(start >= n) return 0;
    assert(attributes);
    const Attribute* data = attributes->data;
    _Static_assert(sizeof(Attribute) == sizeof(DndcAttributePair), "");
    size_t n_copy = n - start;
    if(n_copy > bufflen)
        n_copy = bufflen;
    memcpy(buff, data+start, sizeof(*buff)*n_copy);
    *cookie = start + n_copy;
    return n_copy;
}

DNDC_API
size_t
dndc_node_classes(DndcContext*ctx, DndcNodeHandle dnh, size_t * cookie, DndcStringView* buff, size_t buff_len){
    NodeHandle handle = check_api_handle(ctx, dnh);
    // ambiguous - error or does it not have one?
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return 0;
    Node* node = get_node(ctx, handle);
    const Rarray(StringView)* classes = node->classes;
    size_t n = classes?classes->count: 0;
    size_t start = *cookie;
    if(start >= n) return 0;
    assert(classes);
    const StringView* data = classes->data;
    size_t n_copy = n - start;
    if(n_copy > buff_len)
        n_copy = buff_len;
    memcpy(buff, data+start, n_copy*sizeof(*buff));
    return n_copy;
}

DNDC_API
size_t
dndc_node_classes_count(DndcContext* ctx, DndcNodeHandle dnh){
    NodeHandle handle = check_api_handle(ctx, dnh);
    // ambiguous - error or does it not have one?
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return 0;
    Node* node = get_node(ctx, handle);
    return node->classes? node->classes->count : 0;
}

DNDC_API
int
dndc_node_add_class(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView cls){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    Node* node = get_node(ctx, handle);
    node->classes = Rarray_push(StringView)(node->classes, main_allocator(ctx), cls);
    return 0;
}

DNDC_API
int
dndc_node_remove_class(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView cls){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    Node* node = get_node(ctx, handle);
    RARRAY_FOR_EACH(StringView, c, node->classes){
        if(SV_equals(*c, cls)){
            PushDiagnostic(); SuppressNullableConversion();
            Rarray_remove(StringView)(node->classes, c-node->classes->data);
            PopDiagnostic();
            break;
        }
    }
    return 0;
}

DNDC_API
DndcNodeHandle
dndc_node_get_parent(DndcContext* ctx, DndcNodeHandle dnh){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_NODE_HANDLE_INVALID;
    return get_node(ctx, handle)->parent._value;
}

DNDC_API
int
dndc_node_get_type(DndcContext* ctx, DndcNodeHandle dnh){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return NODE_INVALID;
    return get_node(ctx, handle)->type;
}

DNDC_API
int
dndc_node_set_type(DndcContext* ctx, DndcNodeHandle dnh, int type){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    if(type < 0 || type > NODE_INVALID)
        return DNDC_ERROR_VALUE;
    get_node(ctx, handle)->type = type;
    Marray(NodeHandle)* node_store = NULL;
    switch(type){
        case NODE_IMPORT:
            node_store = &ctx->imports;
            get_node(ctx, handle)->flags |= NODEFLAG_IMPORT;
            break;
        case NODE_STYLESHEETS:
            node_store = &ctx->stylesheets_nodes;
            break;
        case NODE_LINKS:
            node_store = &ctx->link_nodes;
            break;
        case NODE_SCRIPTS:
            node_store = &ctx->script_nodes;
            break;
        case NODE_JS:
            node_store = &ctx->user_script_nodes;
            break;
        case NODE_META:
            node_store = &ctx->meta_nodes;
            break;
        case NODE_TITLE:
            ctx->titlenode = handle;
            break;
        case NODE_TOC:
            ctx->tocnode = handle;
            break;
        default:
            break;
    }
    if(node_store)
        Marray_push(NodeHandle)(node_store, main_allocator(ctx), handle);
    return 0;
}

#define CHECKNF(x) _Static_assert((int)NODEFLAG_##x == (int)DNDC_NODEFLAG_##x, #x)
CHECKNF(IMPORT);
CHECKNF(NOID);
CHECKNF(HIDE);
CHECKNF(NOINLINE);
#undef CHECKNF

DNDC_API
int
dndc_node_get_flags(DndcContext* ctx, DndcNodeHandle dnh){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return 0;
    return get_node(ctx, handle)->flags & PUBLIC_NODE_FLAGS;
}

DNDC_API
int
dndc_node_set_flags(DndcContext* ctx, DndcNodeHandle dnh, int flags){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    if((flags & PUBLIC_NODE_FLAGS) != flags) return DNDC_ERROR_VALUE;
    int old_flags = get_node(ctx, handle)->flags;
    int private_flags = old_flags & ~PUBLIC_NODE_FLAGS;
    get_node(ctx, handle) -> flags = flags | private_flags;
    return 0;
}

DNDC_API
int
dndc_node_get_header(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView* sv){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    *sv = get_node(ctx, handle)->header;
    return 0;
}

DNDC_API
int
dndc_node_set_header(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView sv){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    get_node(ctx, handle)->header = sv;
    return 0;
}

DNDC_API
int
dndc_ctx_expand_to_dnd(DndcContext* ctx, DndcLongString* ls){
    if(NodeHandle_eq(ctx->root_handle, INVALID_NODE_HANDLE)) return DNDC_ERROR_VALUE;
    MStringBuilder output_sb = {.allocator = get_mallocator()};
    int e = expand_to_dnd(ctx, &output_sb);
    if(e) {msb_destroy(&output_sb); return e;}
    *ls = msb_detach_ls(&output_sb);
    return 0;
}
DNDC_API
int
dndc_ctx_render_to_html(DndcContext* ctx, DndcLongString* ls){
    MStringBuilder output_sb = {.allocator = get_mallocator()};
    if(NodeHandle_eq(ctx->root_handle, INVALID_NODE_HANDLE)) return DNDC_ERROR_VALUE;
    int e = render_tree(ctx, &output_sb);
    if(e) {msb_destroy(&output_sb); return e;}
    *ls = msb_detach_ls(&output_sb);
    return 0;
}

DNDC_API
int
dndc_node_render_to_html(DndcContext* ctx, DndcNodeHandle dnh, DndcLongString* ls){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    MStringBuilder output_sb = {.allocator = get_mallocator()};
    int e = render_node(ctx, &output_sb, handle, 1, 0);
    if(e) {msb_destroy(&output_sb); return e;}
    *ls = msb_detach_ls(&output_sb);
    return 0;
}

DNDC_API
int
dndc_ctx_format_tree(DndcContext* ctx, DndcLongString* ls){
    if(NodeHandle_eq(ctx->root_handle, INVALID_NODE_HANDLE)) return DNDC_ERROR_VALUE;
    MStringBuilder output_sb = {.allocator = get_mallocator()};
    int e = format_tree(ctx, &output_sb);
    if(e) {
        msb_destroy(&output_sb);
        return e;
    }
    *ls = msb_detach_ls(&output_sb);
    return 0;
}

DNDC_API
int
dndc_node_format(DndcContext* ctx, DndcNodeHandle dnh, int indent, DndcLongString* ls){
    if(indent < 0 || indent > 50) return DNDC_ERROR_VALUE;
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    Node* node = get_node(ctx, handle);
    MStringBuilder output_sb = {.allocator = get_mallocator()};
    int e = format_node(ctx, &output_sb, node, indent);
    if(e){
        msb_destroy(&output_sb);
        return e;
    }
    *ls = msb_detach_ls(&output_sb);
    return 0;
}

DNDC_API
DndcNodeHandle
dndc_ctx_node_by_id(DndcContext* ctx, DndcStringView sv){
    if(sv.length > 256) return DNDC_NODE_HANDLE_INVALID;
    MStringBuilder msb = {.allocator = temp_allocator(ctx)};
    MStringBuilder msb2 = {.allocator = temp_allocator(ctx)};
    msb_write_kebab(&msb, sv.text, sv.length);
    if(!msb.cursor)
        return DNDC_NODE_HANDLE_INVALID;
    DndcNodeHandle result = DNDC_NODE_HANDLE_INVALID;
    sv = msb_borrow_sv(&msb);
    // OPTIMIZE ME
    for(size_t i = 0; i < ctx->nodes.count; i++){
        NodeHandle handle = {.index=i};
        StringView id = node_get_id(ctx, handle);
        if(!id.length) continue;
        msb_reset(&msb2);
        msb_write_kebab(&msb2, id.text, id.length);
        StringView k = msb_borrow_sv(&msb2);
        if(SV_equals(sv, k)){
            result = handle._value;
            goto done;
        }
    }
    msb_destroy(&msb2);
    msb_destroy(&msb);
    done:
    return result;
}

DNDC_API
int
dndc_node_get_id(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView* outsv){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    *outsv = node_get_id(ctx, handle);
    return 0;
}

DNDC_API
int
dndc_node_set_id(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView sv){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    node_set_id(ctx, handle, sv);
    return 0;
}

DNDC_API
int
dndc_node_append_child(DndcContext* ctx, DndcNodeHandle parent_, DndcNodeHandle child_){
    NodeHandle child = check_api_handle(ctx, child_);
    NodeHandle parent = check_api_handle(ctx, parent_);
    if(NodeHandle_eq(child, INVALID_NODE_HANDLE) || NodeHandle_eq(parent, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    Node* ch = get_node(ctx, child);
    if(!NodeHandle_eq(ch->parent, INVALID_NODE_HANDLE)) // must be orphan
        return DNDC_ERROR_VALUE;
    if(NodeHandle_eq(child, parent)) // can't append to itself
        return DNDC_ERROR_VALUE;
    append_child(ctx, parent, child);
    return 0;
}

DNDC_API
int
dndc_node_insert_child(DndcContext* ctx, DndcNodeHandle parent_, size_t i, DndcNodeHandle child_){
    NodeHandle child = check_api_handle(ctx, child_);
    NodeHandle parent = check_api_handle(ctx, parent_);
    if(NodeHandle_eq(child, INVALID_NODE_HANDLE) || NodeHandle_eq(parent, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    Node* ch = get_node(ctx, child);
    if(!NodeHandle_eq(ch->parent, INVALID_NODE_HANDLE)) // must be orphan
        return DNDC_ERROR_VALUE;
    if(NodeHandle_eq(child, parent)) // can't append to itself
        return DNDC_ERROR_VALUE;
    node_insert_child(ctx, parent, i, child);
    return 0;
}

DNDC_API
int
dndc_node_remove_child(DndcContext* ctx, DndcNodeHandle parent_, size_t i){
    NodeHandle parent = check_api_handle(ctx, parent_);
    if(NodeHandle_eq(parent, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    Node* node = get_node(ctx, parent);
    if(i >= node->children_count)
        return DNDC_ERROR_VALUE;
    node_remove_child(node, i, main_allocator(ctx));
    return 0;
}

DNDC_API
int
dndc_node_has_class(DndcContext* ctx, DndcNodeHandle dnh, DndcStringView sv){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return 0;
    return node_has_class(get_node(ctx, handle), sv);
}

DNDC_API
int
dndc_ctx_node_invalid(DndcContext* ctx, DndcNodeHandle dnh){
    return NodeHandle_eq(check_api_handle(ctx, dnh), INVALID_NODE_HANDLE);
}

DNDC_API
void
dndc_node_detach(DndcContext* ctx, DndcNodeHandle dnh){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return;
    Node* node = get_node(ctx, handle);
    if(NodeHandle_eq(node->parent, INVALID_NODE_HANDLE))
        return;
    Node* parent = get_node(ctx, node->parent);
    node->parent = INVALID_NODE_HANDLE;
    for(size_t i = 0; i < node_children_count(parent); i++){
        if(NodeHandle_eq(handle, node_children(parent)[i])){
            node_remove_child(parent, i, main_allocator(ctx));
            return;
        }
    }
    // Shouldn't get here...
}

DNDC_API
DndcNodeHandle
dndc_ctx_make_node(DndcContext* ctx, int type, DndcStringView header, DndcNodeHandle parent_){
    if(type < 0 || type > DNDC_NODE_TYPE_INVALID) return DNDC_NODE_HANDLE_INVALID;
    NodeHandle handle = alloc_handle(ctx);
    Node* node = get_node(ctx, handle);
    node->type = type;
    NodeHandle parent = check_api_handle(ctx, parent_);
    node->parent = parent;
    node->header = header;
    if(!NodeHandle_eq(parent, INVALID_NODE_HANDLE))
        append_child(ctx, parent, handle);
    Marray(NodeHandle)* node_store = NULL;
    switch(type){
        case NODE_IMPORT:
            node_store = &ctx->imports;
            node->flags |= NODEFLAG_IMPORT;
            break;
        case NODE_STYLESHEETS:
            node_store = &ctx->stylesheets_nodes;
            break;
        case NODE_LINKS:
            node_store = &ctx->link_nodes;
            break;
        case NODE_SCRIPTS:
            node_store = &ctx->script_nodes;
            break;
        case NODE_JS:
            node_store = &ctx->user_script_nodes;
            break;
        case NODE_META:
            node_store = &ctx->meta_nodes;
            break;
        case NODE_TITLE:
            ctx->titlenode = handle;
            break;
        case NODE_TOC:
            ctx->tocnode = handle;
            break;
        default:
            break;
    }
    if(node_store)
        Marray_push(NodeHandle)(node_store, main_allocator(ctx), handle);
    return handle._value;
}

DNDC_API
int
dndc_ctx_resolve_imports(DndcContext* ctx){
    int result = 0;
    for(size_t i = 0; i < ctx->imports.count; i++){
        NodeHandle handle = ctx->imports.data[i];
        if(!(get_node(ctx, handle)->flags & NODEFLAG_IMPORT))
            continue;
        // We parse into a different node and then swap the two.
        NodeHandle newhandle = alloc_handle(ctx);
        Node* node = get_node(ctx, handle);
        node->flags &= ~NODEFLAG_IMPORT;
        bool was_import = false;
        {
            Node* newnode = get_node(ctx, newhandle);
            *newnode = *node;
            newnode->children.count = 0;
            if(newnode->type == NODE_IMPORT){
                newnode->type = NODE_MD;
                was_import = true;
            }
            newnode->attributes = Rarray_clone(Attribute)(node->attributes, main_allocator(ctx));
        }
        if(ctx->imports.count > 1000){
            NODE_LOG_ERROR(ctx, node, LS("More than 1000 imports. Aborting parsing (did you accidentally create an import cycle?)"));
            result = DNDC_ERROR_INVALID_TREE;
            goto cleanup;
        }
        // NOTE: re-get the node every loop as the pointer is invalidated.
        for(size_t j = 0; j < node_children_count(node); j++, node=get_node(ctx, handle)){
            NodeHandle child_handle = node_children(node)[j];
            Node* child = get_node(ctx, child_handle);
            if(child->type != NODE_STRING){
                NODE_LOG_ERROR(ctx, child, LS("import child is not a string"));
                result = DNDC_ERROR_INVALID_TREE;
                goto cleanup;
            }
            StringView filename = child->header;
            StringViewResult imp_e = ctx_load_source_file(ctx, filename);
            if(imp_e.errored){
                if(ctx->base_directory.length)
                    NODE_LOG_ERROR(ctx, child, "Unable to open '", ctx->base_directory, "/", filename, "'");
                else
                    NODE_LOG_ERROR(ctx, child, "Unable to open '", filename, "'");
                result = imp_e.errored;
                goto cleanup;
            }
            StringView imp_text = imp_e.result;
            result = dndc_parse(ctx, newhandle, filename, imp_text.text, imp_text.length);
            if(result){
                goto cleanup;
            }
        }
        Node* newnode = get_node(ctx, newhandle);
        if(was_import){
            // change to container
            newnode->type = NODE_CONTAINER;
        }
        Node* parent = get_node(ctx, newnode->parent);
        NodeHandle* parentchildren = node_children(parent);
        for(size_t j = 0; j < node_children_count(parent); j++){
            if(NodeHandle_eq(parentchildren[j], handle)){
                parentchildren[j] = newhandle;
                break;
            }
        }
        {
            node = get_node(ctx, handle);
            node->type = NODE_INVALID;
            node->parent = INVALID_NODE_HANDLE;
            node->children.count = 0;
        }
        Marray(NodeHandle*) handles = NULL;
        switch(newnode->type){
            case NODE_JS:
                handles = &ctx->user_script_nodes;
                break;
            case NODE_STYLESHEETS:
                handles = &ctx->stylesheets_nodes;
                break;
            case NODE_LINKS:
                handles = &ctx->link_nodes;
                break;
            case NODE_SCRIPTS:
                handles = &ctx->script_nodes;
                break;
            case NODE_IMAGE:
                handles = &ctx->img_nodes;
                break;
            case NODE_IMGLINKS:
                handles = &ctx->imglinks_nodes;
                break;
            default:
                break;
        }
        if(handles){
            for(size_t j = 0; j < handles->count; j++){
                if(NodeHandle_eq(handles->data[j], handle)){
                    handles->data[j] = newhandle;
                    goto foundit;
                }
            }
            // I don't think this can happen.
            Marray_push(NodeHandle)(handles, main_allocator(ctx), newhandle);
            foundit:;
        }
    }
    return result;

    cleanup:
    return result;
}
DNDC_API
size_t
dndc_node_children_count(DndcContext* ctx, DndcNodeHandle dnh){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return 0;
    Node* n = get_node(ctx, handle);
    return node_children_count(n);
}

DNDC_API
int
dndc_node_cat_string_children(DndcContext* ctx, DndcNodeHandle dnh, DndcLongString* out){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    MStringBuilder msb = {.allocator=get_mallocator()};
    Node* n = get_node(ctx, handle);
    NODE_CHILDREN_FOR_EACH(c, n){
        Node* child = get_node(ctx, *c);
        if(child->type != NODE_STRING) continue;
        StringView h = child->header;
        if(h.length)
            msb_write_str(&msb, h.text, h.length);
        msb_write_char(&msb, '\n');
    }
    *out = msb_detach_ls(&msb);
    return 0;
}

DNDC_API
size_t
dndc_node_get_children(DndcContext* ctx, DndcNodeHandle dnh, size_t* cookie, DndcNodeHandle* buff, size_t buff_len){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return 0;
    Node* node = get_node(ctx, handle);
    size_t start = *cookie;
    size_t count = node_children_count(node);
    if(start >= count)
        return 0;
    NodeHandle* children = node_children(node);
    size_t n = count - start;
    if(n > buff_len) n = buff_len;
    memcpy(buff, children+start, n*sizeof(*children));
    *cookie += n;
    return n;
}

DNDC_API
int
dndc_node_location(DndcContext* ctx, DndcNodeHandle dnh, DndcNodeLocation* loc){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    Node* node = get_node(ctx, handle);
    loc->filename = ctx->filenames.data[node->filename_idx];
    loc->row = node->row+1;
    loc->column = node->col+1;
    return 0;
}


DNDC_API
int
dndc_ctx_execute_js(DndcContext* ctx, DndcLongString jsargs){
    int ret = execute_user_scripts(ctx, jsargs);
    return ret;
}

DNDC_API
int
dndc_ctx_gather_links(DndcContext* ctx){
    if(NodeHandle_eq(ctx->root_handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    gather_anchors(ctx);
    return 0;
}

DNDC_API
int
dndc_ctx_build_toc(DndcContext* ctx){
    if(NodeHandle_eq(ctx->tocnode, INVALID_NODE_HANDLE))
        return 0;
    build_toc_block(ctx);
    return 0;
}

DNDC_API
int
dndc_ctx_resolve_links(DndcContext* ctx){
    // Add in the links from explicit link blocks.
    MARRAY_FOR_EACH(NodeHandle, link_handle, ctx->link_nodes){
        Node* link_node = get_node(ctx, *link_handle);
        NODE_CHILDREN_FOR_EACH(it, link_node){
            Node* link_str_node = get_node(ctx, *it);
            if(link_str_node->type != NODE_STRING)
                continue;
            int e = add_link_from_sv(ctx, link_str_node);
            if(e){
                return e;
            }
        }
    }
    return 0;
}

DNDC_API
size_t
dndc_ctx_select_nodes(
        DndcContext* ctx, size_t* cookie,
        int type_,
        DndcStringView*_Nullable attributes, size_t attribute_count,
        DndcStringView*_Nullable classes, size_t class_count,
        DndcNodeHandle* outbuf, size_t buflen
        ){
    if(type_ < 0 || type_ > NODE_INVALID) return 0;
    NodeType type = type_;
    size_t n_writ = 0;
    size_t start = *cookie;
    size_t node_count = ctx->nodes.count;
    if(start >= node_count)
        return 0;
    const Node* nodes = ctx->nodes.data;
    size_t idx = start;
    for(; idx < node_count && buflen > n_writ; idx++){
        const Node* node = &nodes[idx];
        if(node->type == NODE_INVALID) continue;
        if(type != NODE_INVALID && node->type != type) continue;
        if(attribute_count){
            for(size_t i = 0; i < attribute_count; i++){
                if(!node_has_attribute(node, attributes[i])) goto LContinue;
            }
        }
        if(class_count){
            for(size_t i = 0; i < class_count; i++){
                if(!node_has_class(node, classes[i])) goto LContinue;
            }
        }
        outbuf[n_writ++] = (DndcNodeHandle)idx;
        LContinue:;
    }
    *cookie = idx;

    return n_writ;
}


static inline
void
dndc_node_tree_repr_inner(DndcContext* ctx, NodeHandle handle, int depth, MStringBuilder* sb){
    Node* node = get_node(ctx, handle);
    msb_write_nchar(sb, ' ', depth*2);
    LongString nodename = NODENAMES[node->type];
    MSB_FORMAT(sb, "[", nodename, "]");
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
        case NODE_TOC:
        case NODE_KEYVALUE:
        case NODE_IMGLINKS:
        case NODE_DETAILS:
        case NODE_MD:
        case NODE_CONTAINER:
        case NODE_INVALID:
        case NODE_QUOTE:
        case NODE_DIV:{
            MSB_FORMAT(sb, " '", node->header, "' ");
            RARRAY_FOR_EACH(StringView, c, node->classes){
                MSB_FORMAT(sb, ".", *c, " ");
            }
            RARRAY_FOR_EACH(Attribute, a, node->attributes){
                MSB_FORMAT(sb, "@", a->key);
                if(a->value.length)
                    MSB_FORMAT(sb, "(", a->value, ") ");
                else
                    msb_write_char(sb, ' ');
            }
            if(node->flags & NODEFLAG_IMPORT)
                msb_write_literal(sb, "#import ");
            if(node->flags & NODEFLAG_NOID)
                msb_write_literal(sb, "#noid ");
            if(node->flags & NODEFLAG_HIDE)
                msb_write_literal(sb, "#hide ");
            if(node->flags & NODEFLAG_NOINLINE)
                msb_write_literal(sb, "#noinline ");
            if(node->flags & NODEFLAG_ID)
                MSB_FORMAT(sb, "#id(", node_get_id(ctx, handle), ") ");
        }break;
        case NODE_STRING:{
            MSB_FORMAT(sb, " '", node->header, "'");
        }break;
    }
    msb_write_char(sb, '\n');
    NODE_CHILDREN_FOR_EACH(it, node){
        dndc_node_tree_repr_inner(ctx, *it, depth+1, sb);
    }
}
DNDC_API
int
dndc_node_tree_repr(DndcContext* ctx, DndcNodeHandle dnh, DndcLongString* outstring){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    MStringBuilder msb = {.allocator = get_mallocator()};
    dndc_node_tree_repr_inner(ctx, handle, 0, &msb);
    *outstring = msb_detach_ls(&msb);
    return 0;
}

DNDC_API
int
dndc_node_execute_js(DndcContext* ctx, DndcNodeHandle dnh, DndcLongString js){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    ArenaAllocator aa = {0};
    QJSRuntime* rt = new_qjs_rt(&aa);
    if(!rt) return DNDC_ERROR_JS;
    QJSContext* jsctx = new_qjs_ctx(rt, ctx, LS("null"));
    if(!jsctx){
        free_qjs_rt(rt, &aa);
        return DNDC_ERROR_JS;
    }
    int err = execute_qjs_string(jsctx, ctx, js.text, js.length, handle, handle);
    free_qjs_rt(rt, &aa);
    ArenaAllocator_free_all(&aa);
    return err;
}

DNDC_API
int
dndc_ctx_add_link(DndcContext* ctx, DndcStringView k, DndcStringView v){
    if(!v.length) return DNDC_ERROR_VALUE;
    MStringBuilder kebab = {.allocator = string_allocator(ctx)};
    msb_write_kebab(&kebab, k.text, k.length);
    if(!kebab.cursor) return DNDC_ERROR_VALUE;
    k = msb_detach_sv(&kebab);
    v = dndc_ctx_dup_sv(ctx, v);
    add_link_from_pair(ctx, k, v);
    return 0;
}

static inline
void
node_to_json(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb);

DNDC_API
int
dndc_node_to_json(DndcContext* ctx, DndcNodeHandle dnh, DndcLongString*out){
    NodeHandle handle = check_api_handle(ctx, dnh);
    if(NodeHandle_eq(handle, INVALID_NODE_HANDLE))
        return DNDC_ERROR_VALUE;
    MStringBuilder sb = {.allocator=get_mallocator()};
    node_to_json(ctx, handle, &sb);
    *out = msb_detach_ls(&sb);
    return 0;
}

static inline
void
ctx_to_json(DndcContext* ctx, MStringBuilder* sb);

DNDC_API
int
dndc_ctx_to_json(DndcContext* ctx, DndcLongString*out){
    MStringBuilder sb = {.allocator=get_mallocator()};
    ctx_to_json(ctx, &sb);
    *out = msb_detach_ls(&sb);
    return 0;
}

#endif

static inline
void
node_to_json(DndcContext* ctx, NodeHandle handle, MStringBuilder* sb){
    Node* node = get_node(ctx, handle);
    MSB_FORMAT(sb, "{\"type\":",node->type, ",\"handle\":", handle._value);
    if(NodeHandle_eq(node->parent, INVALID_NODE_HANDLE))
        MSB_FORMAT(sb, ",\"parent\":null");
    else
        MSB_FORMAT(sb, ",\"parent\":", node->parent._value);
    MSB_FORMAT(sb, ",\"header\":\"");
    if(node->header.length)
        msb_write_json_escaped_str(sb, node->header.text, node->header.length);
    msb_write_char(sb, '"');
    MSB_FORMAT(sb, ",\"children\":[");
    NODE_CHILDREN_FOR_EACH(ch, node){
        MSB_FORMAT(sb, ch->_value);
        msb_write_char(sb, ',');
    }
    if(node_children_count(node))
        msb_erase(sb, 1);
    msb_write_char(sb, ']');
    msb_write_literal(sb, ",\"attributes\":{");
    RARRAY_FOR_EACH(Attribute, attr, node->attributes){
        msb_write_char(sb, '"');
        msb_write_json_escaped_str(sb, attr->key.text, attr->key.length);
        msb_write_char(sb, '"');
        msb_write_char(sb, ':');
        msb_write_char(sb, '"');
        if(attr->value.length){
            msb_write_json_escaped_str(sb, attr->value.text, attr->value.length);
        }
        msb_write_char(sb, '"');
        msb_write_char(sb, ',');
    }
    if(node->attributes && node->attributes->count)
        msb_erase(sb, 1);
    msb_write_char(sb, '}');
    msb_write_literal(sb, ",\"classes\":[");
    RARRAY_FOR_EACH(StringView, cls, node->classes){
        msb_write_char(sb, '"');
        msb_write_json_escaped_str(sb, cls->text, cls->length);
        msb_write_char(sb, '"');
        msb_write_char(sb, ',');
    }
    if(node->classes && node->classes->count)
        msb_erase(sb, 1);
    msb_write_char(sb, ']');
    MSB_FORMAT(sb, ",\"filename\":", node->filename_idx);
    MSB_FORMAT(sb, ",\"row\":", node->row);
    MSB_FORMAT(sb, ",\"col\":", node->col);
    MSB_FORMAT(sb, ",\"flags\":", node->flags, "}");
}

static inline
void
ctx_to_json(DndcContext* ctx, MStringBuilder* sb){
    msb_write_char(sb, '{');
    msb_write_literal(sb, "\"nodes\":[");
    for(size_t i = 0; i < ctx->nodes.count; i++){
        node_to_json(ctx, (NodeHandle){.index=i}, sb);
        msb_write_char(sb, ',');
    }
    if(ctx->nodes.count)
        msb_erase(sb, 1);
    msb_write_char(sb, ']');
    if(NodeHandle_eq(ctx->root_handle, INVALID_NODE_HANDLE))
        MSB_FORMAT(sb, ",\"root\":null");
    else
        MSB_FORMAT(sb, ",\"root\":", ctx->root_handle._value);
    msb_write_literal(sb, ",\"filenames\":[");
    MARRAY_FOR_EACH(StringView, fn, ctx->filenames){
        msb_write_char(sb, '"');
        msb_write_json_escaped_str(sb, fn->text, fn->length);
        msb_write_char(sb, '"');
        msb_write_char(sb, ',');
    }
    if(ctx->filenames.count)
        msb_erase(sb, 1);
    msb_write_char(sb, ']');
    msb_write_literal(sb, ",\"filename\":\"");
    msb_write_json_escaped_str(sb, ctx->filename.text, ctx->filename.length);
    msb_write_char(sb, '"');
    msb_write_literal(sb, ",\"base_directory\":\"");
    msb_write_json_escaped_str(sb, ctx->base_directory.text, ctx->base_directory.length);
    msb_write_char(sb, '"');
#define WRITE_NODES(nodes) do { \
    msb_write_literal(sb, ",\"" #nodes "\":["); \
    MARRAY_FOR_EACH(NodeHandle, nh, ctx->nodes){ \
        MSB_FORMAT(sb, nh->_value, ","); \
    } \
    if(ctx->nodes.count) \
        msb_erase(sb, 1); \
    msb_write_char(sb, ']'); \
}while(0)

    WRITE_NODES(user_script_nodes);
    WRITE_NODES(imports);
    WRITE_NODES(stylesheets_nodes);
    WRITE_NODES(link_nodes);
    WRITE_NODES(script_nodes);
    WRITE_NODES(meta_nodes);
    WRITE_NODES(img_nodes);
    WRITE_NODES(imglinks_nodes);
#undef WRITE_NODES
    if(NodeHandle_eq(ctx->titlenode, INVALID_NODE_HANDLE))
        MSB_FORMAT(sb, ",\"titlenode\":null");
    else
        MSB_FORMAT(sb, ",\"titlenode\":", ctx->titlenode._value);
    if(NodeHandle_eq(ctx->tocnode, INVALID_NODE_HANDLE))
        MSB_FORMAT(sb, ",\"tocnode\":null");
    else
        MSB_FORMAT(sb, ",\"tocnode\":", ctx->tocnode._value);
    msb_write_literal(sb, ",\"links\":{}");
    msb_write_literal(sb, ",\"explicit_node_ids\":{");
    MARRAY_FOR_EACH(IdItem, eni, ctx->explicit_node_ids){
        MSB_FORMAT(sb, "\"", eni->node._value, "\":\"");
        msb_write_json_escaped_str(sb, eni->text.text, eni->text.length);
        msb_write_char(sb, '"');
        msb_write_char(sb, ',');
    }
    if(ctx->explicit_node_ids.count)
        msb_erase(sb, 1);
    msb_write_char(sb, '}');
    msb_write_literal(sb, ",\"links\":{");
    for(size_t i = 0; i < ctx->links.capacity_; i++){
        StringView key = ctx->links.keys[i];
        if(!key.length) continue;
        StringView value = ctx->links.keys[i+ctx->links.capacity_];
        msb_write_char(sb, '"');
        msb_write_json_escaped_str(sb, key.text, key.length);
        msb_write_literal(sb, "\":\"");
        msb_write_json_escaped_str(sb, value.text, value.length);
        msb_write_char(sb, '"');
        msb_write_char(sb, ',');
    }
    if(ctx->links.count_)
        msb_erase(sb, 1);
    msb_write_char(sb, '}');
    MSB_FORMAT(sb, ",\"flags\":", ctx->flags);
    msb_write_literal(sb, ",\"dependencies\":[");
    MARRAY_FOR_EACH(StringView, dep, ctx->dependencies){
        msb_write_char(sb, '"');
        msb_write_json_escaped_str(sb, dep->text, dep->length);
        msb_write_char(sb, '"');
        msb_write_char(sb, ',');
    }
    if(ctx->dependencies.count)
        msb_erase(sb, 1);
    msb_write_char(sb, ']');
    msb_write_char(sb, '}');
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
