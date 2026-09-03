#include "utils/misc.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
    #include <direct.h>
    #define GETCWD _getcwd
#else
    #include <unistd.h>
    #define GETCWD getcwd
#endif

namespace Engine::Utils {

namespace {
    std::mutex cout_mutex;
}

std::vector<std::string_view> split(std::string_view s, std::string_view delimiter) noexcept {
    std::vector<std::string_view> tokens;
    if (s.empty()) return tokens;

    std::size_t start = 0;
    while (true) {
        const std::size_t end = s.find(delimiter, start);
        if (end == std::string_view::npos) {
            tokens.emplace_back(s.substr(start));
            break;
        }
        tokens.emplace_back(s.substr(start, end - start));
        start = end + delimiter.size();
    }
    return tokens;
}

void trim(std::string& s) noexcept {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

bool is_whitespace(std::string_view s) noexcept {
    return std::all_of(s.begin(), s.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
}

std::optional<std::string> read_file_to_string(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return std::nullopt;

    const auto size = file.tellg();
    if (size < 0) return std::nullopt;

    std::string content(static_cast<std::size_t>(size), '\0');
    file.seekg(0);
    file.read(content.data(), size);
    return content;
}

std::string get_working_directory() {
    char buff[4096];
    char* cwd = GETCWD(buff, sizeof(buff));
    return cwd ? std::string(cwd) : std::string(".");
}

std::string get_binary_directory(std::string argv0) {
    try {
        std::filesystem::path p(argv0);
        if (p.is_absolute()) {
            return p.parent_path().string() + "/";
        }
        return (std::filesystem::current_path() / p).parent_path().string() + "/";
    } catch (...) {
        return "./";
    }
}

std::ostream& operator<<(std::ostream& os, SyncStreamState state) {
    if (state == IO_LOCK) {
        cout_mutex.lock();
    } else if (state == IO_UNLOCK) {
        cout_mutex.unlock();
    }
    return os;
}

std::string engine_version_info() {
    return "V-Chess 1.0 (dev)";
}

std::string compiler_info() {
    std::stringstream ss;
    ss << "\nCompiler: ";
#if defined(__clang__)
    ss << "Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__;
#elif defined(__GNUC__)
    ss << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
#elif defined(_MSC_VER)
    ss << "MSVC " << _MSC_VER;
#else
    ss << "Unknown";
#endif

#if defined(__linux__)
    ss << " on Linux";
#elif defined(_WIN64)
    ss << " on Windows 64-bit";
#elif defined(__APPLE__)
    ss << " on macOS";
#endif

    ss << "\nSIMD Support: ";
#if defined(__AVX512F__)
    ss << "AVX-512 ";
#endif
#if defined(__AVX2__)
    ss << "AVX2 ";
#endif
#if defined(USE_PEXT)
    ss << "BMI2(PEXT) ";
#endif
#if defined(__ARM_NEON)
    ss << "ARM NEON ";
#endif
    ss << "\n";
    return ss.str();
}

} // namespace Engine::Utils