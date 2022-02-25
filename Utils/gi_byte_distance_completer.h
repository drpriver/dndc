#ifndef GI_BYTE_DISTANCE_COMPLETER_H
#define GI_BYTE_DISTANCE_COMPLETER_H
// NOTE: Marray(StringView) must be defined before this is included
// This kind of sucks, but C has no way of detecting if a type is already
// declared!
#include "get_input.h"
static GiTabCompletionFunc byte_distance_completer;
struct ByteDistanceCompleterContext {
    const Marray(StringView)* original;
    Marray(StringView) ordered;
    StringView strip_suff;
};

#endif
