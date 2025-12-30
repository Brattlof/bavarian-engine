/**
 * @file win32_time.c
 * @brief Windows timing implementation
 *
 * High-resolution timing via QueryPerformanceCounter. This is the right
 * way to do timing on Windows - don't use GetTickCount or timeGetTime.
 */

#include <bavarian3d/platform.h>
#include <bavarian3d/types.h>

#ifdef BAV3D_PLATFORM_WINDOWS

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

static LARGE_INTEGER g_frequency = {0};
static b8 g_initialized = false;

static void ensure_initialized(void)
{
    if (!g_initialized)
    {
        QueryPerformanceFrequency(&g_frequency);
        g_initialized = true;
    }
}

u64 win32_time_get_ticks(void)
{
    ensure_initialized();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (u64)counter.QuadPart;
}

u64 win32_time_get_frequency(void)
{
    ensure_initialized();
    return (u64)g_frequency.QuadPart;
}

void win32_time_sleep_ms(u32 ms)
{
    Sleep(ms);
}

#endif
