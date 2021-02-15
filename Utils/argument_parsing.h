#ifndef argument_parsing_h
#define argument_parsing_h
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include "common_macros.h"
#include "long_string.h"
#include "error_handling.h"
#include "parse_numbers.h"
#include "term_util.h"

typedef struct Args {
    // argc/argv should exclude the program name, as it is useless
    int argc;
    char *_Nonnull *_Nonnull argv;
} Args;
typedef struct ArgParser ArgParser;
static inline Errorable_f(void) parse_args(Nonnull(ArgParser*) parser, Nonnull(const Args*) args);
static inline bool check_for_help(Nonnull(Args*) args);
static inline void print_help(Nonnull(const ArgParser*));
static inline bool check_for_version(Nonnull(Args*) args);
static inline void print_version(Nonnull(const ArgParser*));

#define ARGS(apply) \
    apply(ARG_INTEGER64, int64_t, "int64") \
    apply(ARG_INT, int, "int") \
    apply(ARG_FLAG, bool, "flag") \
    apply(ARG_STRING, LongString, "string") \
    apply(ARG_UINTEGER64, uint64_t, "uint64") \

#ifdef WINDOWS
typedef uint8_t _ARG_TYPE;
enum {
#define X(enumname, b, c) enumname,
    ARGS(X)
#undef X
    };
#else
typedef SmallEnum _ARG_TYPE {
#define X(enumname, b, c) enumname,
    ARGS(X)
#undef X
    } _ARG_TYPE;
#endif

static Nonnull(const char*) const Arg_Type_Names[] = {
#define X(a,b, string) string,
    ARGS(X)
#undef X
    };

#define ARGTYPE(_x) _Generic(_x, \
    int64_t: ARG_INTEGER64, \
    uint64_t: ARG_UINTEGER64, \
    int: ARG_INT, \
    bool: ARG_FLAG, \
    LongString: ARG_STRING)

#define ARGDEST(_x) {.type = ARGTYPE((_x)[0]), .pointer=_x}

typedef struct ArgToParse {
    LongString name;
    LongString altname1;
    int min_num;
    int max_num;
    int num_parsed;
    bool hide_default; // maybe we'll want a bitflags field instead
    Nonnull(const char*) help;
    struct {
        _ARG_TYPE type;
        Nonnull(void*) pointer;
    } dest;
} ArgToParse;

typedef struct ArgParser {
    char option_char;
    Nonnull(const char*) name;
    Nonnull(const char*) description;
    Nonnull(const char*) version;
    struct {
        Nonnull(ArgToParse*) args;
        size_t count;
    } positional;
    struct {
        Nonnull(ArgToParse*) args;
        size_t count;
    } keyword;
} ArgParser;

static inline void print_arg_help(Nonnull(const ArgToParse*));


typedef struct HelpState {
    int output_width;
    int lead;
    int remaining;
} HelpState;

static inline
void
help_state_update(Nonnull(HelpState*)hs, int n_to_print){
    if(hs->remaining - n_to_print < 0){
        const char* SPACES = "                                                                                ";
        printf("\n%.*s", hs->lead, SPACES);
        hs->remaining = hs->output_width;
        }
    hs->remaining -= n_to_print;
    }

static inline
void
print_help(Nonnull(const ArgParser*) p){
    auto term_size = get_terminal_size();
    printf("%s: %s\n", p->name, p->description);
    puts("");
    const auto printed = printf("usage: %s", p->name);
    HelpState hs = {.output_width = term_size.columns - printed, .lead = printed, .remaining = 0};
    hs.remaining = hs.output_width;
    for(int i = 0; i < p->positional.count; i++){
        auto arg = &p->positional.args[i];
        auto to_print = 1 + arg->name.length;
        help_state_update(&hs, to_print);
        printf(" %s", arg->name.text);
        }
    for(int i = 0; i < p->keyword.count; i++){
        auto arg = &p->keyword.args[i];
        if(arg->dest.type == ARG_FLAG){
            if(arg->altname1.length){
                auto to_print = sizeof(" [%s | %s]") - 5 + arg->name.length + arg->altname1.length;
                help_state_update(&hs, to_print);
                printf(" [%s | %s]", arg->name.text, arg->altname1.text);
                }
            else{
                auto to_print = sizeof(" [%s]") - 3 + arg->name.length;
                help_state_update(&hs, to_print);
                printf(" [%s]", arg->name.text);
                }
            }
        else {
            if(arg->altname1.text){
                auto to_print = sizeof(" [%s | %s <%s>]") - 7 + arg->name.length + arg->altname1.length + strlen(Arg_Type_Names[arg->dest.type]);
                help_state_update(&hs, to_print);
                printf(" [%s | %s <%s>]", arg->name.text, arg->altname1.text, Arg_Type_Names[arg->dest.type]);
                }
            else{
                auto to_print = sizeof(" [%s <%s>]") - 5 + arg->name.length + strlen(Arg_Type_Names[arg->dest.type]);
                help_state_update(&hs, to_print);
                printf(" [%s <%s>]", arg->name.text, Arg_Type_Names[arg->dest.type]);
                }
            }
        }
    puts("\n");
    if(p->positional.count){
        puts("Positional Arguments:");
        puts("---------------------");
        for(size_t i = 0; i < p->positional.count; i++){
            auto arg = &p->positional.args[i];
            print_arg_help(arg);
            puts("");
            }
        }
    puts("Keyword Arguments:");
    puts("------------------");
    puts("-h, --help: flag = false\n"
         "    Print this help and exit.");
    puts("");
    puts("--version: flag = false\n"
         "    Print version information and exit.");
    puts("");
    for(size_t i = 0; i < p->keyword.count; i++){
        if(i != 0)
            puts("");
        auto arg = &p->keyword.args[i];
        print_arg_help(arg);
        }
    }

static inline
void
print_arg_help(Nonnull(const ArgToParse*) arg){
    const char* help = arg->help;
    auto name = arg->name.text;
    auto type = arg->dest.type;
    auto typename = Arg_Type_Names[type];
    printf("%s", name);
    if(arg->altname1.length){
        printf(", %s", arg->altname1.text);
        }
    printf(": %s", typename);

    if(arg->min_num != 0 or arg->hide_default){
        printf("\n    %s\n", help);
        return;
        }
    switch(type){
        case ARG_INTEGER64:{
            int64_t* data = arg->dest.pointer;
            printf(" = %lld\n    %s\n", (long long)*data, help);
            }break;
        case ARG_UINTEGER64:{
            int64_t* data = arg->dest.pointer;
            printf(" = %llu\n    %s\n", (unsigned long long)*data, help);
            }break;
        case ARG_INT:{
            int* data = arg->dest.pointer;
            printf(" = %d\n    %s\n", *data, help);
            }break;
        case ARG_FLAG:{
            bool* data = arg->dest.pointer;
            printf(" = %s\n    %s\n", *data?"true":"false", help);
            } break;
        case ARG_STRING:{
            LongString* s = arg->dest.pointer;
            printf(" = '%.*s'\n    %s\n", (int)s->length, s->text, help);
            } break;
        }
    }


static inline
Errorable_f(void)
parse_arg(Nonnull(ArgToParse*)arg, LongString s){
    Errorable(void) result = {};
    if(arg->num_parsed >= arg->max_num)
        Raise(EXCESS_KWARGS);
    // If previous num parsed is nonzero, this means
    // that what we are pointing to is an array.
    switch(arg->dest.type){
        case ARG_INTEGER64:{
            auto value = attempt(parse_int64(s.text, s.length));
            int64_t* dest = arg->dest.pointer;
            dest += arg->num_parsed;
            *dest = value;
            arg->num_parsed += 1;
            }break;
        case ARG_UINTEGER64:{
            auto value = attempt(parse_unsigned_human(s.text, s.length));
            uint64_t* dest = arg->dest.pointer;
            dest += arg->num_parsed;
            *dest = value;
            arg->num_parsed += 1;
            }break;
        case ARG_INT: {
            auto value = attempt(parse_int(s.text, s.length));
            int* dest = arg->dest.pointer;
            dest += arg->num_parsed;
            *dest = value;
            arg->num_parsed += 1;
            }break;
        case ARG_FLAG:
            unreachable();
        case ARG_STRING:{
            LongString* dest = arg->dest.pointer;
            dest += arg->num_parsed;
            *dest = s;
            arg->num_parsed += 1;
            }break;
        }
    return result;
    }

static inline
Errorable_f(void)
set_flag(Nonnull(ArgToParse*) arg){
    Errorable(void) result = {};
    assert(arg->dest.type == ARG_FLAG);
    if(arg->num_parsed >= arg->max_num)
        Raise(DUPLICATE_KWARG);
    bool* dest = arg->dest.pointer;
    *dest = true;
    arg->num_parsed += 1;
    return result;
    }

static inline
bool
check_for_help(Nonnull(Args*) args){
    for(int i = 0; i < args->argc; i++){
        auto argstring = LongString_borrowed_from_cstring(args->argv[i]);
        if(LongString_equals(argstring, LS("-h")) or LongString_equals(argstring, LS("--help"))){
            return true;
            }
        }
    return false;
    }

static inline
bool
check_for_version(Nonnull(Args*) args){
    for(int i = 0; i < args->argc; i++){
        auto argstring = LongString_borrowed_from_cstring(args->argv[i]);
        if(LongString_equals(argstring, LS("--version")))
            return true;
        }
    return false;
    }

static inline
void
print_version(Nonnull(const ArgParser*)p){
    printf("%s\n", p->version);
    }

static inline
Errorable_f(void)
parse_args(Nonnull(ArgParser*) parser, Nonnull(const Args*) args){
    Errorable(void) result = {};
    auto argc = args->argc;
    char** argv = args->argv;
    auto past_the_end = argv+argc;
    if(parser->positional.count){
        Nonnull(ArgToParse*) arg = &parser->positional.args[0];
        assert(arg->max_num > 0);
        int which_arg = 0;
        for(;;){
            if(arg->num_parsed == arg->max_num){
                if(which_arg +1 == parser->positional.count){
                    break;
                    }
                arg++;
                which_arg++;
                assert(arg->max_num > 0);
                }
            if(argv == past_the_end)
                break;
            auto s = LongString_borrowed_from_cstring(*argv);
            if(s.length > 1){
                if(s.text[0] == parser->option_char){
                    // make sure it's not actually a number.
                    switch(s.text[1]){
                        case '0' ... '9':
                            break;
                        default:
                            goto Break;
                        }
                    }
                }
            argv++;
            auto e = parse_arg(arg, s);
            // error in converting to argument
            if(e.errored)
                return e;
            }
        Break:;
        for(int i = 0; i < parser->positional.count; i++){
            auto a = &parser->positional.args[i];
            if(a->num_parsed < a->min_num){
                // too few arguments
                Raise(PARSE_ERROR);
                }
            }
        }
    if(parser->keyword.count){
        Nullable(ArgToParse*) arg = NULL;
        bool parsed_an_arg = false;
        for(;;){
            top:;
            if(argv == past_the_end)
                break;
            auto s = LongString_borrowed_from_cstring(*argv);
            argv++;
            // always check for an argument match
            for(int i = 0; i < parser->keyword.count; i++){
                auto a = &parser->keyword.args[i];
                if(LongString_equals(s, a->name) or LongString_equals(s, a->altname1)){
                    if(arg and !parsed_an_arg){
                        // we got something like --foo --bar when --foo expected an argument
                        Raise(MISSING_ARG);
                        }
                    parsed_an_arg = false;
                    if(a->dest.type == ARG_FLAG){
                        auto e = set_flag(a);
                        if(e.errored)
                            return e;
                        arg = NULL;
                        }
                    else
                        arg = a;
                    goto top;
                    }
                }
            // unrecognized argument (or really, isn't an argument)
            if(!arg)
                Raise(PARSE_ERROR);
            // I wish clang would see the !arg and deduce arg is nonnull here.
            auto e = parse_arg((ArgToParse*)arg, s);
            if(e.errored)
                return e;
            parsed_an_arg = true;
            }
        if(arg and !parsed_an_arg){
            // we got something like --foo --bar when --foo expected an argument
            Raise(MISSING_ARG);
            }
        assert(argv == past_the_end);
        for(size_t i = 0; i < parser->keyword.count; i++){
            auto a = &parser->keyword.args[i];
            // got too few (or none)
            if(a->num_parsed < a->min_num)
                Raise(MISSING_KWARG);
            // got too many
            if(a->num_parsed > a->max_num)
                Raise(EXCESS_KWARGS);
            }
        }
    // can happen if we don't have kwargs
    if(argv != past_the_end){
        // didn't consume all arguments
        Raise(PARSE_ERROR);
        }
    return result;
    }

#endif
