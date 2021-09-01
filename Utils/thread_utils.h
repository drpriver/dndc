#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H

#if defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#elif defined(_WIN32)
#include "windowsheader.h"
#endif

#include "common_macros.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
PushDiagnostic();
SuppressUnusedFunction();

//
// Implements a very basic portability layer to spawn a worker thread with an
// argument and then to wait for that thread to return.  Works on windows and
// posix.
//
// Example Usage:
//
//      THREADFUNC(worker);
//
//      typedef struct JobData {
//          const char* msg;
//          int whatever;
//      } JobData;
//
//      int main(void){
//          JobData data = {.msg="some very import information"};
//          ThreadHandle handle;
//          create_thread(&handle, worker, &data);
//          // No touchy the data until we join, the other thread
//          // is reading and possibly mutating it and we didn't set up
//          // any synchronization mechanism.
//
//          // ... do some main thread work ...
//
//          join_thread(handle);
//          if(data.whatever){
//             //  ... do whatever based on results of the worker thread ...
//          }
//          return 0;
//      }
//
//      THREADFUNC(worker){
//          JobData* data = thread_arg;
//          data->whatever = puts(data->msg);
//          // always return 0.
//          return 0;
//      }
//

//
// #define THREADFUNC(name) ret_type_differs (name)(Nullable(void*)thread_arg)

//
// This structure is the handle to the thread. You will pass an unitialized one
// into the create_thread and it will store whatever it needs to within.  You
// can then later give it to join_thread to wait for the thread to finish.


typedef struct ThreadHandle ThreadHandle;
#ifdef _WIN32
typedef unsigned long ThreadReturnValue;
#else
typedef Nullable(void*) ThreadReturnValue;
#endif


// Use this macro to portably define the entry point function to the thread.
// Can be used both in declaration context and in definition.  The function
// will have a single argument - thread_arg - which is a pointer to void. You
// decide if it can be NULL or not. As the return type differs between
// platforms, you should not use it for anything and should always return 0.
// Use the thread_arg to communicate a result back to the caller or set up some
// other message passing scheme.
//
//  Example:
//      THREADFUNC(worker);
//
//      THREADFUNC(worker){
//          JobData* data = thread_arg;
//          ... do some work ...
//          return 0;
//      }
//
#define THREADFUNC(name) ThreadReturnValue (name)(Nullable(void*)thread_arg)
typedef THREADFUNC(thread_func);

//
// Creates and launches that thread, with the given thread func and argument.
// Initializes the given handle with the info that identifies that thread.
// thread_arg should be what the thread_func expects.
static void create_thread(ThreadHandle* handle, thread_func* func, Nullable(void*)thread_arg);
//
// Waits for the corresponding thread to finish.
// This is a synchronization event between the joiner and the joinee.
// (I know that is true for pthreads and assume it is true for Win32 as well.)
//
static void join_thread(ThreadHandle);

typedef struct WorkerThread WorkerThread;
static THREADFUNC(worker_thread_main);
// Create a new worker, with the given job func.
static WorkerThread* worker_create(thread_func*, const char* name);
// Shutdown the worker and free the resources associated with it.
static void worker_destroy(WorkerThread* w);
// Submit a job to the worker
static void worker_submit(WorkerThread* w, void* job_data);
static void worker_wait(WorkerThread* w);

#if defined(__linux__) || defined(__APPLE__)

typedef struct ThreadHandle {
    pthread_t thread;
} ThreadHandle;

typedef struct WorkerThread {
    ThreadHandle thrd;
    pthread_cond_t worker_cond;
    pthread_mutex_t mutex;
    const char* name;
    thread_func* job;
    void*_Nullable job_data;
    bool shutdown;
} WorkerThread;


static
THREADFUNC(worker_thread_main){
    pthread_detach(pthread_self());
    WorkerThread* w = thread_arg;
    pthread_setname_np(w->name);
    pthread_mutex_lock(&w->mutex);
    void* (*job)(void*) = w->job;
    for(;;){
        pthread_cond_wait(&w->worker_cond, &w->mutex);
        if(w->shutdown)
            break;
        void* job_data = w->job_data;
        w->job_data = NULL;
        if(job_data)
            job(job_data);
        }
    pthread_mutex_unlock(&w->mutex);
    pthread_mutex_destroy(&w->mutex);
    pthread_cond_destroy(&w->worker_cond);
    free(w);
    return 0;
    }

static
WorkerThread*
worker_create(thread_func* job, const char* name){
    WorkerThread* w = calloc(1, sizeof(*w));
    w->job = job;
    w->name = name;
    pthread_cond_init(&w->worker_cond, NULL);
    pthread_mutex_init(&w->mutex, NULL);
    create_thread(&w->thrd, worker_thread_main, w);
    return w;
    }

static
void
worker_destroy(WorkerThread* w){
    pthread_mutex_lock(&w->mutex);
    w->shutdown = true;
    pthread_mutex_unlock(&w->mutex);
    pthread_cond_signal(&w->worker_cond);
    }

static
void
worker_submit(WorkerThread* w, void* job_data){
    pthread_mutex_lock(&w->mutex);
    w->job_data = job_data;
    pthread_mutex_unlock(&w->mutex);
    pthread_cond_signal(&w->worker_cond);
    }

static
void
worker_wait(WorkerThread* w){
    pthread_mutex_lock(&w->mutex);
    pthread_mutex_unlock(&w->mutex);
    }

static
void
create_thread(ThreadHandle* handle, thread_func* func, Nullable(void*)thread_arg){
    int err = pthread_create(&handle->thread, NULL, func, thread_arg);
    unhandled_error_condition(err != 0);
    }

static
void
join_thread(ThreadHandle handle){
    int err = pthread_join(handle.thread, NULL);
    unhandled_error_condition(err != 0);
    }

#elif defined(_WIN32)

typedef struct ThreadHandle {
    HANDLE thread;
} ThreadHandle;

typedef struct WorkerThread {
    ThreadHandle thrd;
    CONDITION_VARIABLE worker_cond;
    CRITICAL_SECTION mutex;
    thread_func* job;
    void*_Nullable job_data;
    bool shutdown;
} WorkerThread;

static
THREADFUNC(worker_thread_main){
    CloseHandle(w->thrd.thread);
    WorkerThread* w = thread_arg;
    EnterCriticalSection(&w->mutex);
    unsigned long (*job)(void*) = w->job;
    for(;;){
        SleepConditionVariableCS(&w->worker_cond, &w->mutex, INFINITE);
        if(w->shutdown)
            break;
        void* job_data = w->job_data;
        w->job_data = NULL;
        if(job_data)
            job(job_data);
        }
    LeaveCriticalSection(&w->mutex);
    DeleteCriticalSection(&w->mutex);
    // no need to destroy win32 condition variable
    free(w);
    return 0;
    }

static
WorkerThread*
worker_create(thread_func* job){
    WorkerThread* w = calloc(1, sizeof(*w));
    w->job = job;
    InitializeCriticalSection(&w->mutex);
    InitializeConditionVariable(&w->worker_cond);
    create_thread(&w->thrd, worker_thread_main, w);
    return w;
    }

static
void
worker_destroy(WorkerThread* w){
    EnterCriticalSection(&w->mutex);
    w->shutdown = true;
    LeaveCriticalSection(&w->mutex);
    WakeConditionVariable(&w->worker_cond);
    }

static
void
worker_submit(WorkerThread* w, void* job_data){
    EnterCriticalSection(&w->mutex);
    w->job_data = job_data;
    LeaveCriticalSection(&w->mutex);
    WakeConditionVariable(&w->worker_cond);
    }

static
void
worker_wait(WorkerThread* w){
    EnterCriticalSection(&w->mutex);
    LeaveCriticalSection(&w->mutex);
    }

static
void
create_thread(ThreadHandle* handle, thread_func* func, Nullable(void*)thread_arg){
    handle->thread = CreateThread(NULL, 0, func, thread_arg, 0, NULL);
    unhandled_error_condition(handle->thread == NULL);
    }

static
void
join_thread(ThreadHandle handle){
    DWORD result = WaitForSingleObject(handle.thread, INFINITE);
    unhandled_error_condition(result != WAIT_OBJECT_0);
    }

#elif defined(WASM)

typedef struct ThreadHandle {
    int unused;
} ThreadHandle;

static
void
create_thread(ThreadHandle* handle, thread_func* func, void* thread_arg){
    (void)handle;
    (void)func;
    unimplemented();
    }

static
void
join_thread(ThreadHandle handle){
    (void)handle;
    unimplemented();
    }
typedef struct WorkerThread {
    int unused;
    }WorkerThread;
static THREADFUNC(worker_thread_main);
// Create a new worker, with the given job func.
static WorkerThread* worker_create(thread_func* job){
    unimplemented();
    (void)job;
    return NULL;
    }
// Shutdown the worker and free the resources associated with it.
static void worker_destroy(WorkerThread* w){
    (void)w;
    unimplemented();
    }
// Submit a job to the worker
static void worker_submit(WorkerThread* w, void* job_data){
    (void)w, (void)job_data;
    unimplemented();
    }
static void worker_wait(WorkerThread* w){
    (void)w;
    unimplemented();
    }

#else
#error "Unhandled threading platform."
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
PopDiagnostic();

#endif
