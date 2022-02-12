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

static
void
get_entries(LongString directory, StringView** entries, size_t* nentries);

static
void
print_entries(StringView* entries, size_t nentries);

static
void null_report(void* ud, int type, const char* filename, int filename_len, int line, int col, const char* message, int message_len){
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
    while(directory.length > 1 && directory.text[directory.length-1] == '/')
        ((char*)directory.text)[--directory.length] = 0;

    DndServer* server = dnd_server_create(null_report, NULL, &port);
    if(!server) return 1;

    struct JobData data = {
        .directory = directory,
        .flags = flags,
        .server = server,
    };

    ThreadHandle thrd;
    create_thread(&thrd, &serve, &data);
    GetInputCtx input = {.prompt = SV("> ")};
    // int err = dnd_server_serve(server, flags, directory);
    StringView* entries = NULL;
    size_t nentries = 0;
    get_entries(directory, &entries, &nentries);
    if(!nentries) return 1;
    print_entries(entries, nentries);
    char buff[4092];
    const char* opencmd = "open";
    char openbuff[1024];
    const char* url = "http://localhost";
    #if defined(__APPLE__)
        opencmd = "open";
    #elif defined(__linux__)
        opencmd = "xdg-open";
    #elif defined(_WIN32)
        opencmd = "start";
    #endif
    for(;;){
        ssize_t len = gi_get_input(&input);
        if(len < 0) break;
        if(len == 0) continue;
        StringView b = stripped_view(input.buff, len);
        if(SV_equals(b, SV("l")) || SV_equals(b, SV("list"))){
            print_entries(entries, nentries);
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
            for(size_t i = 0; i < nentries; i++){
                // calculate expand distance, but without .dnd if we don't have .dnd in buffer
                ssize_t dist = byte_expansion_distance(entries[i].text, entries[i].length-4*strip_dnd, b.text, b.length);
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
        if(idx >= nentries) continue;
        gi_add_line_to_history_len(&input, b.text, b.length);
        StringView entry = entries[idx];
        snprintf(openbuff, sizeof openbuff, "%s %s:%d/%.*s", opencmd, url, port, (int)entry.length, entry.text);
        int s = system(openbuff);
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
get_entries_inner(StringView directory, StringView** buff, size_t* count, size_t* cap){
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
            if(*count >= *cap){
                *cap = *cap? *cap*2 : 8;
                *buff = realloc(*buff, cap*sizeof **buff);
            }
            *buff[(*count)++] = (StringView){.text=s, .length=text.length};
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
get_entries(LongString directory, StringView** entries, size_t* nentries){
    StringView* buff = NULL;
    size_t count = 0;
    size_t cap = 0;
#if defined(__APPLE__) || defined(__linux__)
    const char* dirs[] = {directory.text, NULL};
    FTS* handle = fts_open((char**)dirs, FTS_LOGICAL | FTS_NOCHDIR | FTS_NOSTAT, NULL);
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
            if(count >= cap){
                cap = cap? cap*2 : 8;
                buff = realloc(buff, cap*sizeof *buff);
            }
            buff[count++] = (StringView){.length = len, .text = t};
        }
    }
    fts_close(handle);
#elif defined(_WIN32)
    get_entries_inner(LS_to_SV(directory), &buff, &count, &cap);
#endif
    *entries = buff;
    *nentries = count;
}

static
void
print_entries(StringView* entries, size_t nentries){
    putchar('\r');
    qsort(entries, nentries, sizeof *entries, StringView_cmp);
    for(size_t i = 0; i < nentries; i++){
        StringView entry = entries[i];
        fprintf(stdout, "[%2zu] %.*s\n", i, (int)entry.length, entry.text);
    }
}

#include "dndc_local_server.c"
#include "get_input.c"
