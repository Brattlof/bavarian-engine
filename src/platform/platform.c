/**
 * @file platform.c
 * @brief Platform initialization and information
 */

#include <bavarian3d/platform.h>

#include <stdio.h>

static b8 g_platform_initialized = false;
static char g_platform_info[256];

Result platform_init(void)
{
    if (g_platform_initialized)
    {
        return RESULT_OK;
    }

    snprintf(g_platform_info, sizeof(g_platform_info),
             "Bavarian3D Engine | Platform: %s | Arch: %s | Compiler: %s", BAV3D_PLATFORM_NAME,
             BAV3D_ARCH_NAME, BAV3D_COMPILER_NAME);

    g_platform_initialized = true;
    return RESULT_OK;
}

void platform_shutdown(void)
{
    g_platform_initialized = false;
}

const char* platform_get_info(void)
{
    return g_platform_info;
}
