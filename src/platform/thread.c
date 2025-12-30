/**
 * @file thread.c
 * @brief Threading utilities - platform dispatch
 */

#include <bavarian3d/platform.h>
#include <bavarian3d/types.h>

/* Threading types are opaque - defined in platform headers */
typedef struct Thread Thread;
typedef struct Mutex Mutex;
typedef struct CondVar CondVar;
typedef struct Semaphore Semaphore;

typedef void (*ThreadFunc)(void* arg);

/* Platform-specific threading functions */
#if defined(BAV3D_PLATFORM_WINDOWS)
Thread* win32_thread_create(ThreadFunc func, void* arg);
void win32_thread_join(Thread* thread);
void win32_thread_detach(Thread* thread);
Mutex* win32_mutex_create(void);
void win32_mutex_destroy(Mutex* mutex);
void win32_mutex_lock(Mutex* mutex);
void win32_mutex_unlock(Mutex* mutex);
b8 win32_mutex_try_lock(Mutex* mutex);
    #define PLATFORM_THREAD_IMPL(fn) win32_##fn
#elif defined(BAV3D_PLATFORM_MACOS)
    #define PLATFORM_THREAD_IMPL(fn) macos_##fn
#elif defined(BAV3D_PLATFORM_LINUX)
    #define PLATFORM_THREAD_IMPL(fn) linux_##fn
#endif

/**
 * Create and start a new thread.
 */
Thread* thread_create(ThreadFunc func, void* arg)
{
    return PLATFORM_THREAD_IMPL(thread_create)(func, arg);
}

/**
 * Wait for thread to finish and clean up.
 */
void thread_join(Thread* thread)
{
    PLATFORM_THREAD_IMPL(thread_join)(thread);
}

/**
 * Detach thread (will clean up automatically when done).
 */
void thread_detach(Thread* thread)
{
    PLATFORM_THREAD_IMPL(thread_detach)(thread);
}

/**
 * Create a mutex.
 */
Mutex* mutex_create(void)
{
    return PLATFORM_THREAD_IMPL(mutex_create)();
}

/**
 * Destroy a mutex.
 */
void mutex_destroy(Mutex* mutex)
{
    PLATFORM_THREAD_IMPL(mutex_destroy)(mutex);
}

/**
 * Lock a mutex (blocking).
 */
void mutex_lock(Mutex* mutex)
{
    PLATFORM_THREAD_IMPL(mutex_lock)(mutex);
}

/**
 * Unlock a mutex.
 */
void mutex_unlock(Mutex* mutex)
{
    PLATFORM_THREAD_IMPL(mutex_unlock)(mutex);
}

/**
 * Try to lock a mutex (non-blocking).
 * @return true if lock acquired, false if already locked
 */
b8 mutex_try_lock(Mutex* mutex)
{
    return PLATFORM_THREAD_IMPL(mutex_try_lock)(mutex);
}
