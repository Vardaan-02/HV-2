#pragma once

#include <bit>
#include <cstdint>

#if defined(_MSC_VER)
    #include <intrin.h>
    #if !defined(NO_PREFETCH)
        #include <xmmintrin.h>
    #endif
    #if defined(USE_PEXT)
        #include <immintrin.h>
    #endif
#else
    #if defined(USE_PEXT)
        #include <x86intrin.h>
    #endif
#endif

namespace Engine::Core {

#if defined(USE_PEXT)
constexpr bool HasPext = true;
#else
constexpr bool HasPext = false;
#endif

// Parallel bit extract (BMI2) with compile-time branch fallback
// returns 
[[nodiscard]] inline uint64_t pext64(uint64_t val, uint64_t mask) noexcept {
#if defined(USE_PEXT)
    return _pext_u64(val, mask);
#else
    (void)val;
    (void)mask;
    return 0;
#endif
}

// Prefetch memory into cache hierarchy
inline void prefetch(const void* addr) noexcept {
#if !defined(NO_PREFETCH)
    #if defined(_MSC_VER)
        _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
    #elif defined(__GNUC__) || defined(__clang__)
        __builtin_prefetch(addr);
    #endif
#else
    (void)addr;
#endif
}

// Returns number of set bits. 
[[nodiscard]] constexpr int popcount64(uint64_t bb) noexcept {
    return std::popcount(bb);
}

// Returns index of first 1 i.e. Return position of 1st occupied square
[[nodiscard]] constexpr int count_trailing_zeros64(uint64_t bb) noexcept {
    return std::countr_zero(bb);
}

} // namespace Engine::Core