//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#ifndef RECURSIVE_GLOB_H
#define RECURSIVE_GLOB_H

#ifndef RECURSIVE_GLOB_API
#define RECURSIVE_GLOB_API static
#endif

#include "long_string.h"

#ifndef MARRAY_STRINGVIEW_DEFINED
#define MARRAY_STRINGVIEW_DEFINED
#define MARRAY_T StringView
#include "Marray.h"
#endif

RECURSIVE_GLOB_API void recursive_glob_suffix(LongString directory, StringView suffix, Marray(StringView)* entries, int max_depth);

#endif
