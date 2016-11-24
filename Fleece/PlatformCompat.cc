#include "PlatformCompat.hh"
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <wchar.h>
#include <Windows.h>

#define u8_to_wstr(in, out_name, bad_val) wchar_t out_name[256]; \
 if(MultiByteToWideChar(CP_UTF8, 0, in, strnlen_s(in, 255) + 1, out_name, 256) == 0) errno = EINVAL; return bad_val

int mkdir_u8(const char *path, int mode) {
    u8_to_wstr(path, wname, -1);

    return _wmkdir(wname);
}

int stat_u8(const char* const filename, struct stat * const s)
{
    u8_to_wstr(filename, wname, -1);
    return ::_wstat64i32(wname, (struct _stat64i32 *)s);
}

int rmdir_u8(const char* const path)
{
    u8_to_wstr(path, wname, -1);
    return ::_wrmdir(wname);
}

int rename_u8(const char* const oldPath, const char* const newPath)
{
    u8_to_wstr(oldPath, woldPath, -1);
    u8_to_wstr(newPath, wnewPath, -1);
    return ::_wrename(woldPath, wnewPath);
}

int unlink_u8(const char* const filename)
{
    u8_to_wstr(filename, wname, -1);
    return ::_wunlink(wname);
}

int chmod_u8(const char* const filename, int mode)
{
    u8_to_wstr(filename, wname, -1);
    return ::_wchmod(wname, mode);
}

FILE* fopen_u8(const char* const path, const char* const mode)
{
    u8_to_wstr(path, wname, nullptr);
    u8_to_wstr(mode, wmode, nullptr);
    return ::_wfopen(wname, wmode);
}