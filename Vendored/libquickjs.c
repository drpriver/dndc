// Unity build simplifies build system and also allows better
// control of visibility of symbols for library.
#define CONFIG_VERSION "2021-03-27"

#if defined(__clang__)
// These are basically harmless.
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wbad-function-cast"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
// These warrant investigation.
#pragma clang diagnostic ignored "-Wduplicate-enum"
#pragma clang diagnostic ignored "-Wconditional-uninitialized"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wbad-function-cast"
// What is up with these?
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#if !defined(BUILDING_SHARED_OBJECT) && !defined(QJS_API)
#define QJS_API extern __attribute__((visibility("hidden")))
#endif

#include "quickjs/cutils.c"

// definitions conflict with the ones in quickjs.
#define compute_stack_size re_compute_stack_size
#define is_digit re_is_digit
#include "quickjs/libregexp.c"
#undef is_digit
#undef compute_stack_size

#include "quickjs/libunicode.c"
#include "quickjs/quickjs.c"
