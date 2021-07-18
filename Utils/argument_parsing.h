#ifndef argument_parsing_h
#define argument_parsing_h
// bool
#include <stdbool.h>
// integer types
#include <stdint.h>
// strtod, strtof
#include <stdlib.h>
#include "common_macros.h"
#include "long_string.h"
#include "parse_numbers.h"
#include "term_util.h"
//
// Parses argv (like from main) into variables.
// Supports parsing argv into strings, ints, unsigned ints (decimal, binary,
// hex) and flags.
// In the future, this could support floats and files.
//
//
enum ArgParseError {
    ARGPARSE_NO_ERROR = 0,
    // Failed to convert a string into a value, like 'a' can't convert to an integer
    ARGPARSE_CONVERSION_ERROR = 1,
    // Given keyword arg-like parameter doesn't match any known args.
    ARGPARSE_UNKNOWN_KWARG = 2,
    // A keyword argument was given multiple times in the command line.
    ARGPARSE_DUPLICATE_KWARG = 3,
    // More than the maximum number of arguments were given for an arg to parse.
    ARGPARSE_EXCESS_ARGS = 4,
    // Fewer than the minimum number of arguments were given for an arg to parse.
    ARGPARSE_INSUFFICIENT_ARGS = 5,
    // Named at the commandline, but no arguments given. This is a user error
    // even if min_num is 0 as it can be very confusing otherwie.
    ARGPARSE_VISITED_NO_ARG_GIVEN = 6,
};

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
static inline enum ArgParseError parse_args(Nonnull(ArgParser*) parser, Nonnull(const Args*) args);

// After receiving a non-zero error code from `parse_args`, use this function
// to explain what failed to parse and why.
static inline void print_argparse_error(Nonnull(ArgParser*)parser, enum ArgParseError error);

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
    apply(ARG_CSTRING, const char*, "string") \
    apply(ARG_UINTEGER64, uint64_t, "uint64") \
    apply(ARG_FLOAT32, float, "float32") \
    apply(ARG_FLOAT64, double, "float64") \


typedef enum _ARG_TYPE {
    #define X(enumname, b, c) enumname,
    ARGS(X)
    #undef X
    ARG_ENUM,
    ARG_USER_DEFINED,
} _ARG_TYPE;

static const LongString ArgTypeNames[] = {
    #define X(a,b, string) LS(string),
    ARGS(X)
    #undef X
    LS("enum"),
    LS("USER DEFINED THIS IS A BUG"),
};

// Type Generic macro allows us to turn a type into an enum.
#define ARGTYPE(_x) _Generic(_x, \
    int64_t: ARG_INTEGER64, \
    uint64_t: ARG_UINTEGER64, \
    float: ARG_FLOAT32, \
    double: ARG_FLOAT64, \
    int: ARG_INT, \
    bool: ARG_FLAG, \
    const char*: ARG_CSTRING, \
    char*: ARG_CSTRING, \
    StringView: ARG_STRING, \
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
// A structure for allowing the parsing of user defined types.
// Fill out a struct with the given fields and use the
// ARG_USER_DEFINED type.
//
// Instead of using the ARGDEST macro, you fill out the dest field
// of ArgToParse yourself. For example:
//
//  ArgToParse pos_args[] = {
//     ...
//     [2] = {
//         .name = SV("mytype"),
//         .min_num = 0,
//         .max_num = 3,
//         .dest = {
//             .type = ARG_USER_DEFINED,
//             .user_pointer = &mytype_def,
//             .pointer = &myarg,
//         },
//     },
//     ...
//  };
//
typedef struct ArgParseUserDefinedType {
    // Converts the given string into the defined type by writing
    // into the pointer.
    // Return non-zero to indicate a conversion error.
    // First argument is the user_data pointer from this struct.
    int (*_Nonnull converter)(NullUnspec(void*), Nonnull(const char*), size_t, Nonnull(void*));
    // Should do something like:
    //    printf(" = %d,%d,%d", x, y, z);
    // Used when printing the help.
    void(*_Nullable default_printer)(Nonnull(void*));
    // Used when printing the help.
    LongString type_name;
    size_t type_size;
    // If you need complicated state in your converter function,
    // you can store whatever you want here.
    NullUnspec(void*) user_data;
} ArgParseUserDefinedType;

//
// A structure for converting strings into enums.  The enum must start at 0. It
// can have holes in the values, but you will have to fill them in with 0
// length strings.
//
// NOTE: We just do a linear search over the strings.  In theory this is very
// bad, but in practice a typical parse line will need to match against a given
// enum only once so any fancy algorithm would require a pre-pass over all the
// data anyway.  We could be faster if we required enums to be sorted or be
// pre-placed in a perfect hash table, but this harms usability too much.  If
// you need to parse a lot of enums with weird requirements, then just a create
// a user defined type instead of using this.
typedef struct ArgParseEnumType {
    // In order to support packed enums, specify the size of the enum here
    // instead of just assuming it's an int.  Only powers of two are supported
    // and it will be interpreted as an unsigned integer.
    size_t enum_size;
    // This should be the largest enum value + 1.
    size_t enum_count;
    // This should be a pointer to an array of `LongString`s that is
    // `enum_count` in length.
    // These will be used for both printing the help and for
    // parsing strings into the enum, so they should be in a
    // format that you would type in a command line.
    Nonnull(const LongString*) enum_names;
}ArgParseEnumType;

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
    // Allows to have a short and longer version of argument (name is "--help",
    // altname is "-h")
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
    // Whether or not this argument was given at the commandline. For variable
    // length positional arguments that allow 0 args, this distinguishes
    // between an empty list being given versus not being given at all, which
    // is used to return an error in that case.
    // This isn't very useful for users to look at.

    // Also, used internally to avoid allowing duplicate keywords at the commandline.
    bool visited;
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
    // Use the ARGDEST macro to intialize this for basic types.
    struct {
        // The type what pointer points to.
        _ARG_TYPE type;
        // Pointer to the first element.
        Nonnull(void*) pointer;
        union {
            // This should be set if type == ARG_USER_DEFINED. It's a pointer
            // to a structure that defines how to convert a string to the
            // value, how to print, etc.
            // See the struct definition for more information.
            Nullable(const ArgParseUserDefinedType*) user_pointer;
            // This should be set if type == ARG_ENUM. It's a pointer to a
            // structure that defines the value enum values, its size, etc.
            // See the struct definition for more information.
            Nullable(const ArgParseEnumType*) enum_pointer;
        };
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
    // If an error occurred, these are set depending on what error occurred.
    // Exactly when they are set or not is an implementation detail, so use
    // `print_argparse_error` instead.
    struct {
        // If failure happened while an option was identified, this will be set to that arg.
        Nullable(ArgToParse*) arg_to_parse;
        // If failure happened on a specific argument, this will be set.
        Nullable(const char*) arg;
    } failed;
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
    if(term_size.columns > 80)
        term_size.columns = 80;
    printf("%s: %s\n", p->name, p->description);
    puts("");
    const auto printed = printf("usage: %s", p->name);
    HelpState hs = {.output_width = term_size.columns - printed, .lead = printed, .remaining = 0};
    hs.remaining = hs.output_width;
    for(int i = 0; i < p->positional.count; i++){
        auto arg = &p->positional.args[i];
        if(arg->max_num > 1){
            auto to_print = 1 + arg->name.length + 4;
            help_state_update(&hs, to_print);
            printf(" %s ...", arg->name.text);
            }
        else {
            auto to_print = 1 + arg->name.length;
            help_state_update(&hs, to_print);
            printf(" %s", arg->name.text);
            }
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
                auto to_print = sizeof(" [%s | %s <%s>%s]") - 9 + arg->name.length + arg->altname1.length + tn.length + (arg->max_num > 1?sizeof(" ...")-1: 0);
                help_state_update(&hs, to_print);
                printf(" [%s | %s <%s>%s]", arg->name.text, arg->altname1.text, tn.text, arg->max_num > 1?" ...":"");
                }
            else{
                auto tn = ArgTypeNames[arg->dest.type];
                auto to_print = sizeof(" [%s <%s>%s]") - 7 + arg->name.length + tn.length + (arg->max_num > 1?sizeof(" ...")-1:0);
                help_state_update(&hs, to_print);
                printf(" [%s <%s>%s]", arg->name.text, tn.text, arg->max_num>1?" ...":"");
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
             "-h, --help: flag\n"
             "    Print this help and exit.\n"
             "\n"
             "--version: flag\n"
             "    Print version information and exit.");
        }
    else {
        puts("Keyword Arguments:\n"
             "------------------\n"
             "-h, --help: flag\n"
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
print_wrapped_help(Nullable(const char*), TermSize);

static inline
void
print_enum_options(Nullable(const ArgParseEnumType*)enu_){
    if(!enu_) return;
    // cast away nullability
    const ArgParseEnumType* enu = enu_;
    printf("    Options:\n");
    printf("    -------\n");
    for(size_t i = 0; i < enu->enum_count; i++){
        printf("    %s\n", enu->enum_names[i].text);
        }
    }

// See top of file.
static inline
void
print_arg_help(Nonnull(const ArgToParse*) arg, TermSize term_size){
    const char* help = arg->help;
    auto name = arg->name.text;
    auto type = arg->dest.type;

    LongString typename;
    switch(type){
        case ARG_USER_DEFINED:
            typename = arg->dest.user_pointer->type_name;
            break;
        default:
            typename = ArgTypeNames[type];
            break;
        }
    printf("%s", name);
    if(arg->altname1.length){
        printf(", %s", arg->altname1.text);
        }
    printf(": %s", typename.text);

    if(arg->min_num != 0 or arg->hide_default){
        print_wrapped_help(help, term_size);
        if(type == ARG_ENUM)
            print_enum_options(arg->dest.enum_pointer);
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
        case ARG_FLOAT32:{
            float* data = arg->dest.pointer;
            printf(" = %f", (double)*data);
            print_wrapped_help(help, term_size);
            }break;
        case ARG_FLOAT64:{
            double* data = arg->dest.pointer;
            printf(" = %f", *data);
            print_wrapped_help(help, term_size);
            }break;
        case ARG_FLAG:{
            print_wrapped_help(help, term_size);
            } break;
        case ARG_CSTRING:{
            const char* s = arg->dest.pointer;
            printf(" = '%s'", s);
            print_wrapped_help(help, term_size);
            }break;
        case ARG_STRING:{
            LongString* s = arg->dest.pointer;
            printf(" = '%.*s'", (int)s->length, s->text);
            print_wrapped_help(help, term_size);
            } break;
        case ARG_USER_DEFINED:{
            if(arg->dest.user_pointer->default_printer){
                arg->dest.user_pointer->default_printer(arg->dest.pointer);
                }
            print_wrapped_help(help, term_size);
            }break;
        case ARG_ENUM:{
            const ArgParseEnumType* enu = arg->dest.enum_pointer;
            LongString enu_name = LS("???");
            switch(enu->enum_size){
                case 1:{
                    uint8_t* def = arg->dest.pointer;
                    if(*def <  enu->enum_count)
                        enu_name = enu->enum_names[*def];
                    }break;
                case 2:{
                    uint16_t* def = arg->dest.pointer;
                    if(*def <  enu->enum_count)
                        enu_name = enu->enum_names[*def];
                    }break;
                case 4:{
                    uint32_t* def = arg->dest.pointer;
                    if(*def <  enu->enum_count)
                        enu_name = enu->enum_names[*def];
                    }break;
                case 8:{
                    uint64_t* def = arg->dest.pointer;
                    if(*def <  enu->enum_count)
                        enu_name = enu->enum_names[*def];
                    }break;
                }
            printf(" = %s", enu_name.text);
            print_wrapped_help(help, term_size);
            print_enum_options(enu);
            }break;
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
print_wrapped_help(Nullable(const char*)help, TermSize term_size){
    if(!help){
        putchar('\n');
        return;
        }
    printf("\n    ");
    HelpState hs = {.output_width = term_size.columns - 4, .lead = 4, .remaining = 0};
    hs.remaining = hs.output_width;
    for(;*help;){
        auto tok = next_tokenize_help((const char*)help); // cast away nullability
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
int
parse_arg(Nonnull(ArgToParse*)arg, StringView s){
    if(arg->num_parsed >= arg->max_num)
        return ARGPARSE_EXCESS_ARGS;
    // If previous num parsed is nonzero, this means
    // that what we are pointing to is an array.
    switch(arg->dest.type){
        case ARG_INTEGER64:{
            auto e = parse_int64(s.text, s.length);
            if(unlikely(e.errored)) {
                return ARGPARSE_CONVERSION_ERROR;
                }
            auto value = e.result;
            int64_t* dest = arg->dest.pointer;
            dest += arg->num_parsed;
            *dest = value;
            arg->num_parsed += 1;
            }break;
        case ARG_UINTEGER64:{
            auto e = parse_unsigned_human(s.text, s.length);
            if(unlikely(e.errored)) {
                return ARGPARSE_CONVERSION_ERROR;
                }
            auto value = e.result;
            uint64_t* dest = arg->dest.pointer;
            dest += arg->num_parsed;
            *dest = value;
            arg->num_parsed += 1;
            }break;
        case ARG_INT:{
            auto e = parse_int(s.text, s.length);
            if(unlikely(e.errored)) {
                return ARGPARSE_CONVERSION_ERROR;
                }
            auto value = e.result;
            int* dest = arg->dest.pointer;
            dest += arg->num_parsed;
            *dest = value;
            arg->num_parsed += 1;
            }break;
        case ARG_FLOAT32:{
            char* endptr;
            float value = strtof(s.text, &endptr);
            if(endptr == s.text)
                return ARGPARSE_CONVERSION_ERROR;
            if(*endptr != '\0')
                return ARGPARSE_CONVERSION_ERROR;
            float* dest = arg->dest.pointer;
            dest += arg->num_parsed;
            *dest = value;
            arg->num_parsed += 1;
            }break;
        case ARG_FLOAT64:{
            char* endptr;
            double value = strtod(s.text, &endptr);
            if(endptr == s.text)
                return ARGPARSE_CONVERSION_ERROR;
            if(*endptr != '\0')
                return ARGPARSE_CONVERSION_ERROR;
            double* dest = arg->dest.pointer;
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
        case ARG_CSTRING:{
            const char** dest = arg->dest.pointer;
            dest += arg->num_parsed;
            *dest = s.text;
            arg->num_parsed += 1;
            }break;
        case ARG_USER_DEFINED:{
            char* dest = arg->dest.pointer;
            dest += arg->dest.user_pointer->type_size * arg->num_parsed;
            auto e = arg->dest.user_pointer->converter(arg->dest.user_pointer->user_data, s.text, s.length, dest);
            if(e) return ARGPARSE_CONVERSION_ERROR;
            arg->num_parsed += 1;
            }break;
        case ARG_ENUM:{
            if(!s.length) return ARGPARSE_CONVERSION_ERROR;
            const ArgParseEnumType* enu = arg->dest.enum_pointer;
            // We just do a linear search over the strings.
            // In theory this is very bad, but in practice
            // a typical parse line will need to match against a
            // given enum once so any fancy algorithm would
            // require a pre-pass over all the data anyway.
            // We could be faster if we required enums to be
            // sorted, but this harms usability too much.
            // If you need to parse a lot of enums with weird
            // requirements, then just a create a user defined
            // type instead of using this.
            for(size_t i = 0; i < enu->enum_count; i++){
                if(LS_SV_equals(enu->enum_names[i], s)){
                    switch(enu->enum_size){
                        case 1:{
                            uint8_t* dest = arg->dest.pointer;
                            dest += enu->enum_size * arg->num_parsed;
                            *dest = i;
                            arg->num_parsed += 1;
                            }return 0;
                        case 2:{
                            uint16_t* dest = arg->dest.pointer;
                            dest += enu->enum_size * arg->num_parsed;
                            *dest = i;
                            arg->num_parsed += 1;
                            }return 0;
                        case 4:{
                            uint32_t* dest = arg->dest.pointer;
                            dest += enu->enum_size * arg->num_parsed;
                            *dest = i;
                            arg->num_parsed += 1;
                            }return 0;
                        case 8:{
                            uint64_t* dest = arg->dest.pointer;
                            dest += enu->enum_size * arg->num_parsed;
                            *dest = i;
                            arg->num_parsed += 1;
                            }return 0;
                        default:
                            return ARGPARSE_CONVERSION_ERROR;
                        }
                    }
                }
            return ARGPARSE_CONVERSION_ERROR;
            }break;
        }
    return 0;
    }

// Set a flag. I really don't see why you would use this outside of this.
static inline
int
set_flag(Nonnull(ArgToParse*) arg){
    assert(arg->dest.type == ARG_FLAG);
    if(arg->num_parsed >= arg->max_num)
        return ARGPARSE_DUPLICATE_KWARG;
    bool* dest = arg->dest.pointer;
    *dest = true;
    arg->num_parsed += 1;
    return 0;
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

static inline
Nullable(ArgToParse*)
find_matching_kwarg(Nonnull(ArgParser*)parser, StringView sv){
    // do an inefficient linear search for now.
    for(size_t i = 0; i < parser->keyword.count; i++){
        ArgToParse* kw = &parser->keyword.args[i];
        if(SV_equals(kw->name, sv))
            return kw;
        if(kw->altname1.length){
            if(SV_equals(kw->altname1, sv))
                return kw;
            }
        }
    return NULL;
    }

// See top of file.
static inline
enum ArgParseError
parse_args(Nonnull(ArgParser*) parser, Nonnull(const Args*) args){
    ArgToParse* pos_arg = NULL;
    ArgToParse* past_the_end = NULL;
    if(parser->positional.count){
        pos_arg = &parser->positional.args[0];
        past_the_end = pos_arg + parser->positional.count;
        }
    ArgToParse* kwarg = NULL;
    auto argv_end = args->argv+args->argc;
    for(auto arg = args->argv; arg != argv_end; ++arg){
        auto s = cstr_to_SV(*arg);
        if(s.length > 1){
            if(s.text[0] == '-'){
                switch(s.text[1]){
                    case '0' ... '9':
                    case '.':
                        // number, not an argument.
                        break;
                    default:{
                        // Not a number, find matching kwarg
                        ArgToParse* new_kwarg = find_matching_kwarg(parser, s);
                        if(!new_kwarg){
                            parser->failed.arg = *arg;
                            return ARGPARSE_UNKNOWN_KWARG;
                            }
                        if(new_kwarg->visited){
                            parser->failed.arg_to_parse = new_kwarg;
                            parser->failed.arg = *arg;
                            return ARGPARSE_DUPLICATE_KWARG;
                            }
                        if(pos_arg && pos_arg != past_the_end && pos_arg->visited)
                            pos_arg++;
                        kwarg = new_kwarg;
                        kwarg->visited = true;
                        if(kwarg->dest.type == ARG_FLAG){
                            auto error = set_flag(kwarg);
                            if(error) {
                                parser->failed.arg_to_parse = kwarg;
                                parser->failed.arg = *arg;
                                return error;
                                }
                            kwarg = NULL;
                            }
                        continue;
                        }break;
                    }
                }
            }
        if(kwarg){
            auto err = parse_arg(kwarg, s);
            if(err){
                parser->failed.arg = *arg;
                parser->failed.arg_to_parse = kwarg;
                return err;
                }
            if(kwarg->num_parsed == kwarg->max_num)
                kwarg = NULL;
            }
        else if(pos_arg && pos_arg != past_the_end){
            pos_arg->visited = true;
            auto err = parse_arg(pos_arg, s);
            if(err){
                parser->failed.arg = *arg;
                parser->failed.arg_to_parse = pos_arg;
                return err;
                }
            if(pos_arg->num_parsed == pos_arg->max_num)
                pos_arg++;
            }
        else {
            parser->failed.arg = *arg;
            return ARGPARSE_EXCESS_ARGS;
            }
        }
    for(size_t i = 0; i < parser->positional.count; i++){
        auto arg = &parser->positional.args[i];
        if(arg->num_parsed < arg->min_num){
            parser->failed.arg_to_parse = arg;
            return ARGPARSE_INSUFFICIENT_ARGS;
            }
        if(arg->num_parsed > arg->max_num){
            parser->failed.arg_to_parse = arg;
            return ARGPARSE_EXCESS_ARGS;
            }
        }
    for(size_t i = 0; i < parser->keyword.count; i++){
        auto arg = &parser->keyword.args[i];
        if(arg->num_parsed < arg->min_num){
            parser->failed.arg_to_parse = arg;
            return ARGPARSE_INSUFFICIENT_ARGS;
            }
        if(arg->num_parsed > arg->max_num){
            parser->failed.arg_to_parse = arg;
            return ARGPARSE_EXCESS_ARGS;
            }
        // This only makes sense for keyword arguments.
        if(arg->visited && arg->num_parsed == 0){
            parser->failed.arg_to_parse = arg;
            return ARGPARSE_VISITED_NO_ARG_GIVEN;
            }
        }
    return 0;
    }

static inline
void
print_argparse_error(Nonnull(ArgParser*)parser, enum ArgParseError error){
    if(parser->failed.arg_to_parse){
        ArgToParse* arg_to_parse = parser->failed.arg_to_parse;
        fprintf(stderr, "Error when parsing argument for '%s': ", arg_to_parse->name.text);
        }
    switch(error){
        case ARGPARSE_NO_ERROR:
            //fall-through
        PushDiagnostic();
        SuppressCoveredSwitchDefault();
        default:
            fprintf(stderr, "Unknown error when parsing arguments\n");
            return;
        PopDiagnostic();
        case ARGPARSE_CONVERSION_ERROR:
            if(parser->failed.arg_to_parse){
                ArgToParse* arg_to_parse = parser->failed.arg_to_parse;
                if(parser->failed.arg){
                    const char* arg = parser->failed.arg;
                    switch(arg_to_parse->dest.type){
                        case ARG_INTEGER64:
                            fprintf(stderr, "Unable to parse an int64 from '%s'\n", arg);
                            return;
                        case ARG_INT:
                            fprintf(stderr, "Unable to parse an int from '%s'\n", arg);
                            return;
                        // These seem bizarre.
                        case ARG_STRING:
                            // fall-through
                        case ARG_CSTRING:
                            fprintf(stderr, "Unable to parse a string from '%s'\n", arg);
                            return;
                        case ARG_UINTEGER64:
                            fprintf(stderr, "Unable to parse a uint64 from '%s'\n", arg);
                            return;
                        case ARG_FLOAT32:
                            fprintf(stderr, "Unable to parse a float32 from '%s'\n", arg);
                            return;
                        case ARG_FLOAT64:
                            fprintf(stderr, "Unable to parse a float64 from '%s'\n", arg);
                            return;
                        case ARG_USER_DEFINED:
                            fprintf(stderr, "Unable to parse a %s from '%s'\n", arg_to_parse->dest.user_pointer->type_name.text, arg);
                            return;
                        case ARG_ENUM:
                            fprintf(stderr, "Unable to parse a choice from '%s. Not a valid option.\n", arg);
                            return;
                        case ARG_FLAG:
                            fprintf(stderr, "Unable to parse a flag. This is a bug.\n");
                            return;
                        }
                        fprintf(stderr, "Unable to parse an unknown type from '%s'\n", arg);
                        return;
                    }
                else {
                    switch(arg_to_parse->dest.type){
                        case ARG_INTEGER64:
                            fprintf(stderr, "Unable to parse an int64 from unknown argument'\n");
                            return;
                        case ARG_INT:
                            fprintf(stderr, "Unable to parse an int from unknown argument'\n");
                            return;
                        // These seem bizarre.
                        case ARG_STRING:
                            // fall-through
                        case ARG_CSTRING:
                            fprintf(stderr, "Unable to parse a string from unknown argument.\n");
                            return;
                        case ARG_UINTEGER64:
                            fprintf(stderr, "Unable to parse a uint64 from unknown argument.\n");
                            return;
                        case ARG_FLOAT32:
                            fprintf(stderr, "Unable to parse a float32 from unknown argument.\n");
                            return;
                        case ARG_FLOAT64:
                            fprintf(stderr, "Unable to parse a float64 from unknown argument.\n");
                            return;
                        case ARG_USER_DEFINED:
                            fprintf(stderr, "Unable to parse a %s from unknown argument.\n", arg_to_parse->dest.user_pointer->type_name.text);
                            return;
                        case ARG_ENUM:
                            fprintf(stderr, "Unable to parse a choice from unknown argument.\n");
                            return;
                        case ARG_FLAG:
                            fprintf(stderr, "Unable to parse a flag. This is a bug.\n");
                            return;
                        }
                    fprintf(stderr, "Unable to parse an unknown type from unknown argument'\n");
                    return;
                    }
                }
            else if(parser->failed.arg){
                const char* arg = parser->failed.arg;
                fprintf(stderr, "Unable to parse an unknown type from '%s'\n", arg);
                return;
                }
            else {
                fprintf(stderr, "Unable to parse an unknown type from an unknown argument. This is a bug.\n");
                }
            return;
        case ARGPARSE_UNKNOWN_KWARG:
            if(parser->failed.arg)
                fprintf(stderr, "Unrecognized argument '%s'\n", parser->failed.arg);
            else
                fprintf(stderr, "Unrecognized argument is unknown. This is a bug.\n");
            return;
        case ARGPARSE_DUPLICATE_KWARG:
            fprintf(stderr, "Option given more than once.\n");
            return;
        case ARGPARSE_EXCESS_ARGS:{
            // Args were given after all possible args were consumed.
            if(!parser->failed.arg_to_parse){
                fprintf(stderr, "More arguments given than needed. First excess argument: '%s'\n", parser->failed.arg);
                return;
                }
            ArgToParse* arg_to_parse = parser->failed.arg_to_parse;

            if(!parser->failed.arg){
                fprintf(stderr, "Excess arguments. No more than %d arguments needed. Unknown first excess argument (this is a bug)\n", arg_to_parse->max_num);
                return;
                }
            fprintf(stderr, "Excess arguments. No more than %d arguments needed. First excess argument: '%s'\n", arg_to_parse->max_num, parser->failed.arg) ;
            }return;
        case ARGPARSE_INSUFFICIENT_ARGS:{
            if(!parser->failed.arg_to_parse){
                fprintf(stderr, "Insufficent arguments for unknown option. This is a bug\n");
                return;
                }
            ArgToParse* arg_to_parse = parser->failed.arg_to_parse;
            fprintf(stderr, "Insufficient arguments.. %d arguments are required.\n", arg_to_parse->min_num);
            }return;
        case ARGPARSE_VISITED_NO_ARG_GIVEN:{
            ArgToParse* arg_to_parse = parser->failed.arg_to_parse;
            if(!arg_to_parse){
                fprintf(stderr, "An unknown argument was visited. This is a bug.\n");
                return;
                }
            fprintf(stderr, "No arguments given.\n");
            }return;
        }
    }
#endif
