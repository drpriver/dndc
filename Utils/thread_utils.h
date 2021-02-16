#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H
#include "common_macros.h"

#if defined(LINUX) || defined(DARWIN)
#include <pthread.h>
#define THREADFUNC(name) Nullable(void*) (name)(Nullable(void*)thread_arg)
typedef THREADFUNC(thread_func);
typedef struct ThreadHandle {
    NullUnspec(pthread_t) thread;
} ThreadHandle;

static
void
create_thread(Nonnull(ThreadHandle*)handle, Nonnull(thread_func*) func, Nullable(void*)thread_arg){
    auto err = pthread_create(&handle->thread, NULL, func, thread_arg);
    unhandled_error_condition(err != 0);
    }

static
void
join_thread(ThreadHandle handle){
    auto err = pthread_join(handle.thread, NULL);
    unhandled_error_condition(err != 0);
    }

#elif defined(WINDOWS)
#define THREADFUNC(name) unsigned long (name)(Nullable(void*)thread_arg)
typedef THREADFUNC(thread_func);

#include "windowsheader.h"
typedef struct ThreadHandle {
    NullUnspec(HANDLE) thread;
} ThreadHandle;

static
void
create_thread(Nonnull(ThreadHandle*)handle, Nonnull(thread_func*) func, Nullable(void*)thread_arg){
    handle->thread = CreateThread(NULL, 0, func, thread_arg, 0, NULL);
    unhandled_error_condition(handle->thread == NULL);
    }

static
void
join_thread(ThreadHandle handle){
    DWORD result = WaitForSingleObject(handle.thread, INFINITE);
    unhandled_error_condition(result != WAIT_OBJECT_0);
    }

#else
#error "Unhandled threading platform."
#endif


#endif
