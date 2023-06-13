//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#include "Platform/Windows/wincli.h"
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include "common_macros.h"
#include "dndc_long_string.h"
#include "dndc_ast.h"
#include "Utils/thread_utils.h"
#include "Utils/argument_parsing.h"
#include "Utils/term_util.h"
#include "Allocators/mallocator.h"

#define MARRAY_T StringView
#include "Utils/Marray.h"

#define MARRAY_STRINGVIEW_DEFINED
#include "Utils/recursive_glob.h"


typedef struct Tag Tag;
struct Tag {
    StringView tagname;
    StringView filename;
    int row;
};

#define MARRAY_T Tag
#include "Utils/Marray.h"

typedef struct WorkItem WorkItem;
struct WorkItem {
    StringView filename;
    Marray__Tag tags;
};
#define MARRAY_T WorkItem
#include "Utils/Marray.h"

#define MARRAY_T ThreadHandle
#include "Utils/Marray.h"

Marray__WorkItem items;
// My version of msvc doesn't have _Atomic, but volatile
// has atomic semantics.
#if defined(_MSC_VER) && ! defined(__clang__)
volatile size_t item_idx;
#else
_Atomic size_t  item_idx;
#endif
_Bool ignore_errors;

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
int process(WorkItem* item);

static
THREADFUNC(worker_func){
    (void)thread_arg;
    for(;;){
        size_t idx = item_idx++;
        if(idx >= items.count) return 0;
        WorkItem* item = &items.data[idx];
        int e = process(item);
        if(e && !ignore_errors){
            exit(1);
        }
    }
    return 0;
}

static
int
iterate_children(DndcContext*, DndcNodeHandle, Marray__Tag*, StringView);

static
int
process(WorkItem* item){
    StringView filename = item->filename;
    // fprintf(stderr, "[%p] %s\n", pthread_self(), filename.text);
    DndcContext* ctx = dndc_create_ctx(DNDC_NO_THREADS, NULL, NULL);
    if(!ctx) return 1;
    dndc_ctx_set_logger(ctx, dndc_stderr_log_func, NULL);
    DndcNodeHandle root = dndc_ctx_make_root(ctx, filename);
    if(root == DNDC_NODE_HANDLE_INVALID) goto fail;
    int e;
    e = dndc_ctx_parse_file(ctx, root, filename);
    if(e) goto fail;
    e = iterate_children(ctx, root, &item->tags, filename);
    if(e) goto fail;


    dndc_ctx_destroy(ctx);
    return 0;

    fail:
    dndc_ctx_destroy(ctx);
    return 1;
}


static
int
iterate_children(DndcContext* ctx, DndcNodeHandle node, Marray__Tag* tags, StringView filename){
    StringView id;
    int e;
    int type = dndc_node_get_type(ctx, node);
    switch((enum DndcNodeType)type){
        case DNDC_NODE_TYPE_INVALID:
        case DNDC_NODE_TYPE_STRING:
        case DNDC_NODE_TYPE_TOC:
            return 0;
        default: break;
    }
    e = dndc_node_get_id(ctx, node, &id);
    if(e) return 1;
    if(id.length){
        StringView header;
        e = dndc_node_get_header(ctx, node, &header);
        if(e) return 1;
        if(header.length){
            StringView head = {.length=header.length, .text=Allocator_strndup(MALLOCATOR, header.text, header.length)};
            DndcNodeLocation loc;
            e = dndc_node_location(ctx, node, &loc);
            if(e) return 1;
            e = Marray_push__Tag(tags, MALLOCATOR,  (Tag){
                .filename = filename,
                .tagname = head,
                .row = loc.row,
            });
            if(e) return 1;
        }
    }
    DndcNodeHandle handles[32] = {0};
    size_t cookie = 0;
    for(size_t n; (n = dndc_node_get_children(ctx, node, &cookie, handles, arrlen(handles)));){
        for(size_t i = 0; i < n; i++){
            e = iterate_children(ctx, handles[i], tags, filename);
            if(e) return 1;
        }
    }
    return 0;
}
static
void
tag_dnd_files(StringView* filenames, size_t filename_count, LongString outfile, size_t n_threads){
    if(n_threads > 128)
        n_threads = 128;
    for(size_t i = 0; i < filename_count; i++){
        int err = Marray_push__WorkItem(&items, MALLOCATOR,  (WorkItem){.filename = filenames[i]});
        unhandled_error_condition(err);
    }
    Marray__ThreadHandle threads = {0};
    for(size_t i = 0; i < n_threads; i++){
        ThreadHandle* th; int err = Marray_alloc__ThreadHandle(&threads, MALLOCATOR, &th);
        unhandled_error_condition(err);
        int th_err = create_thread(th, worker_func, NULL);
        unhandled_error_condition(th_err);
    }
    worker_func(NULL); // use this thread as well
    for(size_t i = 0; i < threads.count; i++)
        join_thread(threads.data[i]);
    Marray__Tag tags = {0};
    for(size_t i = 0; i < items.count; i++){
        int err = Marray_extend__Tag(&tags, MALLOCATOR, items.data[i].tags.data, items.data[i].tags.count);
        unhandled_error_condition(err);
    }
    qsort(tags.data, tags.count, sizeof(Tag), StringView_cmp);
    FILE* fp = outfile.length? fopen(outfile.text, "w") : stdout;
    fprintf(fp, "!_TAG_FILE_FORMAT\t1\t/basic format; no extension fields/\n");
    fprintf(fp, "!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted, 2=foldcase/\n");
    for(size_t i = 0; i< tags.count; i++){
        Tag tag = tags.data[i];
        fprintf(fp, "%.*s\t%.*s\t%d\n",
                (int)tag.tagname.length, tag.tagname.text,
                (int)tag.filename.length, tag.filename.text,
                tag.row
              );
    }
    fclose(fp);
}

static
int
sv_append(void* p, const void* sv_){
    Marray__StringView* m = p;
    const StringView* sv = sv_;
    int err = Marray_push__StringView(m, MALLOCATOR, *sv);
    unhandled_error_condition(err);
    return 0;
}

int
main(int argc, char** argv){
#ifdef _WIN32
    // unclear if this is needed.
    if(get_main_args(&argc, &argv) != 0) return 1;
#endif
    Args args = argc?(Args){argc-1, (const char*const*)argv+1}: (Args){0, 0};
#ifdef _WIN32
    LongString directory = {.text = _getcwd(0, 0)};
#else
    LongString directory = {.text = getcwd(0, 0)};
#endif
    LongString output = {0};
    directory.length = strlen(directory.text);
    Marray__StringView dnd_files = {0};
    uint64_t n_threads = num_cpus();
    ArgParseUserDefinedType sv_list = {
        .type_name = LS("string"),
    };

    ArgToParse pos_args[] = {
        [0] = {
            .name = SV("files"),
            .max_num = 10000,
            .dest = ArgUserDest(&dnd_files, &sv_list),
            .help = "Which files to tag. If not given, will read from stdin if it is not interactive. If there are still no files, it will instead recursively glob the current directory for files ending in '.dnd'.",
            .append_proc = sv_append,
        },
    };
    ArgToParse kw_args[] = {
        {
            .name = SV("-o"),
            .altname1 = SV("--output"),
            .dest = ARGDEST(&output),
            .help = "Where to write the tag file. Defaults to stdout if not given."
        },
        {
            .name = SV("-j"),
            .altname1 = SV("--n-threads"),
            .dest = ARGDEST(&n_threads),
            .show_default = true,
            .help = "How many additional threads to spawn to process the work",
        },
        {
            .name = SV("--ignore-errors"),
            .altname1 = SV("--keep-going"),
            .dest = ARGDEST(&ignore_errors),
            .show_default = true,
            .help = "Skip files with errors instead of exiting with an error.",
        },
    };

    enum {HELP=0, VERSION, FISH};
    ArgToParse early_args[] = {
        [HELP] = {
            .name = SV("-h"),
            .altname1 = SV("--help"),
            .help = "Print this help and exit.",
        },
        [VERSION] = {
            .name = SV("-v"),
            .altname1 = SV("--version"),
            .help = "Print the version and exit.",
        },
        [FISH] = {
            .name = SV("--fish-completions"),
            .help = "Print out commands for fish shell completions.",
            .hidden = true,
        },
    };

    ArgParser parser = {
        .name = argc?argv[0]:"dndc-tag",
        .description = "A utility for tagging dnd files",
        .positional.args = pos_args,
        .positional.count = arrlen(pos_args),
        .early_out.args = early_args,
        .early_out.count = arrlen(early_args),
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
    };
    int columns = get_terminal_size().columns;
    switch(check_for_early_out_args(&parser, &args)){
        case HELP:
            print_argparse_help(&parser, columns);
            return 0;
        case VERSION:
            puts("dndc-tag v0.0.1");
            return 0;
        case FISH:
            print_argparse_fish_completions(&parser);
            return 0;
        default:
            break;
    }
    enum ArgParseError error = parse_args(&parser, &args, ARGPARSE_FLAGS_NONE);
    if(error){
        print_argparse_error(&parser, error);
        return error;
    }
    char buff[1024];
    if(!isatty(STDIN_FILENO))
        while(fgets(buff, sizeof buff, stdin)){
            size_t len = strlen(buff);
            if(!len || len == 1) continue;
            int err = Marray_push__StringView(&dnd_files, MALLOCATOR, (StringView){
                .length = len-1,
                .text = Allocator_strndup(MALLOCATOR, buff, len-1),
            });
            unhandled_error_condition(err);
        }
    if(!dnd_files.count)
        recursive_glob_suffix(directory, SV(".dnd"), &dnd_files, 100);
    tag_dnd_files(dnd_files.data, dnd_files.count, output, n_threads);
    return 0;
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif


#include "Allocators/allocator.c"
#include "Utils/recursive_glob.c"
