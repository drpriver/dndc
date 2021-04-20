#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H
#include "common_macros.h"

//
// Implements a very basic portability layer to spawn a worker thread with an
// argument and then to wait for that thread to return.  Works on windows and
// posix.
//
// I am doing all the documentation up here in comments as we need the
// definition of the opaque ThreadHandle to declare the functions that take one
// and we need the actual return type of the worker function and we don't have
// that until we're in the platform specific part.
// So here it is.
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
// #define THREADFUNC(name) ret_type_differs (name)(Nullable(void*)thread_arg)

//
// This structure is the handle to the thread. You will pass an unitialized one
// into the create_thread and it will store whatever it needs to within.  You
// can then later give it to join_thread to wait for the thread to finish.
//
// typedef struct ThreadHandle {
//   ... opaque contents ...
// } ThreadHandle;

//
// Creates and launches that thread, with the given thread func and argument.
// Initializes the given handle with the info that identifies that thread.
// thread_arg should be what the thread_func expects.
//
// static
// void
// create_thread(Nonnull(ThreadHandle*)handle, Nonnull(thread_func*) func, Nullable(void*)thread_arg);

//
// Waits for the corresponding thread to finish.
// This is a synchronization event between the joiner and the joinee.
// (I know that is true for pthreads and assume it is true for Win32 as well.)
//
// static
// void
// join_thread(ThreadHandle handle);


#if defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#define THREADFUNC(name) Nullable(void*) (name)(Nullable(void*)thread_arg)
typedef THREADFUNC(thread_func);
PushDiagnostic();
SuppressNullabilityComplete();
typedef struct ThreadHandle {
    pthread_t thread;
} ThreadHandle;
PopDiagnostic();

static
void
create_thread(Nonnull(ThreadHandle*)handle, Nonnull(thread_func*) func, Nullable(void*)thread_arg){
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
#define THREADFUNC(name) unsigned long (name)(Nullable(void*)thread_arg)
typedef THREADFUNC(thread_func);

#include "windowsheader.h"
PushDiagnostic();
SuppressNullabilityComplete();
typedef struct ThreadHandle {
    HANDLE thread;
} ThreadHandle;
PopDiagnostic();

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

#elif defined(WASM)
#define THREADFUNC(name) unsigned long (name)(Nullable(void*)thread_arg)
typedef THREADFUNC(thread_func);
typedef struct ThreadHandle {
    int unused;
} ThreadHandle;

static
void
create_thread(Nonnull(ThreadHandle*)handle, Nonnull(thread_func*) func, Nullable(void*)thread_arg){
    (void)handle;
    (void)func;
    }

static
void
join_thread(ThreadHandle handle){
    (void)handle;
    }
#else
#error "Unhandled threading platform."
#endif


#endif
