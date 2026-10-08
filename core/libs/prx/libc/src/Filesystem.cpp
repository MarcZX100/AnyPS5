#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
int FilesystemError(const std::error_code& error) {
    if (error == std::errc::no_such_file_or_directory) return 2;
    if (error == std::errc::permission_denied) return 13;
    if (error == std::errc::operation_not_permitted) return 1;
    if (error == std::errc::not_a_directory) return 20;
    if (error == std::errc::is_a_directory) return 21;
    if (error == std::errc::directory_not_empty) return 66;
    if (error == std::errc::device_or_resource_busy) return 16;
    if (error == std::errc::read_only_file_system) return 30;
    if (error == std::errc::filename_too_long) return 63;
    if (error == std::errc::too_many_symbolic_link_levels) return 62;
    if (error == std::errc::not_enough_memory) return 12;
    if (error == std::errc::invalid_argument) return 22;
    if (error == std::errc::value_too_large) return 84;
    if (error == std::errc::cross_device_link) return 18;
    if (error == std::errc::file_exists) return 17;
    if (error == std::errc::no_space_on_device) return 28;
    return 5;
}
}

extern "C" int APS5_VABI utime_nid_postfix(const char* path, const void* times) {
    if (!path) { errno = 14; return -1; }
    if (!*path) { errno = 2; return -1; }
    struct GuestUtimeBuffer { std::int64_t actime; std::int64_t modtime; };
    const auto* guestTimes = static_cast<const GuestUtimeBuffer*>(times);
    try {
        const auto resolved = ResolvePath_nid_no_patch(path);
#ifdef _WIN32
        constexpr std::int64_t EpochOffset = 11644473600;
        constexpr std::uint64_t TicksPerSecond = 10000000;
        constexpr std::int64_t MaximumSecond = static_cast<std::int64_t>(std::numeric_limits<std::uint64_t>::max() / TicksPerSecond) - EpochOffset;
        const auto toFileTime = [&](std::int64_t seconds, FILETIME& result) {
            if (seconds < -EpochOffset || seconds > MaximumSecond) return false;
            const auto ticks = static_cast<std::uint64_t>(seconds + EpochOffset) * TicksPerSecond;
            result.dwLowDateTime = static_cast<DWORD>(ticks);
            result.dwHighDateTime = static_cast<DWORD>(ticks >> 32);
            return true;
        };
        FILETIME accessTime{}, writeTime{};
        if (guestTimes && (!toFileTime(guestTimes->actime, accessTime) || !toFileTime(guestTimes->modtime, writeTime))) {
            errno = 84; return -1;
        }
        if (!guestTimes) {
            GetSystemTimeAsFileTime(&accessTime);
            writeTime = accessTime;
        }
        const HANDLE handle = CreateFileW(resolved.c_str(), FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            errno = FilesystemError(std::error_code(static_cast<int>(GetLastError()), std::system_category()));
            return -1;
        }
        const std::unique_ptr<void, decltype(&CloseHandle)> owner(handle, &CloseHandle);
        if (!SetFileTime(handle, nullptr, &accessTime, &writeTime)) {
            errno = FilesystemError(std::error_code(static_cast<int>(GetLastError()), std::system_category()));
            return -1;
        }
#else
        timespec nativeTimes[2]{};
        const timespec* nativeTimesPointer = nullptr;
        if (guestTimes) {
            nativeTimes[0] = {static_cast<time_t>(guestTimes->actime), 0};
            nativeTimes[1] = {static_cast<time_t>(guestTimes->modtime), 0};
            if (static_cast<std::int64_t>(nativeTimes[0].tv_sec) != guestTimes->actime ||
                static_cast<std::int64_t>(nativeTimes[1].tv_sec) != guestTimes->modtime) {
                errno = 84; return -1;
            }
            nativeTimesPointer = nativeTimes;
        }
        if (::utimensat(AT_FDCWD, resolved.c_str(), nativeTimesPointer, 0) != 0) {
            errno = FilesystemError(std::error_code(errno, std::generic_category()));
            return -1;
        }
#endif
        RecordWrittenPath_nid_no_patch(resolved);
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) {
        errno = FilesystemError(error.code());
        return -1;
    }
}

extern "C" int APS5_VABI access_nid_postfix(const char* path, int mode) {
    if (!path) { errno = 14; return -1; }
    if (mode < 0 || (mode & ~7)) { errno = 22; return -1; }
    if (!*path) { errno = 2; return -1; }
    try {
        const auto resolved = ResolvePath_nid_no_patch(path);
#ifdef _WIN32
        const DWORD attributes = GetFileAttributesW(resolved.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            errno = FilesystemError(std::error_code(static_cast<int>(GetLastError()), std::system_category()));
            return -1;
        }
        if (mode == 0) return 0;
        if ((mode & 2) && !(attributes & FILE_ATTRIBUTE_DIRECTORY) && (attributes & FILE_ATTRIBUTE_READONLY)) {
            errno = 13; return -1;
        }
        DWORD size = 0;
        constexpr auto information = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
        GetFileSecurityW(resolved.c_str(), information, nullptr, 0, &size);
        if (!size) { errno = 13; return -1; }
        std::vector<unsigned char> descriptor(size);
        if (!GetFileSecurityW(resolved.c_str(), information, descriptor.data(), size, &size)) {
            errno = 13; return -1;
        }
        struct Token {
            HANDLE value = nullptr;
            ~Token() { if (value) CloseHandle(value); }
        } source, impersonation;
        if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY | TOKEN_DUPLICATE, TRUE, &source.value)) {
            if (GetLastError() != ERROR_NO_TOKEN ||
                !OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &source.value)) {
                errno = 13; return -1;
            }
        }
        if (!DuplicateToken(source.value, SecurityImpersonation, &impersonation.value)) { errno = 13; return -1; }
        DWORD desired = 0;
        if (mode & 4) desired |= FILE_READ_DATA;
        if (mode & 2) desired |= FILE_WRITE_DATA;
        if (mode & 1) desired |= FILE_EXECUTE;
        GENERIC_MAPPING mapping{FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS};
        PRIVILEGE_SET privileges{};
        DWORD privilegeSize = sizeof(privileges), granted = 0;
        BOOL allowed = FALSE;
        if (!AccessCheck(descriptor.data(), impersonation.value, desired, &mapping,
                         &privileges, &privilegeSize, &granted, &allowed) || !allowed) {
            errno = 13; return -1;
        }
        return 0;
#else
        if (::access(resolved.c_str(), mode) == 0) return 0;
        errno = FilesystemError(std::error_code(errno, std::generic_category()));
        return -1;
#endif
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) {
        errno = FilesystemError(error.code()); return -1;
    }
}

extern "C" int APS5_VABI rename_nid_postfix(const char* from, const char* to) {
    if (!from || !to) { errno = 14; return -1; }
    if (!*from || !*to) { errno = 2; return -1; }
    try {
        const auto source = ResolvePath_nid_no_patch(from);
        const auto destination = ResolvePath_nid_no_patch(to);
        std::error_code error;
        std::filesystem::rename(source, destination, error);
        if (error) { errno = FilesystemError(error); return -1; }
        RecordWrittenPath_nid_no_patch(source);
        RecordWrittenPath_nid_no_patch(destination);
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) {
        errno = FilesystemError(error.code());
        return -1;
    }
}

extern "C" int APS5_VABI remove_nid_postfix(const char* path) {
    if (!path) { errno = 14; return -1; }
    if (!*path) { errno = 2; return -1; }
    try {
        const auto resolved = ResolvePath_nid_no_patch(path);
        std::error_code error;
#ifdef _WIN32
        const DWORD attributes = GetFileAttributesW(resolved.c_str());
        bool removed = false;
        if (attributes != INVALID_FILE_ATTRIBUTES) {
            removed = (attributes & FILE_ATTRIBUTE_DIRECTORY) ?
                RemoveDirectoryW(resolved.c_str()) != 0 : DeleteFileW(resolved.c_str()) != 0;
        }
        if (!removed) {
            const DWORD nativeError = GetLastError();
            if (nativeError == ERROR_DIR_NOT_EMPTY) { errno = 66; return -1; }
            error = std::error_code(static_cast<int>(nativeError), std::system_category());
        }
#else
        const bool removed = std::filesystem::remove(resolved, error);
#endif
        if (error || !removed) {
            errno = error ? FilesystemError(error) : 2;
            return -1;
        }
        RecordWrittenPath_nid_no_patch(resolved);
        return 0;
    } catch (const std::bad_alloc&) { errno = 12; return -1; }
      catch (const std::filesystem::filesystem_error& error) {
        errno = FilesystemError(error.code());
        return -1;
    }
}
