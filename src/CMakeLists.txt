set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check for GCC/Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # -mbmi2 enables the BMI2 instruction set (or use -march=native)
    target_compile_options(your_engine_target PRIVATE -mbmi2)
    target_compile_definitions(your_engine_target PRIVATE USE_PEXT)

# Check for MSVC (Visual Studio)
elseif (MSVC)
    # /arch:AVX2 enables AVX2 and BMI2 instructions on MSVC
    target_compile_options(your_engine_target PRIVATE /arch:AVX2)
    target_compile_definitions(your_engine_target PRIVATE USE_PEXT)
endif()