// Unity build simplifies build system and also allows better
// control of visibility of symbols for library.
#define CONFIG_VERSION "2021-03-27"
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
