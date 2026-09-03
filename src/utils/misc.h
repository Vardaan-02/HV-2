#pragma once

#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Utils {

// Monotonic wall clock for search time management
using TimePoint = int64_t;

[[nodiscard]] inline TimePoint now() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

// Zero-allocation string tokenization helper
[[nodiscard]] std::vector<std::string_view> split(std::string_view s, std::string_view delimiter) noexcept;

// Whitespace removal and validation
void trim(std::string& s) noexcept;
[[nodiscard]] bool is_whitespace(std::string_view s) noexcept;

// File I/O
[[nodiscard]] std::optional<std::string> read_file_to_string(const std::string& path);

// Binary and working directory paths
[[nodiscard]] std::string get_working_directory();
[[nodiscard]] std::string get_binary_directory(std::string argv0);

// Thread-safe console stream synchronization
enum SyncStreamState {
    IO_LOCK,
    IO_UNLOCK
};

std::ostream& operator<<(std::ostream& os, SyncStreamState state);

#define sync_cout std::cout << ::Engine::Utils::IO_LOCK
#define sync_endl std::endl << ::Engine::Utils::IO_UNLOCK

// Fast 64-bit FNV-1a byte hasher
[[nodiscard]] constexpr uint64_t hash_bytes(const char* data, std::size_t size) noexcept {
    uint64_t hash = 14695981039346656037ULL;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
        hash *= 1099511628211ULL;
    }
    return hash;
}

[[nodiscard]] inline uint64_t hash_string(std::string_view sv) noexcept {
    return hash_bytes(sv.data(), sv.size());
}

template<typename T>
inline void hash_combine(std::size_t& seed, const T& v) noexcept {
    std::size_t x;
    if constexpr (std::is_integral_v<T>) {
        x = static_cast<std::size_t>(v);
    } else {
        x = std::hash<T>{}(v);
    }
    seed ^= x + 0x9E3779B97F4A7C15ULL + (seed << 6) + (seed >> 2);
}

// Engine and environment info strings
[[nodiscard]] std::string engine_version_info();
[[nodiscard]] std::string compiler_info();

} // namespace Engine::Utils