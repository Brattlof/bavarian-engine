/**
 * @file win32_thread.c
 * @brief Windows threading implementation
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/platform.h>
#include <bavarian3d/types.h>

#ifdef BAV3D_PLATFORM_WINDOWS

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

typedef void (*ThreadFunc)(void* arg);

typedef struct Thread
{
    HANDLE handle;
    ThreadFunc func;
    void* arg;
} Thread;

typedef struct Mutex
{
    CRITICAL_SECTION cs;
} Mutex;

static DWORD WINAPI thread_entry(LPVOID param)
{
    Thread* thread = (Thread*)param;
    thread->func(thread->arg);
    return 0;
}

Thread* win32_thread_create(ThreadFunc func, void* arg)
{
    Thread* thread = MEM_ALLOC_TYPE_ZERO(NULL, Thread);
    if (!thread)
        return NULL;

    thread->func = func;
    thread->arg = arg;

    thread->handle = CreateThread(NULL, 0, thread_entry, thread, 0, NULL);
    if (!thread->handle)
    {
        MEM_FREE_TYPE(NULL, thread, Thread);
        return NULL;
    }

    return thread;
}

void win32_thread_join(Thread* thread)
{
    if (!thread)
        return;
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
    MEM_FREE_TYPE(NULL, thread, Thread);
}

void win32_thread_detach(Thread* thread)
{
    if (!thread)
        return;
    CloseHandle(thread->handle);
    MEM_FREE_TYPE(NULL, thread, Thread);
}

Mutex* win32_mutex_create(void)
{
    Mutex* mutex = MEM_ALLOC_TYPE_ZERO(NULL, Mutex);
    if (!mutex)
        return NULL;
    InitializeCriticalSection(&mutex->cs);
    return mutex;
}

void win32_mutex_destroy(Mutex* mutex)
{
    if (!mutex)
        return;
    DeleteCriticalSection(&mutex->cs);
    MEM_FREE_TYPE(NULL, mutex, Mutex);
}

void win32_mutex_lock(Mutex* mutex)
{
    if (mutex)
        EnterCriticalSection(&mutex->cs);
}

void win32_mutex_unlock(Mutex* mutex)
{
    if (mutex)
        LeaveCriticalSection(&mutex->cs);
}

b8 win32_mutex_try_lock(Mutex* mutex)
{
    if (!mutex)
        return false;
    return TryEnterCriticalSection(&mutex->cs) != 0;
}

#endif
