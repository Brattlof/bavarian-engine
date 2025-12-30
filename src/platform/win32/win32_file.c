/**
 * @file win32_file.c
 * @brief Windows file I/O implementation
 */

#include <bavarian3d/memory.h>
#include <bavarian3d/platform.h>
#include <bavarian3d/types.h>

#ifdef BAV3D_PLATFORM_WINDOWS

    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

Result win32_file_read_entire(const char* path, Allocator* alloc, void** data, usize* size)
{
    /* Convert path to wide string */
    int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    wchar_t* wide_path = (wchar_t*)mem_alloc(NULL, len * sizeof(wchar_t), 2);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, len);

    HANDLE file = CreateFileW(wide_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL);

    mem_free(NULL, wide_path, len * sizeof(wchar_t));

    if (file == INVALID_HANDLE_VALUE)
    {
        return RESULT_ERROR_NOT_FOUND;
    }

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size))
    {
        CloseHandle(file);
        return RESULT_ERROR_IO;
    }

    void* buffer = mem_alloc(alloc, (usize)file_size.QuadPart, 1);
    if (!buffer)
    {
        CloseHandle(file);
        return RESULT_ERROR_OUT_OF_MEMORY;
    }

    DWORD bytes_read;
    if (!ReadFile(file, buffer, (DWORD)file_size.QuadPart, &bytes_read, NULL))
    {
        mem_free(alloc, buffer, (usize)file_size.QuadPart);
        CloseHandle(file);
        return RESULT_ERROR_IO;
    }

    CloseHandle(file);

    *data = buffer;
    *size = (usize)bytes_read;
    return RESULT_OK;
}

Result win32_file_write_entire(const char* path, const void* data, usize size)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    wchar_t* wide_path = (wchar_t*)mem_alloc(NULL, len * sizeof(wchar_t), 2);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, len);

    HANDLE file =
        CreateFileW(wide_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    mem_free(NULL, wide_path, len * sizeof(wchar_t));

    if (file == INVALID_HANDLE_VALUE)
    {
        return RESULT_ERROR_IO;
    }

    DWORD bytes_written;
    if (!WriteFile(file, data, (DWORD)size, &bytes_written, NULL))
    {
        CloseHandle(file);
        return RESULT_ERROR_IO;
    }

    CloseHandle(file);
    return RESULT_OK;
}

b8 win32_file_exists(const char* path)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    wchar_t* wide_path = (wchar_t*)mem_alloc(NULL, len * sizeof(wchar_t), 2);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, len);

    DWORD attrib = GetFileAttributesW(wide_path);
    mem_free(NULL, wide_path, len * sizeof(wchar_t));

    return (attrib != INVALID_FILE_ATTRIBUTES);
}

Result win32_file_delete(const char* path)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    wchar_t* wide_path = (wchar_t*)mem_alloc(NULL, len * sizeof(wchar_t), 2);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, len);

    BOOL result = DeleteFileW(wide_path);
    mem_free(NULL, wide_path, len * sizeof(wchar_t));

    return result ? RESULT_OK : RESULT_ERROR_IO;
}

Result win32_file_get_size(const char* path, usize* size)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    wchar_t* wide_path = (wchar_t*)mem_alloc(NULL, len * sizeof(wchar_t), 2);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wide_path, len);

    WIN32_FILE_ATTRIBUTE_DATA data;
    BOOL result = GetFileAttributesExW(wide_path, GetFileExInfoStandard, &data);
    mem_free(NULL, wide_path, len * sizeof(wchar_t));

    if (!result)
    {
        return RESULT_ERROR_NOT_FOUND;
    }

    LARGE_INTEGER li;
    li.HighPart = data.nFileSizeHigh;
    li.LowPart = data.nFileSizeLow;
    *size = (usize)li.QuadPart;

    return RESULT_OK;
}

#endif
