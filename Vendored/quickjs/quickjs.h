/*
 * QuickJS Javascript Engine
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Modifications by D. Priver are released into the public domain.
 */
#ifndef QUICKJS_H
#define QUICKJS_H

#include <stdio.h>
#include <stdint.h>


#if !defined(QJS_API)
#if defined(QJS_SHARED_LIBRARY)

#if defined(_WIN32)
#define QJS_API __declspec(dllimport)
#else
#define QJS_API extern __attribute__((visibility("default")))
#endif

#else
#if defined(_MSC_VER) && !defined(__clang__)
#define QJS_API extern
#else
#define QJS_API extern __attribute__((visibility("hidden")))
#endif

#endif
#endif


#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define js_likely(x)          __builtin_expect(!!(x), 1)
#define js_unlikely(x)        __builtin_expect(!!(x), 0)
#define js_force_inline       inline __attribute__((always_inline))
#define __js_printf_like(f, a)   __attribute__((format(printf, f, a)))
#else
#define js_likely(x)     (x)
#define js_unlikely(x)   (x)
#define js_force_inline  inline
#define __js_printf_like(a, b)
#endif

#define QJS_BOOL int

typedef struct QJSRuntime QJSRuntime;
typedef struct QJSContext QJSContext;
typedef struct QJSObject QJSObject;
typedef struct QJSClass QJSClass;
typedef uint32_t QJSClassID;
typedef uint32_t QJSAtom;

#if INTPTR_MAX >= INT64_MAX
#define QJS_PTR64
#define QJS_PTR64_DEF(a) a
#else
#define QJS_PTR64_DEF(a)
#endif

#ifndef QJS_PTR64
#define QJS_NAN_BOXING
#endif

enum {
    /* all tags with a reference count are negative */
    QJS_TAG_FIRST       = -11, /* first negative tag */
    QJS_TAG_BIG_DECIMAL = -11,
    QJS_TAG_BIG_INT     = -10,
    QJS_TAG_BIG_FLOAT   = -9,
    QJS_TAG_SYMBOL      = -8,
    QJS_TAG_STRING      = -7,
    QJS_TAG_MODULE      = -3, /* used internally */
    QJS_TAG_FUNCTION_BYTECODE = -2, /* used internally */
    QJS_TAG_OBJECT      = -1,

    QJS_TAG_INT         = 0,
    QJS_TAG_BOOL        = 1,
    QJS_TAG_NULL        = 2,
    QJS_TAG_UNDEFINED   = 3,
    QJS_TAG_UNINITIALIZED = 4,
    QJS_TAG_CATCH_OFFSET = 5,
    QJS_TAG_EXCEPTION   = 6,
    QJS_TAG_FLOAT64     = 7,
    /* any larger tag is FLOAT64 if QJS_NAN_BOXING */
};

typedef struct QJSRefCountHeader {
    int ref_count;
} QJSRefCountHeader;

#define QJS_FLOAT64_NAN NAN

#ifdef CONFIG_CHECK_JSVALUE
/* QJSValue consistency : it is not possible to run the code in this
   mode, but it is useful to detect simple reference counting
   errors. It would be interesting to modify a static C analyzer to
   handle specific annotations (clang has such annotations but only
   for objective C) */
typedef struct __JSValue *QJSValue;
typedef const struct __JSValue *QJSValueConst;

#define QJS_VALUE_GET_TAG(v) (int)((uintptr_t)(v) & 0xf)
/* same as QJS_VALUE_GET_TAG, but return QJS_TAG_FLOAT64 with NaN boxing */
#define QJS_VALUE_GET_NORM_TAG(v) QJS_VALUE_GET_TAG(v)
#define QJS_VALUE_GET_INT(v) (int)((intptr_t)(v) >> 4)
#define QJS_VALUE_GET_BOOL(v) QJS_VALUE_GET_INT(v)
#define QJS_VALUE_GET_FLOAT64(v) (double)QJS_VALUE_GET_INT(v)
#define QJS_VALUE_GET_PTR(v) (void *)((intptr_t)(v) & ~0xf)

#define QJS_MKVAL(tag, val) (QJSValue)(intptr_t)(((val) << 4) | (tag))
#define QJS_MKPTR(tag, p) (QJSValue)((intptr_t)(p) | (tag))

#define QJS_TAG_IS_FLOAT64(tag) ((unsigned)(tag) == QJS_TAG_FLOAT64)

#define QJS_NAN QJS_MKVAL(QJS_TAG_FLOAT64, 1)

static inline QJSValue __JS_NewFloat64(QJSContext *ctx, double d)
{
    return QJS_MKVAL(QJS_TAG_FLOAT64, (int)d);
}

static inline QJS_BOOL QJS_VALUE_IS_NAN(QJSValue v)
{
    return 0;
}

#elif defined(QJS_NAN_BOXING)

typedef uint64_t QJSValue;

#define QJSValueConst QJSValue

#define QJS_VALUE_GET_TAG(v) (int)((v) >> 32)
#define QJS_VALUE_GET_INT(v) (int)(v)
#define QJS_VALUE_GET_BOOL(v) (int)(v)
#define QJS_VALUE_GET_PTR(v) (void *)(intptr_t)(v)

#define QJS_MKVAL(tag, val) (((uint64_t)(tag) << 32) | (uint32_t)(val))
#define QJS_MKPTR(tag, ptr) (((uint64_t)(tag) << 32) | (uintptr_t)(ptr))

#define QJS_FLOAT64_TAG_ADDEND (0x7ff80000 - QJS_TAG_FIRST + 1) /* quiet NaN encoding */

static inline double QJS_VALUE_GET_FLOAT64(QJSValue v)
{
    union {
        QJSValue v;
        double d;
    } u;
    u.v = v;
    u.v += (uint64_t)QJS_FLOAT64_TAG_ADDEND << 32;
    return u.d;
}

#define QJS_NAN (0x7ff8000000000000 - ((uint64_t)QJS_FLOAT64_TAG_ADDEND << 32))

static inline QJSValue __JS_NewFloat64(QJSContext *ctx, double d)
{
    union {
        double d;
        uint64_t u64;
    } u;
    QJSValue v;
    u.d = d;
    /* normalize NaN */
    if (js_unlikely((u.u64 & 0x7fffffffffffffff) > 0x7ff0000000000000))
        v = QJS_NAN;
    else
        v = u.u64 - ((uint64_t)QJS_FLOAT64_TAG_ADDEND << 32);
    return v;
}

#define QJS_TAG_IS_FLOAT64(tag) ((unsigned)((tag) - QJS_TAG_FIRST) >= (QJS_TAG_FLOAT64 - QJS_TAG_FIRST))

/* same as QJS_VALUE_GET_TAG, but return QJS_TAG_FLOAT64 with NaN boxing */
static inline int QJS_VALUE_GET_NORM_TAG(QJSValue v)
{
    uint32_t tag;
    tag = QJS_VALUE_GET_TAG(v);
    if (QJS_TAG_IS_FLOAT64(tag))
        return QJS_TAG_FLOAT64;
    else
        return tag;
}

static inline QJS_BOOL QJS_VALUE_IS_NAN(QJSValue v)
{
    uint32_t tag;
    tag = QJS_VALUE_GET_TAG(v);
    return tag == (QJS_NAN >> 32);
}

#else /* !QJS_NAN_BOXING */

typedef union QJSValueUnion {
    int32_t int32;
    double float64;
    void *ptr;
} QJSValueUnion;

typedef struct QJSValue {
    QJSValueUnion u;
    int64_t tag;
} QJSValue;

#define QJSValueConst QJSValue

#define QJS_VALUE_GET_TAG(v) ((int32_t)(v).tag)
/* same as QJS_VALUE_GET_TAG, but return QJS_TAG_FLOAT64 with NaN boxing */
#define QJS_VALUE_GET_NORM_TAG(v) QJS_VALUE_GET_TAG(v)
#define QJS_VALUE_GET_INT(v) ((v).u.int32)
#define QJS_VALUE_GET_BOOL(v) ((v).u.int32)
#define QJS_VALUE_GET_FLOAT64(v) ((v).u.float64)
#define QJS_VALUE_GET_PTR(v) ((v).u.ptr)

#define QJS_MKVAL(tag, val) (QJSValue){ (QJSValueUnion){ .int32 = val }, tag }
#define QJS_MKPTR(tag, p) (QJSValue){ (QJSValueUnion){ .ptr = p }, tag }

#define QJS_TAG_IS_FLOAT64(tag) ((unsigned)(tag) == QJS_TAG_FLOAT64)

#define QJS_NAN (QJSValue){ .u.float64 = QJS_FLOAT64_NAN, QJS_TAG_FLOAT64 }

static inline QJSValue __JS_NewFloat64(QJSContext *ctx, double d)
{
    (void)ctx;
    QJSValue v;
    v.tag = QJS_TAG_FLOAT64;
    v.u.float64 = d;
    return v;
}

static inline QJS_BOOL QJS_VALUE_IS_NAN(QJSValue v)
{
    union {
        double d;
        uint64_t u64;
    } u;
    if (v.tag != QJS_TAG_FLOAT64)
        return 0;
    u.d = v.u.float64;
    return (u.u64 & 0x7fffffffffffffff) > 0x7ff0000000000000;
}

#endif /* !QJS_NAN_BOXING */

#define QJS_VALUE_IS_BOTH_INT(v1, v2) ((QJS_VALUE_GET_TAG(v1) | QJS_VALUE_GET_TAG(v2)) == 0)
#define QJS_VALUE_IS_BOTH_FLOAT(v1, v2) (QJS_TAG_IS_FLOAT64(QJS_VALUE_GET_TAG(v1)) && QJS_TAG_IS_FLOAT64(QJS_VALUE_GET_TAG(v2)))

#define QJS_VALUE_GET_OBJ(v) ((QJSObject *)QJS_VALUE_GET_PTR(v))
#define QJS_VALUE_GET_STRING(v) ((QJSString *)QJS_VALUE_GET_PTR(v))
#define QJS_VALUE_HAS_REF_COUNT(v) ((unsigned)QJS_VALUE_GET_TAG(v) >= (unsigned)QJS_TAG_FIRST)

/* special values */
#define QJS_NULL      QJS_MKVAL(QJS_TAG_NULL, 0)
#define QJS_UNDEFINED QJS_MKVAL(QJS_TAG_UNDEFINED, 0)
#define QJS_FALSE     QJS_MKVAL(QJS_TAG_BOOL, 0)
#define QJS_TRUE      QJS_MKVAL(QJS_TAG_BOOL, 1)
#define QJS_EXCEPTION QJS_MKVAL(QJS_TAG_EXCEPTION, 0)
#define QJS_UNINITIALIZED QJS_MKVAL(QJS_TAG_UNINITIALIZED, 0)

/* flags for object properties */
#define QJS_PROP_CONFIGURABLE  (1 << 0)
#define QJS_PROP_WRITABLE      (1 << 1)
#define QJS_PROP_ENUMERABLE    (1 << 2)
#define QJS_PROP_C_W_E         (QJS_PROP_CONFIGURABLE | QJS_PROP_WRITABLE | QJS_PROP_ENUMERABLE)
#define QJS_PROP_LENGTH        (1 << 3) /* used internally in Arrays */
#define QJS_PROP_TMASK         (3 << 4) /* mask for NORMAL, GETSET, VARREF, AUTOINIT */
#define QJS_PROP_NORMAL         (0 << 4)
#define QJS_PROP_GETSET         (1 << 4)
#define QJS_PROP_VARREF         (2 << 4) /* used internally */
#define QJS_PROP_AUTOINIT       (3 << 4) /* used internally */

/* flags for QJS_DefineProperty */
#define QJS_PROP_HAS_SHIFT        8
#define QJS_PROP_HAS_CONFIGURABLE (1 << 8)
#define QJS_PROP_HAS_WRITABLE     (1 << 9)
#define QJS_PROP_HAS_ENUMERABLE   (1 << 10)
#define QJS_PROP_HAS_GET          (1 << 11)
#define QJS_PROP_HAS_SET          (1 << 12)
#define QJS_PROP_HAS_VALUE        (1 << 13)

/* throw an exception if false would be returned
   (QJS_DefineProperty/QJS_SetProperty) */
#define QJS_PROP_THROW            (1 << 14)
/* throw an exception if false would be returned in strict mode
   (QJS_SetProperty) */
#define QJS_PROP_THROW_STRICT     (1 << 15)

#define QJS_PROP_NO_ADD           (1 << 16) /* internal use */
#define QJS_PROP_NO_EXOTIC        (1 << 17) /* internal use */

#define QJS_DEFAULT_STACK_SIZE (256 * 1024)

/* QJS_Eval() flags */
#define QJS_EVAL_TYPE_GLOBAL   (0 << 0) /* global code (default) */
#define QJS_EVAL_TYPE_MODULE   (1 << 0) /* module code */
#define QJS_EVAL_TYPE_DIRECT   (2 << 0) /* direct call (internal use) */
#define QJS_EVAL_TYPE_INDIRECT (3 << 0) /* indirect call (internal use) */
#define QJS_EVAL_TYPE_MASK     (3 << 0)

#define QJS_EVAL_FLAG_STRICT   (1 << 3) /* force 'strict' mode */
#define QJS_EVAL_FLAG_STRIP    (1 << 4) /* force 'strip' mode */
/* compile but do not run. The result is an object with a
   QJS_TAG_FUNCTION_BYTECODE or QJS_TAG_MODULE tag. It can be executed
   with QJS_EvalFunction(). */
#define QJS_EVAL_FLAG_COMPILE_ONLY (1 << 5)
/* don't include the stack frames before this eval in the Error() backtraces */
#define QJS_EVAL_FLAG_BACKTRACE_BARRIER (1 << 6)

typedef QJSValue QJSCFunction(QJSContext *ctx, QJSValueConst this_val, int argc, QJSValueConst *argv);
typedef QJSValue QJSCFunctionMagic(QJSContext *ctx, QJSValueConst this_val, int argc, QJSValueConst *argv, int magic);
typedef QJSValue QJSCFunctionData(QJSContext *ctx, QJSValueConst this_val, int argc, QJSValueConst *argv, int magic, QJSValue *func_data);

typedef struct QJSMallocState {
    size_t malloc_count;
    size_t malloc_size;
    size_t malloc_limit;
    void *opaque; /* user opaque */
} QJSMallocState;

typedef struct QJSMallocFunctions {
    void *(*js_malloc)(QJSMallocState *s, size_t size);
    void (*js_free)(QJSMallocState *s, void *ptr);
    void *(*js_realloc)(QJSMallocState *s, void *ptr, size_t size);
    size_t (*js_malloc_usable_size)(const void *ptr);
} QJSMallocFunctions;

typedef struct QJSGCObjectHeader QJSGCObjectHeader;

QJS_API QJSRuntime *QJS_NewRuntime(void);
/* info lifetime must exceed that of rt */
QJS_API void QJS_SetRuntimeInfo(QJSRuntime *rt, const char *info);
QJS_API void QJS_SetMemoryLimit(QJSRuntime *rt, size_t limit);
QJS_API void QJS_SetGCThreshold(QJSRuntime *rt, size_t gc_threshold);
/* use 0 to disable maximum stack size check */
QJS_API void QJS_SetMaxStackSize(QJSRuntime *rt, size_t stack_size);
/* should be called when changing thread to update the stack top value
           used to check stack overflow. */
QJS_API void QJS_UpdateStackTop(QJSRuntime *rt);
QJS_API QJSRuntime *QJS_NewRuntime2(const QJSMallocFunctions *mf, void *opaque);
QJS_API void QJS_FreeRuntime(QJSRuntime *rt);
QJS_API void *QJS_GetRuntimeOpaque(QJSRuntime *rt);
QJS_API void QJS_SetRuntimeOpaque(QJSRuntime *rt, void *opaque);
typedef void QJS_MarkFunc(QJSRuntime *rt, QJSGCObjectHeader *gp);
QJS_API void QJS_MarkValue(QJSRuntime *rt, QJSValueConst val, QJS_MarkFunc *mark_func);
QJS_API void QJS_RunGC(QJSRuntime *rt);
QJS_API QJS_BOOL QJS_IsLiveObject(QJSRuntime *rt, QJSValueConst obj);

QJS_API QJSContext *QJS_NewContext(QJSRuntime *rt);
QJS_API void QJS_FreeContext(QJSContext *s);
QJS_API QJSContext *QJS_DupContext(QJSContext *ctx);
QJS_API void *QJS_GetContextOpaque(QJSContext *ctx);
QJS_API void QJS_SetContextOpaque(QJSContext *ctx, void *opaque);
QJS_API QJSRuntime *QJS_GetRuntime(QJSContext *ctx);
QJS_API void QJS_SetClassProto(QJSContext *ctx, QJSClassID class_id, QJSValue obj);
QJS_API QJSValue QJS_GetClassProto(QJSContext *ctx, QJSClassID class_id);

/* the following functions are used to select the intrinsic object to
   save memory */
QJS_API QJSContext *QJS_NewContextRaw(QJSRuntime *rt);
QJS_API void QJS_AddIntrinsicBaseObjects(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicDate(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicEval(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicStringNormalize(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicRegExpCompiler(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicRegExp(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicJSON(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicProxy(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicMapSet(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicTypedArrays(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicPromise(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicBigInt(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicBigFloat(QJSContext *ctx);
QJS_API void QJS_AddIntrinsicBigDecimal(QJSContext *ctx);
/* enable operator overloading */
QJS_API void QJS_AddIntrinsicOperators(QJSContext *ctx);
/* enable "use math" */
QJS_API void QJS_EnableBignumExt(QJSContext *ctx, QJS_BOOL enable);

QJS_API QJSValue js_string_codePointRange(QJSContext *ctx, QJSValueConst this_val,
                                 int argc, QJSValueConst *argv);

QJS_API void *js_malloc_rt(QJSRuntime *rt, size_t size);
QJS_API void js_free_rt(QJSRuntime *rt, void *ptr);
QJS_API void *js_realloc_rt(QJSRuntime *rt, void *ptr, size_t size);
QJS_API size_t js_malloc_usable_size_rt(QJSRuntime *rt, const void *ptr);
QJS_API void *js_mallocz_rt(QJSRuntime *rt, size_t size);

QJS_API void *js_malloc(QJSContext *ctx, size_t size);
QJS_API void js_free(QJSContext *ctx, void *ptr);
QJS_API void *js_realloc(QJSContext *ctx, void *ptr, size_t size);
QJS_API size_t js_malloc_usable_size(QJSContext *ctx, const void *ptr);
QJS_API void *js_realloc2(QJSContext *ctx, void *ptr, size_t size, size_t *pslack);
QJS_API void *js_mallocz(QJSContext *ctx, size_t size);
QJS_API char *js_strdup(QJSContext *ctx, const char *str);
QJS_API char *js_strndup(QJSContext *ctx, const char *s, size_t n);

typedef struct QJSMemoryUsage {
    int64_t malloc_size, malloc_limit, memory_used_size;
    int64_t malloc_count;
    int64_t memory_used_count;
    int64_t atom_count, atom_size;
    int64_t str_count, str_size;
    int64_t obj_count, obj_size;
    int64_t prop_count, prop_size;
    int64_t shape_count, shape_size;
    int64_t js_func_count, js_func_size, js_func_code_size;
    int64_t js_func_pc2line_count, js_func_pc2line_size;
    int64_t c_func_count, array_count;
    int64_t fast_array_count, fast_array_elements;
    int64_t binary_object_count, binary_object_size;
} QJSMemoryUsage;

QJS_API void QJS_ComputeMemoryUsage(QJSRuntime *rt, QJSMemoryUsage *s);
QJS_API void QJS_DumpMemoryUsage(FILE *fp, const QJSMemoryUsage *s, QJSRuntime *rt);

/* atom support */
#define QJS_ATOM_NULL 0

QJS_API QJSAtom QJS_NewAtomLen(QJSContext *ctx, const char *str, size_t len);
QJS_API QJSAtom QJS_NewAtom(QJSContext *ctx, const char *str);
QJS_API QJSAtom QJS_NewAtomUInt32(QJSContext *ctx, uint32_t n);
QJS_API QJSAtom QJS_DupAtom(QJSContext *ctx, QJSAtom v);
QJS_API void QJS_FreeAtom(QJSContext *ctx, QJSAtom v);
QJS_API void QJS_FreeAtomRT(QJSRuntime *rt, QJSAtom v);
QJS_API QJSValue QJS_AtomToValue(QJSContext *ctx, QJSAtom atom);
QJS_API QJSValue QJS_AtomToString(QJSContext *ctx, QJSAtom atom);
QJS_API const char *QJS_AtomToCString(QJSContext *ctx, QJSAtom atom);
QJS_API QJSAtom QJS_ValueToAtom(QJSContext *ctx, QJSValueConst val);

/* object class support */

typedef struct QJSPropertyEnum {
    QJS_BOOL is_enumerable;
    QJSAtom atom;
} QJSPropertyEnum;

typedef struct QJSPropertyDescriptor {
    int flags;
    QJSValue value;
    QJSValue getter;
    QJSValue setter;
} QJSPropertyDescriptor;

typedef struct QJSClassExoticMethods {
    /* Return -1 if exception (can only happen in case of Proxy object),
       FALSE if the property does not exists, TRUE if it exists. If 1 is
       returned, the property descriptor 'desc' is filled if != NULL. */
    int (*get_own_property)(QJSContext *ctx, QJSPropertyDescriptor *desc,
                             QJSValueConst obj, QJSAtom prop);
    /* '*ptab' should hold the '*plen' property keys. Return 0 if OK,
       -1 if exception. The 'is_enumerable' field is ignored.
    */
    int (*get_own_property_names)(QJSContext *ctx, QJSPropertyEnum **ptab,
                                  uint32_t *plen,
                                  QJSValueConst obj);
    /* return < 0 if exception, or TRUE/FALSE */
    int (*delete_property)(QJSContext *ctx, QJSValueConst obj, QJSAtom prop);
    /* return < 0 if exception or TRUE/FALSE */
    int (*define_own_property)(QJSContext *ctx, QJSValueConst this_obj,
                               QJSAtom prop, QJSValueConst val,
                               QJSValueConst getter, QJSValueConst setter,
                               int flags);
    /* The following methods can be emulated with the previous ones,
       so they are usually not needed */
    /* return < 0 if exception or TRUE/FALSE */
    int (*has_property)(QJSContext *ctx, QJSValueConst obj, QJSAtom atom);
    QJSValue (*get_property)(QJSContext *ctx, QJSValueConst obj, QJSAtom atom,
                            QJSValueConst receiver);
    /* return < 0 if exception or TRUE/FALSE */
    int (*set_property)(QJSContext *ctx, QJSValueConst obj, QJSAtom atom,
                        QJSValueConst value, QJSValueConst receiver, int flags);
} QJSClassExoticMethods;

typedef void QJSClassFinalizer(QJSRuntime *rt, QJSValue val);
typedef void QJSClassGCMark(QJSRuntime *rt, QJSValueConst val,
                           QJS_MarkFunc *mark_func);
#define QJS_CALL_FLAG_CONSTRUCTOR (1 << 0)
typedef QJSValue QJSClassCall(QJSContext *ctx, QJSValueConst func_obj,
                            QJSValueConst this_val, int argc, QJSValueConst *argv,
                            int flags);

typedef struct QJSClassDef {
    const char *class_name;
    QJSClassFinalizer *finalizer;
    QJSClassGCMark *gc_mark;
    /* if call != NULL, the object is a function. If (flags &
       QJS_CALL_FLAG_CONSTRUCTOR) != 0, the function is called as a
       constructor. In this case, 'this_val' is new.target. A
       constructor call only happens if the object constructor bit is
       set (see QJS_SetConstructorBit()). */
    QJSClassCall *call;
    /* XXX: suppress this indirection ? It is here only to save memory
       because only a few classes need these methods */
    QJSClassExoticMethods *exotic;
} QJSClassDef;

QJS_API QJSClassID QJS_NewClassID(QJSClassID *pclass_id);
QJS_API int QJS_NewClass(QJSRuntime *rt, QJSClassID class_id, const QJSClassDef *class_def);
QJS_API int QJS_IsRegisteredClass(QJSRuntime *rt, QJSClassID class_id);

/* value handling */

static js_force_inline QJSValue QJS_NewBool(QJSContext *ctx, QJS_BOOL val)
{
    (void)ctx;
    return QJS_MKVAL(QJS_TAG_BOOL, (val != 0));
}

static js_force_inline QJSValue QJS_NewInt32(QJSContext *ctx, int32_t val)
{
    (void)ctx;
    return QJS_MKVAL(QJS_TAG_INT, val);
}

static js_force_inline QJSValue QJS_NewCatchOffset(QJSContext *ctx, int32_t val)
{
    (void)ctx;
    return QJS_MKVAL(QJS_TAG_CATCH_OFFSET, val);
}

static js_force_inline QJSValue QJS_NewInt64(QJSContext *ctx, int64_t val)
{
    QJSValue v;
    if (val == (int32_t)val) {
        v = QJS_NewInt32(ctx, val);
    } else {
        v = __JS_NewFloat64(ctx, val);
    }
    return v;
}

static js_force_inline QJSValue QJS_NewUint32(QJSContext *ctx, uint32_t val)
{
    QJSValue v;
    if (val <= 0x7fffffff) {
        v = QJS_NewInt32(ctx, val);
    } else {
        v = __JS_NewFloat64(ctx, val);
    }
    return v;
}

QJS_API QJSValue QJS_NewBigInt64(QJSContext *ctx, int64_t v);
QJS_API QJSValue QJS_NewBigUint64(QJSContext *ctx, uint64_t v);

#if defined(__GNUC__) || defined(__clang__)
__attribute__((no_sanitize("undefined")))
#endif
static js_force_inline QJSValue QJS_NewFloat64(QJSContext *ctx, double d)
{
    QJSValue v;
    int32_t val;
    union {
        double d;
        uint64_t u;
    } u, t;
    u.d = d;
    val = (int32_t)d;
    t.d = val;
    /* -0 cannot be represented as integer, so we compare the bit
        representation */
    if (u.u == t.u) {
        v = QJS_MKVAL(QJS_TAG_INT, val);
    } else {
        v = __JS_NewFloat64(ctx, d);
    }
    return v;
}

static inline QJS_BOOL QJS_IsNumber(QJSValueConst v)
{
    int tag = QJS_VALUE_GET_TAG(v);
    return tag == QJS_TAG_INT || QJS_TAG_IS_FLOAT64(tag);
}

static inline QJS_BOOL QJS_IsBigInt(QJSContext *ctx, QJSValueConst v)
{
    (void)ctx;
    int tag = QJS_VALUE_GET_TAG(v);
    return tag == QJS_TAG_BIG_INT;
}

static inline QJS_BOOL QJS_IsBigFloat(QJSValueConst v)
{
    int tag = QJS_VALUE_GET_TAG(v);
    return tag == QJS_TAG_BIG_FLOAT;
}

static inline QJS_BOOL QJS_IsBigDecimal(QJSValueConst v)
{
    int tag = QJS_VALUE_GET_TAG(v);
    return tag == QJS_TAG_BIG_DECIMAL;
}

static inline QJS_BOOL QJS_IsBool(QJSValueConst v)
{
    return QJS_VALUE_GET_TAG(v) == QJS_TAG_BOOL;
}

static inline QJS_BOOL QJS_IsNull(QJSValueConst v)
{
    return QJS_VALUE_GET_TAG(v) == QJS_TAG_NULL;
}

static inline QJS_BOOL QJS_IsUndefined(QJSValueConst v)
{
    return QJS_VALUE_GET_TAG(v) == QJS_TAG_UNDEFINED;
}

static inline QJS_BOOL QJS_IsException(QJSValueConst v)
{
    return js_unlikely(QJS_VALUE_GET_TAG(v) == QJS_TAG_EXCEPTION);
}

static inline QJS_BOOL QJS_IsUninitialized(QJSValueConst v)
{
    return js_unlikely(QJS_VALUE_GET_TAG(v) == QJS_TAG_UNINITIALIZED);
}

static inline QJS_BOOL QJS_IsString(QJSValueConst v)
{
    return QJS_VALUE_GET_TAG(v) == QJS_TAG_STRING;
}

static inline QJS_BOOL QJS_IsSymbol(QJSValueConst v)
{
    return QJS_VALUE_GET_TAG(v) == QJS_TAG_SYMBOL;
}

static inline QJS_BOOL QJS_IsObject(QJSValueConst v)
{
    return QJS_VALUE_GET_TAG(v) == QJS_TAG_OBJECT;
}

QJS_API QJSValue QJS_Throw(QJSContext *ctx, QJSValue obj);
QJS_API QJSValue QJS_GetException(QJSContext *ctx);
QJS_API QJS_BOOL QJS_IsError(QJSContext *ctx, QJSValueConst val);
QJS_API void QJS_ResetUncatchableError(QJSContext *ctx);
QJS_API QJSValue QJS_NewError(QJSContext *ctx);
QJS_API QJSValue __js_printf_like(2, 3) QJS_ThrowSyntaxError(QJSContext *ctx, const char *fmt, ...);
QJS_API QJSValue __js_printf_like(2, 3) QJS_ThrowTypeError(QJSContext *ctx, const char *fmt, ...);
QJS_API QJSValue __js_printf_like(2, 3) QJS_ThrowReferenceError(QJSContext *ctx, const char *fmt, ...);
QJS_API QJSValue __js_printf_like(2, 3) QJS_ThrowRangeError(QJSContext *ctx, const char *fmt, ...);
QJS_API QJSValue __js_printf_like(2, 3) QJS_ThrowInternalError(QJSContext *ctx, const char *fmt, ...);
QJS_API QJSValue QJS_ThrowOutOfMemory(QJSContext *ctx);

QJS_API void __JS_FreeValue(QJSContext *ctx, QJSValue v);
static inline void QJS_FreeValue(QJSContext *ctx, QJSValue v)
{
    if (QJS_VALUE_HAS_REF_COUNT(v)) {
        QJSRefCountHeader *p = (QJSRefCountHeader *)QJS_VALUE_GET_PTR(v);
        if (--p->ref_count <= 0) {
            __JS_FreeValue(ctx, v);
        }
    }
}
QJS_API void __JS_FreeValueRT(QJSRuntime *rt, QJSValue v);
static inline void QJS_FreeValueRT(QJSRuntime *rt, QJSValue v)
{
    if (QJS_VALUE_HAS_REF_COUNT(v)) {
        QJSRefCountHeader *p = (QJSRefCountHeader *)QJS_VALUE_GET_PTR(v);
        if (--p->ref_count <= 0) {
            __JS_FreeValueRT(rt, v);
        }
    }
}

static inline QJSValue QJS_DupValue(QJSContext *ctx, QJSValueConst v)
{
    (void)ctx;
    if (QJS_VALUE_HAS_REF_COUNT(v)) {
        QJSRefCountHeader *p = (QJSRefCountHeader *)QJS_VALUE_GET_PTR(v);
        p->ref_count++;
    }
    #ifdef CONFIG_CHECK_JSVALUE
    return (QJSValue)v;
    #else
    return v;
    #endif
}

static inline QJSValue QJS_DupValueRT(QJSRuntime *rt, QJSValueConst v)
{
    (void)rt;
    if (QJS_VALUE_HAS_REF_COUNT(v)) {
        QJSRefCountHeader *p = (QJSRefCountHeader *)QJS_VALUE_GET_PTR(v);
        p->ref_count++;
    }
    #ifdef CONFIG_CHECK_JSVALUE
    return (QJSValue)v;
    #else
    return v;
    #endif
}

QJS_API int QJS_ToBool(QJSContext *ctx, QJSValueConst val); /* return -1 for QJS_EXCEPTION */
QJS_API int QJS_ToInt32(QJSContext *ctx, int32_t *pres, QJSValueConst val);
static inline int QJS_ToUint32(QJSContext *ctx, uint32_t *pres, QJSValueConst val)
{
    return QJS_ToInt32(ctx, (int32_t*)pres, val);
}
QJS_API int QJS_ToInt64(QJSContext *ctx, int64_t *pres, QJSValueConst val);
QJS_API int QJS_ToIndex(QJSContext *ctx, uint64_t *plen, QJSValueConst val);
QJS_API int QJS_ToFloat64(QJSContext *ctx, double *pres, QJSValueConst val);
/* return an exception if 'val' is a Number */
QJS_API int QJS_ToBigInt64(QJSContext *ctx, int64_t *pres, QJSValueConst val);
/* same as QJS_ToInt64() but allow BigInt */
QJS_API int QJS_ToInt64Ext(QJSContext *ctx, int64_t *pres, QJSValueConst val);

QJS_API QJSValue QJS_NewStringLen(QJSContext *ctx, const char *str1, size_t len1);
QJS_API QJSValue QJS_NewString(QJSContext *ctx, const char *str);
QJS_API QJSValue QJS_NewAtomString(QJSContext *ctx, const char *str);
QJS_API QJSValue QJS_ToString(QJSContext *ctx, QJSValueConst val);
QJS_API QJSValue QJS_ToPropertyKey(QJSContext *ctx, QJSValueConst val);
QJS_API const char *QJS_ToCStringLen2(QJSContext *ctx, size_t *plen, QJSValueConst val1, QJS_BOOL cesu8);
static inline const char *QJS_ToCStringLen(QJSContext *ctx, size_t *plen, QJSValueConst val1)
{
    return QJS_ToCStringLen2(ctx, plen, val1, 0);
}
static inline const char *QJS_ToCString(QJSContext *ctx, QJSValueConst val1)
{
    return QJS_ToCStringLen2(ctx, NULL, val1, 0);
}
QJS_API void QJS_FreeCString(QJSContext *ctx, const char *ptr);

QJS_API QJSValue QJS_NewObjectProtoClass(QJSContext *ctx, QJSValueConst proto, QJSClassID class_id);
QJS_API QJSValue QJS_NewObjectClass(QJSContext *ctx, int class_id);
QJS_API QJSValue QJS_NewObjectProto(QJSContext *ctx, QJSValueConst proto);
QJS_API QJSValue QJS_NewObject(QJSContext *ctx);

QJS_API QJS_BOOL QJS_IsFunction(QJSContext* ctx, QJSValueConst val);
QJS_API QJS_BOOL QJS_IsConstructor(QJSContext* ctx, QJSValueConst val);
QJS_API QJS_BOOL QJS_SetConstructorBit(QJSContext *ctx, QJSValueConst func_obj, QJS_BOOL val);

QJS_API QJSValue QJS_NewArray(QJSContext *ctx);
QJS_API int QJS_IsArray(QJSContext *ctx, QJSValueConst val);

QJS_API QJSValue QJS_GetPropertyInternal(QJSContext *ctx, QJSValueConst obj,
                               QJSAtom prop, QJSValueConst receiver,
                               QJS_BOOL throw_ref_error);
static js_force_inline QJSValue QJS_GetProperty(QJSContext *ctx, QJSValueConst this_obj,
                                              QJSAtom prop)
{
    return QJS_GetPropertyInternal(ctx, this_obj, prop, this_obj, 0);
}
QJS_API QJSValue QJS_GetPropertyStr(QJSContext *ctx, QJSValueConst this_obj,
                          const char *prop);
QJS_API QJSValue QJS_GetPropertyUint32(QJSContext *ctx, QJSValueConst this_obj,
                             uint32_t idx);

QJS_API int QJS_SetPropertyInternal(QJSContext *ctx, QJSValueConst this_obj,
                           QJSAtom prop, QJSValue val,
                           int flags);
static inline int QJS_SetProperty(QJSContext *ctx, QJSValueConst this_obj,
                                 QJSAtom prop, QJSValue val)
{
    return QJS_SetPropertyInternal(ctx, this_obj, prop, val, QJS_PROP_THROW);
}
QJS_API int QJS_SetPropertyUint32(QJSContext *ctx, QJSValueConst this_obj,
                         uint32_t idx, QJSValue val);
QJS_API int QJS_SetPropertyInt64(QJSContext *ctx, QJSValueConst this_obj,
                        int64_t idx, QJSValue val);
QJS_API int QJS_SetPropertyStr(QJSContext *ctx, QJSValueConst this_obj,
                      const char *prop, QJSValue val);
QJS_API int QJS_HasProperty(QJSContext *ctx, QJSValueConst this_obj, QJSAtom prop);
QJS_API int QJS_IsExtensible(QJSContext *ctx, QJSValueConst obj);
QJS_API int QJS_PreventExtensions(QJSContext *ctx, QJSValueConst obj);
QJS_API int QJS_DeleteProperty(QJSContext *ctx, QJSValueConst obj, QJSAtom prop, int flags);
QJS_API int QJS_SetPrototype(QJSContext *ctx, QJSValueConst obj, QJSValueConst proto_val);
QJS_API QJSValue QJS_GetPrototype(QJSContext *ctx, QJSValueConst val);

#define QJS_GPN_STRING_MASK  (1 << 0)
#define QJS_GPN_SYMBOL_MASK  (1 << 1)
#define QJS_GPN_PRIVATE_MASK (1 << 2)
/* only include the enumerable properties */
#define QJS_GPN_ENUM_ONLY    (1 << 4)
/* set theJSPropertyEnum.is_enumerable field */
#define QJS_GPN_SET_ENUM     (1 << 5)

QJS_API int QJS_GetOwnPropertyNames(QJSContext *ctx, QJSPropertyEnum **ptab,
                           uint32_t *plen, QJSValueConst obj, int flags);
QJS_API int QJS_GetOwnProperty(QJSContext *ctx, QJSPropertyDescriptor *desc,
                      QJSValueConst obj, QJSAtom prop);

QJS_API QJSValue QJS_Call(QJSContext *ctx, QJSValueConst func_obj, QJSValueConst this_obj,
                int argc, QJSValueConst *argv);
QJS_API QJSValue QJS_Invoke(QJSContext *ctx, QJSValueConst this_val, QJSAtom atom,
                  int argc, QJSValueConst *argv);
QJS_API QJSValue QJS_CallConstructor(QJSContext *ctx, QJSValueConst func_obj,
                           int argc, QJSValueConst *argv);
QJS_API QJSValue QJS_CallConstructor2(QJSContext *ctx, QJSValueConst func_obj,
                            QJSValueConst new_target,
                            int argc, QJSValueConst *argv);
QJS_API QJS_BOOL QJS_DetectModule(const char *input, size_t input_len);
/* 'input' must be zero terminated i.e. input[input_len] = '\0'. */
QJS_API QJSValue QJS_Eval(QJSContext *ctx, const char *input, size_t input_len,
                const char *filename, int eval_flags);
/* same as QJS_Eval() but with an explicit 'this_obj' parameter */
QJS_API QJSValue QJS_EvalThis(QJSContext *ctx, QJSValueConst this_obj,
                    const char *input, size_t input_len,
                    const char *filename, int eval_flags);
QJS_API QJSValue QJS_GetGlobalObject(QJSContext *ctx);
QJS_API int QJS_IsInstanceOf(QJSContext *ctx, QJSValueConst val, QJSValueConst obj);
QJS_API int QJS_DefineProperty(QJSContext *ctx, QJSValueConst this_obj,
                      QJSAtom prop, QJSValueConst val,
                      QJSValueConst getter, QJSValueConst setter, int flags);
QJS_API int QJS_DefinePropertyValue(QJSContext *ctx, QJSValueConst this_obj,
                           QJSAtom prop, QJSValue val, int flags);
QJS_API int QJS_DefinePropertyValueUint32(QJSContext *ctx, QJSValueConst this_obj,
                                 uint32_t idx, QJSValue val, int flags);
QJS_API int QJS_DefinePropertyValueStr(QJSContext *ctx, QJSValueConst this_obj,
                              const char *prop, QJSValue val, int flags);
QJS_API int QJS_DefinePropertyGetSet(QJSContext *ctx, QJSValueConst this_obj,
                            QJSAtom prop, QJSValue getter, QJSValue setter,
                            int flags);
QJS_API void QJS_SetOpaque(QJSValue obj, void *opaque);
QJS_API void *QJS_GetOpaque(QJSValueConst obj, QJSClassID class_id);
QJS_API void *QJS_GetOpaque2(QJSContext *ctx, QJSValueConst obj, QJSClassID class_id);

/* 'buf' must be zero terminated i.e. buf[buf_len] = '\0'. */
QJS_API QJSValue QJS_ParseJSON(QJSContext *ctx, const char *buf, size_t buf_len,
                     const char *filename);
#define QJS_PARSE_JSON_EXT (1 << 0) /* allow extended JSON */
QJS_API QJSValue QJS_ParseJSON2(QJSContext *ctx, const char *buf, size_t buf_len,
                      const char *filename, int flags);
QJS_API QJSValue QJS_JSONStringify(QJSContext *ctx, QJSValueConst obj,
                         QJSValueConst replacer, QJSValueConst space0);

typedef void QJSFreeArrayBufferDataFunc(QJSRuntime *rt, void *opaque, void *ptr);
QJS_API QJSValue QJS_NewArrayBuffer(QJSContext *ctx, uint8_t *buf, size_t len,
                          QJSFreeArrayBufferDataFunc *free_func, void *opaque,
                          QJS_BOOL is_shared);
QJS_API QJSValue QJS_NewArrayBufferCopy(QJSContext *ctx, const uint8_t *buf, size_t len);
QJS_API void QJS_DetachArrayBuffer(QJSContext *ctx, QJSValueConst obj);
QJS_API uint8_t *QJS_GetArrayBuffer(QJSContext *ctx, size_t *psize, QJSValueConst obj);
QJS_API QJSValue QJS_GetTypedArrayBuffer(QJSContext *ctx, QJSValueConst obj,
                               size_t *pbyte_offset,
                               size_t *pbyte_length,
                               size_t *pbytes_per_element);
typedef struct {
    void *(*sab_alloc)(void *opaque, size_t size);
    void (*sab_free)(void *opaque, void *ptr);
    void (*sab_dup)(void *opaque, void *ptr);
    void *sab_opaque;
} QJSSharedArrayBufferFunctions;
QJS_API void QJS_SetSharedArrayBufferFunctions(QJSRuntime *rt,
                                      const QJSSharedArrayBufferFunctions *sf);

QJS_API QJSValue QJS_NewPromiseCapability(QJSContext *ctx, QJSValue *resolving_funcs);

/* is_handled = TRUE means that the rejection is handled */
typedef void QJSHostPromiseRejectionTracker(QJSContext *ctx, QJSValueConst promise,
                                           QJSValueConst reason,
                                           QJS_BOOL is_handled, void *opaque);
QJS_API void QJS_SetHostPromiseRejectionTracker(QJSRuntime *rt, QJSHostPromiseRejectionTracker *cb, void *opaque);

/* return != 0 if the QJS code needs to be interrupted */
typedef int QJSInterruptHandler(QJSRuntime *rt, void *opaque);
QJS_API void QJS_SetInterruptHandler(QJSRuntime *rt, QJSInterruptHandler *cb, void *opaque);
/* if can_block is TRUE, Atomics.wait() can be used */
QJS_API void QJS_SetCanBlock(QJSRuntime *rt, QJS_BOOL can_block);
/* set the [IsHTMLDDA] internal slot */
QJS_API void QJS_SetIsHTMLDDA(QJSContext *ctx, QJSValueConst obj);

typedef struct QJSModuleDef QJSModuleDef;

/* return the module specifier (allocated with js_malloc()) or NULL if
   exception */
typedef char *QJSModuleNormalizeFunc(QJSContext *ctx,
                                    const char *module_base_name,
                                    const char *module_name, void *opaque);
typedef QJSModuleDef *QJSModuleLoaderFunc(QJSContext *ctx,
                                        const char *module_name, void *opaque);

/* module_normalize = NULL is allowed and invokes the default module
   filename normalizer */
QJS_API void QJS_SetModuleLoaderFunc(QJSRuntime *rt,
                            QJSModuleNormalizeFunc *module_normalize,
                            QJSModuleLoaderFunc *module_loader, void *opaque);
/* return the import.meta object of a module */
QJS_API QJSValue QJS_GetImportMeta(QJSContext *ctx, QJSModuleDef *m);
QJS_API QJSAtom QJS_GetModuleName(QJSContext *ctx, QJSModuleDef *m);

/* QJS Job support */

typedef QJSValue QJSJobFunc(QJSContext *ctx, int argc, QJSValueConst *argv);
QJS_API int QJS_EnqueueJob(QJSContext *ctx, QJSJobFunc *job_func, int argc, QJSValueConst *argv);

QJS_API QJS_BOOL QJS_IsJobPending(QJSRuntime *rt);
QJS_API int QJS_ExecutePendingJob(QJSRuntime *rt, QJSContext **pctx);

/* Object Writer/Reader (currently only used to handle precompiled code) */
#define QJS_WRITE_OBJ_BYTECODE  (1 << 0) /* allow function/module */
#define QJS_WRITE_OBJ_BSWAP     (1 << 1) /* byte swapped output */
#define QJS_WRITE_OBJ_SAB       (1 << 2) /* allow SharedArrayBuffer */
#define QJS_WRITE_OBJ_REFERENCE (1 << 3) /* allow object references to
                                           encode arbitrary object
                                           graph */
QJS_API uint8_t *QJS_WriteObject(QJSContext *ctx, size_t *psize, QJSValueConst obj,
                        int flags);
QJS_API uint8_t *QJS_WriteObject2(QJSContext *ctx, size_t *psize, QJSValueConst obj,
                         int flags, uint8_t ***psab_tab, size_t *psab_tab_len);

#define QJS_READ_OBJ_BYTECODE  (1 << 0) /* allow function/module */
#define QJS_READ_OBJ_ROM_DATA  (1 << 1) /* avoid duplicating 'buf' data */
#define QJS_READ_OBJ_SAB       (1 << 2) /* allow SharedArrayBuffer */
#define QJS_READ_OBJ_REFERENCE (1 << 3) /* allow object references */
QJS_API QJSValue QJS_ReadObject(QJSContext *ctx, const uint8_t *buf, size_t buf_len,
                      int flags);
/* instantiate and evaluate a bytecode function. Only used when
   reading a script or module with QJS_ReadObject() */
QJS_API QJSValue QJS_EvalFunction(QJSContext *ctx, QJSValue fun_obj);
/* load the dependencies of the module 'obj'. Useful when QJS_ReadObject()
   returns a module. */
QJS_API int QJS_ResolveModule(QJSContext *ctx, QJSValueConst obj);

/* only exported for os.Worker() */
QJS_API QJSAtom QJS_GetScriptOrModuleName(QJSContext *ctx, int n_stack_levels);
/* only exported for os.Worker() */
QJS_API QJSModuleDef *QJS_RunModule(QJSContext *ctx, const char *basename,
                          const char *filename);

/* C function definition */
typedef enum QJSCFunctionEnum {  /* XXX: should rename for namespace isolation */
    QJS_CFUNC_generic,
    QJS_CFUNC_generic_magic,
    QJS_CFUNC_constructor,
    QJS_CFUNC_constructor_magic,
    QJS_CFUNC_constructor_or_func,
    QJS_CFUNC_constructor_or_func_magic,
    QJS_CFUNC_f_f,
    QJS_CFUNC_f_f_f,
    QJS_CFUNC_getter,
    QJS_CFUNC_setter,
    QJS_CFUNC_getter_magic,
    QJS_CFUNC_setter_magic,
    QJS_CFUNC_iterator_next,
} QJSCFunctionEnum;

typedef union QJSCFunctionType {
    QJSCFunction *generic;
    QJSValue (*generic_magic)(QJSContext *ctx, QJSValueConst this_val, int argc, QJSValueConst *argv, int magic);
    QJSCFunction *constructor;
    QJSValue (*constructor_magic)(QJSContext *ctx, QJSValueConst new_target, int argc, QJSValueConst *argv, int magic);
    QJSCFunction *constructor_or_func;
    double (*f_f)(double);
    double (*f_f_f)(double, double);
    QJSValue (*getter)(QJSContext *ctx, QJSValueConst this_val);
    QJSValue (*setter)(QJSContext *ctx, QJSValueConst this_val, QJSValueConst val);
    QJSValue (*getter_magic)(QJSContext *ctx, QJSValueConst this_val, int magic);
    QJSValue (*setter_magic)(QJSContext *ctx, QJSValueConst this_val, QJSValueConst val, int magic);
    QJSValue (*iterator_next)(QJSContext *ctx, QJSValueConst this_val,
                             int argc, QJSValueConst *argv, int *pdone, int magic);
} QJSCFunctionType;

QJS_API QJSValue QJS_NewCFunction2(QJSContext *ctx, QJSCFunction *func,
                         const char *name,
                         int length, QJSCFunctionEnum cproto, int magic);
QJS_API QJSValue QJS_NewCFunctionData(QJSContext *ctx, QJSCFunctionData *func,
                            int length, int magic, int data_len,
                            QJSValueConst *data);

static inline QJSValue QJS_NewCFunction(QJSContext *ctx, QJSCFunction *func, const char *name,
                                      int length)
{
    return QJS_NewCFunction2(ctx, func, name, length, QJS_CFUNC_generic, 0);
}

static inline QJSValue QJS_NewCFunctionMagic(QJSContext *ctx, QJSCFunctionMagic *func,
                                           const char *name,
                                           int length, QJSCFunctionEnum cproto, int magic)
{
    // XXX: suppress bad function cast.
    return QJS_NewCFunction2(ctx, (QJSCFunction *)(void*)func, name, length, cproto, magic);
}
QJS_API void QJS_SetConstructor(QJSContext *ctx, QJSValueConst func_obj,
                       QJSValueConst proto);

/* C property definition */

typedef struct QJSCFunctionListEntry {
    const char *name;
    uint8_t prop_flags;
    uint8_t def_type;
    int16_t magic;
    union {
        struct {
            uint8_t length; /* XXX: should move outside union */
            uint8_t cproto; /* XXX: should move outside union */
            QJSCFunctionType cfunc;
        } func;
        struct {
            QJSCFunctionType get;
            QJSCFunctionType set;
        } getset;
        struct {
            const char *name;
            int base;
        } alias;
        struct {
            const struct QJSCFunctionListEntry *tab;
            int len;
        } prop_list;
        const char *str;
        int32_t i32;
        int64_t i64;
        double f64;
    } u;
} QJSCFunctionListEntry;

#define QJS_DEF_CFUNC          0
#define QJS_DEF_CGETSET        1
#define QJS_DEF_CGETSET_MAGIC  2
#define QJS_DEF_PROP_STRING    3
#define QJS_DEF_PROP_INT32     4
#define QJS_DEF_PROP_INT64     5
#define QJS_DEF_PROP_DOUBLE    6
#define QJS_DEF_PROP_UNDEFINED 7
#define QJS_DEF_OBJECT         8
#define QJS_DEF_ALIAS          9

/* Note: c++ does not like nested designators */
#define QJS_CFUNC_DEF(name, length, func1) { name, QJS_PROP_WRITABLE | QJS_PROP_CONFIGURABLE, QJS_DEF_CFUNC, 0, .u = { .func = { length, QJS_CFUNC_generic, { .generic = func1 } } } }
#define QJS_CFUNC_MAGIC_DEF(name, length, func1, magic) { name, QJS_PROP_WRITABLE | QJS_PROP_CONFIGURABLE, QJS_DEF_CFUNC, magic, .u = { .func = { length, QJS_CFUNC_generic_magic, { .generic_magic = func1 } } } }
#define QJS_CFUNC_SPECIAL_DEF(name, length, cproto, func1) { name, QJS_PROP_WRITABLE | QJS_PROP_CONFIGURABLE, QJS_DEF_CFUNC, 0, .u = { .func = { length, QJS_CFUNC_ ## cproto, { .cproto = func1 } } } }
#define QJS_ITERATOR_NEXT_DEF(name, length, func1, magic) { name, QJS_PROP_WRITABLE | QJS_PROP_CONFIGURABLE, QJS_DEF_CFUNC, magic, .u = { .func = { length, QJS_CFUNC_iterator_next, { .iterator_next = func1 } } } }
#define QJS_CGETSET_DEF(name, fgetter, fsetter) { name, QJS_PROP_CONFIGURABLE, QJS_DEF_CGETSET, 0, .u = { .getset = { .get = { .getter = fgetter }, .set = { .setter = fsetter } } } }
#define QJS_CGETSET_MAGIC_DEF(name, fgetter, fsetter, magic) { name, QJS_PROP_CONFIGURABLE, QJS_DEF_CGETSET_MAGIC, magic, .u = { .getset = { .get = { .getter_magic = fgetter }, .set = { .setter_magic = fsetter } } } }
#define QJS_PROP_STRING_DEF(name, cstr, prop_flags) { name, prop_flags, QJS_DEF_PROP_STRING, 0, .u = { .str = cstr } }
#define QJS_PROP_INT32_DEF(name, val, prop_flags) { name, prop_flags, QJS_DEF_PROP_INT32, 0, .u = { .i32 = val } }
#define QJS_PROP_INT64_DEF(name, val, prop_flags) { name, prop_flags, QJS_DEF_PROP_INT64, 0, .u = { .i64 = val } }
#define QJS_PROP_DOUBLE_DEF(name, val, prop_flags) { name, prop_flags, QJS_DEF_PROP_DOUBLE, 0, .u = { .f64 = val } }
#define QJS_PROP_UNDEFINED_DEF(name, prop_flags) { name, prop_flags, QJS_DEF_PROP_UNDEFINED, 0, .u = { .i32 = 0 } }
#define QJS_OBJECT_DEF(name, tab, len, prop_flags) { name, prop_flags, QJS_DEF_OBJECT, 0, .u = { .prop_list = { tab, len } } }
#define QJS_ALIAS_DEF(name, from) { name, QJS_PROP_WRITABLE | QJS_PROP_CONFIGURABLE, QJS_DEF_ALIAS, 0, .u = { .alias = { from, -1 } } }
#define QJS_ALIAS_BASE_DEF(name, from, base) { name, QJS_PROP_WRITABLE | QJS_PROP_CONFIGURABLE, QJS_DEF_ALIAS, 0, .u = { .alias = { from, base } } }

QJS_API void QJS_SetPropertyFunctionList(QJSContext *ctx, QJSValueConst obj,
                                const QJSCFunctionListEntry *tab,
                                int len);

/* C module definition */

typedef int QJSModuleInitFunc(QJSContext *ctx, QJSModuleDef *m);

QJS_API QJSModuleDef *QJS_NewCModule(QJSContext *ctx, const char *name_str,
                           QJSModuleInitFunc *func);
/* can only be called before the module is instantiated */
QJS_API int QJS_AddModuleExport(QJSContext *ctx, QJSModuleDef *m, const char *name_str);
QJS_API int QJS_AddModuleExportList(QJSContext *ctx, QJSModuleDef *m,
                           const QJSCFunctionListEntry *tab, int len);
/* can only be called after the module is instantiated */
QJS_API int QJS_SetModuleExport(QJSContext *ctx, QJSModuleDef *m, const char *export_name,
                       QJSValue val);
QJS_API int QJS_SetModuleExportList(QJSContext *ctx, QJSModuleDef *m,
                           const QJSCFunctionListEntry *tab, int len);

#undef js_unlikely
#undef js_force_inline

//
// NOTE(dpriver): This was not originally in quickjs
// I added it so I could implement logging
//
// returns -1 on error, without setting exception.
QJS_API
int
QJS_get_caller_location(QJSContext* ctx, const char** filename, const char** funcname, int* line_num);

//
// NOTE(dpriver): This not being exposed was super annoying.
QJS_API
QJSValue
QJS_ArrayPush(QJSContext *ctx, QJSValueConst this_val, int argc, QJSValueConst *argv);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* QUICKJS_H */
