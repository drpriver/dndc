#ifndef DNDC_TYPES_H
#define DNDC_TYPES_H

#define MARRAY_T StringView
#include "Marray.h"
#define MARRAY_T LongString
#include "Marray.h"

typedef struct LoadedSource {
    LongString sourcepath; // doesn't have to be a filename
    LongString sourcetext; // the actual source text
    } LoadedSource;
#define MARRAY_T LoadedSource
#include "Marray.h"

typedef struct LoadedBin {
    LongString path; // doesn't have to be a filename
    ByteBuffer bytes;
    } LoadedBin;
#define MARRAY_T LoadedBin
#include "Marray.h"

typedef struct LinkItem {
    StringView key;
    StringView value;
} LinkItem;
#define MARRAY_T LinkItem
#include "Marray.h"

typedef struct DataItem {
    StringView key;
    LongString value;
} DataItem;
#define MARRAY_T DataItem
#include "Marray.h"

typedef struct Attribute {
    StringView key;
    StringView value; // often null
    } Attribute;
#define MARRAY_T Attribute
#include "Marray.h"

typedef union NodeHandle {
    struct{uint32_t index; };
    uint32_t _value;
    } NodeHandle;

#define INVALID_NODE_HANDLE ((NodeHandle){._value=-1})
static inline
force_inline
bool
NodeHandle_eq(NodeHandle a, NodeHandle b){
    return a._value == b._value;
    }
#define MARRAY_T NodeHandle
#include "Marray.h"

#endif
