#include "common_macros.h"
#include "dndc_local_server.h"
#include "argument_parsing.h"
#include "term_util.h"
#include "murmur_hash.h"
#include "thread_utils.h"
#include "get_input.h"
#include "str_util.h"
#include "allocator.h"
#include "mallocator.h"
#include "string_distances.h"
#include "MStringBuilder.h"
#include "msb_url_helpers.h"
#include "msb_format.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#if defined(__APPLE__) || defined(__linux__)
#include <fts.h>
#elif defined(_WIN32)
#endif

#define MARRAY_T StringView
#include "Marray.h"

typedef Marray(StringView) Entries;


struct JobData {
    DndServer* server;
    LongString directory;
    uint64_t flags;
};

static
THREADFUNC(serve){
    struct JobData* data = thread_arg;
    dnd_server_serve(data->server, data->flags, data->directory);
    return 0;
}

struct TabContext {
    const Entries* original;
    Entries ordered;
};
static GiTabCompletionFunc entry_completer;

static
void
get_entries(LongString directory, Entries*entries);

static
void
print_entries(Entries);

static
void null_report(void* ud, int type, const char* filename, int filename_len, int line, int col, const char* message, int message_len){
    (void)ud;
    (void)type;
    (void)filename;
    (void)filename_len;
    (void)line;
    (void)col;
    (void)message;
    (void)message_len;
}


int
main(int argc, char** argv){
    LongString directory = LS(".");
    int port = 0;
    {
        char* cwd;
        #ifdef _WIN32
            cwd = _getcwd(NULL, 0);
        #else
            cwd = getcwd(NULL, 0);
        #endif
        if(!cwd) return 1;
        size_t cwdlen = strlen(cwd);
        port = (3000 + murmur3_32((const uint8_t*)cwd, cwdlen, 0x1337)) & 0x7fff;
        free(cwd);
    }

    uint64_t flags = 0 | DNDC_ALLOW_BAD_LINKS;

    ArgToParse pos_args[] = {
        [0] = {
            .name = SV("directory"),
            .dest = ARGDEST(&directory),
            .help = "What directory to serve dnd from.",
            .min_num = 0,
            .max_num = 1,
            .show_default = true,
        },
    };
    _Bool should_log = false;
    ArgToParse kw_args[] = {
        {
            .name = SV("-p"),
            .altname1 = SV("--port"),
            .dest = ARGDEST(&port),
            .help = "What port to use. If not given, automatically derived from current directory. Pass 0 to have OS choose.",
            .min_num = 0,
            .max_num = 1,
            .show_default = 1,
        },
        {
            .name = SV("--stats"),
            .dest = ArgBitFlagDest(&flags, DNDC_PRINT_STATS),
            .help = "Print dndc timing statistics",
        },
        {
            .name = SV("--link-imgs"),
            .dest = ArgBitFlagDest(&flags, DNDC_DONT_INLINE_IMAGES),
            .help = "Use links instead of inlining images",
        },
        {
            .name = SV("--log"),
            .dest = ARGDEST(&should_log),
            .help = "log requests etc. (can be chatty)",

        },
    };
    enum {HELP, VERSION, FISH};
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
        [FISH] = {
            .name = SV("--fish-completions"),
            .help = "Print out commands for fish shell completions.",
            .hidden = true,
        },
    };
    const char* version = "dndc-browse 0.0.1. Compiled " __DATE__ " " __TIME__ ".";
    ArgParser argparser = {
        .name = argc? argv[0]: "dndc-browse",
        .description = "A local dnd web server.",
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
        case FISH:
            print_argparse_fish_completions(&argparser);
            return 0;
        default:
            break;
    }
    enum ArgParseError e = parse_args(&argparser, &args, ARGPARSE_FLAGS_NONE);
    if(e){
        print_argparse_error(&argparser, e);
        fprintf(stderr, "Use --help to see usage.\n");
        return e;
    }
    while(directory.length > 1 && directory.text[directory.length-1] == '/'){
        PushDiagnostic();
        SuppressCastQual();
        ((char*)directory.text)[--directory.length] = 0;
        PopDiagnostic();
    }

    DndServer* server = dnd_server_create(should_log?dndc_stderr_error_func:null_report, NULL, &port);
    if(!server) return 1;

    struct JobData data = {
        .directory = directory,
        .flags = flags,
        .server = server,
    };

    ThreadHandle thrd;
    create_thread(&thrd, &serve, &data);
    // int err = dnd_server_serve(server, flags, directory);
    Entries entries = {0};
    get_entries(directory, &entries);
    if(!entries.count) return 1;
    print_entries(entries);
    struct TabContext tabctx = {
        .original = &entries,
    };
    GetInputCtx input = {.prompt = SV("> "), .prompt_display_length=2, .tab_completion_func = entry_completer, .tab_completion_user_data=&tabctx};
    const char* opencmd = "open";
    const char* url = "http://localhost";
    #if defined(__APPLE__)
        opencmd = "open";
    #elif defined(__linux__)
        opencmd = "xdg-open";
    #elif defined(_WIN32)
        opencmd = "start";
    #endif
    MStringBuilder opensb = {.allocator = get_mallocator()};
    MSB_FORMAT(&opensb, opencmd, " ", url, ":", port, "/");
    size_t before_entry = opensb.cursor;
    for(;;){
        ssize_t len = gi_get_input(&input);
        if(len < 0) break;
        if(len == 0) continue;
        StringView b = stripped_view(input.buff, len);
        if(SV_equals(b, SV("l")) || SV_equals(b, SV("list"))){
            print_entries(entries);
            gi_add_line_to_history_len(&input, b.text, b.length);
            continue;
        }
        if(SV_equals(b, SV("q")) || SV_equals(b, SV("quit"))){
            break;
        }
        Int64Result ir = parse_int64(b.text, b.length);
        int64_t idx = -1;
        if(ir.errored) {
            ssize_t best = 1LL<<32;
            int strip_dnd = !endswith(b, SV(".dnd"));
            for(size_t i = 0; i < entries.count; i++){
                // calculate expand distance, but without .dnd if we don't have .dnd in buffer
                ssize_t dist = byte_expansion_distance(entries.data[i].text, entries.data[i].length-4*strip_dnd, b.text, b.length);
                if(dist < 0) continue;
                if(dist < best){
                    best = dist;
                    idx = i;
                    if(dist == 0) break;
                }
            }
        }
        else
            idx = ir.result;
        if(idx < 0) continue;
        if((size_t)idx >= entries.count) continue;
        gi_add_line_to_history_len(&input, b.text, b.length);
        StringView entry = entries.data[idx];
        opensb.cursor = before_entry;
        // percent encoding takes care of bad shell characters.
        msb_url_percent_encode_filepath(&opensb, entry.text, entry.length);
        LongString op = msb_borrow_ls(&opensb);
        int s = system(op.text);
        (void)s;
    }
    puts("\r");
    return 0;
}

#if defined(_WIN32)
// This code is totally untested, but is basically a copy of how the dndc_qjs
// does it. Too lazy to boot up windows right now.
//
// Maybe I should add a recursive glob utility function? Trouble is that
// you would want it iterator style, which is annoying to make nice in C.
#include "MStringBuilder.h"
static
void
get_entries_inner(StringView directory, Entries* entries){
    MStringBuilder sb = {.allocator = get_mallocator()};
    msb_write_str(&sb, directory.text, directory.length);
    msb_write_literal(&sb, "/*.dnd");
    msb_nul_terminate(&sb);
    LongString dndwildcard = msb_borrow_ls(&sb);
    WIN32_FIND_DATAA findd;
    HANDLE handle = FindFirstFileExA(dndwildcard.text, FindExInfoBasic, &findd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    msb_erase(&sb, sizeof("/*.dnd")-1);
    if(handle == INVALID_HANDLE_VALUE){
    }
    else{
        do {
            size_t cursor = sb.cursor;
            msb_write_char(&sb, '/');
            msb_write_str(&sb, findd.cFileName, strlen(findd.cFileName));
            StringView text = msb_borrow_sv(&sb);
            char* s = Allocator_strndup(get_mallocator(), text.text, text.length);
            StringView* it = Marray_alloc(StringView)(entries, get_mallocator());
            *it = (StringView){.text=s, .length=text.length};
            sb.cursor = cursor;
        }while(FindNextFileA(handle, &findd));
        FindClose(handle);
    }
    msb_write_literal(&sb, "/*");
    msb_nul_terminate(&sb);
    LongString thisdir = msb_borrow_ls(&sb);
    handle = FindFirstFileExA(thisdir.text, FindExInfoBasic, &findd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if(handle == INVALID_HANDLE_VALUE){
        // fprintf(stderr, "Invalid handle: '%s'\n", thisdir.text);
        goto end;
    }
    msb_erase(&sb, sizeof("/*")-1);
    do {
        if(findd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN){
            continue;
        }
        if(!(findd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)){
            continue;
        }
        StringView fn = {.text = findd.cFileName, .length = strlen(findd.cFileName)};
        if(fn.text[0] == '.'){
            continue;
        }
        msb_write_char(&sb, '/');
        msb_write_str(&sb, fn.text, fn.length);
        msb_nul_terminate(&sb);
        StringView nextdir = msb_borrow_sv(&sb);
        get_entries_inner(nextdir, buff, count, cap);
        msb_erase(&sb, 1+fn.length);
    }while(FindNextFileA(handle, &findd));
    end:
    msb_destroy(&sb);
}
#endif

static
void
get_entries(LongString directory, Entries* entries){
#if defined(__APPLE__) || defined(__linux__)
    const char* dirs[] = {directory.text, NULL};
    PushDiagnostic();
    SuppressCastQual();
    FTS* handle = fts_open((char**)dirs, FTS_LOGICAL | FTS_NOCHDIR | FTS_NOSTAT, NULL);
    PopDiagnostic();
    if(!handle) return;
    for(;;){
        FTSENT* ent = fts_read(handle);
        if(!ent) break;
        if(ent->fts_namelen > 1 && ent->fts_name[0] == '.'){
            fts_set(handle, ent, FTS_SKIP);
            continue;
        }
        if(ent->fts_info & (FTS_F | FTS_NSOK)){
            StringView name = {.text = ent->fts_name, .length=ent->fts_namelen};
            if(!endswith(name, SV(".dnd")))
                continue;
            char* p = ent->fts_path + directory.length+1;
            size_t len = strlen(p);
            char* t = Allocator_strndup(get_mallocator(), p, len);
            StringView* it = Marray_alloc(StringView)(entries, get_mallocator());
            *it = (StringView){.length = len, .text = t};
        }
    }
    fts_close(handle);
#elif defined(_WIN32)
    get_entries_inner(LS_to_SV(directory), entries);
#endif
}

static
void
print_entries(Entries entries){
    putchar('\r');
    qsort(entries.data, entries.count, sizeof entries.data[0], StringView_cmp);
    for(size_t i = 0; i < entries.count; i++){
        StringView entry = entries.data[i];
        fprintf(stdout, "[%2zu] %.*s\n", i, (int)entry.length, entry.text);
    }
}

struct Pair {
    size_t idx;
    ssize_t distance;
};
static
int
distance_cmp(const void* a, const void* b){
    const struct Pair* pa = a;
    const struct Pair* pb = b;
    if(pa->distance < pb->distance) return -1;
    if(pa->distance > pb->distance) return 1;
    return 0;
}

static
int
entry_completer(GetInputCtx* ctx, size_t original_cursor, size_t original_count, int n_tabs){
    struct TabContext* tctx = ctx->tab_completion_user_data;
    if(n_tabs == 1){
        StringView original = {.length=original_count, .text=ctx->altbuff};
        int strip_dnd = !endswith(original, SV(".dnd"));
        tctx->ordered.count = 0;
        struct Pair* distances = malloc(tctx->original->count * sizeof*distances);
        size_t n = 0;
        for(size_t i = 0; i < tctx->original->count; i++){
            StringView hay = tctx->original->data[i];
            ssize_t distance = byte_expansion_distance(hay.text, hay.length-4*strip_dnd, ctx->altbuff, original_count);
            if(distance < 0) continue;
            distances[n].idx = i;
            distances[n].distance = distance;
            n++;
        }
        qsort(distances, n, sizeof *distances, distance_cmp);
        for(size_t i = 0; i < n; i++){
            StringView* sv = Marray_alloc(StringView)(&tctx->ordered, get_mallocator());
            *sv = tctx->original->data[distances[i].idx];
        }
        free(distances);
        // initialize
    }
    if(ctx->tab_completion_cookie >= tctx->ordered.count){
        memcpy(ctx->buff, ctx->altbuff, original_count);
        ctx->buff_count = original_count;
        ctx->buff_cursor = original_cursor;
        ctx->buff[original_count] = 0;
        ctx->tab_completion_cookie = 0;
        return 0;
    }
    StringView completion = tctx->ordered.data[ctx->tab_completion_cookie++];
    if(completion.length >= GI_BUFF_SIZE-1)
        return 1;
    memcpy(ctx->buff, completion.text, completion.length);
    ctx->buff[completion.length] = 0;
    ctx->buff_count = completion.length;
    ctx->buff_cursor = completion.length;
    return 0;
}

#include "dndc_local_server.c"
#include "get_input.c"
