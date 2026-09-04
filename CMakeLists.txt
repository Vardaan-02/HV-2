cmake_minimum_required(VERSION 3.20)
project(v_chess LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags & native CPU features
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-O3 -march=native -Wall -Wextra -pedantic)
elseif (MSVC)
    add_compile_options(/O2 /arch:AVX2 /W4)
endif()

# Check for BMI2 (pext) support
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <immintrin.h>
    int main() {
        unsigned long long b = 0, m = 0;
        return (int)_pext_u64(b, m);
    }
" HAS_BMI2)

if (HAS_BMI2)
    add_compile_definitions(USE_PEXT)
    message(STATUS "Hardware PEXT (BMI2) detected and enabled.")
endif()

include_directories(src)

set(SOURCES
    src/core/bitboard.cpp
    src/utils/misc.cpp
    src/position/position.cpp
    src/movegen/movegen.cpp
    src/search/tt.cpp
    src/main.cpp
)

add_executable(v_chess ${SOURCES})