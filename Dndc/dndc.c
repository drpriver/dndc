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
#include "dndc_file_cache.h"

#include "path_util.h"
#include "MStringBuilder.h"
#include "msb_extensions.h"
#include "msb_format.h"
#include "allocator.h"
#include "mallocator.h"
#include "linear_allocator.h"
#include "recording_allocator.h"
#include "arena_allocator.h"
#include "measure_time.h"
#include "thread_utils.h"

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
        if(!c) return NULL;
        if(hayend - c < needlesz) return NULL;
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
execute_user_scripts(DndcContext* ctx){
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
        NodeType type;
        NodeHandle firstchild;
        MStringBuilder msb = (MStringBuilder){.allocator=ctx->string_allocator};
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
                    result = GENERIC_ERROR;
                    goto cleanup;
                }
                assert(!jsctx);
                DndcJsFlags jsflags = DNDC_JS_FLAGS_NONE;
                jsctx = new_qjs_ctx(rt, ctx, jsflags);
                if(!jsctx){
                    report_system_error(ctx, SV("Failed to initialize javascript context"));
                    result = GENERIC_ERROR;
                    goto cleanup;
                }
                uint64_t after_init = get_t();
                report_time(ctx, SV("qjs init took: "), after_init-before_init);
            }
            int js_err = execute_qjs_string(jsctx, ctx, str.text, str.length, handle, firstchild);
            msb_destroy(&msb);
            if(js_err){
                report_set_error(ctx);
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
                    node_remove_child(parent, j, ctx->allocator);
                    goto after;
                }
            }
            // don't both warning here, but leave the scaffolding in case I want to.
            after:;
        }
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
execute_user_scripts_and_load_images(DndcContext* ctx, Nullable(WorkerThread*) worker){
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
                        StringView* sv = Marray_alloc(StringView)(&job.sourcepaths, ctx->allocator);
                        *sv = child->header;
                    }
                }
                else {
                    // Otherwise we build the path relative to the given
                    // include directory.
                    // Get's cleaned up with the string allocator.
                    MStringBuilder path_builder = {.allocator=ctx->string_allocator};
                    msb_write_str(&path_builder, ctx->base_directory.text, ctx->base_directory.length);
                    msb_append_path(&path_builder, child->header.text, child->header.length);
                    StringView path = msb_borrow_sv(&path_builder);
                    if(! FileCache_has_file(job.b64cache, path)){
                        StringView* sv = Marray_alloc(StringView)(&job.sourcepaths, ctx->allocator);
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

    result = execute_user_scripts(ctx);

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
        Marray_cleanup(StringView)(&job.sourcepaths, ctx->allocator);
    }
    else {
        report_info(ctx, SV("No binary work was to be done."));
    }
    return result;
}

static
int
run_the_dndc(uint64_t flags,
        StringView base_directory,
        StringView source_text,
        StringView source_path,
        StringView outpath,
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
    // Strings live for the entire duration of this function, so the linear
    // arena allocator is appropriate.
    ArenaAllocator arena_allocator = {0};
    const Allocator string_allocator = {.type=ALLOCATOR_ARENA, ._data=&arena_allocator};
    // General purpose allocation (nodes, attributes, etc.).
    ArenaAllocator main_arena = {0};
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
        .b64cache = external_b64cache? external_b64cache: dndc_create_filecache(),
        // The text cache only runs on this thread so we can just use the
        // general allocator.
        .textcache = external_textcache?  external_textcache : dndc_create_filecache(),
        .error_func = error_func,
        .error_user_data = error_user_data,
    };
    ctx_add_builtins(&ctx);
    if(!source_text.text){
        report_system_error(&ctx, SV("String with no data given as input"));
        result = UNEXPECTED_END;
        goto cleanup;
    }
    // Store the input text as a builtin so user scripts can access it.
    ctx_store_builtin_file(&ctx, source_path, source_text);
    // Quick and dirty estimate of how many nodes we will need.
    Marray_ensure_total(Node)(&ctx.nodes, ctx.allocator, source_text.length/10+1);

    // Setup the root node.
    {
        NodeHandle root_handle = alloc_handle(&ctx);
        ctx.root_handle = root_handle;
        Node* root = get_node(&ctx, root_handle);
        root->col = 0;
        root->row = 0;
        Marray_push(StringView)(&ctx.filenames, ctx.allocator, source_path);
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
            report_set_error(&ctx);
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
            node_print_err(&ctx, node, LS("Imports are illegal for untrusted input."));
            result = PARSE_ERROR;
            goto cleanup;
        }
        if(ctx.user_script_nodes.count){
            NodeHandle handle = ctx.user_script_nodes.data[0];
            Node* node = get_node(&ctx, handle);
            node_print_err(&ctx, node, LS("JS blocks are illegal for untrusted input."));
            result = PARSE_ERROR;
            goto cleanup;
        }
        if(ctx.script_nodes.count){
            NodeHandle handle = ctx.script_nodes.data[0];
            Node* node = get_node(&ctx, handle);
            node_print_err(&ctx, node, LS("Script blocks are illegal for untrusted input."));
            result = PARSE_ERROR;
            goto cleanup;
        }
    }
    else {
        // Handle imports. Imports can import more imports, so don't use a FOR_EACH.
        uint64_t before_imports = get_t();
        // for(size_t i = 0; i < ctx.imports.count; progbar("Imports", &i, ctx.imports.count)){
        for(size_t i = 0; i < ctx.imports.count; i++){
            NodeHandle handle = ctx.imports.data[i];
            // We parse into a different node and then swap the two.
            NodeHandle newhandle = alloc_handle(&ctx);
            Node* node = get_node(&ctx, handle);
            bool was_import = false;
            {
                Node* newnode = get_node(&ctx, newhandle);
                *newnode = *node;
                newnode->children.count = 0;
                if(newnode->type == NODE_IMPORT){
                    newnode->type = NODE_MD;
                    was_import = true;
                }
                newnode->attributes = NULL;
                RARRAY_FOR_EACH(Attribute, attr, node->attributes){
                    if(!SV_equals(attr->key, SV("import")))
                        node_set_attribute(newnode, ctx.allocator, attr->key, attr->value);
                }
            }
            if(ctx.imports.count > 1000){
                node_print_err(&ctx, node, LS("More than 1000 imports. Aborting parsing (did you accidentally create an import cycle?)"));
                result = PARSE_ERROR;
                goto cleanup;
            }
            // NOTE: re-get the node every loop as the pointer is invalidated.
            for(size_t j = 0; j < node_children_count(node); j++, node=get_node(&ctx, handle)){
                NodeHandle child_handle = node_children(node)[j];
                Node* child = get_node(&ctx, child_handle);
                if(child->type != NODE_STRING){
                    node_print_err(&ctx, child, LS("import child is not a string"));
                    result = PARSE_ERROR;
                    goto cleanup;
                }
                StringView filename = child->header;
                StringViewResult imp_e = ctx_load_source_file(&ctx, filename);
                if(imp_e.errored){
                    MStringBuilder err_builder = {.allocator = ctx.temp_allocator};
                    if(ctx.base_directory.length){
                        MSB_FORMAT(&err_builder, "Unable to open '", ctx.base_directory, "/", filename, "'");
                    }
                    else{
                        MSB_FORMAT(&err_builder, "Unable to open '", filename, "'");
                    }
                    node_print_err(&ctx, child, msb_borrow_ls(&err_builder));
                    msb_destroy(&err_builder);
                    result = imp_e.errored;
                    goto cleanup;
                }
                StringView imp_text = imp_e.result;
                int parse_e = dndc_parse(&ctx, newhandle, filename, imp_text.text, imp_text.length);
                if(parse_e){
                    report_set_error(&ctx);
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
                case NODE_DATA:
                    handles = &ctx.data_nodes;
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
                Marray_push(NodeHandle)(handles, ctx.allocator, newhandle);
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
        int e = execute_user_scripts_and_load_images(&ctx, worker);
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
        result = PARSE_ERROR;
        goto cleanup;
    }
    // Check that the tree is not too deep!
    if(!wasm){
        uint64_t before = get_t();
        int e = check_depth(&ctx);
        if(e){
            report_set_error(&ctx);
            result = e;
            goto cleanup;
        }
        uint64_t after = get_t();
        report_time(&ctx, SV("Checking depth took "), after-before);
    }
    // Create links from headers.
    {
        uint64_t before = get_t();
        gather_anchors(&ctx);
        uint64_t after = get_t();
        report_time(&ctx, SV("Link resolving took: "), after-before);
        // FIXME: if an error can be set while gathering anchors, we should
        // return an error!
        if(ctx.error.message.length){
            report_set_error(&ctx);
            result = PARSE_ERROR;
            goto cleanup;
        }
    }

    // Render the nav block if we have one.
    {
        uint64_t before = get_t();
        if(! NodeHandle_eq(ctx.navnode, INVALID_NODE_HANDLE))
            build_nav_block(&ctx);
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
        // Sort so we can do a binary search.
        if(ctx.links.count){
            uint64_t before_sort = get_t();
            #if defined(WASM) || 1
                LinkItem__array_sort(ctx.links.data, ctx.links.count);
            #else
                qsort(ctx.links.data, ctx.links.count, sizeof(ctx.links.data[0]), StringView_cmp);
            #endif
            uint64_t after_sort = get_t();
            report_time(&ctx, SV("Sorting links took: "), (after_sort-before_sort));
        }
        report_size(&ctx, SV("ctx.links.count = "), ctx.links.count);
    }

    // Render data nodes into the data blob.
    if(!wasm){
        uint64_t before_data = get_t();
        MStringBuilder sb = {.allocator=ctx.allocator};
        MARRAY_FOR_EACH(NodeHandle, handle, ctx.data_nodes){
            Node* data_node = get_node(&ctx, *handle);
            // Node could've been mutated after being registered.
            if(data_node->type != NODE_DATA)
                continue;
            NODE_CHILDREN_FOR_EACH(it, data_node){
                Node* child = get_node(&ctx, *it);
                if(!child->header.length){
                    node_print_warning(&ctx, child, SV("Missing header from data child?"));
                }
                // FIXME:
                // A maliciously crafted js block could bypass our depth check
                // up above by detaching the data node and making one too deep,
                // thus making us vulnerable to stack exhaustion during this
                // recursive call.
                {
                    msb_reset(&sb);
                    int e = render_node(&ctx, &sb, *it, 1);
                    if(e){
                        report_set_error(&ctx);
                        result = e;
                        goto cleanup;
                    }
                }
                if(!sb.cursor){
                    node_print_warning(&ctx, child, SV("Rendered a data node with no data. Not outputting it."));
                    continue;
                }
                LongString text = msb_detach_ls(&sb);
                DataItem* di = Marray_alloc(DataItem)(&ctx.rendered_data, ctx.allocator);
                di->key = child->header;
                di->value = text;
            }
        }
        uint64_t after_data = get_t();
        report_time(&ctx, SV("Data blob rendering took: "), after_data-before_data);
        report_size(&ctx, SV("ctx.rendered_data.count = "), ctx.rendered_data.count);
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
            report_set_error(&ctx);
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
            report_set_error(&ctx);
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
    report_size(&ctx, SV("la_.high_water = "), la_.high_water);
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
            uint64_t after = get_t();
            report_time(&ctx, SV("Reporting sizes: "), after-before);
        }
        {
            uint64_t before = get_t();
            Allocator_free_all(string_allocator);
            uint64_t after = get_t();
            report_time(&ctx, SV("Cleaning string allocator: "), after-before);
        }
        {
            uint64_t before = get_t();
            Allocator_free_all(allocator);
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
#include "dndc_file_cache.c"
#include "allocator.c"

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
    return OS_ERROR;
}

static
QJSRuntime*_Nullable
new_qjs_rt(ArenaAllocator*aa){
    (void)aa;
    return NULL;
}

static
QJSContext*_Nullable
new_qjs_ctx(QJSRuntime*rt, DndcContext*ctx, DndcJsFlags flags){
    (void)rt, (void)ctx, (void)flags;
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
dndc_format(StringView source_text, LongString* output, Nullable(DndcErrorFunc*)error_func, Nullable(void*)error_user_data){
    uint64_t flags = 0
        | DNDC_SUPPRESS_WARNINGS
        | DNDC_ALLOW_BAD_LINKS
        | DNDC_REFORMAT_ONLY
        ;
    int e = run_the_dndc(flags, SV(""), source_text, SV(""), SV(""), output, NULL, NULL, error_func, error_user_data, NULL, NULL, NULL, NULL, NULL);
    return e;
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
    switch((enum DndcErrorMessageType)type){
        case DNDC_NODELESS_MESSAGE:
            fprintf(stderr, "[ERROR]: %.*s\n", message_len, message);
            return;
        case DNDC_STATISTIC_MESSAGE:
            fprintf(stderr, "[INFO] %.*s\n", message_len, message);
            return;
        case DNDC_DEBUG_MESSAGE:
            if(filename_len){
                if(col >= 0){
                    fprintf(stderr, "[DEBUG] %.*s:%d:%d: %.*s\n", filename_len, filename, line+1, col+1, message_len, message);
                }
                else {
                    fprintf(stderr, "[DEBUG] %.*s:%d: %.*s\n", filename_len, filename, line+1, message_len, message);
                }
            }
            else
                fprintf(stderr, "[DEBUG] %.*s\n", message_len, message);
            return;
        case DNDC_ERROR_MESSAGE:
            if(col >= 0){
                fprintf(stderr, "[ERROR] %.*s:%d:%d: %.*s\n", filename_len, filename, line+1, col+1, message_len, message);
            }
            else {
                fprintf(stderr, "[ERROR] %.*s:%d: %.*s\n", filename_len, filename, line+1, message_len, message);
            }
            return;
        case DNDC_WARNING_MESSAGE:
            if(col >= 0){
                fprintf(stderr, "[WARN] %.*s:%d:%d: %.*s\n", filename_len, filename, line+1, col+1, message_len, message);
            }
            else {
                fprintf(stderr, "[WARN] %.*s:%d: %.*s\n", filename_len, filename, line+1, message_len, message);
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
    #define words "|MD|DIV|STRING|PARA|TITLE|HEADING|TABLE|TABLE_ROW|STYLESHEETS|LINKS|SCRIPTS|IMPORT|IMAGE|BULLETS|RAW|PRE|LIST|LIST_ITEM|KEYVALUE|KEYVALUEPAIR|IMGLINKS|NAV|DATA|COMMENT|CONTAINER|QUOTE|HR|JS|DETAILS|"
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
    #define words u"|MD|DIV|STRING|PARA|TITLE|HEADING|TABLE|TABLE_ROW|STYLESHEETS|LINKS|SCRIPTS|IMPORT|IMAGE|BULLETS|RAW|PRE|LIST|LIST_ITEM|KEYVALUE|KEYVALUEPAIR|IMGLINKS|NAV|DATA|COMMENT|CONTAINER|QUOTE|HR|JS|DETAILS|"
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
    DndcStringView outpath,
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
            | DNDC_FRAGMENT_ONLY
            | DNDC_DONT_WRITE
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
        return GENERIC_ERROR;
    if(!outstring)
        return GENERIC_ERROR;
    int err = run_the_dndc(flags, base_directory, source_text, source_path, outpath, outstring, base64cache, textcache, error_func, error_user_data, dependency_func, dependency_user_data, NULL, NULL, (WorkerThread*)worker_thread);
    return err;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
