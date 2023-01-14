#ifndef NATIVE_PICKER_H
#define NATIVE_PICKER_H

#include "long_string.h"

#ifndef warn_unused
#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif


static
warn_unused
int
native_gui_pick_directory(LongString* directory);

// TODO: file picker
#if 0
static int
native_gui_pick_file(LongString* file);
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
