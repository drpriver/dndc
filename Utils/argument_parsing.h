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
//
// Parses argv (like from main) into variables.
// Supports parsing argv into strings, ints, unsigned ints (decimal, binary,
// hex) and flags.
// In the future, this could support floats and files.
//

typedef struct Args {
    // argc/argv should exclude the program name, as it is useless
    int argc;
    const char *_Nonnull const *_Nonnull argv;
} Args;
typedef struct ArgParser ArgParser;
//
// Parses the Args into the variables. Returns an error if there was any issue
// while parsing. Note that this function does not print anything if parsing failed.
// If parsing failed, the destination variables could have some initialized and
// some not. Safe to assume they are all indeterminate.
//
// If parsing failed, the calling application should probably print the help and
// exit. This doesn't do that for you as libraries that call exit() are evil.
//
static inline Errorable_f(void) parse_args(Nonnull(ArgParser*) parser, Nonnull(const Args*) args);

//
// Checks if there is a -h or --help in the args. If there is, you probably
// want to print out the help and exit as parsing will fail due to the --help.
// This doesn't do that for you as libraries that call exit() are evil.
//
static inline bool check_for_help(Nonnull(Args*) args);

//
// Prints a formatted help display for the command line arguments.
//
static inline void print_help(Nonnull(const ArgParser*));

//
// Checks if there is a --version in the args. If there is, you probably
// want to just print the version and exit.
//
static inline bool check_for_version(Nonnull(Args*) args);

//
// Prints the formatted version number.
//
static inline void print_version(Nonnull(const ArgParser*));

//
// X-macro for the current kinds of args we can parse.
// Self explanatory, except ARG_UINTEGER64 accepts decimal (95), binary
// (0b1011111), and hex (0x5f) format.
//
#define ARGS(apply) \
    apply(ARG_INTEGER64, int64_t, "int64") \
    apply(ARG_INT, int, "int") \
    apply(ARG_FLAG, bool, "flag") \
    apply(ARG_STRING, LongString, "string") \
    apply(ARG_UINTEGER64, uint64_t, "uint64") \

#ifdef _WIN32
// Packing doesn't work on enums with Windows.
// Manually pack by not using the type.
typedef uint8_t _ARG_TYPE;
enum {
    #define X(enumname, b, c) enumname,
    ARGS(X)
    #undef X
};
#else
// On linux and macos, packing enums works fine.
typedef enum __attribute__((__packed__)) _ARG_TYPE {
    #define X(enumname, b, c) enumname,
    ARGS(X)
    #undef X
} _ARG_TYPE;
#endif
_Static_assert(sizeof(_ARG_TYPE) == 1, "");

static const LongString ArgTypeNames[] = {
    #define X(a,b, string) LS(string),
    ARGS(X)
    #undef X
};

// Type Generic macro allows us to turn a type into an enum.
#define ARGTYPE(_x) _Generic(_x, \
    int64_t: ARG_INTEGER64, \
    uint64_t: ARG_UINTEGER64, \
    int: ARG_INT, \
    bool: ARG_FLAG, \
    LongString: ARG_STRING)

//
// Given a pointer to the storage for an argument, sets
// the correct type tag enum (ARG_INTEGER64 or whatever).
// Use this to initialize the dest member of ArgToParse.
// If the storage is an array, give the pointer to the first
// element of the array and set the max_num appropriately.
//
#define ARGDEST(_x) {.type = ARGTYPE((_x)[0]), .pointer=_x}

//
// A structure describing an argument to be parsed.
// Create an array of these, one for positional args and another for
// keyword args. The order in the array for the positional args
// will be the order they need to be parsed in.
typedef struct ArgToParse {
    //
    // The name of the argument (include the "-" for keyword arguments).
    StringView name;
    //
    // An alternate name of the argument. Optional.
    StringView altname1;
    //
    // Mininum number of arguments for this arg. Fewer than this is an error.
    int min_num;
    //
    // Maximum number of arguments for this arg. More than this is an error.
    // Greater than 1 means the dest is a pointer to the first element
    // of an array.
    int max_num;
    //
    // How many were actually parsed. Initialize to 0. You can check
    // this to see if the arg was actually set or not.
    int num_parsed;
    //
    // Whether to show the default value in the help printout.
    bool hide_default; // maybe we'll want a bitflags field with options instead.
    // Whether to hide this flag from the help output.
    // Keyword argument only.
    bool hidden;
    //
    // The description of the argument. When printed, the helpstring will be
    // tokenized and adjacent whitespace will be merged into a single space.
    // Newlines are preserved, so don't hardwrap your helpstring.
    // The helptext will be appropriately soft-wrapped on word boundaries.
    Nonnull(const char*) help;
    //
    // Use the ARGDEST macro to intialize this.
    struct {
        _ARG_TYPE type;
        Nonnull(void*) pointer;
    } dest;
} ArgToParse;

//
// Parser structure.
typedef struct ArgParser {
    //
    // The name of the program. Usually argv[0], but you can do whatever you
    // want.
    Nonnull(const char*) name;
    //
    // A one-line description of the program.
    Nonnull(const char*) description;
    //
    // The text to be printed for --version.
    Nullable(const char*) version;
    //
    // The positional arguments. Create an array of these. The order in the
    // array will be the order they need to be parsed in.
    struct {
        Nonnull(ArgToParse*) args;
        size_t count;
    } positional;
    //
    // The keyword arguments. Create an array of these.
    // The order doesn't matter.
    struct {
        Nonnull(ArgToParse*) args;
        size_t count;
    } keyword;
} ArgParser;

//
// Prints the help for a single argument.
static inline void print_arg_help(Nonnull(const ArgToParse*),TermSize);


// Internal helper struct for text-wrapping.
typedef struct HelpState {
    int output_width;
    int lead;
    int remaining;
} HelpState;

// Handle text-wrapping, printing a newline and indenting if necessary.
static inline
void
help_state_update(Nonnull(HelpState*)hs, int n_to_print){
    if(hs->remaining - n_to_print < 0){
        // This is a string with 80 spaces in it.
        const char* SPACES = "                                                                                ";
        printf("\n%.*s", hs->lead, SPACES);
        hs->remaining = hs->output_width;
        }
    hs->remaining -= n_to_print;
    }

// See top of file.
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
        if(arg->hidden)
            continue;
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
                auto tn = ArgTypeNames[arg->dest.type];
                auto to_print = sizeof(" [%s | %s <%s>]") - 7 + arg->name.length + arg->altname1.length + tn.length;
                help_state_update(&hs, to_print);
                printf(" [%s | %s <%s>]", arg->name.text, arg->altname1.text, tn.text);
                }
            else{
                auto tn = ArgTypeNames[arg->dest.type];
                auto to_print = sizeof(" [%s <%s>]") - 5 + arg->name.length + tn.length;
                help_state_update(&hs, to_print);
                printf(" [%s <%s>]", arg->name.text, tn.text);
                }
            }
        }
    puts("\n");
    if(p->positional.count){
        puts("Positional Arguments:\n"
             "---------------------");
        for(size_t i = 0; i < p->positional.count; i++){
            auto arg = &p->positional.args[i];
            print_arg_help(arg, term_size);
            putchar('\n');
            }
        }
    if(p->version){
        puts("Keyword Arguments:\n"
             "------------------\n"
             "-h, --help: flag = false\n"
             "    Print this help and exit.\n"
             "\n"
             "--version: flag = false\n"
             "    Print version information and exit.");
        }
    else {
        puts("Keyword Arguments:\n"
             "------------------\n"
             "-h, --help: flag = false\n"
             "    Print this help and exit.");
        }
    for(size_t i = 0; i < p->keyword.count; i++){
        auto arg = &p->keyword.args[i];
        if(arg->hidden)
            continue;
        putchar('\n');
        print_arg_help(arg, term_size);
        }
    }

static inline
void
print_wrapped_help(Nonnull(const char*), TermSize);

// See top of file.
static inline
void
print_arg_help(Nonnull(const ArgToParse*) arg, TermSize term_size){
    const char* help = arg->help;
    auto name = arg->name.text;
    auto type = arg->dest.type;
    auto typename = ArgTypeNames[type];
    printf("%s", name);
    if(arg->altname1.length){
        printf(", %s", arg->altname1.text);
        }
    printf(": %s", typename.text);

    if(arg->min_num != 0 or arg->hide_default){
        print_wrapped_help(help, term_size);
        return;
        }
    switch(type){
        case ARG_INTEGER64:{
            int64_t* data = arg->dest.pointer;
            printf(" = %lld", (long long)*data);
        print_wrapped_help(help, term_size);
            }break;
        case ARG_UINTEGER64:{
            int64_t* data = arg->dest.pointer;
            printf(" = %llu", (unsigned long long)*data);
            print_wrapped_help(help, term_size);
            }break;
        case ARG_INT:{
            int* data = arg->dest.pointer;
            printf(" = %d", *data);
            print_wrapped_help(help, term_size);
            }break;
        case ARG_FLAG:{
            bool* data = arg->dest.pointer;
            printf(" = %s", *data?"true":"false");
            print_wrapped_help(help, term_size);
            } break;
        case ARG_STRING:{
            LongString* s = arg->dest.pointer;
            printf(" = '%.*s'", (int)s->length, s->text);
            print_wrapped_help(help, term_size);
            } break;
        }
    }

struct HelpTokenized {
    StringView token;
    bool is_newline;
    Nonnull(const char*) rest;
    };
// Tokenizes the string on whitespace.
// Internal helper for printing the help wrapped.
static inline
struct HelpTokenized
next_tokenize_help(Nonnull(const char*) help){
    for(;;help++){
        switch(*help){
            case ' ': case '\r': case '\t': case '\f':
                continue;
            default:
                break;
            }
        break;
        }
    if(*help == '\n'){
        return (struct HelpTokenized){
            .is_newline = true,
            .rest = help+1,
            };
        }
    const char* begin = help;
    for(;;help++){
        switch(*help){
            // Note that this list includes '\0' as a word boundary.
            case ' ': case '\n': case '\r': case '\t': case '\f': case '\0':{
                return (struct HelpTokenized){
                    .token.text = begin,
                    .token.length = help - begin,
                    .rest = help,
                    };
                }break;
            default:
                continue;
            }
        }
    unreachable();
    }

static inline
void
print_wrapped_help(Nonnull(const char*)help, TermSize term_size){
    printf("\n    ");
    HelpState hs = {.output_width = term_size.columns - 4, .lead = 4, .remaining = 0};
    hs.remaining = hs.output_width;
    for(;*help;){
        auto tok = next_tokenize_help(help);
        help = tok.rest;
        if(tok.is_newline){
            if(hs.remaining != hs.output_width){
                printf("\n    ");
                hs.remaining = hs.output_width;
                }
            continue;
            }
        help_state_update(&hs, tok.token.length);
        printf("%.*s", (int)tok.token.length, tok.token.text);
        if(hs.remaining){
            putchar(' ');
            hs.remaining--;
            }
        }
    putchar('\n');
    }

// Parse a single argument from a string.
// Used internally. I guess you could use it if you really wanted to, but you
// don't need this type generic version?
static inline
Errorable_f(void)
parse_arg(Nonnull(ArgToParse*)arg, StringView s){
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
            // This is a hack, our target
            // is actually a LongString.
            StringView* dest = arg->dest.pointer;
            dest += arg->num_parsed;
            *dest = s;
            arg->num_parsed += 1;
            }break;
        }
    return result;
    }

// Set a flag. I really don't see why you would use this outside of this.
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

// See top of file.
static inline
bool
check_for_help(Nonnull(Args*) args){
    for(int i = 0; i < args->argc; i++){
        auto argstring = cstr_to_SV(args->argv[i]);
        if(SV_equals(argstring, SV("-h")) or SV_equals(argstring, SV("--help"))){
            return true;
            }
        }
    return false;
    }

// See top of file.
static inline
bool
check_for_version(Nonnull(Args*) args){
    for(int i = 0; i < args->argc; i++){
        auto argstring = cstr_to_SV(args->argv[i]);
        if(SV_equals(argstring, SV("--version")))
            return true;
        }
    return false;
    }

// See top of file.
static inline
void
print_version(Nonnull(const ArgParser*)p){
    if(p->version)
        printf("%s\n", p->version);
    else
        puts("No version information available.");
    }

// See top of file.
static inline
Errorable_f(void)
parse_args(Nonnull(ArgParser*) parser, Nonnull(const Args*) args){
    Errorable(void) result = {};
    auto argc = args->argc;
    const char*const* argv = args->argv;
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
            auto s = cstr_to_SV(*argv);
            if(s.length > 1){
                if(s.text[0] == '-'){
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
            auto s = cstr_to_SV(*argv);
            argv++;
            // always check for an argument match
            for(int i = 0; i < parser->keyword.count; i++){
                auto a = &parser->keyword.args[i];
                if(SV_equals(s, a->name) or SV_equals(s, a->altname1)){
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
