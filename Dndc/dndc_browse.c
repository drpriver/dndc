#include "common_macros.h"
#include "dndc_local_server.h"
#include "Utils/argument_parsing.h"
#include "Utils/term_util.h"
#include "Utils/hash_func.h"
#include "Utils/thread_utils.h"
#include "Utils/get_input.h"
#include "Utils/str_util.h"
#include "Allocators/allocator.h"
#include "Allocators/mallocator.h"
#include "Utils/string_distances.h"
#include "Utils/MStringBuilder.h"
#include "Utils/msb_url_helpers.h"
#include "Utils/msb_format.h"
#include "Utils/native_picker.h"
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <synchapi.h>
#include "Platform/Windows/wincli.h"
#elif defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#include <fts.h>
#else
#error "Unhandled platform"
#endif

#define MARRAY_T StringView
#include "Utils/Marray.h"

#define MARRAY_STRINGVIEW_DEFINED
#include "Utils/recursive_glob.h"
#include "Utils/gi_byte_distance_completer.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef Marray__StringView Entries;


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
static
void
wrap_report(void* ud, int type, const char* filename, int filename_len, int line, int col, const char* message, int message_len){
    if(filename_len)
        fprintf(stderr, "\r%.*s: ", filename_len, filename);
    else
        fprintf(stderr, "\r");
    dndc_stderr_log_func(ud, type, filename, filename_len, line, col, message, message_len);
    fprintf(stderr, "\r> ");
}

THREADFUNC(quit_after){
#ifdef _WIN32
    Sleep((intptr_t)thread_arg);
#else
    sleep((intptr_t)thread_arg);
#endif
    puts("\r");
    exit(0);
    return 0;
}


int
main(int argc, char** argv){
#ifdef _WIN32
    // unclear if this is needed.
    if(get_main_args(&argc, &argv) != 0) return 1;
#endif
    LongString directory = {0};
    StringView file_to_open = {0};
    int port = 0;
    int loglevel = 3;
    {
        char* cwd;
        #ifdef _WIN32
            cwd = _getcwd(NULL, 0);
        #else
            cwd = getcwd(NULL, 0);
        #endif
        if(!cwd) return 1;
        size_t cwdlen = strlen(cwd);
        port = (3000 + hash_align1(cwd, cwdlen)) & 0x7fff;
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
            .show_default = 1,
        },
    };
    _Bool should_log = 0;
    _Bool bind_all = 0;
    int depth = 10;
    int quit = 0;
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
        {
            .name = SV("--loglevel"),
            .dest = ARGDEST(&loglevel),
            .help = "log level. Only relevant if --log is passed.",
            .show_default = 1,
        },
        {
            .name = SV("--bind-all"),
            .dest = ARGDEST(&bind_all),
            .help = "Bind to 0.0.0.0 instead of loopback. Don't do this if you don't trust the network and for god's sake don't put it on the internet.",
        },
        {
            .name = SV("-d"),
            .altname1 = SV("--max-depth"),
            .help = "How many directories deep to look for dnd files. 1 means only check the current directory",
            .dest = ARGDEST(&depth),
            .show_default = 1,
        },
        {
            .name = SV("--open"),
            .help = "Open this file after starting",
            .dest = ARGDEST(&file_to_open),
        },
        {
            .name = SV("-q"),
            .altname1 = SV("--quit"),
            .help = "Quit after this many seconds",
            .dest = ARGDEST(&quit),
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
            .hidden = 1,
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
    if(!directory.length){
        int err = native_gui_pick_directory(&directory);
        if(err) return err;
        assert(directory.length);
    }
    while(directory.length > 1 && directory.text[directory.length-1] == '/'){
        PushDiagnostic();
        SuppressCastQual();
        ((char*)directory.text)[--directory.length] = 0;
        PopDiagnostic();
    }

    DndServer* server = dnd_server_create(should_log?wrap_report:null_report, NULL, loglevel, &port, bind_all);
    if(!server) {
        fprintf(stderr, "No server\n");
        return 1;
    }

    struct JobData data = {
        .directory = directory,
        .flags = flags,
        .server = server,
    };

    {
        ThreadHandle thrd;
        int th_err = create_thread(&thrd, &serve, &data);
        if(th_err != 0) return 1;
    }
    Entries entries = {0};
    recursive_glob_suffix(directory, SV(".dnd"), &entries, depth);
    if(!entries.count) return 1;
    for(size_t i = 0; i < entries.count; i++){
        if(SV_equals(entries.data[i], SV("index.dnd"))) goto LHasIndex;
    }
    {
        int err = Marray_push__StringView(&entries, MALLOCATOR, SV("index.dnd"));
        unhandled_error_condition(err);
    }
    LHasIndex:
    print_entries(entries);
    struct ByteDistanceCompleterContext tabctx = {
        .original = &entries,
        .strip_suff = SV(".dnd"),
    };
    GetInputCtx input = {
        .prompt = SV("> "),
        .prompt_display_length=2,
        .tab_completion_func=byte_distance_completer,
        .tab_completion_user_data=&tabctx,
    };
    const char* opencmd = "open";
    const char* url = "http://localhost";
    #if defined(__APPLE__)
        opencmd = "open";
    #elif defined(__linux__)
        opencmd = "xdg-open";
    #elif defined(_WIN32)
        opencmd = "start";
    #endif
    MStringBuilder opensb = {.allocator = MALLOCATOR};
    MSB_FORMAT(&opensb, opencmd, " ", url, ":", port, "/");
    size_t before_entry = opensb.cursor;
    if(file_to_open.length){
        msb_url_percent_encode_filepath(&opensb, file_to_open.text, file_to_open.length);
        LongString op = msb_borrow_ls(&opensb);
        int s = system(op.text);
        (void)s;
    }
    if(quit){
        ThreadHandle thrd;
        int err = create_thread(&thrd, quit_after, (void*)(intptr_t)quit);
        (void)err;
    }
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
        if(SV_equals(b, SV("h")) || SV_equals(b, SV("help"))){
            puts(
                "\rl, list - list entries\n"
                "q, quit - quit\n"
                "h, help - print this help");
            continue;
        }
        Int64Result ir = parse_int64(b.text, b.length);
        int64_t idx = -1;
        if(ir.errored) {
            ssize_t best = 1LL<<30;
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
#ifdef __clang__
#pragma clang assume_nonnull end
#endif


#include "dndc_local_server.c"
#include "Utils/get_input.c"
#include "Utils/gi_byte_distance_completer.c"
#include "Utils/recursive_glob.c"
#include "Utils/native_picker.c"
