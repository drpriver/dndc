//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
#ifndef GI_BYTE_DISTANCE_COMPLETER_H
#define GI_BYTE_DISTANCE_COMPLETER_H
#include "get_input.h"
#include "long_string.h"

#ifndef MARRAY_STRINGVIEW_DEFINED
#define MARRAY_STRINGVIEW_DEFINED
#define MARRAY_T StringView
#include "Marray.h"
#endif

static GiTabCompletionFunc byte_distance_completer;
struct ByteDistanceCompleterContext {
    const Marray(StringView)* original;
    Marray(StringView) ordered;
    StringView strip_suff;
};

#endif
