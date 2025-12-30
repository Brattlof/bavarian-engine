/**
 * @file file.c
 * @brief File I/O utilities - platform dispatch
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/platform.h>
#include <bavarian3d/types.h>

/* Platform-specific file functions */
#if defined(BAV3D_PLATFORM_WINDOWS)
Result win32_file_read_entire(const char* path, Allocator* alloc, void** data, usize* size);
Result win32_file_write_entire(const char* path, const void* data, usize size);
b8 win32_file_exists(const char* path);
Result win32_file_delete(const char* path);
Result win32_file_get_size(const char* path, usize* size);
    #define PLATFORM_FILE_IMPL(fn) win32_file_##fn
#elif defined(BAV3D_PLATFORM_MACOS)
    #define PLATFORM_FILE_IMPL(fn) macos_file_##fn
#elif defined(BAV3D_PLATFORM_LINUX)
    #define PLATFORM_FILE_IMPL(fn) linux_file_##fn
#endif

/**
 * Read entire file into memory.
 *
 * @param path File path (UTF-8)
 * @param alloc Allocator for data (NULL for system allocator)
 * @param data Output pointer to file contents
 * @param size Output size of file in bytes
 * @return RESULT_OK on success
 */
Result file_read_entire(const char* path, Allocator* alloc, void** data, usize* size)
{
    return PLATFORM_FILE_IMPL(read_entire)(path, alloc, data, size);
}

/**
 * Write entire buffer to file.
 *
 * @param path File path (UTF-8)
 * @param data Data to write
 * @param size Size of data in bytes
 * @return RESULT_OK on success
 */
Result file_write_entire(const char* path, const void* data, usize size)
{
    return PLATFORM_FILE_IMPL(write_entire)(path, data, size);
}

/**
 * Check if file exists.
 */
b8 file_exists(const char* path)
{
    return PLATFORM_FILE_IMPL(exists)(path);
}

/**
 * Delete a file.
 */
Result file_delete(const char* path)
{
    return PLATFORM_FILE_IMPL(delete)(path);
}

/**
 * Get file size without reading the file.
 */
Result file_get_size(const char* path, usize* size)
{
    return PLATFORM_FILE_IMPL(get_size)(path, size);
}
