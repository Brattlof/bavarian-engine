/**
 * @file time.c
 * @brief Timing utilities - platform dispatch
 */

#include <bavarian3d/platform.h>
#include <bavarian3d/types.h>

/* Platform-specific time functions */
#if defined(BAV3D_PLATFORM_WINDOWS)
u64 win32_time_get_ticks(void);
u64 win32_time_get_frequency(void);
void win32_time_sleep_ms(u32 ms);
    #define PLATFORM_TIME_IMPL(fn) win32_time_##fn
#elif defined(BAV3D_PLATFORM_MACOS)
    #define PLATFORM_TIME_IMPL(fn) macos_time_##fn
#elif defined(BAV3D_PLATFORM_LINUX)
    #define PLATFORM_TIME_IMPL(fn) linux_time_##fn
#endif

/**
 * Get high-resolution timer ticks.
 */
u64 time_get_ticks(void)
{
    return PLATFORM_TIME_IMPL(get_ticks)();
}

/**
 * Get timer frequency (ticks per second).
 */
u64 time_get_frequency(void)
{
    return PLATFORM_TIME_IMPL(get_frequency)();
}

/**
 * Get elapsed time in seconds.
 */
f64 time_get_seconds(void)
{
    return (f64)time_get_ticks() / (f64)time_get_frequency();
}

/**
 * Calculate elapsed seconds between two tick values.
 */
f64 time_elapsed_seconds(u64 start, u64 end)
{
    return (f64)(end - start) / (f64)time_get_frequency();
}

/**
 * Sleep for specified milliseconds.
 */
void time_sleep_ms(u32 ms)
{
    PLATFORM_TIME_IMPL(sleep_ms)(ms);
}
