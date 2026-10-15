#pragma once
#include <filesystem>
#include <stdexcept>
#include <vector>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace app {
// Resolve the running binary, never argv[0] or a searched command name: the
// recording contract hashes this actual executable before accepting a bundle.
inline std::filesystem::path actual_executable() {
#ifdef __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> path(size);
    if (_NSGetExecutablePath(path.data(), &size) != 0)
        throw std::runtime_error("Cannot identify actual host executable");
    return std::filesystem::canonical(path.data());
#elif defined(_WIN32)
    std::vector<wchar_t> path(32768);
    const DWORD size = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
    if (!size || size >= path.size()) throw std::runtime_error("Cannot identify actual host executable");
    return std::filesystem::canonical(std::filesystem::path(path.data(), path.data() + size));
#elif defined(__linux__)
    return std::filesystem::canonical("/proc/self/exe");
#else
#error Unsupported desktop executable identity platform
#endif
}
inline void replace_save_file(const std::filesystem::path &temporary, const std::filesystem::path &destination,
                              std::error_code &error) {
#ifdef _WIN32
    // std::filesystem::rename does not replace an existing file on Windows.
    if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        error = std::error_code(int(GetLastError()), std::system_category());
    else error.clear();
#else
    std::filesystem::rename(temporary, destination, error);
#endif
}
inline void replace_save_file(const std::filesystem::path &temporary, const std::filesystem::path &destination) {
    std::error_code error;
    replace_save_file(temporary, destination, error);
    if (error) throw std::filesystem::filesystem_error("Cannot replace save", temporary, destination, error);
}
} // namespace app
