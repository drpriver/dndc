//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#ifndef DNDC_LONG_STRING_H
#define DNDC_LONG_STRING_H
// The existence of this file sucks, but allows to put the LongString
// types in the public API while also allowing my shared utilities to be developed
// independently of dndc.
// This header needs to be included before any other header that might potentially
// include "long_string.h"
#include "dndc.h"
typedef struct DndcLongString LongString;
typedef struct DndcStringView StringView;
typedef struct DndcStringViewUtf16 StringViewUtf16;
#define LONGSTRING_DEFINED
#include "Utils/long_string.h"
#endif
