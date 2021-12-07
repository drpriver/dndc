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
#define QJS_API extern __attribute__((visibility("hidden")))

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

#define JS_BOOL int

typedef struct QJSRuntime QJSRuntime;
typedef struct QJSContext QJSContext;
typedef struct JSObject JSObject;
typedef struct JSClass JSClass;
typedef uint32_t JSClassID;
typedef uint32_t JSAtom;

#if INTPTR_MAX >= INT64_MAX
#define JS_PTR64
#define JS_PTR64_DEF(a) a
#else
#define JS_PTR64_DEF(a)
#endif

#ifndef JS_PTR64
#define JS_NAN_BOXING
#endif

enum {
    /* all tags with a reference count are negative */
    JS_TAG_FIRST       = -11, /* first negative tag */
    JS_TAG_BIG_DECIMAL = -11,
    JS_TAG_BIG_INT     = -10,
    JS_TAG_BIG_FLOAT   = -9,
    JS_TAG_SYMBOL      = -8,
    JS_TAG_STRING      = -7,
    JS_TAG_MODULE      = -3, /* used internally */
    JS_TAG_FUNCTION_BYTECODE = -2, /* used internally */
    JS_TAG_OBJECT      = -1,

    JS_TAG_INT         = 0,
    JS_TAG_BOOL        = 1,
    JS_TAG_NULL        = 2,
    JS_TAG_UNDEFINED   = 3,
    JS_TAG_UNINITIALIZED = 4,
    JS_TAG_CATCH_OFFSET = 5,
    JS_TAG_EXCEPTION   = 6,
    JS_TAG_FLOAT64     = 7,
    /* any larger tag is FLOAT64 if JS_NAN_BOXING */
};

typedef struct JSRefCountHeader {
    int ref_count;
} JSRefCountHeader;

#define JS_FLOAT64_NAN NAN

#ifdef CONFIG_CHECK_JSVALUE
/* QJSValue consistency : it is not possible to run the code in this
   mode, but it is useful to detect simple reference counting
   errors. It would be interesting to modify a static C analyzer to
   handle specific annotations (clang has such annotations but only
   for objective C) */
typedef struct __JSValue *QJSValue;
typedef const struct __JSValue *QJSValueConst;

#define JS_VALUE_GET_TAG(v) (int)((uintptr_t)(v) & 0xf)
/* same as JS_VALUE_GET_TAG, but return JS_TAG_FLOAT64 with NaN boxing */
#define JS_VALUE_GET_NORM_TAG(v) JS_VALUE_GET_TAG(v)
#define JS_VALUE_GET_INT(v) (int)((intptr_t)(v) >> 4)
#define JS_VALUE_GET_BOOL(v) JS_VALUE_GET_INT(v)
#define JS_VALUE_GET_FLOAT64(v) (double)JS_VALUE_GET_INT(v)
#define JS_VALUE_GET_PTR(v) (void *)((intptr_t)(v) & ~0xf)

#define JS_MKVAL(tag, val) (QJSValue)(intptr_t)(((val) << 4) | (tag))
#define JS_MKPTR(tag, p) (QJSValue)((intptr_t)(p) | (tag))

#define JS_TAG_IS_FLOAT64(tag) ((unsigned)(tag) == JS_TAG_FLOAT64)

#define JS_NAN JS_MKVAL(JS_TAG_FLOAT64, 1)

static inline QJSValue __JS_NewFloat64(QJSContext *ctx, double d)
{
    return JS_MKVAL(JS_TAG_FLOAT64, (int)d);
}

static inline JS_BOOL JS_VALUE_IS_NAN(QJSValue v)
{
    return 0;
}

#elif defined(JS_NAN_BOXING)

typedef uint64_t QJSValue;

#define QJSValueConst QJSValue

#define JS_VALUE_GET_TAG(v) (int)((v) >> 32)
#define JS_VALUE_GET_INT(v) (int)(v)
#define JS_VALUE_GET_BOOL(v) (int)(v)
#define JS_VALUE_GET_PTR(v) (void *)(intptr_t)(v)

#define JS_MKVAL(tag, val) (((uint64_t)(tag) << 32) | (uint32_t)(val))
#define JS_MKPTR(tag, ptr) (((uint64_t)(tag) << 32) | (uintptr_t)(ptr))

#define JS_FLOAT64_TAG_ADDEND (0x7ff80000 - JS_TAG_FIRST + 1) /* quiet NaN encoding */

static inline double JS_VALUE_GET_FLOAT64(QJSValue v)
{
    union {
        QJSValue v;
        double d;
    } u;
    u.v = v;
    u.v += (uint64_t)JS_FLOAT64_TAG_ADDEND << 32;
    return u.d;
}

#define JS_NAN (0x7ff8000000000000 - ((uint64_t)JS_FLOAT64_TAG_ADDEND << 32))

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
        v = JS_NAN;
    else
        v = u.u64 - ((uint64_t)JS_FLOAT64_TAG_ADDEND << 32);
    return v;
}

#define JS_TAG_IS_FLOAT64(tag) ((unsigned)((tag) - JS_TAG_FIRST) >= (JS_TAG_FLOAT64 - JS_TAG_FIRST))

/* same as JS_VALUE_GET_TAG, but return JS_TAG_FLOAT64 with NaN boxing */
static inline int JS_VALUE_GET_NORM_TAG(QJSValue v)
{
    uint32_t tag;
    tag = JS_VALUE_GET_TAG(v);
    if (JS_TAG_IS_FLOAT64(tag))
        return JS_TAG_FLOAT64;
    else
        return tag;
}

static inline JS_BOOL JS_VALUE_IS_NAN(QJSValue v)
{
    uint32_t tag;
    tag = JS_VALUE_GET_TAG(v);
    return tag == (JS_NAN >> 32);
}

#else /* !JS_NAN_BOXING */

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

#define JS_VALUE_GET_TAG(v) ((int32_t)(v).tag)
/* same as JS_VALUE_GET_TAG, but return JS_TAG_FLOAT64 with NaN boxing */
#define JS_VALUE_GET_NORM_TAG(v) JS_VALUE_GET_TAG(v)
#define JS_VALUE_GET_INT(v) ((v).u.int32)
#define JS_VALUE_GET_BOOL(v) ((v).u.int32)
#define JS_VALUE_GET_FLOAT64(v) ((v).u.float64)
#define JS_VALUE_GET_PTR(v) ((v).u.ptr)

#define JS_MKVAL(tag, val) (QJSValue){ (QJSValueUnion){ .int32 = val }, tag }
#define JS_MKPTR(tag, p) (QJSValue){ (QJSValueUnion){ .ptr = p }, tag }

#define JS_TAG_IS_FLOAT64(tag) ((unsigned)(tag) == JS_TAG_FLOAT64)

#define JS_NAN (QJSValue){ .u.float64 = JS_FLOAT64_NAN, JS_TAG_FLOAT64 }

static inline QJSValue __JS_NewFloat64(QJSContext *ctx, double d)
{
    (void)ctx;
    QJSValue v;
    v.tag = JS_TAG_FLOAT64;
    v.u.float64 = d;
    return v;
}

static inline JS_BOOL JS_VALUE_IS_NAN(QJSValue v)
{
    union {
        double d;
        uint64_t u64;
    } u;
    if (v.tag != JS_TAG_FLOAT64)
        return 0;
    u.d = v.u.float64;
    return (u.u64 & 0x7fffffffffffffff) > 0x7ff0000000000000;
}

#endif /* !JS_NAN_BOXING */

#define JS_VALUE_IS_BOTH_INT(v1, v2) ((JS_VALUE_GET_TAG(v1) | JS_VALUE_GET_TAG(v2)) == 0)
#define JS_VALUE_IS_BOTH_FLOAT(v1, v2) (JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(v1)) && JS_TAG_IS_FLOAT64(JS_VALUE_GET_TAG(v2)))

#define JS_VALUE_GET_OBJ(v) ((JSObject *)JS_VALUE_GET_PTR(v))
#define JS_VALUE_GET_STRING(v) ((JSString *)JS_VALUE_GET_PTR(v))
#define JS_VALUE_HAS_REF_COUNT(v) ((unsigned)JS_VALUE_GET_TAG(v) >= (unsigned)JS_TAG_FIRST)

/* special values */
#define JS_NULL      JS_MKVAL(JS_TAG_NULL, 0)
#define JS_UNDEFINED JS_MKVAL(JS_TAG_UNDEFINED, 0)
#define JS_FALSE     JS_MKVAL(JS_TAG_BOOL, 0)
#define JS_TRUE      JS_MKVAL(JS_TAG_BOOL, 1)
#define JS_EXCEPTION JS_MKVAL(JS_TAG_EXCEPTION, 0)
#define JS_UNINITIALIZED JS_MKVAL(JS_TAG_UNINITIALIZED, 0)

/* flags for object properties */
#define JS_PROP_CONFIGURABLE  (1 << 0)
#define JS_PROP_WRITABLE      (1 << 1)
#define JS_PROP_ENUMERABLE    (1 << 2)
#define JS_PROP_C_W_E         (JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE | JS_PROP_ENUMERABLE)
#define JS_PROP_LENGTH        (1 << 3) /* used internally in Arrays */
#define JS_PROP_TMASK         (3 << 4) /* mask for NORMAL, GETSET, VARREF, AUTOINIT */
#define JS_PROP_NORMAL         (0 << 4)
#define JS_PROP_GETSET         (1 << 4)
#define JS_PROP_VARREF         (2 << 4) /* used internally */
#define JS_PROP_AUTOINIT       (3 << 4) /* used internally */

/* flags for JS_DefineProperty */
#define JS_PROP_HAS_SHIFT        8
#define JS_PROP_HAS_CONFIGURABLE (1 << 8)
#define JS_PROP_HAS_WRITABLE     (1 << 9)
#define JS_PROP_HAS_ENUMERABLE   (1 << 10)
#define JS_PROP_HAS_GET          (1 << 11)
#define JS_PROP_HAS_SET          (1 << 12)
#define JS_PROP_HAS_VALUE        (1 << 13)

/* throw an exception if false would be returned
   (JS_DefineProperty/JS_SetProperty) */
#define JS_PROP_THROW            (1 << 14)
/* throw an exception if false would be returned in strict mode
   (JS_SetProperty) */
#define JS_PROP_THROW_STRICT     (1 << 15)

#define JS_PROP_NO_ADD           (1 << 16) /* internal use */
#define JS_PROP_NO_EXOTIC        (1 << 17) /* internal use */

#define JS_DEFAULT_STACK_SIZE (256 * 1024)

/* JS_Eval() flags */
#define JS_EVAL_TYPE_GLOBAL   (0 << 0) /* global code (default) */
#define JS_EVAL_TYPE_MODULE   (1 << 0) /* module code */
#define JS_EVAL_TYPE_DIRECT   (2 << 0) /* direct call (internal use) */
#define JS_EVAL_TYPE_INDIRECT (3 << 0) /* indirect call (internal use) */
#define JS_EVAL_TYPE_MASK     (3 << 0)

#define JS_EVAL_FLAG_STRICT   (1 << 3) /* force 'strict' mode */
#define JS_EVAL_FLAG_STRIP    (1 << 4) /* force 'strip' mode */
/* compile but do not run. The result is an object with a
   JS_TAG_FUNCTION_BYTECODE or JS_TAG_MODULE tag. It can be executed
   with JS_EvalFunction(). */
#define JS_EVAL_FLAG_COMPILE_ONLY (1 << 5)
/* don't include the stack frames before this eval in the Error() backtraces */
#define JS_EVAL_FLAG_BACKTRACE_BARRIER (1 << 6)

typedef QJSValue JSCFunction(QJSContext *ctx, QJSValueConst this_val, int argc, QJSValueConst *argv);
typedef QJSValue JSCFunctionMagic(QJSContext *ctx, QJSValueConst this_val, int argc, QJSValueConst *argv, int magic);
typedef QJSValue JSCFunctionData(QJSContext *ctx, QJSValueConst this_val, int argc, QJSValueConst *argv, int magic, QJSValue *func_data);

typedef struct JSMallocState {
    size_t malloc_count;
    size_t malloc_size;
    size_t malloc_limit;
    void *opaque; /* user opaque */
} JSMallocState;

typedef struct JSMallocFunctions {
    void *(*js_malloc)(JSMallocState *s, size_t size);
    void (*js_free)(JSMallocState *s, void *ptr);
    void *(*js_realloc)(JSMallocState *s, void *ptr, size_t size);
    size_t (*js_malloc_usable_size)(const void *ptr);
} JSMallocFunctions;

typedef struct JSGCObjectHeader JSGCObjectHeader;

QJS_API QJSRuntime *JS_NewRuntime(void);
/* info lifetime must exceed that of rt */
QJS_API void JS_SetRuntimeInfo(QJSRuntime *rt, const char *info);
QJS_API void JS_SetMemoryLimit(QJSRuntime *rt, size_t limit);
QJS_API void JS_SetGCThreshold(QJSRuntime *rt, size_t gc_threshold);
/* use 0 to disable maximum stack size check */
QJS_API void JS_SetMaxStackSize(QJSRuntime *rt, size_t stack_size);
/* should be called when changing thread to update the stack top value
           used to check stack overflow. */
QJS_API void JS_UpdateStackTop(QJSRuntime *rt);
QJS_API QJSRuntime *JS_NewRuntime2(const JSMallocFunctions *mf, void *opaque);
QJS_API void JS_FreeRuntime(QJSRuntime *rt);
QJS_API void *JS_GetRuntimeOpaque(QJSRuntime *rt);
QJS_API void JS_SetRuntimeOpaque(QJSRuntime *rt, void *opaque);
typedef void JS_MarkFunc(QJSRuntime *rt, JSGCObjectHeader *gp);
QJS_API void JS_MarkValue(QJSRuntime *rt, QJSValueConst val, JS_MarkFunc *mark_func);
QJS_API void JS_RunGC(QJSRuntime *rt);
QJS_API JS_BOOL JS_IsLiveObject(QJSRuntime *rt, QJSValueConst obj);

QJS_API QJSContext *JS_NewContext(QJSRuntime *rt);
QJS_API void JS_FreeContext(QJSContext *s);
QJS_API QJSContext *JS_DupContext(QJSContext *ctx);
QJS_API void *JS_GetContextOpaque(QJSContext *ctx);
QJS_API void JS_SetContextOpaque(QJSContext *ctx, void *opaque);
QJS_API QJSRuntime *JS_GetRuntime(QJSContext *ctx);
QJS_API void JS_SetClassProto(QJSContext *ctx, JSClassID class_id, QJSValue obj);
QJS_API QJSValue JS_GetClassProto(QJSContext *ctx, JSClassID class_id);

/* the following functions are used to select the intrinsic object to
   save memory */
QJS_API QJSContext *JS_NewContextRaw(QJSRuntime *rt);
QJS_API void JS_AddIntrinsicBaseObjects(QJSContext *ctx);
QJS_API void JS_AddIntrinsicDate(QJSContext *ctx);
QJS_API void JS_AddIntrinsicEval(QJSContext *ctx);
QJS_API void JS_AddIntrinsicStringNormalize(QJSContext *ctx);
QJS_API void JS_AddIntrinsicRegExpCompiler(QJSContext *ctx);
QJS_API void JS_AddIntrinsicRegExp(QJSContext *ctx);
QJS_API void JS_AddIntrinsicJSON(QJSContext *ctx);
QJS_API void JS_AddIntrinsicProxy(QJSContext *ctx);
QJS_API void JS_AddIntrinsicMapSet(QJSContext *ctx);
QJS_API void JS_AddIntrinsicTypedArrays(QJSContext *ctx);
QJS_API void JS_AddIntrinsicPromise(QJSContext *ctx);
QJS_API void JS_AddIntrinsicBigInt(QJSContext *ctx);
QJS_API void JS_AddIntrinsicBigFloat(QJSContext *ctx);
QJS_API void JS_AddIntrinsicBigDecimal(QJSContext *ctx);
/* enable operator overloading */
QJS_API void JS_AddIntrinsicOperators(QJSContext *ctx);
/* enable "use math" */
QJS_API void JS_EnableBignumExt(QJSContext *ctx, JS_BOOL enable);

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

typedef struct JSMemoryUsage {
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
} JSMemoryUsage;

QJS_API void JS_ComputeMemoryUsage(QJSRuntime *rt, JSMemoryUsage *s);
QJS_API void JS_DumpMemoryUsage(FILE *fp, const JSMemoryUsage *s, QJSRuntime *rt);

/* atom support */
#define JS_ATOM_NULL 0

QJS_API JSAtom JS_NewAtomLen(QJSContext *ctx, const char *str, size_t len);
QJS_API JSAtom JS_NewAtom(QJSContext *ctx, const char *str);
QJS_API JSAtom JS_NewAtomUInt32(QJSContext *ctx, uint32_t n);
QJS_API JSAtom JS_DupAtom(QJSContext *ctx, JSAtom v);
QJS_API void JS_FreeAtom(QJSContext *ctx, JSAtom v);
QJS_API void JS_FreeAtomRT(QJSRuntime *rt, JSAtom v);
QJS_API QJSValue JS_AtomToValue(QJSContext *ctx, JSAtom atom);
QJS_API QJSValue JS_AtomToString(QJSContext *ctx, JSAtom atom);
QJS_API const char *JS_AtomToCString(QJSContext *ctx, JSAtom atom);
QJS_API JSAtom JS_ValueToAtom(QJSContext *ctx, QJSValueConst val);

/* object class support */

typedef struct JSPropertyEnum {
    JS_BOOL is_enumerable;
    JSAtom atom;
} JSPropertyEnum;

typedef struct JSPropertyDescriptor {
    int flags;
    QJSValue value;
    QJSValue getter;
    QJSValue setter;
} JSPropertyDescriptor;

typedef struct JSClassExoticMethods {
    /* Return -1 if exception (can only happen in case of Proxy object),
       FALSE if the property does not exists, TRUE if it exists. If 1 is
       returned, the property descriptor 'desc' is filled if != NULL. */
    int (*get_own_property)(QJSContext *ctx, JSPropertyDescriptor *desc,
                             QJSValueConst obj, JSAtom prop);
    /* '*ptab' should hold the '*plen' property keys. Return 0 if OK,
       -1 if exception. The 'is_enumerable' field is ignored.
    */
    int (*get_own_property_names)(QJSContext *ctx, JSPropertyEnum **ptab,
                                  uint32_t *plen,
                                  QJSValueConst obj);
    /* return < 0 if exception, or TRUE/FALSE */
    int (*delete_property)(QJSContext *ctx, QJSValueConst obj, JSAtom prop);
    /* return < 0 if exception or TRUE/FALSE */
    int (*define_own_property)(QJSContext *ctx, QJSValueConst this_obj,
                               JSAtom prop, QJSValueConst val,
                               QJSValueConst getter, QJSValueConst setter,
                               int flags);
    /* The following methods can be emulated with the previous ones,
       so they are usually not needed */
    /* return < 0 if exception or TRUE/FALSE */
    int (*has_property)(QJSContext *ctx, QJSValueConst obj, JSAtom atom);
    QJSValue (*get_property)(QJSContext *ctx, QJSValueConst obj, JSAtom atom,
                            QJSValueConst receiver);
    /* return < 0 if exception or TRUE/FALSE */
    int (*set_property)(QJSContext *ctx, QJSValueConst obj, JSAtom atom,
                        QJSValueConst value, QJSValueConst receiver, int flags);
} JSClassExoticMethods;

typedef void JSClassFinalizer(QJSRuntime *rt, QJSValue val);
typedef void JSClassGCMark(QJSRuntime *rt, QJSValueConst val,
                           JS_MarkFunc *mark_func);
#define JS_CALL_FLAG_CONSTRUCTOR (1 << 0)
typedef QJSValue JSClassCall(QJSContext *ctx, QJSValueConst func_obj,
                            QJSValueConst this_val, int argc, QJSValueConst *argv,
                            int flags);

typedef struct JSClassDef {
    const char *class_name;
    JSClassFinalizer *finalizer;
    JSClassGCMark *gc_mark;
    /* if call != NULL, the object is a function. If (flags &
       JS_CALL_FLAG_CONSTRUCTOR) != 0, the function is called as a
       constructor. In this case, 'this_val' is new.target. A
       constructor call only happens if the object constructor bit is
       set (see JS_SetConstructorBit()). */
    JSClassCall *call;
    /* XXX: suppress this indirection ? It is here only to save memory
       because only a few classes need these methods */
    JSClassExoticMethods *exotic;
} JSClassDef;

QJS_API JSClassID JS_NewClassID(JSClassID *pclass_id);
QJS_API int JS_NewClass(QJSRuntime *rt, JSClassID class_id, const JSClassDef *class_def);
QJS_API int JS_IsRegisteredClass(QJSRuntime *rt, JSClassID class_id);

/* value handling */

static js_force_inline QJSValue JS_NewBool(QJSContext *ctx, JS_BOOL val)
{
    (void)ctx;
    return JS_MKVAL(JS_TAG_BOOL, (val != 0));
}

static js_force_inline QJSValue JS_NewInt32(QJSContext *ctx, int32_t val)
{
    (void)ctx;
    return JS_MKVAL(JS_TAG_INT, val);
}

static js_force_inline QJSValue JS_NewCatchOffset(QJSContext *ctx, int32_t val)
{
    (void)ctx;
    return JS_MKVAL(JS_TAG_CATCH_OFFSET, val);
}

static js_force_inline QJSValue JS_NewInt64(QJSContext *ctx, int64_t val)
{
    QJSValue v;
    if (val == (int32_t)val) {
        v = JS_NewInt32(ctx, val);
    } else {
        v = __JS_NewFloat64(ctx, val);
    }
    return v;
}

static js_force_inline QJSValue JS_NewUint32(QJSContext *ctx, uint32_t val)
{
    QJSValue v;
    if (val <= 0x7fffffff) {
        v = JS_NewInt32(ctx, val);
    } else {
        v = __JS_NewFloat64(ctx, val);
    }
    return v;
}

QJS_API QJSValue JS_NewBigInt64(QJSContext *ctx, int64_t v);
QJS_API QJSValue JS_NewBigUint64(QJSContext *ctx, uint64_t v);

static js_force_inline QJSValue JS_NewFloat64(QJSContext *ctx, double d)
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
        v = JS_MKVAL(JS_TAG_INT, val);
    } else {
        v = __JS_NewFloat64(ctx, d);
    }
    return v;
}

static inline JS_BOOL JS_IsNumber(QJSValueConst v)
{
    int tag = JS_VALUE_GET_TAG(v);
    return tag == JS_TAG_INT || JS_TAG_IS_FLOAT64(tag);
}

static inline JS_BOOL JS_IsBigInt(QJSContext *ctx, QJSValueConst v)
{
    (void)ctx;
    int tag = JS_VALUE_GET_TAG(v);
    return tag == JS_TAG_BIG_INT;
}

static inline JS_BOOL JS_IsBigFloat(QJSValueConst v)
{
    int tag = JS_VALUE_GET_TAG(v);
    return tag == JS_TAG_BIG_FLOAT;
}

static inline JS_BOOL JS_IsBigDecimal(QJSValueConst v)
{
    int tag = JS_VALUE_GET_TAG(v);
    return tag == JS_TAG_BIG_DECIMAL;
}

static inline JS_BOOL JS_IsBool(QJSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_BOOL;
}

static inline JS_BOOL JS_IsNull(QJSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_NULL;
}

static inline JS_BOOL JS_IsUndefined(QJSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_UNDEFINED;
}

static inline JS_BOOL JS_IsException(QJSValueConst v)
{
    return js_unlikely(JS_VALUE_GET_TAG(v) == JS_TAG_EXCEPTION);
}

static inline JS_BOOL JS_IsUninitialized(QJSValueConst v)
{
    return js_unlikely(JS_VALUE_GET_TAG(v) == JS_TAG_UNINITIALIZED);
}

static inline JS_BOOL JS_IsString(QJSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_STRING;
}

static inline JS_BOOL JS_IsSymbol(QJSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_SYMBOL;
}

static inline JS_BOOL JS_IsObject(QJSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_OBJECT;
}

QJS_API QJSValue JS_Throw(QJSContext *ctx, QJSValue obj);
QJS_API QJSValue JS_GetException(QJSContext *ctx);
QJS_API JS_BOOL JS_IsError(QJSContext *ctx, QJSValueConst val);
QJS_API void JS_ResetUncatchableError(QJSContext *ctx);
QJS_API QJSValue JS_NewError(QJSContext *ctx);
QJS_API QJSValue __js_printf_like(2, 3) JS_ThrowSyntaxError(QJSContext *ctx, const char *fmt, ...);
QJS_API QJSValue __js_printf_like(2, 3) JS_ThrowTypeError(QJSContext *ctx, const char *fmt, ...);
QJS_API QJSValue __js_printf_like(2, 3) JS_ThrowReferenceError(QJSContext *ctx, const char *fmt, ...);
QJS_API QJSValue __js_printf_like(2, 3) JS_ThrowRangeError(QJSContext *ctx, const char *fmt, ...);
QJS_API QJSValue __js_printf_like(2, 3) JS_ThrowInternalError(QJSContext *ctx, const char *fmt, ...);
QJS_API QJSValue JS_ThrowOutOfMemory(QJSContext *ctx);

QJS_API void __JS_FreeValue(QJSContext *ctx, QJSValue v);
static inline void JS_FreeValue(QJSContext *ctx, QJSValue v)
{
    if (JS_VALUE_HAS_REF_COUNT(v)) {
        JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(v);
        if (--p->ref_count <= 0) {
            __JS_FreeValue(ctx, v);
        }
    }
}
QJS_API void __JS_FreeValueRT(QJSRuntime *rt, QJSValue v);
static inline void JS_FreeValueRT(QJSRuntime *rt, QJSValue v)
{
    if (JS_VALUE_HAS_REF_COUNT(v)) {
        JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(v);
        if (--p->ref_count <= 0) {
            __JS_FreeValueRT(rt, v);
        }
    }
}

static inline QJSValue JS_DupValue(QJSContext *ctx, QJSValueConst v)
{
    (void)ctx;
    if (JS_VALUE_HAS_REF_COUNT(v)) {
        JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(v);
        p->ref_count++;
    }
    #ifdef CONFIG_CHECK_JSVALUE
    return (QJSValue)v;
    #else
    return v;
    #endif
}

static inline QJSValue JS_DupValueRT(QJSRuntime *rt, QJSValueConst v)
{
    (void)rt;
    if (JS_VALUE_HAS_REF_COUNT(v)) {
        JSRefCountHeader *p = (JSRefCountHeader *)JS_VALUE_GET_PTR(v);
        p->ref_count++;
    }
    #ifdef CONFIG_CHECK_JSVALUE
    return (QJSValue)v;
    #else
    return v;
    #endif
}

QJS_API int JS_ToBool(QJSContext *ctx, QJSValueConst val); /* return -1 for JS_EXCEPTION */
QJS_API int JS_ToInt32(QJSContext *ctx, int32_t *pres, QJSValueConst val);
static inline int JS_ToUint32(QJSContext *ctx, uint32_t *pres, QJSValueConst val)
{
    return JS_ToInt32(ctx, (int32_t*)pres, val);
}
QJS_API int JS_ToInt64(QJSContext *ctx, int64_t *pres, QJSValueConst val);
QJS_API int JS_ToIndex(QJSContext *ctx, uint64_t *plen, QJSValueConst val);
QJS_API int JS_ToFloat64(QJSContext *ctx, double *pres, QJSValueConst val);
/* return an exception if 'val' is a Number */
QJS_API int JS_ToBigInt64(QJSContext *ctx, int64_t *pres, QJSValueConst val);
/* same as JS_ToInt64() but allow BigInt */
QJS_API int JS_ToInt64Ext(QJSContext *ctx, int64_t *pres, QJSValueConst val);

QJS_API QJSValue JS_NewStringLen(QJSContext *ctx, const char *str1, size_t len1);
QJS_API QJSValue JS_NewString(QJSContext *ctx, const char *str);
QJS_API QJSValue JS_NewAtomString(QJSContext *ctx, const char *str);
QJS_API QJSValue JS_ToString(QJSContext *ctx, QJSValueConst val);
QJS_API QJSValue JS_ToPropertyKey(QJSContext *ctx, QJSValueConst val);
QJS_API const char *JS_ToCStringLen2(QJSContext *ctx, size_t *plen, QJSValueConst val1, JS_BOOL cesu8);
static inline const char *JS_ToCStringLen(QJSContext *ctx, size_t *plen, QJSValueConst val1)
{
    return JS_ToCStringLen2(ctx, plen, val1, 0);
}
static inline const char *JS_ToCString(QJSContext *ctx, QJSValueConst val1)
{
    return JS_ToCStringLen2(ctx, NULL, val1, 0);
}
QJS_API void JS_FreeCString(QJSContext *ctx, const char *ptr);

QJS_API QJSValue JS_NewObjectProtoClass(QJSContext *ctx, QJSValueConst proto, JSClassID class_id);
QJS_API QJSValue JS_NewObjectClass(QJSContext *ctx, int class_id);
QJS_API QJSValue JS_NewObjectProto(QJSContext *ctx, QJSValueConst proto);
QJS_API QJSValue JS_NewObject(QJSContext *ctx);

QJS_API JS_BOOL JS_IsFunction(QJSContext* ctx, QJSValueConst val);
QJS_API JS_BOOL JS_IsConstructor(QJSContext* ctx, QJSValueConst val);
QJS_API JS_BOOL JS_SetConstructorBit(QJSContext *ctx, QJSValueConst func_obj, JS_BOOL val);

QJS_API QJSValue JS_NewArray(QJSContext *ctx);
QJS_API int JS_IsArray(QJSContext *ctx, QJSValueConst val);

QJS_API QJSValue JS_GetPropertyInternal(QJSContext *ctx, QJSValueConst obj,
                               JSAtom prop, QJSValueConst receiver,
                               JS_BOOL throw_ref_error);
static js_force_inline QJSValue JS_GetProperty(QJSContext *ctx, QJSValueConst this_obj,
                                              JSAtom prop)
{
    return JS_GetPropertyInternal(ctx, this_obj, prop, this_obj, 0);
}
QJS_API QJSValue JS_GetPropertyStr(QJSContext *ctx, QJSValueConst this_obj,
                          const char *prop);
QJS_API QJSValue JS_GetPropertyUint32(QJSContext *ctx, QJSValueConst this_obj,
                             uint32_t idx);

QJS_API int JS_SetPropertyInternal(QJSContext *ctx, QJSValueConst this_obj,
                           JSAtom prop, QJSValue val,
                           int flags);
static inline int JS_SetProperty(QJSContext *ctx, QJSValueConst this_obj,
                                 JSAtom prop, QJSValue val)
{
    return JS_SetPropertyInternal(ctx, this_obj, prop, val, JS_PROP_THROW);
}
QJS_API int JS_SetPropertyUint32(QJSContext *ctx, QJSValueConst this_obj,
                         uint32_t idx, QJSValue val);
QJS_API int JS_SetPropertyInt64(QJSContext *ctx, QJSValueConst this_obj,
                        int64_t idx, QJSValue val);
QJS_API int JS_SetPropertyStr(QJSContext *ctx, QJSValueConst this_obj,
                      const char *prop, QJSValue val);
QJS_API int JS_HasProperty(QJSContext *ctx, QJSValueConst this_obj, JSAtom prop);
QJS_API int JS_IsExtensible(QJSContext *ctx, QJSValueConst obj);
QJS_API int JS_PreventExtensions(QJSContext *ctx, QJSValueConst obj);
QJS_API int JS_DeleteProperty(QJSContext *ctx, QJSValueConst obj, JSAtom prop, int flags);
QJS_API int JS_SetPrototype(QJSContext *ctx, QJSValueConst obj, QJSValueConst proto_val);
QJS_API QJSValue JS_GetPrototype(QJSContext *ctx, QJSValueConst val);

#define JS_GPN_STRING_MASK  (1 << 0)
#define JS_GPN_SYMBOL_MASK  (1 << 1)
#define JS_GPN_PRIVATE_MASK (1 << 2)
/* only include the enumerable properties */
#define JS_GPN_ENUM_ONLY    (1 << 4)
/* set theJSPropertyEnum.is_enumerable field */
#define JS_GPN_SET_ENUM     (1 << 5)

QJS_API int JS_GetOwnPropertyNames(QJSContext *ctx, JSPropertyEnum **ptab,
                           uint32_t *plen, QJSValueConst obj, int flags);
QJS_API int JS_GetOwnProperty(QJSContext *ctx, JSPropertyDescriptor *desc,
                      QJSValueConst obj, JSAtom prop);

QJS_API QJSValue JS_Call(QJSContext *ctx, QJSValueConst func_obj, QJSValueConst this_obj,
                int argc, QJSValueConst *argv);
QJS_API QJSValue JS_Invoke(QJSContext *ctx, QJSValueConst this_val, JSAtom atom,
                  int argc, QJSValueConst *argv);
QJS_API QJSValue JS_CallConstructor(QJSContext *ctx, QJSValueConst func_obj,
                           int argc, QJSValueConst *argv);
QJS_API QJSValue JS_CallConstructor2(QJSContext *ctx, QJSValueConst func_obj,
                            QJSValueConst new_target,
                            int argc, QJSValueConst *argv);
QJS_API JS_BOOL JS_DetectModule(const char *input, size_t input_len);
/* 'input' must be zero terminated i.e. input[input_len] = '\0'. */
QJS_API QJSValue JS_Eval(QJSContext *ctx, const char *input, size_t input_len,
                const char *filename, int eval_flags);
/* same as JS_Eval() but with an explicit 'this_obj' parameter */
QJS_API QJSValue JS_EvalThis(QJSContext *ctx, QJSValueConst this_obj,
                    const char *input, size_t input_len,
                    const char *filename, int eval_flags);
QJS_API QJSValue JS_GetGlobalObject(QJSContext *ctx);
QJS_API int JS_IsInstanceOf(QJSContext *ctx, QJSValueConst val, QJSValueConst obj);
QJS_API int JS_DefineProperty(QJSContext *ctx, QJSValueConst this_obj,
                      JSAtom prop, QJSValueConst val,
                      QJSValueConst getter, QJSValueConst setter, int flags);
QJS_API int JS_DefinePropertyValue(QJSContext *ctx, QJSValueConst this_obj,
                           JSAtom prop, QJSValue val, int flags);
QJS_API int JS_DefinePropertyValueUint32(QJSContext *ctx, QJSValueConst this_obj,
                                 uint32_t idx, QJSValue val, int flags);
QJS_API int JS_DefinePropertyValueStr(QJSContext *ctx, QJSValueConst this_obj,
                              const char *prop, QJSValue val, int flags);
QJS_API int JS_DefinePropertyGetSet(QJSContext *ctx, QJSValueConst this_obj,
                            JSAtom prop, QJSValue getter, QJSValue setter,
                            int flags);
QJS_API void JS_SetOpaque(QJSValue obj, void *opaque);
QJS_API void *JS_GetOpaque(QJSValueConst obj, JSClassID class_id);
QJS_API void *JS_GetOpaque2(QJSContext *ctx, QJSValueConst obj, JSClassID class_id);

/* 'buf' must be zero terminated i.e. buf[buf_len] = '\0'. */
QJS_API QJSValue JS_ParseJSON(QJSContext *ctx, const char *buf, size_t buf_len,
                     const char *filename);
#define JS_PARSE_JSON_EXT (1 << 0) /* allow extended JSON */
QJS_API QJSValue JS_ParseJSON2(QJSContext *ctx, const char *buf, size_t buf_len,
                      const char *filename, int flags);
QJS_API QJSValue JS_JSONStringify(QJSContext *ctx, QJSValueConst obj,
                         QJSValueConst replacer, QJSValueConst space0);

typedef void JSFreeArrayBufferDataFunc(QJSRuntime *rt, void *opaque, void *ptr);
QJS_API QJSValue JS_NewArrayBuffer(QJSContext *ctx, uint8_t *buf, size_t len,
                          JSFreeArrayBufferDataFunc *free_func, void *opaque,
                          JS_BOOL is_shared);
QJS_API QJSValue JS_NewArrayBufferCopy(QJSContext *ctx, const uint8_t *buf, size_t len);
QJS_API void JS_DetachArrayBuffer(QJSContext *ctx, QJSValueConst obj);
QJS_API uint8_t *JS_GetArrayBuffer(QJSContext *ctx, size_t *psize, QJSValueConst obj);
QJS_API QJSValue JS_GetTypedArrayBuffer(QJSContext *ctx, QJSValueConst obj,
                               size_t *pbyte_offset,
                               size_t *pbyte_length,
                               size_t *pbytes_per_element);
typedef struct {
    void *(*sab_alloc)(void *opaque, size_t size);
    void (*sab_free)(void *opaque, void *ptr);
    void (*sab_dup)(void *opaque, void *ptr);
    void *sab_opaque;
} JSSharedArrayBufferFunctions;
QJS_API void JS_SetSharedArrayBufferFunctions(QJSRuntime *rt,
                                      const JSSharedArrayBufferFunctions *sf);

QJS_API QJSValue JS_NewPromiseCapability(QJSContext *ctx, QJSValue *resolving_funcs);

/* is_handled = TRUE means that the rejection is handled */
typedef void JSHostPromiseRejectionTracker(QJSContext *ctx, QJSValueConst promise,
                                           QJSValueConst reason,
                                           JS_BOOL is_handled, void *opaque);
QJS_API void JS_SetHostPromiseRejectionTracker(QJSRuntime *rt, JSHostPromiseRejectionTracker *cb, void *opaque);

/* return != 0 if the JS code needs to be interrupted */
typedef int JSInterruptHandler(QJSRuntime *rt, void *opaque);
QJS_API void JS_SetInterruptHandler(QJSRuntime *rt, JSInterruptHandler *cb, void *opaque);
/* if can_block is TRUE, Atomics.wait() can be used */
QJS_API void JS_SetCanBlock(QJSRuntime *rt, JS_BOOL can_block);
/* set the [IsHTMLDDA] internal slot */
QJS_API void JS_SetIsHTMLDDA(QJSContext *ctx, QJSValueConst obj);

typedef struct JSModuleDef JSModuleDef;

/* return the module specifier (allocated with js_malloc()) or NULL if
   exception */
typedef char *JSModuleNormalizeFunc(QJSContext *ctx,
                                    const char *module_base_name,
                                    const char *module_name, void *opaque);
typedef JSModuleDef *JSModuleLoaderFunc(QJSContext *ctx,
                                        const char *module_name, void *opaque);

/* module_normalize = NULL is allowed and invokes the default module
   filename normalizer */
QJS_API void JS_SetModuleLoaderFunc(QJSRuntime *rt,
                            JSModuleNormalizeFunc *module_normalize,
                            JSModuleLoaderFunc *module_loader, void *opaque);
/* return the import.meta object of a module */
QJS_API QJSValue JS_GetImportMeta(QJSContext *ctx, JSModuleDef *m);
QJS_API JSAtom JS_GetModuleName(QJSContext *ctx, JSModuleDef *m);

/* JS Job support */

typedef QJSValue JSJobFunc(QJSContext *ctx, int argc, QJSValueConst *argv);
QJS_API int JS_EnqueueJob(QJSContext *ctx, JSJobFunc *job_func, int argc, QJSValueConst *argv);

QJS_API JS_BOOL JS_IsJobPending(QJSRuntime *rt);
QJS_API int JS_ExecutePendingJob(QJSRuntime *rt, QJSContext **pctx);

/* Object Writer/Reader (currently only used to handle precompiled code) */
#define JS_WRITE_OBJ_BYTECODE  (1 << 0) /* allow function/module */
#define JS_WRITE_OBJ_BSWAP     (1 << 1) /* byte swapped output */
#define JS_WRITE_OBJ_SAB       (1 << 2) /* allow SharedArrayBuffer */
#define JS_WRITE_OBJ_REFERENCE (1 << 3) /* allow object references to
                                           encode arbitrary object
                                           graph */
QJS_API uint8_t *JS_WriteObject(QJSContext *ctx, size_t *psize, QJSValueConst obj,
                        int flags);
QJS_API uint8_t *JS_WriteObject2(QJSContext *ctx, size_t *psize, QJSValueConst obj,
                         int flags, uint8_t ***psab_tab, size_t *psab_tab_len);

#define JS_READ_OBJ_BYTECODE  (1 << 0) /* allow function/module */
#define JS_READ_OBJ_ROM_DATA  (1 << 1) /* avoid duplicating 'buf' data */
#define JS_READ_OBJ_SAB       (1 << 2) /* allow SharedArrayBuffer */
#define JS_READ_OBJ_REFERENCE (1 << 3) /* allow object references */
QJS_API QJSValue JS_ReadObject(QJSContext *ctx, const uint8_t *buf, size_t buf_len,
                      int flags);
/* instantiate and evaluate a bytecode function. Only used when
   reading a script or module with JS_ReadObject() */
QJS_API QJSValue JS_EvalFunction(QJSContext *ctx, QJSValue fun_obj);
/* load the dependencies of the module 'obj'. Useful when JS_ReadObject()
   returns a module. */
QJS_API int JS_ResolveModule(QJSContext *ctx, QJSValueConst obj);

/* only exported for os.Worker() */
QJS_API JSAtom JS_GetScriptOrModuleName(QJSContext *ctx, int n_stack_levels);
/* only exported for os.Worker() */
QJS_API JSModuleDef *JS_RunModule(QJSContext *ctx, const char *basename,
                          const char *filename);

/* C function definition */
typedef enum JSCFunctionEnum {  /* XXX: should rename for namespace isolation */
    JS_CFUNC_generic,
    JS_CFUNC_generic_magic,
    JS_CFUNC_constructor,
    JS_CFUNC_constructor_magic,
    JS_CFUNC_constructor_or_func,
    JS_CFUNC_constructor_or_func_magic,
    JS_CFUNC_f_f,
    JS_CFUNC_f_f_f,
    JS_CFUNC_getter,
    JS_CFUNC_setter,
    JS_CFUNC_getter_magic,
    JS_CFUNC_setter_magic,
    JS_CFUNC_iterator_next,
} JSCFunctionEnum;

typedef union JSCFunctionType {
    JSCFunction *generic;
    QJSValue (*generic_magic)(QJSContext *ctx, QJSValueConst this_val, int argc, QJSValueConst *argv, int magic);
    JSCFunction *constructor;
    QJSValue (*constructor_magic)(QJSContext *ctx, QJSValueConst new_target, int argc, QJSValueConst *argv, int magic);
    JSCFunction *constructor_or_func;
    double (*f_f)(double);
    double (*f_f_f)(double, double);
    QJSValue (*getter)(QJSContext *ctx, QJSValueConst this_val);
    QJSValue (*setter)(QJSContext *ctx, QJSValueConst this_val, QJSValueConst val);
    QJSValue (*getter_magic)(QJSContext *ctx, QJSValueConst this_val, int magic);
    QJSValue (*setter_magic)(QJSContext *ctx, QJSValueConst this_val, QJSValueConst val, int magic);
    QJSValue (*iterator_next)(QJSContext *ctx, QJSValueConst this_val,
                             int argc, QJSValueConst *argv, int *pdone, int magic);
} JSCFunctionType;

QJS_API QJSValue JS_NewCFunction2(QJSContext *ctx, JSCFunction *func,
                         const char *name,
                         int length, JSCFunctionEnum cproto, int magic);
QJS_API QJSValue JS_NewCFunctionData(QJSContext *ctx, JSCFunctionData *func,
                            int length, int magic, int data_len,
                            QJSValueConst *data);

static inline QJSValue JS_NewCFunction(QJSContext *ctx, JSCFunction *func, const char *name,
                                      int length)
{
    return JS_NewCFunction2(ctx, func, name, length, JS_CFUNC_generic, 0);
}

static inline QJSValue JS_NewCFunctionMagic(QJSContext *ctx, JSCFunctionMagic *func,
                                           const char *name,
                                           int length, JSCFunctionEnum cproto, int magic)
{
    // XXX: suppress bad function cast.
    return JS_NewCFunction2(ctx, (JSCFunction *)(void*)func, name, length, cproto, magic);
}
QJS_API void JS_SetConstructor(QJSContext *ctx, QJSValueConst func_obj,
                       QJSValueConst proto);

/* C property definition */

typedef struct JSCFunctionListEntry {
    const char *name;
    uint8_t prop_flags;
    uint8_t def_type;
    int16_t magic;
    union {
        struct {
            uint8_t length; /* XXX: should move outside union */
            uint8_t cproto; /* XXX: should move outside union */
            JSCFunctionType cfunc;
        } func;
        struct {
            JSCFunctionType get;
            JSCFunctionType set;
        } getset;
        struct {
            const char *name;
            int base;
        } alias;
        struct {
            const struct JSCFunctionListEntry *tab;
            int len;
        } prop_list;
        const char *str;
        int32_t i32;
        int64_t i64;
        double f64;
    } u;
} JSCFunctionListEntry;

#define JS_DEF_CFUNC          0
#define JS_DEF_CGETSET        1
#define JS_DEF_CGETSET_MAGIC  2
#define JS_DEF_PROP_STRING    3
#define JS_DEF_PROP_INT32     4
#define JS_DEF_PROP_INT64     5
#define JS_DEF_PROP_DOUBLE    6
#define JS_DEF_PROP_UNDEFINED 7
#define JS_DEF_OBJECT         8
#define JS_DEF_ALIAS          9

/* Note: c++ does not like nested designators */
#define JS_CFUNC_DEF(name, length, func1) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_CFUNC, 0, .u = { .func = { length, JS_CFUNC_generic, { .generic = func1 } } } }
#define JS_CFUNC_MAGIC_DEF(name, length, func1, magic) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_CFUNC, magic, .u = { .func = { length, JS_CFUNC_generic_magic, { .generic_magic = func1 } } } }
#define JS_CFUNC_SPECIAL_DEF(name, length, cproto, func1) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_CFUNC, 0, .u = { .func = { length, JS_CFUNC_ ## cproto, { .cproto = func1 } } } }
#define JS_ITERATOR_NEXT_DEF(name, length, func1, magic) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_CFUNC, magic, .u = { .func = { length, JS_CFUNC_iterator_next, { .iterator_next = func1 } } } }
#define JS_CGETSET_DEF(name, fgetter, fsetter) { name, JS_PROP_CONFIGURABLE, JS_DEF_CGETSET, 0, .u = { .getset = { .get = { .getter = fgetter }, .set = { .setter = fsetter } } } }
#define JS_CGETSET_MAGIC_DEF(name, fgetter, fsetter, magic) { name, JS_PROP_CONFIGURABLE, JS_DEF_CGETSET_MAGIC, magic, .u = { .getset = { .get = { .getter_magic = fgetter }, .set = { .setter_magic = fsetter } } } }
#define JS_PROP_STRING_DEF(name, cstr, prop_flags) { name, prop_flags, JS_DEF_PROP_STRING, 0, .u = { .str = cstr } }
#define JS_PROP_INT32_DEF(name, val, prop_flags) { name, prop_flags, JS_DEF_PROP_INT32, 0, .u = { .i32 = val } }
#define JS_PROP_INT64_DEF(name, val, prop_flags) { name, prop_flags, JS_DEF_PROP_INT64, 0, .u = { .i64 = val } }
#define JS_PROP_DOUBLE_DEF(name, val, prop_flags) { name, prop_flags, JS_DEF_PROP_DOUBLE, 0, .u = { .f64 = val } }
#define JS_PROP_UNDEFINED_DEF(name, prop_flags) { name, prop_flags, JS_DEF_PROP_UNDEFINED, 0, .u = { .i32 = 0 } }
#define JS_OBJECT_DEF(name, tab, len, prop_flags) { name, prop_flags, JS_DEF_OBJECT, 0, .u = { .prop_list = { tab, len } } }
#define JS_ALIAS_DEF(name, from) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_ALIAS, 0, .u = { .alias = { from, -1 } } }
#define JS_ALIAS_BASE_DEF(name, from, base) { name, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE, JS_DEF_ALIAS, 0, .u = { .alias = { from, base } } }

QJS_API void JS_SetPropertyFunctionList(QJSContext *ctx, QJSValueConst obj,
                                const JSCFunctionListEntry *tab,
                                int len);

/* C module definition */

typedef int JSModuleInitFunc(QJSContext *ctx, JSModuleDef *m);

QJS_API JSModuleDef *JS_NewCModule(QJSContext *ctx, const char *name_str,
                           JSModuleInitFunc *func);
/* can only be called before the module is instantiated */
QJS_API int JS_AddModuleExport(QJSContext *ctx, JSModuleDef *m, const char *name_str);
QJS_API int JS_AddModuleExportList(QJSContext *ctx, JSModuleDef *m,
                           const JSCFunctionListEntry *tab, int len);
/* can only be called after the module is instantiated */
QJS_API int JS_SetModuleExport(QJSContext *ctx, JSModuleDef *m, const char *export_name,
                       QJSValue val);
QJS_API int JS_SetModuleExportList(QJSContext *ctx, JSModuleDef *m,
                           const JSCFunctionListEntry *tab, int len);

#undef js_unlikely
#undef js_force_inline

//
// NOTE(dpriver): This was not originally in quickjs
// I added it so I could implement logging
//
// returns -1 on error, without setting exception.
QJS_API 
int
JS_get_caller_location(QJSContext* ctx, const char** filename, const char** funcname, int* line_num);

//
// NOTE(dpriver): This not being exposed was super annoying.
QJS_API 
QJSValue
JS_ArrayPush(QJSContext *ctx, QJSValueConst this_val, int argc, QJSValueConst *argv);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* QUICKJS_H */
