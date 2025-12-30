# ==============================================================================
# Assembly Module
# ==============================================================================
# SIMD-optimized implementations of performance-critical functions
# Provides architecture-specific versions of math, transform, etc.
# ==============================================================================

# Detect assembler and set up assembly sources
# MSVC requires MASM (.asm) files which we don't have yet - use C fallbacks
if(MSVC)
    set(ASM_SOURCES)
    message(STATUS "ASM: MSVC detected, using C fallbacks (MASM files not yet available)")
else()
    # GCC/Clang use GAS syntax (.S files)
    enable_language(ASM)

    if(BAV_ARCH_X86_64)
        set(ASM_SOURCES
            x86_64/math_sse.S
            x86_64/math_avx.S
            x86_64/transform.S
        )
        message(STATUS "ASM: Using x86_64 SIMD implementations")
    elseif(BAV_ARCH_ARM64)
        set(ASM_SOURCES
            arm64/math_neon.S
            arm64/transform.S
        )
        message(STATUS "ASM: Using ARM64 NEON implementations")
    else()
        set(ASM_SOURCES)
        message(STATUS "ASM: No architecture-specific implementations")
    endif()
endif()

# Dispatch layer (C code that selects appropriate implementation)
add_library(bavarian_asm OBJECT
    dispatch.c
    ${ASM_SOURCES}
)

target_include_directories(bavarian_asm PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
